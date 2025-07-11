//  match_in_old.cpp
//  sync_client
//  Created by housisong on 2019-09-22.
/*
 The MIT License (MIT)
 Copyright (c) 2019-2023 HouSisong

 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation
 files (the "Software"), to deal in the Software without
 restriction, including without limitation the rights to use,
 copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following
 conditions:

 The above copyright notice and this permission notice shall be
 included in all copies of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
 */
#include "match_in_old.h"
#include "match_in_types.h"
#include "sync_client_private.h"
namespace sync_private{

    #define check(value,info) { if (!(value)) { throw std::runtime_error(info); } }
    #define checkv(value)     check(value,"check "#value" error!")

    TStreamDataCache_base::TStreamDataCache_base(const hpatch_TStreamInput* baseStream,hpatch_StreamPos_t streamRollBegin,
                          hpatch_StreamPos_t streamRollEnd,size_t backupSize,size_t backZeroLen,uint32_t kSyncBlockSize,
                          hpatch_TChecksum* strongChecksumPlugin,void* _readLocker)
    :m_baseStream(baseStream),m_readedPos(streamRollBegin),m_streamRollEnd(streamRollEnd),
    m_kBackupSize(backupSize),m_kBackZeroLen(backZeroLen),
    m_strongChecksum_buf(0),m_cur(0),m_kSyncBlockSize(kSyncBlockSize),m_checksumByteSize(0),
    m_strongChecksumPlugin(strongChecksumPlugin),m_checksumHandle(0),m_readLocker(_readLocker){
        size_t cacheSize=(size_t)kSyncBlockSize*2+m_kBackupSize;
        check((((cacheSize-m_kBackupSize)>>1)==kSyncBlockSize),"TStreamDataCache_base mem error!");
        cacheSize=(cacheSize>=kBestReadSize)?cacheSize:kBestReadSize;
        hpatch_StreamPos_t maxDataSize=(streamRollPosEnd()-streamRollBegin);
        if (cacheSize>maxDataSize) cacheSize=(size_t)maxDataSize;
#if (_IS_USED_MULTITHREAD)
        if (_readLocker)
            cacheSize=(size_t)maxDataSize;
#endif

        m_checksumHandle=strongChecksumPlugin->open(strongChecksumPlugin);
        checkv(m_checksumHandle!=0);
        m_checkChecksum=strongChecksumPlugin->open(strongChecksumPlugin);
        checkv(m_checkChecksum!=0);
        m_checksumByteSize=(uint32_t)m_strongChecksumPlugin->checksumByteSize();
        m_cache.realloc(cacheSize+m_checksumByteSize+m_checksumByteSize);
        m_cache.reduceSize(cacheSize);
        m_strongChecksum_buf=m_cache.data_end();
        m_cur=m_cache.data_end();
        _cache();
    }
    TStreamDataCache_base::~TStreamDataCache_base(){
        if (m_checksumHandle)
            m_strongChecksumPlugin->close(m_strongChecksumPlugin,m_checksumHandle);
        if (m_checkChecksum)
            m_strongChecksumPlugin->close(m_strongChecksumPlugin,m_checkChecksum);
    }
    void TStreamDataCache_base::_cache(){
        // all:[        streamDataSize      +     backZeroLen    ]
        //                 ^                         ^
        //          streamRollBegin            streamRollEnd
        //self:            [                          ]
        //                       ^             ^
        //                                 readedPos
        //
        //cache:             [         | kBackupSize ]
        //                        ^
        //                       cur
        if (isRollEnded()) return;
        const hpatch_StreamPos_t rollPosEnd=streamRollPosEnd();
        if (m_readedPos>=rollPosEnd) { _setRollEnded(); return; }//set end tag

        size_t needLen=m_cur-m_cache.data();
        if (m_readedPos+needLen>rollPosEnd)
            needLen=(size_t)(rollPosEnd-m_readedPos);
        if (m_cur-needLen+m_kSyncBlockSize+m_kBackupSize>m_cache.data_end()) { _setRollEnded(); return; } //set end tag

        memmove(m_cur-needLen,m_cur,m_cache.data_end()-m_cur);
        size_t readLen=needLen;
        if (m_readedPos+readLen>m_baseStream->streamSize){
            if (m_readedPos<m_baseStream->streamSize)
                readLen=(size_t)(m_baseStream->streamSize-m_readedPos);
            else
                readLen=0;
        }
        if (readLen>0){
            TByte* buf=m_cache.data_end()-needLen;
#if (_IS_USED_MULTITHREAD)
            CAutoLocker _autoLocker((HLocker)m_readLocker);
#endif
            check(m_baseStream->read(m_baseStream,m_readedPos,buf,buf+readLen),
                  "TStreamDataRoll read streamData error!");
        }
        size_t zerolen=needLen-readLen;
        if (zerolen>0)
            memset(m_cache.data_end()-zerolen,0,zerolen);
        m_readedPos+=needLen;
        m_cur-=needLen;
    }

    const TByte* TStreamDataCache_base::_calcPartStrongChecksum(const TByte* buf,size_t bufSize,size_t outPartBits){
        TByte* strongChecksum=m_strongChecksum_buf+m_checksumByteSize;
        m_strongChecksumPlugin->begin(m_checksumHandle);
        m_strongChecksumPlugin->append(m_checksumHandle,buf,buf+bufSize);
        m_strongChecksumPlugin->end(m_checksumHandle,strongChecksum,strongChecksum+m_checksumByteSize);
        toPartChecksum(m_strongChecksum_buf,outPartBits,strongChecksum,m_checksumByteSize);
        return m_strongChecksum_buf;
    }

    const uint32_t* getSortedIndexsTable(TAutoMem& _mem_table,const TNewDataSyncInfo* newSyncInfo,
                                         const uint32_t* sorted_newIndexs,unsigned int* out_kTableHashShlBit){//optimize for std::equal_range
        unsigned int& kTableHashShlBit=*out_kTableHashShlBit;
        const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
        const uint32_t kMatchBlockCount=kBlockCount-newSyncInfo->samePairCount;
        const size_t needRollHashByteSize=newSyncInfo->savedRollHashByteSize*(newSyncInfo->isSeqMatch?2:1);
        unsigned int kTableBit =getBetterCacheBlockTableBit(kMatchBlockCount);
        const size_t needRollHashBits=newSyncInfo->savedRollHashBits*(newSyncInfo->isSeqMatch?2:1);
        if (kTableBit>needRollHashBits) kTableBit=(unsigned int)needRollHashBits;
        kTableHashShlBit=(int)(needRollHashBits-kTableBit);
        _mem_table.realloc((size_t)sizeof(uint32_t)*((1<<kTableBit)+1));
        uint32_t* sorted_newIndexs_table=(uint32_t*)_mem_table.data();
        {
            TIndex_comp0 icomp0(newSyncInfo->rollHashs,newSyncInfo->savedRollHashByteSize,newSyncInfo->isSeqMatch);
            const uint32_t* pos=sorted_newIndexs;
            uint8_t part[sizeof(tm_roll_uint)]={0};
            for (uint32_t i=0; i<((uint32_t)1<<kTableBit); ++i) {
                tm_roll_uint digest=((tm_roll_uint)i)<<kTableHashShlBit;
                writeRollHashBytes(part,digest,needRollHashByteSize);
                TIndex_comp0::TDigest digest_value(part);
                pos=std::lower_bound(pos,sorted_newIndexs+kMatchBlockCount,digest_value,icomp0);
                sorted_newIndexs_table[i]=(uint32_t)(pos-sorted_newIndexs);
            }
            sorted_newIndexs_table[((size_t)1<<kTableBit)]=kMatchBlockCount;
        }
        return sorted_newIndexs_table;
    }


static inline hpatch_StreamPos_t _indexMapTo(uint32_t i){
    return (hpatch_StreamPos_t)(kBlockType_needSync-1)-i; }
static inline uint32_t _indexMapFrom(hpatch_StreamPos_t pos){
    return (uint32_t)((kBlockType_needSync-1)-pos); }

typedef volatile hpatch_StreamPos_t volStreamPos_t;

bool _matchRange(hpatch_StreamPos_t* out_newBlockDataInOldPoss,const uint32_t* range_begin,const uint32_t* range_end,
                 TStreamDataCache_base& oldData,const TNewDataSyncInfo* newSyncInfo,hpatch_StreamPos_t kMinRevSameIndex,void* _mt){
    const TByte* oldPartStrongChecksum=0;
    const size_t savedStrongChecksumByteSize=newSyncInfo->savedStrongChecksumByteSize;
    bool isMatched=false;
    int hitOutLimit=kMatchHitOutLimit;
    do {
        size_t newBlockIndex=*range_begin;
        volStreamPos_t* pNewBlockDataInOldPos=&out_newBlockDataInOldPoss[newBlockIndex];
        hpatch_StreamPos_t newBlockOldPosBack=*pNewBlockDataInOldPos;   
        if (newBlockOldPosBack>=kMinRevSameIndex){
            if (oldPartStrongChecksum==0)
                oldPartStrongChecksum=oldData.calcPartStrongChecksum(newSyncInfo->savedStrongChecksumBits);
            const TByte* newPairStrongChecksum=newSyncInfo->partChecksums+newBlockIndex*savedStrongChecksumByteSize;
            if (0==memcmp(oldPartStrongChecksum,newPairStrongChecksum,savedStrongChecksumByteSize)){
                isMatched=true;
                hpatch_StreamPos_t curPos=oldData.curStreamPos();
                {
#if (_IS_USED_MULTITHREAD)
                    TMt* mt=(TMt*)_mt;
                    CAutoLocker _autoLocker(mt?mt->writeLocker.locker:0);
                    newBlockOldPosBack=*pNewBlockDataInOldPos;
                    if (newBlockOldPosBack<kMinRevSameIndex){// other thread done
                        if ((--hitOutLimit)<=0) break;
                    }else
#endif
                    {
                        while(true){ //hit
                            (*pNewBlockDataInOldPos)=curPos;
                            if (!newSyncInfo->isNotCheckChecksumNewWhenMatch)
                                checkChecksumAppendData(newSyncInfo->savedNewDataCheckChecksum,(uint32_t)newBlockIndex,
                                                        oldData.strongChecksumPlugin(),oldData.checkChecksum(),
                                                        oldData.strongChecksum(),0,0);
                            if (newBlockOldPosBack==kBlockType_needSync)
                                break;
                            //next same block
                            newBlockIndex=_indexMapFrom(newBlockOldPosBack);
                            //assert(newBlockIndex<kBlockCount);
                            pNewBlockDataInOldPos=&out_newBlockDataInOldPoss[newBlockIndex];
                            newBlockOldPosBack=*pNewBlockDataInOldPos;
                        }
                        //continue;
                    }
                }
            }else{
                if ((--hitOutLimit)<=0) break;
            }
        }else{
            if ((--hitOutLimit)<=0) break;
        }
    }while ((++range_begin)!=range_end);
    return isMatched;
}

#if (_IS_USED_MULTITHREAD)
void _rollMatch_mt(int threadIndex,void* workData){
    TMt& mt=*(TMt*)workData;
    TMtByChannel::TAutoThreadEnd __auto_thread_end(mt);

    hpatch_StreamPos_t clipSize=kBestMTClipSize;
    hpatch_StreamPos_t _minClipSize=(hpatch_StreamPos_t)(mt.matchDatas.newSyncInfo->kSyncBlockSize)*4;
    if (clipSize<_minClipSize) clipSize=_minClipSize;
    uint32_t threadNum=mt.matchDatas.threadNum;
    hpatch_StreamPos_t oldSize=mt.matchDatas.oldStream->streamSize;
    hpatch_StreamPos_t _maxClipSize=(oldSize+threadNum-1)/threadNum;
    if (clipSize>_maxClipSize) clipSize=_maxClipSize;
    size_t workCount=(size_t)((oldSize+clipSize-1)/clipSize);
    assert(workCount*clipSize>=oldSize);
    try{
    #if (_IS_USED_CPP_ATOMIC)
        std::atomic<size_t>& workIndex=*(std::atomic<size_t>*)&mt.workIndex;
        while (true){
            size_t curWorkIndex=workIndex++;
            if (curWorkIndex>=workCount) break;
    #else
        for (size_t curWorkIndex=0;curWorkIndex<workCount;++curWorkIndex) {
            if (!mt.getWorki(curWorkIndex))
                continue;
    #endif
            hpatch_StreamPos_t oldPosBegin=curWorkIndex*clipSize;
            hpatch_StreamPos_t oldPosEnd = oldPosBegin+clipSize;
            if (oldPosEnd>oldSize) oldPosEnd=oldSize;
            mt.matchDatas.rollMatch(mt.matchDatas,oldPosBegin,oldPosEnd,&mt);
        }
    }catch(...){
        mt.on_error();
    }
}
#endif


void _matchNewDataInOld(_TMatchDatas& matchDatas,int threadNum){
    const TNewDataSyncInfo* newSyncInfo=matchDatas.newSyncInfo;
    checkv((!newSyncInfo->isSeqMatch)||(0==newSyncInfo->samePairCount));
    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    hpatch_StreamPos_t* out_newBlockDataInOldPoss=matchDatas.out_newBlockDataInOldPoss;
    _init_newBlockDataInOldPoss(out_newBlockDataInOldPoss,kBlockCount);
    
    const hpatch_StreamPos_t oldDataSize=matchDatas.oldStream->streamSize;
    if (oldDataSize==0) return;
    checkv(oldDataSize<(hpatch_StreamPos_t)(kBlockType_needSync-1-kBlockCount));
    
    TAutoMem _mem_sorted;
    const uint32_t* sorted_newIndexs=matchDatas.getSortedIndexs(_mem_sorted,newSyncInfo,matchDatas.filter);
    {
        uint32_t curPair=0;
        for (uint32_t i=0;i<kBlockCount;++i){
            if ((curPair<newSyncInfo->samePairCount)&&(i==newSyncInfo->samePairList[curPair].curIndex)){
                uint32_t sameIndex=newSyncInfo->samePairList[curPair].sameIndex;
                while (out_newBlockDataInOldPoss[sameIndex]!=kBlockType_needSync){
                    sameIndex=_indexMapFrom(out_newBlockDataInOldPoss[sameIndex]);
                    assert(sameIndex<kBlockCount);
                }
                assert(sameIndex<i);
                out_newBlockDataInOldPoss[sameIndex]=_indexMapTo(i);
                ++curPair;
            }
        }
        assert(curPair==newSyncInfo->samePairCount);
    }
    
    TAutoMem _mem_table;
    unsigned int kTableHashShlBit=0;
    const uint32_t* sorted_newIndexs_table=getSortedIndexsTable(_mem_table,newSyncInfo,sorted_newIndexs,&kTableHashShlBit);

    matchDatas.sorted_newIndexs=sorted_newIndexs;
    matchDatas.sorted_newIndexs_table=sorted_newIndexs_table;
    matchDatas.kTableHashShlBit=kTableHashShlBit;
#if (_IS_USED_MULTITHREAD)
    const hpatch_StreamPos_t _bestWorkCount=oldDataSize/kBestMTClipSize;
    threadNum=(_bestWorkCount<=threadNum)?(int)_bestWorkCount:threadNum;
    if (threadNum>1){
        matchDatas.threadNum=threadNum;
        TMt mt(matchDatas);
        checkv(mt.start_threads(threadNum,_rollMatch_mt,&mt,true));
        mt.wait_all_thread_end();
        checkv(!mt.is_on_error());
    }else
#endif
    {
        matchDatas.rollMatch(matchDatas,0,oldDataSize,0);
    }

    _end_newBlockDataInOldPoss(out_newBlockDataInOldPoss,kBlockCount,oldDataSize);
}

static void _rollMatch(_TMatchDatas& rd,hpatch_StreamPos_t oldRollBegin,hpatch_StreamPos_t oldRollEnd,void* _mt){
    _tm_rollMatch<TStreamDataRoll,TBloomFilter<tm_roll_uint>,false>
            (rd,oldRollBegin,oldRollEnd,toSavedPartRollHash,_mt);
}

const uint32_t* getSortedIndexs(TAutoMem& _mem_sorted,const TNewDataSyncInfo* newSyncInfo,TBloomFilter<tm_roll_uint>& filter){
    return _tm_getSortedIndexs(_mem_sorted,newSyncInfo,filter);
}
static const uint32_t* _getSortedIndexs(TAutoMem& _mem_sorted,const TNewDataSyncInfo* newSyncInfo,void* filter){
    return getSortedIndexs(_mem_sorted,newSyncInfo,*(TBloomFilter<tm_roll_uint>*)filter);
}

void matchNewDataInOld(hpatch_StreamPos_t* out_newBlockDataInOldPoss,const TNewDataSyncInfo* newSyncInfo,
                       const hpatch_TStreamInput* oldStream,int threadNum){
    TBloomFilter<tm_roll_uint> filter;
    filter.init((size_t)(TNewDataSyncInfo_blockCount(newSyncInfo)-newSyncInfo->samePairCount));

    _TMatchDatas matchDatas; memset(&matchDatas,0,sizeof(matchDatas));
    matchDatas.filter=&filter;
    matchDatas.out_newBlockDataInOldPoss=out_newBlockDataInOldPoss;
    matchDatas.newSyncInfo=newSyncInfo;
    matchDatas.oldStream=oldStream;
    matchDatas.rollMatch=_rollMatch;
    matchDatas.getSortedIndexs=_getSortedIndexs;
    _matchNewDataInOld(matchDatas,threadNum);
}

} //namespace sync_private
