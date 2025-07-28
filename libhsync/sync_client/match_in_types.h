//  match_in_types.h
//  sync_client
//  move from match_in_old.cpp by housisong on 2025-04-18.
/*
 The MIT License (MIT)
 Copyright (c) 2019-2025 HouSisong

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
#ifndef match_in_types_h
#define match_in_types_h
#include "string.h" //memmove
#include <algorithm> //sort, equal_range lower_bound
#include "sync_client_type_private.h"
#include "../../libHDiffPatch/HDiff/private_diff/mem_buf.h"
#include "../../libHDiffPatch/HDiff/private_diff/limit_mem_diff/bloom_filter.h"
#include "../../libParallel/parallel_channel.h"
#include "sync_diff_data.h"
#if (_IS_USED_MULTITHREAD)
# ifndef _IS_USED_CPP_ATOMIC
#   define _IS_USED_CPP_ATOMIC  1
# endif
# if (_IS_USED_CPP_ATOMIC)
#   include <atomic>
# endif
#endif
using namespace hdiff_private;
namespace sync_private{

//kIsSkipMatchedBlock 0: roll byte by byte  1: skip matched block, speed++, but patchSize+
#define     kIsSkipMatchedBlock     1
static const int kMatchHitOutLimit =16;       //limit match deep
static const size_t kBestReadSize  =1024*256; //for sequence read
#if (_IS_USED_MULTITHREAD)
static const size_t kBestMTClipSize=1*1024*1024; //for multi-thread read once
#endif //_IS_USED_MULTITHREAD

typedef unsigned char TByte;
typedef hpatch_uint64_t tm_roll_uint;

struct TIndex_comp0{
    inline explicit TIndex_comp0(const uint8_t* _hashs,size_t _byteSize,bool isSeqMatch=false)
    :hashs(_hashs),byteSize(_byteSize),cmpSize(_byteSize*(isSeqMatch?2:1)){ }
    typedef uint32_t TIndex;
    struct TDigest{
        const uint8_t*  digests;
        inline explicit TDigest(const uint8_t* _digests):digests(_digests){}
    };
    inline bool operator()(const TIndex x,const TDigest& y)const { //for equal_range
        return _cmp(hashs+x*byteSize,y.digests,cmpSize)<0; }
    bool operator()(const TDigest& x,const TIndex y)const { //for equal_range
        return _cmp(x.digests,hashs+y*byteSize,cmpSize)<0; }
    
    inline bool operator()(const TIndex x, const TIndex y)const {//for sort
        return _cmp(hashs+x*byteSize,hashs+y*byteSize,cmpSize)<0; }
protected:
    const uint8_t*  hashs;
    size_t          byteSize;
    size_t          cmpSize;
public:
    inline static int _cmp(const uint8_t* px,const uint8_t* py,size_t cmpSize){
        const uint8_t* px_end=px+cmpSize;
        for (;px!=px_end;++px,++py){
            int sub=(int)(*px)-(*py);
            if (sub!=0) return sub; //value sort
        }
        return 0;
    }
};

struct TIndex_comp01{
    inline explicit TIndex_comp01(const uint8_t* _hashs0,size_t _byteSize0,
                                  const uint8_t* _hashs1,size_t _byteSize1,bool isSeqMatch0=false)
    :hashs0(_hashs0),hashs1(_hashs1),byteSize0(_byteSize0),byteSize1(_byteSize1),
        cmpSize0(_byteSize0*(isSeqMatch0?2:1)){ }
    typedef uint32_t TIndex;
    inline bool operator()(const TIndex x, const TIndex y)const {//for sort
        int cmp0=TIndex_comp0::_cmp(hashs0+x*byteSize0,hashs0+y*byteSize0,cmpSize0);
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
    size_t          cmpSize0;
};


struct TStreamDataCache_base {
    TStreamDataCache_base(const hpatch_TStreamInput* baseStream,hpatch_StreamPos_t streamRollBegin,
                          hpatch_StreamPos_t streamRollEnd,size_t backupSize,size_t backZeroLen,uint32_t kSyncBlockSize,
                          hpatch_TChecksum* strongChecksumPlugin,void* _readLocker=0);
    ~TStreamDataCache_base();
    inline hpatch_StreamPos_t streamRollPosEnd()const{ return m_streamRollEnd+m_kBackZeroLen; }
    void _cache();

    inline bool isRollEnded()const{ return m_cur>m_cache.data_end(); }
    inline const TByte* calcPartStrongChecksum(size_t outPartBits){
        return _calcPartStrongChecksum(m_cur,m_kSyncBlockSize,outPartBits); }
    inline const TByte* strongChecksum()const{//must after do calcPartStrongChecksum()
        return m_strongChecksum_buf+m_checksumByteSize; }
    inline size_t strongChecksumByteSize()const{ return m_checksumByteSize; }
    inline hpatch_TChecksum* strongChecksumPlugin()const{ return m_strongChecksumPlugin; }
    inline hpatch_checksumHandle checkChecksum()const{ return m_checkChecksum; }
    inline hpatch_StreamPos_t curStreamPos()const{ return m_readedPos-(m_cache.data_end()-m_cur); }
    inline hpatch_uint32_t  getSyncBlockSize()const{ return m_kSyncBlockSize; }
    inline hpatch_StreamPos_t getStreamSize()const{ return m_baseStream->streamSize; }
protected:
    const hpatch_TStreamInput* m_baseStream;
    hpatch_StreamPos_t      m_readedPos;
    hpatch_StreamPos_t      m_streamRollEnd;
    TAutoMem                m_cache;
    TByte*                  m_strongChecksum_buf;
    TByte*                  m_cur;
    const size_t            m_kBackZeroLen; //default kSyncBlockSize-1
    const size_t            m_kBackupSize; //default 0
    const uint32_t          m_kSyncBlockSize;
    uint32_t                m_checksumByteSize;
    hpatch_TChecksum*       m_strongChecksumPlugin;
    hpatch_checksumHandle   m_checksumHandle;
    hpatch_checksumHandle   m_checkChecksum;
    void*                   m_readLocker;

    inline void _setRollEnded() { m_cur=m_cache.data_end()+1; }
    const TByte* _calcPartStrongChecksum(const TByte* buf,size_t bufSize,size_t outPartBits);
};

struct TStreamDataRoll:public TStreamDataCache_base {
    inline TStreamDataRoll(const hpatch_TStreamInput* baseStream,hpatch_StreamPos_t streamRollBegin,
                            hpatch_StreamPos_t streamRollEnd,uint32_t kSyncBlockSize,
                            hpatch_TChecksum* strongChecksumPlugin,void* _readLocker=0)
            :TStreamDataCache_base(baseStream,streamRollBegin,streamRollEnd,0,kSyncBlockSize-1,kSyncBlockSize,
                                   strongChecksumPlugin,_readLocker),m_rollHash(0){
                if (isRollEnded()) return;
                m_rollHash=roll_hash_start(m_cur,m_kSyncBlockSize);
            }
    hpatch_force_inline tm_roll_uint hashValue()const{ return m_rollHash; }
    hpatch_force_inline bool roll(){
        const TByte* curIn=m_cur+m_kSyncBlockSize;
        assert(curIn>=m_cache.data());
        if (curIn<m_cache.data_end()){
            m_rollHash=roll_hash_roll(m_rollHash,m_kSyncBlockSize,*m_cur,*curIn);
            ++m_cur;
            return true;
        }else{
            return _cacheAndRoll();
        }
    }
    bool nextBlock(){
        m_cur+=m_kSyncBlockSize;
        if (m_cur+m_kSyncBlockSize>m_cache.data_end()){
            TStreamDataCache_base::_cache();
            if (isRollEnded()) return false;
        }
        m_rollHash=roll_hash_start(m_cur,m_kSyncBlockSize);
        return true;
    }
    inline const uint8_t* partHashNext(size_t savedRollHashByteSize){ return 0; } //not used
protected:
    tm_roll_uint            m_rollHash;

    bool _cacheAndRoll(){
        TStreamDataCache_base::_cache();
        if (isRollEnded()) return false;
        return roll();
    }
};

const uint32_t* getSortedIndexs(TAutoMem& _mem_sorted,const TNewDataSyncInfo* newSyncInfo,
                                TBloomFilter<tm_roll_uint>& filter);
const uint32_t* getSortedIndexsTable(TAutoMem& _mem_table,const TNewDataSyncInfo* newSyncInfo,
                                     const uint32_t* sorted_newIndexs,unsigned int* out_kTableHashShlBit);

} //namespace sync_private
#endif //match_in_types_h