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
#include "string.h" //memmove
#include <algorithm> //sort, equal_range lower_bound
#include "sync_client_type_private.h"
#include "../../libHDiffPatch/HDiff/private_diff/mem_buf.h"
#include "../../libHDiffPatch/HDiff/private_diff/limit_mem_diff/bloom_filter.h"
#include "../../libParallel/parallel_channel.h"
#include "sync_diff_data.h"
using namespace hdiff_private;
namespace sync_private{

#define check(value,info) { if (!(value)) { throw std::runtime_error(info); } }
#define checkv(value)     check(value,"check "#value" error!")

//kIsSkipMatchedBlock 0: roll byte by byte  1: skip matched block, speed++, but patchSize+  2: skip next matched block  3: skip continue matched block 
#define     kIsSkipMatchedBlock     1
static const int kMatchHitOutLimit =16;       //limit match deep
static const size_t kBestReadSize  =1024*256; //for sequence read
#if (_IS_USED_MULTITHREAD)
static const size_t kBestMTClipSize=1*1024*1024; //for multi-thread read once
struct _TMatchDatas;
namespace{
    struct TMt:public TMtByChannel{
        inline explicit TMt(struct _TMatchDatas& _matchDatas)
            :matchDatas(_matchDatas),curWorki(0){}
        CHLocker readLocker;
        CHLocker workiLocker;
        CHLocker checkLocker;
        volatile size_t      curWorki;
        struct _TMatchDatas& matchDatas;

        inline bool getWorki(size_t worki){
            if (curWorki>worki) return false;
            CAutoLocker _auto_locker(workiLocker.locker);
            if (curWorki==worki){
                ++curWorki;
                return true;
            }else
                return false;
        }
    };
}
#endif //_IS_USED_MULTITHREAD

typedef unsigned char TByte;
typedef uint64_t tm_roll_uint;

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
        cacheSize=(cacheSize>=kBestReadSize)?cacheSize:kBestReadSize;
        hpatch_StreamPos_t maxDataSize=oldRollPosEnd()-oldRollBegin;
        if (cacheSize>maxDataSize) cacheSize=(size_t)maxDataSize;
#if (_IS_USED_MULTITHREAD)
        if (_mt)
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
    ~TOldDataCache_base(){
        if (m_checksumHandle)
            m_strongChecksumPlugin->close(m_strongChecksumPlugin,m_checksumHandle);
        if (m_checkChecksum)
            m_strongChecksumPlugin->close(m_strongChecksumPlugin,m_checkChecksum);
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
        if (isRollEnded()) return;
        const hpatch_StreamPos_t rollPosEnd=oldRollPosEnd();
        if (m_readedPos>=rollPosEnd) { _setRollEnded(); return; }//set end tag

        size_t needLen=m_cur-m_cache.data();
        if (m_readedPos+needLen>rollPosEnd)
            needLen=(size_t)(rollPosEnd-m_readedPos);
        if (m_cur-needLen+m_kSyncBlockSize>m_cache.data_end()) { _setRollEnded(); return; } //set end tag

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
#if (_IS_USED_MULTITHREAD)
            TMt* mt=(TMt*)m_mt;
            CAutoLocker _autoLocker(mt?mt->readLocker.locker:0);
#endif
            check(m_oldStream->read(m_oldStream,m_readedPos,buf,buf+readLen),
                  "TOldDataCache read oldData error!");
        }
        size_t zerolen=needLen-readLen;
        if (zerolen>0)
            memset(m_cache.data_end()-zerolen,0,zerolen);
        m_readedPos+=needLen;
        m_cur-=needLen;
    }

    inline bool isRollEnded()const{ return m_cur>m_cache.data_end(); }
    inline const TByte* calcPartStrongChecksum(size_t outPartBits){
        return _calcPartStrongChecksum(m_cur,m_kSyncBlockSize,outPartBits); }
    inline const TByte* strongChecksum()const{//must after do calcPartStrongChecksum()
        return m_strongChecksum_buf+m_checksumByteSize; }
    inline size_t strongChecksumByteSize()const{ return m_checksumByteSize; }
    inline hpatch_TChecksum* strongChecksumPlugin()const{ return m_strongChecksumPlugin; }
    inline hpatch_checksumHandle checkChecksum()const{ return m_checkChecksum; }
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
    hpatch_checksumHandle   m_checkChecksum;
    void*                   m_mt;

    inline void _setRollEnded() { m_cur=m_cache.data_end()+1; }
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
                if (isRollEnded()) return;
                m_rollHash=roll_hash_start(m_cur,m_kSyncBlockSize);
            }
    bool _cacheAndRoll(){
        TOldDataCache_base::_cache();
        if (isRollEnded()) return false;
        return roll();
    }
    inline tm_roll_uint hashValue()const{ return m_rollHash; }
    inline bool roll(){
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
            TOldDataCache_base::_cache();
            if (isRollEnded()) return false;
        }
        m_rollHash=roll_hash_start(m_cur,m_kSyncBlockSize);
        return true;
    }
protected:
    tm_roll_uint            m_rollHash;
};


static inline hpatch_StreamPos_t _indexMapTo(uint32_t i){
    return (hpatch_StreamPos_t)(kBlockType_needSync-1)-i; }
static inline uint32_t _indexMapFrom(hpatch_StreamPos_t pos){
    return (uint32_t)((kBlockType_needSync-1)-pos); }

typedef volatile hpatch_StreamPos_t volStreamPos_t;

static bool matchRange(hpatch_StreamPos_t* out_newBlockDataInOldPoss,
                       const uint32_t* range_begin,const uint32_t* range_end,TOldDataCache_base& oldData,
                       const TByte* partChecksums,size_t outPartChecksumBits,
                       TByte* newDataCheckChecksum,hpatch_StreamPos_t kMinRevSameIndex,void* _mt
  #if (kIsSkipMatchedBlock==3)
                       ,bool _isSkipContinue){
  #else
                       ){
  #endif
    const TByte* oldPartStrongChecksum=0;
    const size_t outPartChecksumSize=_bitsToBytes(outPartChecksumBits);
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
                oldPartStrongChecksum=oldData.calcPartStrongChecksum(outPartChecksumBits);
            const TByte* newPairStrongChecksum=partChecksums+newBlockIndex*outPartChecksumSize;
            if (0==memcmp(oldPartStrongChecksum,newPairStrongChecksum,outPartChecksumSize)){
                isMatched=true;
              #if (kIsSkipMatchedBlock==3)
                if (newBlockOldPosBack<kMinRevSameIndex){
                    if ((--hitOutLimit)<=0) break;
                }
              #endif
                hpatch_StreamPos_t curPos=oldData.curOldPos();
                {
#if (_IS_USED_MULTITHREAD)
                    TMt* mt=(TMt*)_mt;
                    CAutoLocker _autoLocker(mt?mt->checkLocker.locker:0);
                    newBlockOldPosBack=*pNewBlockDataInOldPos;
                    if (newBlockOldPosBack<kMinRevSameIndex){// other thread done
                        if ((--hitOutLimit)<=0) break;
                    }else
#endif
                    {
                        while(true){ //hit
                            (*pNewBlockDataInOldPos)=curPos;
                            checkChecksumAppendData(newDataCheckChecksum,newBlockIndex,
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
            const hpatch_StreamPos_t curOldPos=oldData.curOldPos(); \
            const bool _isSkipContinue=(curOldPos==_skipPosBack0)|(curOldPos==_skipPosBack1)|(curOldPos==_skipPosBack2)
    
static void _rollMatch(_TMatchDatas& rd,hpatch_StreamPos_t oldRollBegin,
                       hpatch_StreamPos_t oldRollEnd,void* _mt=0){
    if (rd.oldStream->streamSize<rd.newSyncInfo->kSyncBlockSize)
        return;
    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(rd.newSyncInfo);
    const hpatch_StreamPos_t kMinRevSameIndex=kBlockType_needSync-1-kBlockCount;
    TIndex_comp0 icomp0(rd.newSyncInfo->rollHashs,rd.newSyncInfo->savedRollHashByteSize);
    TOldDataCache oldData(rd.oldStream,oldRollBegin,oldRollEnd,
                          rd.newSyncInfo->kSyncBlockSize,rd.strongChecksumPlugin,_mt);
    uint8_t part[sizeof(tm_roll_uint)]={0};
    const size_t savedRollHashBits=rd.newSyncInfo->savedRollHashBits;
    const TBloomFilter<tm_roll_uint>& filter=*(TBloomFilter<tm_roll_uint>*)rd.filter;
    tm_roll_uint digestFull_back=~oldData.hashValue(); //not same digest
  #if (kIsSkipMatchedBlock>=2)
    hpatch_StreamPos_t _skipPosBack0=0;
    hpatch_StreamPos_t _skipPosBack1=0;
    hpatch_StreamPos_t _skipPosBack2=oldData.curOldPos();
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
                                  rd.newSyncInfo->partChecksums,rd.newSyncInfo->savedStrongChecksumBits,
                                  rd.newSyncInfo->savedNewDataCheckChecksum,kMinRevSameIndex,_mt
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
                if (oldData.nextBlock()) continue; else break;
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
        for (size_t i=0;i<workCount;++i) {
            if (!mt.getWorki(i))
                continue;
            hpatch_StreamPos_t oldPosBegin=i*clipSize;
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
    const size_t savedRollHashByteSize=newSyncInfo->savedRollHashByteSize;
    const size_t savedRollHashBits=newSyncInfo->savedRollHashBits;
    _init_newBlockDataInOldPoss(out_newBlockDataInOldPoss,kBlockCount);
    
    TAutoMem _mem_sorted(kMatchBlockCount*(size_t)sizeof(uint32_t));
    uint32_t* sorted_newIndexs=(uint32_t*)_mem_sorted.data();
    TBloomFilter<tm_roll_uint> filter; filter.init(kMatchBlockCount);
    {
        checkv(matchDatas.oldStream->streamSize<(hpatch_StreamPos_t)(kBlockType_needSync-1-kBlockCount));
        const uint8_t* partRollHash=newSyncInfo->rollHashs;
        uint32_t curPair=0;
        uint32_t indexi=0;
        for (uint32_t i=0;i<kBlockCount;++i,partRollHash+=savedRollHashByteSize){
            if ((curPair<newSyncInfo->samePairCount)&&(i==newSyncInfo->samePairList[curPair].curIndex)){
                uint32_t sameIndex=newSyncInfo->samePairList[curPair].sameIndex;
                while (out_newBlockDataInOldPoss[sameIndex]!=kBlockType_needSync){
                    sameIndex=_indexMapFrom(out_newBlockDataInOldPoss[sameIndex]);
                    assert(sameIndex<kBlockCount);
                }
                assert(sameIndex<i);
                out_newBlockDataInOldPoss[sameIndex]=_indexMapTo(i);
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
    }
    
    //optimize for std::equal_range
    unsigned int kTableBit =getBetterCacheBlockTableBit(kMatchBlockCount);
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
            pos=std::lower_bound(pos,sorted_newIndexs+kMatchBlockCount,digest_value,icomp0);
            sorted_newIndexs_table[i]=(uint32_t)(pos-sorted_newIndexs);
        }
        sorted_newIndexs_table[((size_t)1<<kTableBit)]=kMatchBlockCount;
    }

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
    uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    
    _TMatchDatas matchDatas; memset(&matchDatas,0,sizeof(matchDatas));
    matchDatas.out_newBlockDataInOldPoss=out_newBlockDataInOldPoss;
    matchDatas.newSyncInfo=newSyncInfo;
    matchDatas.oldStream=oldStream;
    matchDatas.strongChecksumPlugin=strongChecksumPlugin;
    _matchNewDataInOld(matchDatas,threadNum);
}

} //namespace sync_private
