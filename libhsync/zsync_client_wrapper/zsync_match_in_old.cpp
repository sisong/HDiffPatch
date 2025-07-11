//  zsync_match_in_old.cpp
//  zsync_client
/*
 The MIT License (MIT)
 Copyright (c) 2025 HouSisong

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
#include "zsync_match_in_old.h"
#include "zsync_client_type_private.h"
#include "../sync_client/sync_client_private.h"
#include "../sync_client/match_in_types.h"
namespace sync_private{

template<bool isSeqMatch>
struct TStreamDataRoll_z:public TStreamDataCache_base{
    inline TStreamDataRoll_z(const hpatch_TStreamInput* baseStream,hpatch_StreamPos_t streamRollBegin,
                             hpatch_StreamPos_t streamRollEnd,uint32_t kSyncBlockSize,
                             hpatch_TChecksum* strongChecksumPlugin,void* _readLocker=0)
        :TStreamDataCache_base(baseStream,streamRollBegin,streamRollEnd,
                               (isSeqMatch?kSyncBlockSize:0),kSyncBlockSize-1+(isSeqMatch?kSyncBlockSize:0),
                               kSyncBlockSize,strongChecksumPlugin,_readLocker){
            if (isRollEnded()) return;
            _startCurHash();
        }
    inline tm_roll_uint hashValue()const{
        if (isSeqMatch)
            return (((tm_roll_uint)m_rollHash)<<(sizeof(uint32_t)*8)) | m_rollHashNext;
        else
            return m_rollHash; }
    inline bool roll(){
        const TByte* curIn=m_cur+m_kSyncBlockSize;
        assert(curIn>=m_cache.data());
        if (curIn+(isSeqMatch?m_kSyncBlockSize:0)<m_cache.data_end()){
            m_rollHash=z_roll_hash32_roll(m_rollHash,m_kSyncBlockSize,*m_cur,*curIn);
            if (isSeqMatch)
                m_rollHashNext=z_roll_hash32_roll(m_rollHashNext,m_kSyncBlockSize,m_cur[m_kSyncBlockSize],curIn[m_kSyncBlockSize]);
            ++m_cur;
            return true;
        }else{
            return _cacheAndRoll();
        }
    }
    bool nextBlock(){
        m_cur+=m_kSyncBlockSize;
        if (m_cur+m_kSyncBlockSize+(isSeqMatch?m_kSyncBlockSize:0)>m_cache.data_end()){
            TStreamDataCache_base::_cache();
            if (isRollEnded()) return false;
        }
        _startCurHash();
        return true;
    }
protected:
    uint32_t    m_rollHash;
    uint32_t    m_rollHashNext;

    void _startCurHash(){
        m_rollHash=z_roll_hash32_start(m_cur,m_kSyncBlockSize);
        if (isSeqMatch)
            m_rollHashNext=z_roll_hash32_start(m_cur+m_kSyncBlockSize,m_kSyncBlockSize);   
    }
    bool _cacheAndRoll(){
        TStreamDataCache_base::_cache();
        if (isRollEnded()) return false;
        return roll();
    }
};

static inline
uint64_t z_seq_toSavedPartRollHash(uint64_t rollHash,size_t savedRollHashBits){
    return (z_toSavedPartRollHash(rollHash>>32,savedRollHashBits)<<savedRollHashBits)
          |z_toSavedPartRollHash(rollHash,savedRollHashBits);
}


    static inline bool _isCanBitFilter(size_t savedRollHashBits)
                                      { return (savedRollHashBits<=24); }//mem used 2^savedRollHashBits

    #define __Filter_Select_(_needHashBits,isUsedBitFilter,isTryUseHash32)  \
            const size_t _needHashBits=newSyncInfo->savedRollHashBits*(newSyncInfo->isSeqMatch?2:1); \
            const bool isUsedBitFilter=_isCanBitFilter(_needHashBits);      \
            const bool isTryUseHash32=(_needHashBits<=32)&&(sizeof(size_t)==sizeof(uint32_t));

static void _z_rollMatch(_TMatchDatas& rd,hpatch_StreamPos_t oldRollBegin,hpatch_StreamPos_t oldRollEnd,void* _mt){
    const TNewDataSyncInfo* newSyncInfo=rd.newSyncInfo;
    __Filter_Select_(_needHashBits,isUsedBitFilter,isTryUseHash32);
    if (newSyncInfo->isSeqMatch){
        if (isUsedBitFilter)
            _tm_rollMatch<TStreamDataRoll_z<true>,TBitSet,true>(rd,oldRollBegin,oldRollEnd,
                    z_seq_toSavedPartRollHash, _mt);
        else if (isTryUseHash32)
            _tm_rollMatch<TStreamDataRoll_z<true>,TBloomFilter<uint32_t>,true>(rd,oldRollBegin,oldRollEnd,
                    z_seq_toSavedPartRollHash, _mt);
        else
            _tm_rollMatch<TStreamDataRoll_z<true>,TBloomFilter<uint64_t>,true>(rd,oldRollBegin,oldRollEnd,
                    z_seq_toSavedPartRollHash, _mt);
    }else{
        if (isUsedBitFilter)
            _tm_rollMatch<TStreamDataRoll_z<false>,TBitSet,false>(rd,oldRollBegin,oldRollEnd,
                    z_toSavedPartRollHash, _mt);
        else if (isTryUseHash32)
            _tm_rollMatch<TStreamDataRoll_z<false>,TBloomFilter<uint32_t>,false>(rd,oldRollBegin,oldRollEnd,
                    z_toSavedPartRollHash, _mt);
        else
            assert(false); //not need 64bit
    }
}

static const uint32_t* _z_getSortedIndexs(TAutoMem& _mem_sorted,const TNewDataSyncInfo* newSyncInfo,void* filter){
    __Filter_Select_(_needHashBits,isUsedBitFilter,isTryUseHash32);
    if (isUsedBitFilter)
        return _tm_getSortedIndexs(_mem_sorted,newSyncInfo,*(TBitSet*)filter);
    else if (isTryUseHash32)
        return _tm_getSortedIndexs(_mem_sorted,newSyncInfo,*(TBloomFilter<uint32_t>*)filter);
    else
        return _tm_getSortedIndexs(_mem_sorted,newSyncInfo,*(TBloomFilter<uint64_t>*)filter);
}

void z_matchNewDataInOld(hpatch_StreamPos_t* out_newBlockDataInOldPoss,const TNewDataSyncInfo* newSyncInfo,
                         const hpatch_TStreamInput* oldStream,int threadNum){
    TBloomFilter<uint64_t> filter64;
    TBloomFilter<uint32_t> filter32;
    TBitSet                bitFilter;
    __Filter_Select_(_needHashBits,isUsedBitFilter,isTryUseHash32);
    if (isUsedBitFilter)
        bitFilter.clear((size_t)1<<_needHashBits);
    else if (isTryUseHash32)
        filter32.init((size_t)(TNewDataSyncInfo_blockCount(newSyncInfo)-newSyncInfo->samePairCount));
    else
        filter64.init((size_t)(TNewDataSyncInfo_blockCount(newSyncInfo)-newSyncInfo->samePairCount));

    _TMatchDatas matchDatas; memset(&matchDatas,0,sizeof(matchDatas));
    matchDatas.filter=isUsedBitFilter?(void*)&bitFilter:(isTryUseHash32?(void*)&filter32:(void*)&filter64);
    matchDatas.out_newBlockDataInOldPoss=out_newBlockDataInOldPoss;
    matchDatas.newSyncInfo=newSyncInfo;
    matchDatas.oldStream=oldStream;
    matchDatas.rollMatch=_z_rollMatch;
    matchDatas.getSortedIndexs=_z_getSortedIndexs;
    _matchNewDataInOld(matchDatas,threadNum);
}

} //namespace sync_private
