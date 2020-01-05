//  match_in_old.cpp
//  sync_client
//  Created by housisong on 2019-09-22.
/*
 The MIT License (MIT)
 Copyright (c) 2019-2019 HouSisong

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
#include "string.h" //memmove
#include <algorithm> //sort, equal_range lower_bound
#include "../../libHDiffPatch/HDiff/private_diff/mem_buf.h"
#include "../../libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.h"
#include "../../libHDiffPatch/HDiff/private_diff/limit_mem_diff/bloom_filter.h"
#include "mt_by_queue.h"

#define check(value,info) { if (!(value)) { throw std::runtime_error(info); } }
#define checkv(value)     check(value,"check "#value" error!")

using namespace hdiff_private;
typedef unsigned char TByte;

template<class tm_roll_uint>
struct TIndex_comp{
    inline explicit TIndex_comp(const tm_roll_uint* _blocks):blocks(_blocks){ }
    struct TDigest{
        tm_roll_uint value;
        inline explicit TDigest(tm_roll_uint _value):value(_value){}
    };
    template<class TIndex> inline
    bool operator()(const TIndex& x,const TDigest& y)const { return blocks[x]<y.value; }
    template<class TIndex> inline
    bool operator()(const TDigest& x,const TIndex& y)const { return x.value<blocks[y]; }
    template<class TIndex> inline
    bool operator()(const TIndex& x, const TIndex& y)const { return blocks[x]<blocks[y]; }
protected:
    const tm_roll_uint* blocks;
};

struct TOldDataCache_base {
    TOldDataCache_base(const hpatch_TStreamInput* oldStream,hpatch_StreamPos_t oldRollBegin,
                       hpatch_StreamPos_t oldRollEnd,uint32_t kMatchBlockSize,
                       hpatch_TChecksum* strongChecksumPlugin,void* _mt=0)
    :m_oldStream(oldStream),m_readedPos(oldRollBegin),m_oldRollEnd(oldRollEnd),
    m_strongChecksum_buf(0),m_cur(0),m_kMatchBlockSize(kMatchBlockSize),m_checksumByteSize(0),
    m_strongChecksumPlugin(strongChecksumPlugin),m_checksumHandle(0),m_mt(_mt){
        size_t cacheSize=(size_t)kMatchBlockSize*2;
        check((cacheSize>>1)==kMatchBlockSize,"TOldDataCache mem error!");
        cacheSize=(cacheSize>=hpatch_kFileIOBufBetterSize)?cacheSize:hpatch_kFileIOBufBetterSize;

        m_checksumHandle=strongChecksumPlugin->open(strongChecksumPlugin);
        checkv(m_checksumHandle!=0);
        m_checksumByteSize=(uint32_t)m_strongChecksumPlugin->checksumByteSize();
        m_cache.realloc(cacheSize+m_checksumByteSize);
        m_cache.reduceSize(cacheSize);
        m_strongChecksum_buf=m_cache.data_end();
        _initCache();
    }
    ~TOldDataCache_base(){
        if (m_checksumHandle)
            m_strongChecksumPlugin->close(m_strongChecksumPlugin,m_checksumHandle);
    }
    void _initCache(){
        hpatch_StreamPos_t dataSize=m_oldRollEnd-m_readedPos;
        if (dataSize<m_kMatchBlockSize){ m_cur=0; return; } //end

        if (m_cache.size()>dataSize)
            m_cache.reduceSize((size_t)dataSize);
        m_cur=m_cache.data_end();
        _cache();
    }
    // all:[          oldDataSize         +     backZeroLen    ]
    //                 ^                         ^
    //            oldRollBegin               oldRollEnd
    //self:            [                         ]
    //                       ^             ^
    //                                 readedPos
    //cache:                 [             ]
    //                           ^
    //                          cur
    void _cache(){
        if (m_readedPos >= m_oldRollEnd){ m_cur=0; return; } //end

        size_t needLen=m_cur-m_cache.data();
        if (m_readedPos+needLen>m_oldRollEnd)
            needLen=(size_t)(m_oldRollEnd-m_readedPos);
        memmove(m_cur-needLen,m_cur,m_cache.data_end()-m_cur);
        size_t readLen=needLen;
        if (m_readedPos+readLen>m_oldStream->streamSize){
            if (m_readedPos<=m_oldStream->streamSize)
                readLen=(size_t)(m_oldStream->streamSize-m_readedPos);
            else
                readLen=0;
        }
        if (readLen>0){
#if (_IS_USED_MULTITHREAD)
            TMt_by_queue::TAutoInputLocker _autoLocker((TMt_by_queue*)m_mt);
#endif
            TByte* buf=m_cache.data_end()-needLen;
            check(m_oldStream->read(m_oldStream,m_readedPos,buf,buf+readLen),
                  "TOldDataCache read oldData error!");
        }
        size_t zerolen=needLen-readLen;
        if (zerolen>0)
            memset(m_cache.data_end()-zerolen,0,zerolen);
        m_readedPos+=needLen;
        m_cur-=needLen;
    }

    inline bool isEnd()const{ return m_cur==0; }
    inline const TByte* calcPartStrongChecksum(){
        return _calcPartStrongChecksum(m_cur,m_kMatchBlockSize);
    }
    inline const TByte* calcLastPartStrongChecksum(size_t lastNewNodeSize){
        return _calcPartStrongChecksum(m_cache.data_end()-lastNewNodeSize,lastNewNodeSize);
    }
    inline hpatch_StreamPos_t curOldPos()const{
        return m_cur-m_cache.data();
    }
protected:
    const hpatch_TStreamInput* m_oldStream;
    hpatch_StreamPos_t      m_readedPos;
    hpatch_StreamPos_t      m_oldRollEnd;
    TAutoMem                m_cache;
    TByte*                  m_strongChecksum_buf;
    TByte*                  m_cur;
    uint32_t                m_kMatchBlockSize;
    uint32_t                m_checksumByteSize;
    hpatch_TChecksum*       m_strongChecksumPlugin;
    hpatch_checksumHandle   m_checksumHandle;
    void*                   m_mt;

    inline const TByte* _calcPartStrongChecksum(const TByte* buf,size_t bufSize){
        m_strongChecksumPlugin->begin(m_checksumHandle);
        m_strongChecksumPlugin->append(m_checksumHandle,buf,buf+bufSize);
        m_strongChecksumPlugin->end(m_checksumHandle,m_strongChecksum_buf,
                                    m_strongChecksum_buf+m_checksumByteSize);
        toSyncPartChecksum(m_strongChecksum_buf,m_strongChecksum_buf,m_checksumByteSize);
        return m_strongChecksum_buf;
    }
};

template<class tm_roll_uint>
struct TOldDataCache:public TOldDataCache_base {
    inline TOldDataCache(const hpatch_TStreamInput* oldStream,hpatch_StreamPos_t oldRollBegin,
                         hpatch_StreamPos_t oldRollEnd,uint32_t kMatchBlockSize,
                         hpatch_TChecksum* strongChecksumPlugin,void* _mt=0)
            :TOldDataCache_base(oldStream,oldRollBegin,oldRollEnd,kMatchBlockSize,
                                strongChecksumPlugin,_mt){
                if (isEnd()) return;
                m_roolHash=roll_hash_start((tm_roll_uint*)0,m_cur,m_kMatchBlockSize);
            }
    void _cache(){
        TOldDataCache_base::_cache();
        if (isEnd()) return;
        roll();
    }
    inline tm_roll_uint hashValue()const{ return m_roolHash; }
    inline void roll(){
        const TByte* curIn=m_cur+m_kMatchBlockSize;
        if (curIn!=m_cache.data_end()){
            m_roolHash=roll_hash_roll(m_roolHash,m_kMatchBlockSize,*m_cur,*curIn);
            ++m_cur;
        }else{
            _cache();
        }
    }
protected:
    tm_roll_uint            m_roolHash;
};

inline static size_t getBackZeroLen(hpatch_StreamPos_t newDataSize,uint32_t kMatchBlockSize){
    size_t len=(size_t)(newDataSize % kMatchBlockSize);
    if (len!=0) len=kMatchBlockSize-len;
    return len;
}

static void matchRange(hpatch_StreamPos_t* out_newDataPoss,const TByte* partChecksums,TOldDataCache_base& oldData,
                       const uint32_t* range_begin,const uint32_t* range_end,void* _mt=0){
    const TByte* oldPartStrongChecksum=0;
    do {
        uint32_t newBlockIndex=*range_begin;
        if (out_newDataPoss[newBlockIndex]==kBlockType_needSync){
            if (oldPartStrongChecksum==0)
                oldPartStrongChecksum=oldData.calcPartStrongChecksum();
            const TByte* newPairStrongChecksum=partChecksums+newBlockIndex*(size_t)kPartStrongChecksumByteSize;
            if (0==memcmp(oldPartStrongChecksum,newPairStrongChecksum,kPartStrongChecksumByteSize)){
#if (_IS_USED_MULTITHREAD)
                TMt_by_queue::TAutoLocker _autoLocker((TMt_by_queue*)_mt);
                hpatch_StreamPos_t cur=out_newDataPoss[newBlockIndex];
                if ((cur==kBlockType_needSync)||(cur>oldData.curOldPos()))
#endif
                    out_newDataPoss[newBlockIndex]=oldData.curOldPos();
                break;
            }
        }
        ++range_begin;
    }while (range_begin!=range_end);
}


struct _TMatchDatas{
    hpatch_StreamPos_t*         out_newDataPoss;
    const TNewDataSyncInfo*     newSyncInfo;
    const hpatch_TStreamInput*  oldStream;
    hpatch_TChecksum*           strongChecksumPlugin;
    const void*         filter;
    const uint32_t*     sorted_newIndexs;
    const uint32_t*     sorted_newIndexs_table;
    unsigned int        kTableHashShlBit;
};

template<class tm_roll_uint>
static void _rollMatch(_TMatchDatas& rd,hpatch_StreamPos_t oldRollBegin,
                       hpatch_StreamPos_t oldRollEnd,void* _mt=0){
    TIndex_comp<tm_roll_uint> icomp((tm_roll_uint*)rd.newSyncInfo->rollHashs);
    TOldDataCache<tm_roll_uint> oldData(rd.oldStream,oldRollBegin,oldRollEnd,
                                        rd.newSyncInfo->kMatchBlockSize,
                                        rd.strongChecksumPlugin,_mt);
    const TBloomFilter<tm_roll_uint>& filter=*(TBloomFilter<tm_roll_uint>*)rd.filter;
    for (;!oldData.isEnd();oldData.roll()) {
        tm_roll_uint digest=oldData.hashValue();
        if (!filter.is_hit(digest)) continue;
        
        const uint32_t* ti_pos=&rd.sorted_newIndexs_table[digest>>rd.kTableHashShlBit];
        typename TIndex_comp<tm_roll_uint>::TDigest digest_value(digest);
        std::pair<const uint32_t*,const uint32_t*>
        //range=std::equal_range(sorted_newIndexs,sorted_newIndexs+sortedBlockCount,digest_value,icomp);
        range=std::equal_range(rd.sorted_newIndexs+ti_pos[0],
                               rd.sorted_newIndexs+ti_pos[1],digest_value,icomp);
        if (range.first!=range.second){
            matchRange(rd.out_newDataPoss,rd.newSyncInfo->partChecksums,
                       oldData,range.first,range.second,_mt);
        }
            
    }
}

#if (_IS_USED_MULTITHREAD)
struct TMt_threadDatas {
    _TMatchDatas*       matchDatas;
    hpatch_StreamPos_t  oldRollEnd;
    TMt_by_queue*       shareDatas;
};

static void _mt_threadRunCallBackProc(int threadIndex,void* workData){
    TMt_threadDatas* tdatas=(TMt_threadDatas*)workData;
    hpatch_StreamPos_t oldClipSize=tdatas->matchDatas->oldStream->streamSize/(uint32_t)tdatas->shareDatas->threadNum;
    bool isMainThread=(threadIndex==tdatas->shareDatas->threadNum-1);
    hpatch_StreamPos_t oldPosBegin=oldClipSize*(uint32_t)threadIndex;
    hpatch_StreamPos_t oldPosEnd = isMainThread ? tdatas->oldRollEnd
                                :(oldPosBegin+oldClipSize+(tdatas->matchDatas->newSyncInfo->kMatchBlockSize-1));
    if (tdatas->matchDatas->newSyncInfo->is32Bit_rollHash)
        _rollMatch<uint32_t>(*tdatas->matchDatas,oldPosBegin,oldPosEnd,tdatas->shareDatas);
    else
        _rollMatch<uint64_t>(*tdatas->matchDatas,oldPosBegin,oldPosEnd,tdatas->shareDatas);
    
    tdatas->shareDatas->finish();
    if (isMainThread) tdatas->shareDatas->waitAllFinish();
}
#endif

template<class tm_roll_uint>
static void tm_matchNewDataInOld(_TMatchDatas& matchDatas,int threadNum){
    const TNewDataSyncInfo* newSyncInfo=matchDatas.newSyncInfo;
    uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    TIndex_comp<tm_roll_uint> icomp((tm_roll_uint*)newSyncInfo->rollHashs);
    
    TAutoMem _mem_sorted(kBlockCount*(size_t)sizeof(uint32_t));
    uint32_t* sorted_newIndexs=(uint32_t*)_mem_sorted.data();
    TBloomFilter<tm_roll_uint> filter; filter.init(kBlockCount);
    uint32_t sortedBlockCount=0;
    {
        uint32_t curPair=0;
        for (uint32_t i=0; i<kBlockCount; ++i){
            if ((curPair<newSyncInfo->samePairCount)
                &&(i==newSyncInfo->samePairList[curPair].curIndex)){
                ++curPair;
            }else{
                sorted_newIndexs[sortedBlockCount++]=i;
                filter.insert(((tm_roll_uint*)newSyncInfo->rollHashs)[i]);
            }
        }
        assert(curPair==newSyncInfo->samePairCount);
        assert(sortedBlockCount==kBlockCount-newSyncInfo->samePairCount);
    }
    std::sort(sorted_newIndexs,sorted_newIndexs+sortedBlockCount,icomp);
    
    //optimize for std::equal_range
    const unsigned int kTableBit =getBetterCacheBlockTableBit(sortedBlockCount);
    const unsigned int kTableHashShlBit=(sizeof(tm_roll_uint)*8-kTableBit);
    TAutoMem _mem_table((size_t)sizeof(uint32_t)*((1<<kTableBit)+1));
    uint32_t* sorted_newIndexs_table=(uint32_t*)_mem_table.data();
    {
        uint32_t* pos=sorted_newIndexs;
        for (uint32_t i=0; i<((uint32_t)1<<kTableBit); ++i) {
            tm_roll_uint digest=((tm_roll_uint)i)<<kTableHashShlBit;
            typename TIndex_comp<tm_roll_uint>::TDigest digest_value(digest);
            pos=std::lower_bound(pos,sorted_newIndexs+sortedBlockCount,digest_value,icomp);
            sorted_newIndexs_table[i]=(uint32_t)(pos-sorted_newIndexs);
        }
        sorted_newIndexs_table[((size_t)1<<kTableBit)]=sortedBlockCount;
    }

    matchDatas.filter=&filter;
    matchDatas.sorted_newIndexs=sorted_newIndexs;
    matchDatas.sorted_newIndexs_table=sorted_newIndexs_table;
    matchDatas.kTableHashShlBit=kTableHashShlBit;
    size_t backZeroLen=getBackZeroLen(newSyncInfo->newDataSize,newSyncInfo->kMatchBlockSize);
    hpatch_StreamPos_t oldRollEnd=matchDatas.oldStream->streamSize+backZeroLen;
#if (_IS_USED_MULTITHREAD)
    if (threadNum>1){
        TMt_by_queue   shareDatas((int)threadNum,0,false);
        TMt_threadDatas  tdatas;  memset(&tdatas,0,sizeof(tdatas));
        tdatas.shareDatas=&shareDatas;
        tdatas.matchDatas=&matchDatas;
        tdatas.oldRollEnd=oldRollEnd;
        thread_parallel((int)threadNum,_mt_threadRunCallBackProc,&tdatas,1);
    }else
#endif
    {
        _rollMatch<tm_roll_uint>(matchDatas,0,oldRollEnd);
    }

}

void matchNewDataInOld(hpatch_StreamPos_t* out_newDataPoss,const TNewDataSyncInfo* newSyncInfo,
                       const hpatch_TStreamInput* oldStream,hpatch_TChecksum* strongChecksumPlugin,int threadNum){
    uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    for (uint32_t i=0; i<kBlockCount; ++i)
        out_newDataPoss[i]=kBlockType_needSync;
    
    _TMatchDatas matchDatas; memset(&matchDatas,0,sizeof(matchDatas));
    matchDatas.out_newDataPoss=out_newDataPoss;
    matchDatas.newSyncInfo=newSyncInfo;
    matchDatas.oldStream=oldStream;
    matchDatas.strongChecksumPlugin=strongChecksumPlugin;
    if (newSyncInfo->is32Bit_rollHash)
        tm_matchNewDataInOld<uint32_t>(matchDatas,threadNum);
    else
        tm_matchNewDataInOld<uint64_t>(matchDatas,threadNum);
}
