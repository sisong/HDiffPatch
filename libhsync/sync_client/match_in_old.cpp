//  match_in_old.cpp
//  sync_client
//  Created by housisong on 2019-09-22.
/*
 The MIT License (MIT)
 Copyright (c) 2019-2020 HouSisong

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
#include "sync_client_type_private.h"
#include "../../libHDiffPatch/HDiff/private_diff/mem_buf.h"
#include "../../libHDiffPatch/HDiff/private_diff/limit_mem_diff/bloom_filter.h"
#include "sync_diff_data.h"
using namespace hdiff_private;
namespace sync_private{

#define check(value,info) { if (!(value)) { throw std::runtime_error(info); } }
#define checkv(value)     check(value,"check "#value" error!")

typedef unsigned char TByte;
typedef uint64_t tm_roll_uint;
const size_t kBestClipSize=256*1024;

struct TIndex_comp0{
    inline explicit TIndex_comp0(const uint8_t* _hashs,size_t _byteSize)
    :hashs(_hashs),byteSize(_byteSize){ }
    typedef uint32_t TIndex;
    struct TDigest{
        const uint8_t*  digests;
        inline explicit TDigest(const uint8_t* _digests):digests(_digests){}
    };
    inline bool operator()(const TIndex x,const TDigest& y)const { //for equal_range
        return _cmp(hashs+x*byteSize,y.digests,byteSize)<0; }
    bool operator()(const TDigest& x,const TIndex y)const { //for equal_range
        return _cmp(x.digests,hashs+y*byteSize,byteSize)<0; }
    
    inline bool operator()(const TIndex x, const TIndex y)const {//for sort
        return _cmp(hashs+x*byteSize,hashs+y*byteSize,byteSize)<0; }
protected:
    const uint8_t*  hashs;
    size_t          byteSize;
public:
    inline static int _cmp(const uint8_t* px,const uint8_t* py,size_t byteSize){
        const uint8_t* px_end=px+byteSize;
        for (;px!=px_end;++px,++py){
            int sub=(int)(*px)-(*py);
            if (sub!=0) return sub; //value sort
        }
        return 0;
    }
};

struct TIndex_comp01{
    inline explicit TIndex_comp01(const uint8_t* _hashs0,size_t _byteSize0,
                                  const uint8_t* _hashs1,size_t _byteSize1)
    :hashs0(_hashs0),hashs1(_hashs1),byteSize0(_byteSize0),byteSize1(_byteSize1){ }
    typedef uint32_t TIndex;
    inline bool operator()(const TIndex x, const TIndex y)const {//for sort
        int cmp0=TIndex_comp0::_cmp(hashs0+x*byteSize0,hashs0+y*byteSize0,byteSize0);
        if (cmp0!=0)
            return cmp0<0;
        else
            return TIndex_comp0::_cmp(hashs1+x*byteSize1,hashs1+y*byteSize1,byteSize1)<0;
    }
protected:
    const uint8_t*  hashs0;
    const uint8_t*  hashs1;
    size_t          byteSize0;
    size_t          byteSize1;
};


struct TOldDataCache_base {
    TOldDataCache_base(const hpatch_TStreamInput* oldStream,hpatch_StreamPos_t oldRollBegin,
                       hpatch_StreamPos_t oldRollEnd,uint32_t kSyncBlockSize,
                       hpatch_TChecksum* strongChecksumPlugin,void* _mt=0)
    :m_oldStream(oldStream),m_readedPos(oldRollBegin),m_oldRollEnd(oldRollEnd),
    m_strongChecksum_buf(0),m_cur(0),m_kSyncBlockSize(kSyncBlockSize),m_checksumByteSize(0),
    m_strongChecksumPlugin(strongChecksumPlugin),m_checksumHandle(0),m_mt(_mt){
        size_t cacheSize=(size_t)kSyncBlockSize*2;
        check((cacheSize>>1)==kSyncBlockSize,"TOldDataCache mem error!");
        cacheSize=(cacheSize>=hpatch_kFileIOBufBetterSize)?cacheSize:hpatch_kFileIOBufBetterSize;
        hpatch_StreamPos_t maxDataSize=oldRollPosEnd()-oldRollBegin;
        if (cacheSize>maxDataSize) cacheSize=(size_t)maxDataSize;

        m_checksumHandle=strongChecksumPlugin->open(strongChecksumPlugin);
        checkv(m_checksumHandle!=0);
        m_checksumByteSize=(uint32_t)m_strongChecksumPlugin->checksumByteSize();
        m_cache.realloc(cacheSize+m_checksumByteSize+m_checksumByteSize);
        m_cache.reduceSize(cacheSize);
        m_strongChecksum_buf=m_cache.data_end();
        m_cur=m_cache.data_end();
        _cache();
    }
    ~TOldDataCache_base(){
        if (m_checksumHandle)
            m_strongChecksumPlugin->close(m_strongChecksumPlugin,m_checksumHandle);
    }
    inline hpatch_StreamPos_t oldRollPosEnd()const{ return m_oldRollEnd+m_kSyncBlockSize-1; }
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
        const hpatch_StreamPos_t rollPosEnd=oldRollPosEnd();
        if (m_readedPos >=rollPosEnd){ m_cur=0; return; } //set end tag

        size_t needLen=m_cur-m_cache.data();
        if (m_readedPos+needLen>rollPosEnd)
            needLen=(size_t)(rollPosEnd-m_readedPos);
        memmove(m_cur-needLen,m_cur,m_cache.data_end()-m_cur);
        size_t readLen=needLen;
        if (m_readedPos+readLen>m_oldStream->streamSize){
            if (m_readedPos<m_oldStream->streamSize)
                readLen=(size_t)(m_oldStream->streamSize-m_readedPos);
            else
                readLen=0;
        }
        if (readLen>0){
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
    inline const TByte* calcPartStrongChecksum(size_t outPartBits){
        return _calcPartStrongChecksum(m_cur,m_kSyncBlockSize,outPartBits); }
    inline const TByte* strongChecksum()const{//must after do calcPartStrongChecksum()
        return m_strongChecksum_buf+m_checksumByteSize; }
    inline size_t strongChecksumByteSize()const{ return m_checksumByteSize; }
    inline hpatch_StreamPos_t curOldPos()const{ return m_readedPos-(m_cache.data_end()-m_cur); }
protected:
    const hpatch_TStreamInput* m_oldStream;
    hpatch_StreamPos_t      m_readedPos;
    hpatch_StreamPos_t      m_oldRollEnd;
    TAutoMem                m_cache;
    TByte*                  m_strongChecksum_buf;
    TByte*                  m_cur;
    uint32_t                m_kSyncBlockSize;
    uint32_t                m_checksumByteSize;
    hpatch_TChecksum*       m_strongChecksumPlugin;
    hpatch_checksumHandle   m_checksumHandle;
    void*                   m_mt;

    inline const TByte* _calcPartStrongChecksum(const TByte* buf,size_t bufSize,size_t outPartBits){
        TByte* strongChecksum=m_strongChecksum_buf+m_checksumByteSize;
        m_strongChecksumPlugin->begin(m_checksumHandle);
        m_strongChecksumPlugin->append(m_checksumHandle,buf,buf+bufSize);
        m_strongChecksumPlugin->end(m_checksumHandle,strongChecksum,strongChecksum+m_checksumByteSize);
        toPartChecksum(m_strongChecksum_buf,outPartBits,strongChecksum,m_checksumByteSize);
        return m_strongChecksum_buf;
    }
};

struct TOldDataCache:public TOldDataCache_base {
    inline TOldDataCache(const hpatch_TStreamInput* oldStream,hpatch_StreamPos_t oldRollBegin,
                         hpatch_StreamPos_t oldRollEnd,uint32_t kSyncBlockSize,
                         hpatch_TChecksum* strongChecksumPlugin,void* _mt=0)
            :TOldDataCache_base(oldStream,oldRollBegin,oldRollEnd,kSyncBlockSize,
                                strongChecksumPlugin,_mt){
                if (isEnd()) return;
                m_roolHash=roll_hash_start((tm_roll_uint*)0,m_cur,m_kSyncBlockSize);
            }
    void _cache(){
        TOldDataCache_base::_cache();
        if (isEnd()) return;
        roll();
    }
    inline tm_roll_uint hashValue()const{ return m_roolHash; }
    inline void roll(){
        const TByte* curIn=m_cur+m_kSyncBlockSize;
        if (curIn!=m_cache.data_end()){
            m_roolHash=roll_hash_roll(m_roolHash,m_kSyncBlockSize,*m_cur,*curIn);
            ++m_cur;
        }else{
            _cache();
        }
    }
protected:
    tm_roll_uint            m_roolHash;
};

static void matchRange(hpatch_StreamPos_t* out_newBlockDataInOldPoss,
                       const uint32_t* range_begin,const uint32_t* range_end,TOldDataCache_base& oldData,
                       const TByte* partChecksums,size_t outPartChecksumBits,TByte* newDataCheckChecksum,void* _mt=0){
    const TByte* oldPartStrongChecksum=0;
    const size_t outPartChecksumSize=_bitsToBytes(outPartChecksumBits);
    bool isMatched=false;
    do {
        uint32_t newBlockIndex=*range_begin;
        if (out_newBlockDataInOldPoss[newBlockIndex]==kBlockType_needSync){
            if (oldPartStrongChecksum==0)
                oldPartStrongChecksum=oldData.calcPartStrongChecksum(outPartChecksumBits);
            const TByte* newPairStrongChecksum=partChecksums+newBlockIndex*outPartChecksumSize;
            if (0==memcmp(oldPartStrongChecksum,newPairStrongChecksum,outPartChecksumSize)){
                isMatched=true;
                hpatch_StreamPos_t curPos=oldData.curOldPos();
                out_newBlockDataInOldPoss[newBlockIndex]=curPos;
                checkChecksumAppendData(newDataCheckChecksum,newBlockIndex,
                                        oldData.strongChecksum(),oldData.strongChecksumByteSize());
                //continue;
            }else{
                if (isMatched)
                    return;
            }
        }
        ++range_begin;
    }while (range_begin!=range_end);
}


struct _TMatchDatas{
    hpatch_StreamPos_t*         out_newBlockDataInOldPoss;
    const TNewDataSyncInfo*     newSyncInfo;
    const hpatch_TStreamInput*  oldStream;
    hpatch_TChecksum*           strongChecksumPlugin;
    const void*         filter;
    const uint32_t*     sorted_newIndexs;
    const uint32_t*     sorted_newIndexs_table;
    unsigned int        kTableHashShlBit;
};
    
static void _rollMatch(_TMatchDatas& rd,hpatch_StreamPos_t oldRollBegin,
                       hpatch_StreamPos_t oldRollEnd,void* _mt=0){
    if (rd.oldStream->streamSize<rd.newSyncInfo->kSyncBlockSize)
        return;
    TIndex_comp0 icomp0(rd.newSyncInfo->rollHashs,rd.newSyncInfo->savedRollHashByteSize);
    TOldDataCache oldData(rd.oldStream,oldRollBegin,oldRollEnd,
                          rd.newSyncInfo->kSyncBlockSize,rd.strongChecksumPlugin,_mt);
    uint8_t part[sizeof(tm_roll_uint)]={0};
    const size_t savedRollHashBits=rd.newSyncInfo->savedRollHashBits;
    const TBloomFilter<tm_roll_uint>& filter=*(TBloomFilter<tm_roll_uint>*)rd.filter;
    for (;!oldData.isEnd();oldData.roll()) {
        tm_roll_uint digest=oldData.hashValue();
        digest=toSavedPartRollHash(digest,savedRollHashBits);
        if (!filter.is_hit(digest)) continue;
        
        const uint32_t* ti_pos=&rd.sorted_newIndexs_table[digest>>rd.kTableHashShlBit];
        writeRollHashBytes(part,digest,rd.newSyncInfo->savedRollHashByteSize);
        TIndex_comp0::TDigest digest_value(part);
        std::pair<const uint32_t*,const uint32_t*>
        //range=std::equal_range(sorted_newIndexs,sorted_newIndexs+sortedBlockCount,digest_value,icomp);
        range=std::equal_range(rd.sorted_newIndexs+ti_pos[0],
                               rd.sorted_newIndexs+ti_pos[1],digest_value,icomp0);
        if (range.first!=range.second){
            matchRange(rd.out_newBlockDataInOldPoss,range.first,range.second,oldData,
                       rd.newSyncInfo->partChecksums,rd.newSyncInfo->savedStrongChecksumBits,
                       rd.newSyncInfo->savedNewDataCheckChecksum,_mt);
        }
    }
}

static void matchNewDataInOld(_TMatchDatas& matchDatas,int threadNum){
    const TNewDataSyncInfo* newSyncInfo=matchDatas.newSyncInfo;
    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    const size_t savedRollHashByteSize=newSyncInfo->savedRollHashByteSize;
    const size_t savedRollHashBits=newSyncInfo->savedRollHashBits;
    
    TAutoMem _mem_sorted(kBlockCount*(size_t)sizeof(uint32_t));
    uint32_t* sorted_newIndexs=(uint32_t*)_mem_sorted.data();
    TBloomFilter<tm_roll_uint> filter; filter.init(kBlockCount);
    {
        const uint8_t* partRollHash=newSyncInfo->rollHashs;
        for (uint32_t i=0; i<kBlockCount; ++i,partRollHash+=savedRollHashByteSize){
            sorted_newIndexs[i]=i;
            filter.insert(readRollHashBytes(partRollHash,savedRollHashByteSize));
        }
        TIndex_comp01 icomp01(newSyncInfo->rollHashs,savedRollHashByteSize,
                              newSyncInfo->partChecksums,newSyncInfo->savedStrongChecksumByteSize);
        std::sort(sorted_newIndexs,sorted_newIndexs+kBlockCount,icomp01);
    }
    
    //optimize for std::equal_range
    unsigned int kTableBit =getBetterCacheBlockTableBit(kBlockCount);
    if (kTableBit>savedRollHashBits) kTableBit=(unsigned int)savedRollHashBits;
    const unsigned int kTableHashShlBit=(int)(newSyncInfo->savedRollHashBits-kTableBit);
    TAutoMem _mem_table((size_t)sizeof(uint32_t)*((1<<kTableBit)+1));
    uint32_t* sorted_newIndexs_table=(uint32_t*)_mem_table.data();
    {
        TIndex_comp0  icomp0(newSyncInfo->rollHashs,savedRollHashByteSize);
        uint32_t* pos=sorted_newIndexs;
        uint8_t part[sizeof(tm_roll_uint)]={0};
        for (uint32_t i=0; i<((uint32_t)1<<kTableBit); ++i) {
            tm_roll_uint digest=((tm_roll_uint)i)<<kTableHashShlBit;
            writeRollHashBytes(part,digest,savedRollHashByteSize);
            TIndex_comp0::TDigest digest_value(part);
            pos=std::lower_bound(pos,sorted_newIndexs+kBlockCount,digest_value,icomp0);
            sorted_newIndexs_table[i]=(uint32_t)(pos-sorted_newIndexs);
        }
        sorted_newIndexs_table[((size_t)1<<kTableBit)]=kBlockCount;
    }

    matchDatas.filter=&filter;
    matchDatas.sorted_newIndexs=sorted_newIndexs;
    matchDatas.sorted_newIndexs_table=sorted_newIndexs_table;
    matchDatas.kTableHashShlBit=kTableHashShlBit;
    {
        _rollMatch(matchDatas,0,matchDatas.oldStream->streamSize);
    }
}

    
void matchNewDataInOld(hpatch_StreamPos_t* out_newBlockDataInOldPoss,const TNewDataSyncInfo* newSyncInfo,
                       const hpatch_TStreamInput* oldStream,hpatch_TChecksum* strongChecksumPlugin,int threadNum){
    uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    for (uint32_t i=0; i<kBlockCount; ++i)
        out_newBlockDataInOldPoss[i]=kBlockType_needSync;
    
    _TMatchDatas matchDatas; memset(&matchDatas,0,sizeof(matchDatas));
    matchDatas.out_newBlockDataInOldPoss=out_newBlockDataInOldPoss;
    matchDatas.newSyncInfo=newSyncInfo;
    matchDatas.oldStream=oldStream;
    matchDatas.strongChecksumPlugin=strongChecksumPlugin;
    matchNewDataInOld(matchDatas,threadNum);
}

} //namespace sync_private
