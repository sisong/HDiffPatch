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
#include <math.h> //log2
#include "../../libHDiffPatch/HDiff/private_diff/mem_buf.h"
#include "../../libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.h"
#include "../../libHDiffPatch/HDiff/private_diff/limit_mem_diff/bloom_filter.h"

#define check(value,info) { if (!(value)) { throw std::runtime_error(info); } }
#define checkv(value)     check(value,"check "#value" error!")

using namespace hdiff_private;
typedef unsigned char TByte;

struct TIndex_comp{
    inline explicit TIndex_comp(const roll_uint_t* _blocks):blocks(_blocks){ }
    struct TDigest{
        roll_uint_t value;
        inline explicit TDigest(roll_uint_t _value):value(_value){}
    };
    template<class TIndex> inline
    bool operator()(const TIndex& x,const TDigest& y)const { return blocks[x]<y.value; }
    template<class TIndex> inline
    bool operator()(const TDigest& x,const TIndex& y)const { return x.value<blocks[y]; }
    template<class TIndex> inline
    bool operator()(const TIndex& x, const TIndex& y)const { return blocks[x]<blocks[y]; }
protected:
    const roll_uint_t* blocks;
};


struct TOldDataCache {
    TOldDataCache(const hpatch_TStreamInput* oldStream,uint32_t kMatchBlockSize,
                  hpatch_TChecksum* strongChecksumPlugin,size_t backZeroLen)
    :m_oldStream(oldStream),m_readedPos(0),m_strongChecksum_buf(0),m_cur(0),m_checksumHandle(0),
    m_kMatchBlockSize(kMatchBlockSize),m_backZeroLen(backZeroLen),
    m_strongChecksumPlugin(strongChecksumPlugin){
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
    ~TOldDataCache(){
        if (m_checksumHandle)
            m_strongChecksumPlugin->close(m_strongChecksumPlugin,m_checksumHandle);
    }
    void _initCache(){
        if (m_oldStream->streamSize+m_backZeroLen<m_kMatchBlockSize){ m_cur=0; return; } //end
        
        size_t needLen=m_cache.size();
        if (needLen>m_oldStream->streamSize+m_backZeroLen){
            needLen=m_oldStream->streamSize+m_backZeroLen;
            m_cache.reduceSize(needLen);
        }
        size_t readLen=needLen;
        if (readLen>m_oldStream->streamSize)
            readLen=m_oldStream->streamSize;
        check(m_oldStream->read(m_oldStream,0,m_cache.data(),m_cache.data()+readLen),
              "TOldDataCache read oldData error!");
        if (readLen<needLen)
            memset(m_cache.data()+readLen,0,needLen-readLen);
        m_readedPos=0+needLen;
        m_cur=m_cache.data();
        m_roolHash=roll_hash_start(m_cur,m_kMatchBlockSize);
        // [       oldDataSize     +   backZeroLen  ]
        //       ^              ^
        //cache: [     readedPos]
        //         ^
        //        cur
    }
    void _cache(){
        if (m_readedPos >= m_oldStream->streamSize+m_backZeroLen){ m_cur=0; return; } //end
        
        size_t needLen=m_cur-m_cache.data();
        if (m_readedPos+needLen>m_oldStream->streamSize+m_backZeroLen)
            needLen=m_oldStream->streamSize+m_backZeroLen-m_readedPos;
        memmove(m_cur-needLen,m_cur,m_cache.data_end()-m_cur);
        if (m_readedPos<m_oldStream->streamSize){
            size_t readLen=needLen;
            if (m_readedPos+readLen>m_oldStream->streamSize)
                readLen=m_oldStream->streamSize-m_readedPos;
            TByte* buf=m_cache.data_end()-needLen;
            check(m_oldStream->read(m_oldStream,m_readedPos,buf,buf+readLen),
                  "TOldDataCache read oldData error!");
            size_t zerolen=needLen-readLen;
            if (zerolen>0)
                memset(m_cache.data_end()-zerolen,0,zerolen);
        }else{
            memset(m_cache.data_end()-needLen,0,needLen);
        }
        m_readedPos+=needLen;
        m_cur-=needLen;
        roll();
    }

    inline bool isEnd()const{ return m_cur==0; }
    inline roll_uint_t hashValue()const{ return m_roolHash; }
    inline void roll(){
        const TByte* curIn=m_cur+m_kMatchBlockSize;
        if (curIn!=m_cache.data_end()){
            m_roolHash=roll_hash_roll(m_roolHash,m_kMatchBlockSize,*m_cur,*curIn);
            ++m_cur;
        }else{
            _cache();
        }
    }
    inline const TByte* calcPartStrongChecksum(){
        return _calcPartStrongChecksum(m_cur,m_kMatchBlockSize);
    }
    inline const TByte* calcLastPartStrongChecksum(size_t lastNewNodeSize){
        return _calcPartStrongChecksum(m_cache.data_end()-lastNewNodeSize,lastNewNodeSize);
    }
    inline hpatch_StreamPos_t curOldPos()const{
        return m_cur-m_cache.data();
    }
    const hpatch_TStreamInput* m_oldStream;
    hpatch_StreamPos_t      m_readedPos;
    TAutoMem                m_cache;
    TByte*                  m_strongChecksum_buf;
    TByte*                  m_cur;
    roll_uint_t             m_roolHash;
    uint32_t                m_kMatchBlockSize;
    uint32_t                m_checksumByteSize;
    size_t                  m_backZeroLen;
    hpatch_TChecksum*       m_strongChecksumPlugin;
    hpatch_checksumHandle   m_checksumHandle;
    
    inline const TByte* _calcPartStrongChecksum(const TByte* buf,size_t bufSize){
        m_strongChecksumPlugin->begin(m_checksumHandle);
        m_strongChecksumPlugin->append(m_checksumHandle,buf,buf+bufSize);
        m_strongChecksumPlugin->end(m_checksumHandle,m_strongChecksum_buf,
                                    m_strongChecksum_buf+m_checksumByteSize);
        toPartChecksum(m_strongChecksum_buf,m_strongChecksum_buf,m_checksumByteSize);
        return m_strongChecksum_buf;
    }
};

inline static size_t getBackZeroLen(hpatch_StreamPos_t newDataSize,uint32_t kMatchBlockSize){
    size_t len=(size_t)(newDataSize % kMatchBlockSize);
    if (len!=0) len=kMatchBlockSize-len;
    return len;
}

static int getBestTableBit(uint32_t blockCount){
    const int kMinBit = 8;
    const int kMaxBit = 23;
    int bit=(int)(log2((1<<kMinBit)+1.0+blockCount));
    if (bit>kMaxBit) bit=kMaxBit;
    return bit;
}

//cpp code used stdexcept
void matchNewDataInOld(hpatch_StreamPos_t* out_newDataPoss,uint32_t* out_needSyncCount,
                       hpatch_StreamPos_t* out_needSyncSize,const TNewDataSyncInfo* newSyncInfo,
                       const hpatch_TStreamInput* oldStream,hpatch_TChecksum* strongChecksumPlugin){
    uint32_t kMatchBlockSize=newSyncInfo->kMatchBlockSize;
    uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    TIndex_comp icomp(newSyncInfo->rollHashs);

    TAutoMem _mem_sorted(kBlockCount*sizeof(uint32_t));
    uint32_t* sorted_newIndexs=(uint32_t*)_mem_sorted.data();
    TBloomFilter<roll_uint_t> filter; filter.init(kBlockCount);
    uint32_t sortedBlockCount=0;
    {
        uint32_t curPair=0;
        for (uint32_t i=0; i<kBlockCount; ++i){
            out_newDataPoss[i]=kBlockType_needSync;
            if ((curPair<newSyncInfo->samePairCount)
                &&(i==newSyncInfo->samePairList[curPair].curIndex)){
                ++curPair;
            }else{
                sorted_newIndexs[sortedBlockCount++]=i;
                filter.insert(newSyncInfo->rollHashs[i]);
            }
        }
        assert(sortedBlockCount==kBlockCount-newSyncInfo->samePairCount);
    }
    
    //optimize for std::equal_range
    const int kTableBit =getBestTableBit(sortedBlockCount);
    const int kTableHashShlBit=(sizeof(roll_uint_t)*8-kTableBit);
    TAutoMem _mem_table(sizeof(uint32_t)*((1<<kTableBit)+1));
    uint32_t* sorted_newIndexs_table=(uint32_t*)_mem_table.data();
    {
        std::sort(sorted_newIndexs,sorted_newIndexs+sortedBlockCount,icomp);
        uint32_t* pos=sorted_newIndexs;
        for (uint32_t i=0; i<(1<<kTableBit); ++i) {
            roll_uint_t digest=((roll_uint_t)i)<<kTableHashShlBit;
            typename TIndex_comp::TDigest digest_value(digest);
            pos=std::lower_bound(pos,sorted_newIndexs+sortedBlockCount,digest_value,icomp);
            sorted_newIndexs_table[i]=(uint32_t)(pos-sorted_newIndexs);
        }
        sorted_newIndexs_table[(1<<kTableBit)]=sortedBlockCount;
    }

    TOldDataCache oldData(oldStream,kMatchBlockSize,strongChecksumPlugin,
                          getBackZeroLen(newSyncInfo->newDataSize,kMatchBlockSize));
    uint32_t matchedCount=0;
    hpatch_StreamPos_t matchedSyncSize=0;
    for (;!oldData.isEnd();oldData.roll()) {
        roll_uint_t digest=oldData.hashValue();
        if (!filter.is_hit(digest)) continue;
        
        typename TIndex_comp::TDigest digest_value(digest);
        const uint32_t* ti_pos=&sorted_newIndexs_table[digest>>kTableHashShlBit];
        std::pair<const uint32_t*,const uint32_t*>
            //range=std::equal_range(sorted_newIndexs,sorted_newIndexs+sortedBlockCount,digest_value,icomp);
            range=std::equal_range(sorted_newIndexs+ti_pos[0],sorted_newIndexs+ti_pos[1],digest_value,icomp);
        if (range.first!=range.second){
            const TByte* oldPartStrongChecksum=0;
            do {
                uint32_t newBlockIndex=*range.first;
                if (out_newDataPoss[newBlockIndex]==kBlockType_needSync){
                    if (oldPartStrongChecksum==0)
                        oldPartStrongChecksum=oldData.calcPartStrongChecksum();
                    const TByte* newPairStrongChecksum = newSyncInfo->partChecksums
                                                    + newBlockIndex*(size_t)kPartStrongChecksumByteSize;
                    if (0==memcmp(oldPartStrongChecksum,newPairStrongChecksum,kPartStrongChecksumByteSize)){
                        out_newDataPoss[newBlockIndex]=oldData.curOldPos();
                        ++matchedCount;
                        matchedSyncSize+=TNewDataSyncInfo_syncBlockSize(newSyncInfo,newBlockIndex);
                        break;
                    }
                }
                ++range.first;
            }while (range.first!=range.second);
        }
    }
    { //samePair set same oldPos
        uint32_t curPair=0;
        for (uint32_t i=0; i<kBlockCount; ++i){
            if ((curPair<newSyncInfo->samePairCount)
                &&(i==newSyncInfo->samePairList[curPair].curIndex)){
                hpatch_StreamPos_t sameOldPos=out_newDataPoss[newSyncInfo->samePairList[curPair].sameIndex];
                if (sameOldPos!=kBlockType_needSync){
                    out_newDataPoss[i]=sameOldPos;
                    ++matchedCount;
                    matchedSyncSize+=TNewDataSyncInfo_syncBlockSize(newSyncInfo,i);
                }
                ++curPair;
            }
        }
    }
    *out_needSyncCount=kBlockCount-matchedCount;
    *out_needSyncSize=newSyncInfo->newSyncDataSize-matchedSyncSize;
}

