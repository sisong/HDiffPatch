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
namespace sync_private{

    TStreamDataCache_base::TStreamDataCache_base(const hpatch_TStreamInput* baseStream,hpatch_StreamPos_t streamRollBegin,
                          hpatch_StreamPos_t streamRollEnd,uint32_t kSyncBlockSize,
                          hpatch_TChecksum* strongChecksumPlugin,void* _readLocker)
    :m_baseStream(baseStream),m_readedPos(streamRollBegin),m_streamRollEnd(streamRollEnd),
    m_strongChecksum_buf(0),m_cur(0),m_kSyncBlockSize(kSyncBlockSize),m_checksumByteSize(0),
    m_strongChecksumPlugin(strongChecksumPlugin),m_checksumHandle(0),m_readLocker(_readLocker){
        size_t cacheSize=(size_t)kSyncBlockSize*2;
        check((cacheSize>>1)==kSyncBlockSize,"TStreamDataCache_base mem error!");
        cacheSize=(cacheSize>=kBestReadSize)?cacheSize:kBestReadSize;
        hpatch_StreamPos_t maxDataSize=streamRollPosEnd()-streamRollBegin;
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
        //self:            [                         ]
        //                       ^             ^
        //                                 readedPos
        //cache:                 [             ]
        //                           ^
        //                          cur
        if (isRollEnded()) return;
        const hpatch_StreamPos_t rollPosEnd=streamRollPosEnd();
        if (m_readedPos>=rollPosEnd) { _setRollEnded(); return; }//set end tag

        size_t needLen=m_cur-m_cache.data();
        if (m_readedPos+needLen>rollPosEnd)
            needLen=(size_t)(rollPosEnd-m_readedPos);
        if (m_cur-needLen+m_kSyncBlockSize>m_cache.data_end()) { _setRollEnded(); return; } //set end tag

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


    const uint32_t* getSortedIndexs(TAutoMem& _mem_sorted,const TNewDataSyncInfo* newSyncInfo,
                                    TBloomFilter<tm_roll_uint>& filter){
        const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
        const uint32_t kMatchBlockCount=kBlockCount-newSyncInfo->samePairCount;
        const size_t savedRollHashByteSize=newSyncInfo->savedRollHashByteSize;
        _mem_sorted.realloc(kMatchBlockCount*(size_t)sizeof(uint32_t));
        uint32_t* sorted_newIndexs=(uint32_t*)_mem_sorted.data();
        filter.init(kMatchBlockCount);

        const uint8_t* partRollHash=newSyncInfo->rollHashs;
        uint32_t curPair=0;
        uint32_t indexi=0;
        for (uint32_t i=0;i<kBlockCount;++i,partRollHash+=savedRollHashByteSize){
            if ((curPair<newSyncInfo->samePairCount)&&(i==newSyncInfo->samePairList[curPair].curIndex)){
                ++curPair;
            }else{
                sorted_newIndexs[indexi++]=i;
                filter.insert(readRollHashBytes(partRollHash,savedRollHashByteSize));
            }
        }
        assert(indexi==kMatchBlockCount);
        assert(curPair==newSyncInfo->samePairCount);

        TIndex_comp01 icomp01(newSyncInfo->rollHashs,savedRollHashByteSize,
                              newSyncInfo->partChecksums,newSyncInfo->savedStrongChecksumByteSize);
        std::sort(sorted_newIndexs,sorted_newIndexs+kMatchBlockCount,icomp01);
        return sorted_newIndexs;
    }

    const uint32_t* getSortedIndexsTable(TAutoMem& _mem_table,const TNewDataSyncInfo* newSyncInfo,
                                         const uint32_t* sorted_newIndexs,unsigned int* out_kTableHashShlBit){//optimize for std::equal_range
        unsigned int& kTableHashShlBit=*out_kTableHashShlBit;
        const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
        const uint32_t kMatchBlockCount=kBlockCount-newSyncInfo->samePairCount;
        const size_t savedRollHashByteSize=newSyncInfo->savedRollHashByteSize;
        unsigned int kTableBit =getBetterCacheBlockTableBit(kMatchBlockCount);
        const size_t savedRollHashBits=newSyncInfo->savedRollHashBits;
        if (kTableBit>savedRollHashBits) kTableBit=(unsigned int)savedRollHashBits;
        kTableHashShlBit=(int)(savedRollHashBits-kTableBit);
        _mem_table.realloc((size_t)sizeof(uint32_t)*((1<<kTableBit)+1));
        uint32_t* sorted_newIndexs_table=(uint32_t*)_mem_table.data();
        {
            TIndex_comp0  icomp0(newSyncInfo->rollHashs,savedRollHashByteSize);
            const uint32_t* pos=sorted_newIndexs;
            uint8_t part[sizeof(tm_roll_uint)]={0};
            for (uint32_t i=0; i<((uint32_t)1<<kTableBit); ++i) {
                tm_roll_uint digest=((tm_roll_uint)i)<<kTableHashShlBit;
                writeRollHashBytes(part,digest,savedRollHashByteSize);
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

#if (_IS_USED_MULTITHREAD)
struct _TMatchDatas;
namespace{
    struct TMt:public TMtByChannel{
        inline explicit TMt(struct _TMatchDatas& _matchDatas)
            :matchDatas(_matchDatas),workIndex(0){}
        CHLocker readLocker;
        CHLocker writeLocker;
    #if (_IS_USED_CPP_ATOMIC)
        std::atomic<size_t>  workIndex;
    #else
        CHLocker         workiLocker;
        volatile size_t  workIndex;
        inline bool getWorki(size_t worki){
            if (workIndex>worki) return false;
            CAutoLocker _auto_locker(workiLocker.locker);
            if (workIndex==worki){
                ++workIndex;
                return true;
            }else
                return false;
        }
    #endif
        struct _TMatchDatas& matchDatas;
    };
}
#endif //_IS_USED_MULTITHREAD

static bool matchRange(hpatch_StreamPos_t* out_newBlockDataInOldPoss,const uint32_t* range_begin,const uint32_t* range_end,
                       TStreamDataCache_base& oldData,const TNewDataSyncInfo* newSyncInfo,hpatch_StreamPos_t kMinRevSameIndex,void* _mt
  #if (kIsSkipMatchedBlock==3)
                       ,bool _isSkipContinue){
  #else
                       ){
  #endif
    const TByte* oldPartStrongChecksum=0;
    const size_t outPartChecksumSize=_bitsToBytes(newSyncInfo->savedStrongChecksumBits);
    bool isMatched=false;
    int hitOutLimit=kMatchHitOutLimit;
    do {
        uint32_t newBlockIndex=*range_begin;
        volStreamPos_t* pNewBlockDataInOldPos=&out_newBlockDataInOldPoss[newBlockIndex];
        hpatch_StreamPos_t newBlockOldPosBack=*pNewBlockDataInOldPos;   
      #if (kIsSkipMatchedBlock==3)
        if (_isSkipContinue|(newBlockOldPosBack>=kMinRevSameIndex)){
      #else
        if (newBlockOldPosBack>=kMinRevSameIndex){
      #endif
            if (oldPartStrongChecksum==0)
                oldPartStrongChecksum=oldData.calcPartStrongChecksum(newSyncInfo->savedStrongChecksumBits);
            const TByte* newPairStrongChecksum=newSyncInfo->partChecksums+newBlockIndex*outPartChecksumSize;
            if (0==memcmp(oldPartStrongChecksum,newPairStrongChecksum,outPartChecksumSize)){
                isMatched=true;
              #if (kIsSkipMatchedBlock==3)
                if (newBlockOldPosBack<kMinRevSameIndex){
                    if ((--hitOutLimit)<=0) break;
                }
              #endif
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
                            checkChecksumAppendData(newSyncInfo->savedNewDataCheckChecksum,newBlockIndex,
                                                    oldData.strongChecksumPlugin(),oldData.checkChecksum(),
                                                    oldData.strongChecksum(),oldData.strongChecksumByteSize());
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


struct _TMatchDatas{
    hpatch_StreamPos_t*         out_newBlockDataInOldPoss;
    const TNewDataSyncInfo*     newSyncInfo;
    const hpatch_TStreamInput*  oldStream;
    hpatch_TChecksum*           strongChecksumPlugin;
    const void*         filter;
    const uint32_t*     sorted_newIndexs;
    const uint32_t*     sorted_newIndexs_table;
    size_t              kMatchBlockCount;
    unsigned int        kTableHashShlBit;
    uint32_t            threadNum;
};

#define _DEF_isSkipContinue()  \
            const hpatch_StreamPos_t curOldPos=oldData.curStreamPos(); \
            const bool _isSkipContinue=(curOldPos==_skipPosBack0)|(curOldPos==_skipPosBack1)|(curOldPos==_skipPosBack2)
    
static void _rollMatch(_TMatchDatas& rd,hpatch_StreamPos_t oldRollBegin,
                       hpatch_StreamPos_t oldRollEnd,void* _mt=0){
    if (rd.oldStream->streamSize<rd.newSyncInfo->kSyncBlockSize)
        return;
    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(rd.newSyncInfo);
    const hpatch_StreamPos_t kMinRevSameIndex=kBlockType_needSync-1-kBlockCount;
    TIndex_comp0 icomp0(rd.newSyncInfo->rollHashs,rd.newSyncInfo->savedRollHashByteSize);
    TStreamDataRoll oldData(rd.oldStream,oldRollBegin,oldRollEnd,
                            rd.newSyncInfo->kSyncBlockSize,rd.strongChecksumPlugin
                        #if (_IS_USED_MULTITHREAD)
                            ,_mt?((TMt*)_mt)->readLocker.locker:0
                        #endif
                            );
    uint8_t part[sizeof(tm_roll_uint)]={0};
    const size_t savedRollHashBits=rd.newSyncInfo->savedRollHashBits;
    const TBloomFilter<tm_roll_uint>& filter=*(TBloomFilter<tm_roll_uint>*)rd.filter;
    tm_roll_uint digestFull_back=~oldData.hashValue(); //not same digest
  #if (kIsSkipMatchedBlock>=2)
    hpatch_StreamPos_t _skipPosBack0=0;
    hpatch_StreamPos_t _skipPosBack1=0;
    hpatch_StreamPos_t _skipPosBack2=oldData.curStreamPos();
  #endif
    while (true) {
        tm_roll_uint digest=oldData.hashValue();
        if (digestFull_back!=digest){
            digestFull_back=digest;
            digest=toSavedPartRollHash(digest,savedRollHashBits);
            if (!filter.is_hit(digest))
                { if (oldData.roll()) continue; else break; }//finish
        }else{
            if (oldData.roll()) continue; else break; //finish
        }
        
        const uint32_t* ti_pos=&rd.sorted_newIndexs_table[digest>>rd.kTableHashShlBit];
        writeRollHashBytes(part,digest,rd.newSyncInfo->savedRollHashByteSize);
        TIndex_comp0::TDigest digest_value(part);
        std::pair<const uint32_t*,const uint32_t*>
        //range=std::equal_range(rd.sorted_newIndexs,rd.sorted_newIndexs+rd.kMatchBlockCount,digest_value,icomp0);
        range=std::equal_range(rd.sorted_newIndexs+ti_pos[0],
                               rd.sorted_newIndexs+ti_pos[1],digest_value,icomp0);
        if (range.first==range.second)
            { if (oldData.roll()) continue; else break; }//finish

      #if (kIsSkipMatchedBlock==3)
        _DEF_isSkipContinue();
      #endif
        bool isMatched=matchRange(rd.out_newBlockDataInOldPoss,range.first,range.second,oldData,
                                  rd.newSyncInfo,kMinRevSameIndex,_mt
      #if (kIsSkipMatchedBlock==3)
                                  ,_isSkipContinue);
      #else
                                  );
      #endif
        if (kIsSkipMatchedBlock&&isMatched){
          #if (kIsSkipMatchedBlock==2)
            _DEF_isSkipContinue();
          #endif
          #if (kIsSkipMatchedBlock>=2)
            _skipPosBack0=_skipPosBack1;
            _skipPosBack1=_skipPosBack2;
            _skipPosBack2=curOldPos+rd.newSyncInfo->kSyncBlockSize;
            if (_isSkipContinue)
          #endif
            {
                if (oldData.nextBlock()) { digestFull_back=~oldData.hashValue(); continue; } else break;
            }//else if (!_isSkipContinue) roll
        }//else roll
        if (oldData.roll()) continue; else break;
    }
}

#if (_IS_USED_MULTITHREAD)
static void _rollMatch_mt(int threadIndex,void* workData){
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
            _rollMatch(mt.matchDatas,oldPosBegin,oldPosEnd,&mt);
        }
    }catch(...){
        mt.on_error();
    }
}
#endif

    static inline void _init_newBlockDataInOldPoss(hpatch_StreamPos_t* newBlockDataInOldPoss,uint32_t kBlockCount){
        for (uint32_t i=0; i<kBlockCount; ++i) newBlockDataInOldPoss[i]=kBlockType_needSync; }

static void _matchNewDataInOld(_TMatchDatas& matchDatas,int threadNum){
    const TNewDataSyncInfo* newSyncInfo=matchDatas.newSyncInfo;
    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    const uint32_t kMatchBlockCount=kBlockCount-newSyncInfo->samePairCount;
    hpatch_StreamPos_t* out_newBlockDataInOldPoss=matchDatas.out_newBlockDataInOldPoss;
    _init_newBlockDataInOldPoss(out_newBlockDataInOldPoss,kBlockCount);
    
    TAutoMem _mem_sorted;
    TBloomFilter<tm_roll_uint> filter;
    const uint32_t* sorted_newIndexs=getSortedIndexs(_mem_sorted,newSyncInfo,filter);
    {
        checkv(matchDatas.oldStream->streamSize<(hpatch_StreamPos_t)(kBlockType_needSync-1-kBlockCount));
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

    matchDatas.filter=&filter;
    matchDatas.sorted_newIndexs=sorted_newIndexs;
    matchDatas.kMatchBlockCount=kMatchBlockCount;
    matchDatas.sorted_newIndexs_table=sorted_newIndexs_table;
    matchDatas.kTableHashShlBit=kTableHashShlBit;
#if (_IS_USED_MULTITHREAD)
    const hpatch_StreamPos_t _bestWorkCount=matchDatas.oldStream->streamSize/kBestMTClipSize;
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
        _rollMatch(matchDatas,0,matchDatas.oldStream->streamSize);
    }
    {
        hpatch_StreamPos_t oldSize=matchDatas.oldStream->streamSize;
        for (uint32_t i=0; i<kBlockCount; ++i){
            hpatch_StreamPos_t& pos=out_newBlockDataInOldPoss[i];
            pos=(pos<oldSize)?pos:kBlockType_needSync;
        }
    }
}

    
void matchNewDataInOld(hpatch_StreamPos_t* out_newBlockDataInOldPoss,const TNewDataSyncInfo* newSyncInfo,
                       const hpatch_TStreamInput* oldStream,hpatch_TChecksum* strongChecksumPlugin,int threadNum){
    _TMatchDatas matchDatas; memset(&matchDatas,0,sizeof(matchDatas));
    matchDatas.out_newBlockDataInOldPoss=out_newBlockDataInOldPoss;
    matchDatas.newSyncInfo=newSyncInfo;
    matchDatas.oldStream=oldStream;
    matchDatas.strongChecksumPlugin=strongChecksumPlugin;
    _matchNewDataInOld(matchDatas,threadNum);
}

} //namespace sync_private
