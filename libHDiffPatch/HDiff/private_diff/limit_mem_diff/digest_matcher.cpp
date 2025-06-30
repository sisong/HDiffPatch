//digest_macher.cpp
/*
 The MIT License (MIT)
 Copyright (c) 2012-2017 HouSisong
 
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

#include "digest_matcher.h"
#include <stdexcept>  //std::runtime_error
#include <algorithm>  //std::sort,std::equal_range
#include "../compress_detect.h" //_getUIntCost
#include "../../../../libParallel/parallel_channel.h"
#include "../qsort_parallel.h"
#if (_IS_USED_MULTITHREAD)
#include <atomic>
#endif
namespace hdiff_private{
static  const size_t kMinTrustMatchedLength=1024*16;
static  const size_t kMinMatchedLength = 16;
static  const size_t kMatchBlockSize_min=4;//sizeof(hpatch_uint32_t);
static  const size_t kBestReadSize=1024*256; //for sequence read
static  const size_t kMinReadSize=1024*4;    //for random first read speed
static  const size_t kMinBackupReadSize=256;
static  const size_t kBestMatchRange=1024*64;
static  const size_t kMaxLinkIndexFindCount=64;
static  const size_t kMinParallelSize=1024*1024*2;
static  const size_t kBestParallelSize=1024*1024*8;


#define readStream(stream,pos,dst,n) { \
    if (((n)>0)&&(!(stream)->read(stream,pos,dst,dst+(n)))) \
        throw std::runtime_error("TStreamCache::_resetPos_continue() stream->read() error!"); }

struct TStreamCache{
    TStreamCache(const hpatch_TStreamInput* _stream,unsigned char* _cache,size_t _cacheSize,void* _locker)
    :stream(_stream),m_readPos(0),m_readPosEnd(0),m_locker(_locker),
        cache(_cache),cacheSize(_cacheSize),cachePos(_cacheSize){ }
    inline hpatch_StreamPos_t streamSize()const{ return stream->streamSize; }
    inline hpatch_StreamPos_t pos()const { return m_readPosEnd-dataLength(); }
    inline const size_t   dataLength()const{ return (size_t)(cacheSize-cachePos); }
    inline const unsigned char* data()const { return cache+cachePos; }
    inline bool resetPos(size_t kBackupCacheSize,hpatch_StreamPos_t streamPos,size_t kMinCacheDataSize){
        if (_is_hit_cache(kBackupCacheSize,streamPos,kMinCacheDataSize)){
            cachePos=cacheSize-(size_t)(m_readPosEnd-streamPos);
            return true; //hit cache
        }
        return _resetPos_continue(kBackupCacheSize,streamPos,kMinCacheDataSize);
    }
protected:
    inline bool _is_hit_cache(size_t kBackupCacheSize,hpatch_StreamPos_t streamPos,size_t kMinCacheDataSize){
        return (streamPos+kMinCacheDataSize<=m_readPosEnd)&&(streamPos>=m_readPos+kBackupCacheSize);
    }
    inline unsigned char* cachedData(){ return cache+cacheSize-cachedLength(); }
    inline const size_t   cachedLength()const{ return (size_t)(m_readPosEnd-m_readPos); }
    bool _resetPos_continue(size_t kBackupCacheSize,hpatch_StreamPos_t streamPos,size_t kMinCacheDataSize){
        //stream:[...     |                |                   |                  |                      |      ...]
        //             readPos (streamPos-kBackupCacheSize) streamPos (streamPos+kMinCacheDataSize) (readPosEnd)
        //cache:     [    |                                    |                  |                      ]
        //           0                                   (init cachePos)                              cacheSize
        hpatch_StreamPos_t streamSize=stream->streamSize;
        if (streamPos+kMinCacheDataSize>streamSize) return false;
        hpatch_StreamPos_t readPos=(streamPos>=kBackupCacheSize)?(streamPos-kBackupCacheSize):0;
        size_t readLen=((streamSize-readPos)>=cacheSize)?cacheSize:(size_t)(streamSize-readPos);
#if (_IS_USED_MULTITHREAD)
        if (m_locker){
            CAutoLocker _autoLocker(m_locker);
            _resetPos_continue_read(readPos,readLen);
        }else
#endif
        {
            _resetPos_continue_read(readPos,readLen);
        }
        m_readPos=readPos;
        m_readPosEnd=readPos+readLen;
        cachePos=cacheSize-(size_t)(m_readPosEnd-streamPos);
        return true;
    }
private:
    void _resetPos_continue_read(hpatch_StreamPos_t readPos,size_t readLen){
        unsigned char* dst=cache+cacheSize-readLen;
        if ((m_readPosEnd>readPos)&&(m_readPos<=readPos)){
            size_t moveLen=(size_t)(m_readPosEnd-readPos);
            memmove(dst,cache+cacheSize-moveLen,moveLen);
            readStream(stream,readPos+moveLen,dst+moveLen,readLen-moveLen);
        }else if ((m_readPos<readPos+readLen)&&(m_readPosEnd>=readPos+readLen)){
            size_t moveLen=(size_t)(readPos+readLen-m_readPos);
            memmove(dst+readLen-moveLen,cachedData(),moveLen);
            readStream(stream,readPos,dst,readLen-moveLen);
        }else{
            readStream(stream,readPos,dst,readLen);
        }
    }
    const hpatch_TStreamInput* stream;
protected:
    hpatch_StreamPos_t         m_readPos;
    hpatch_StreamPos_t         m_readPosEnd;
    void*                      m_locker;
    unsigned char*             cache;
    size_t                     cacheSize;
    size_t                     cachePos;
};

template <class T,class Tn>
inline static T upperCount(T all_size,Tn node_size){ return (all_size+node_size-1)/node_size; }

inline static size_t getBackupSize(size_t kMatchBlockSize) {
    return (kMatchBlockSize>=kMinBackupReadSize)?kMatchBlockSize:kMinBackupReadSize; }

static hpatch_StreamPos_t getBlockCount(size_t kMatchBlockSize,
                                        hpatch_StreamPos_t streamSize){
    return upperCount(streamSize,kMatchBlockSize);
}
static hpatch_StreamPos_t blockIndexToPos(size_t index,size_t kMatchBlockSize,
                                          hpatch_StreamPos_t streamSize){
    hpatch_StreamPos_t pos=(hpatch_StreamPos_t)index * kMatchBlockSize;
    if (pos+kMatchBlockSize>streamSize)
        pos=streamSize-kMatchBlockSize;
    return pos;
}
    
static size_t posToBlockIndex(hpatch_StreamPos_t pos,size_t kMatchBlockSize,size_t blocksSize){
    size_t result=(size_t)((pos+(kMatchBlockSize>>1))/kMatchBlockSize);
    if (result>=blocksSize) result=blocksSize-1;
    return result;
}


TDigestMatcher::~TDigestMatcher(){
}
    
size_t TDigestMatcher::getSearchThreadNum()const{
#if (_IS_USED_MULTITHREAD)
    const size_t threadNum=m_mtsets.threadNumForSearch;
    hpatch_StreamPos_t size=m_newData->streamSize;
    if ((threadNum>1)&&(m_oldData->streamSize>=m_kMatchBlockSize)
      &&(size>=kMinParallelSize)&&(size/2>=m_kMatchBlockSize)) {
        const hpatch_StreamPos_t maxThreanNum=size/(kMinParallelSize/2);
        return (threadNum<=maxThreanNum)?threadNum:(size_t)maxThreanNum;
    }else
#endif
    {
        return 1;
    }
}

TDigestMatcher::TDigestMatcher(const hpatch_TStreamInput* oldData,const hpatch_TStreamInput* newData,
                               size_t kMatchBlockSize,const hdiff_TMTSets_s& mtsets)
:m_oldData(oldData),m_newData(newData),m_isUseLargeSorted(true),m_mtsets(mtsets),
m_newCacheSize(0),m_oldCacheSize(0),m_oldMinCacheSize(0),m_backupCacheSize(0),m_kMatchBlockSize(0){
    if (kMatchBlockSize>(oldData->streamSize+1)/2)
        kMatchBlockSize=(size_t)((oldData->streamSize+1)/2);
    if (kMatchBlockSize<kMatchBlockSize_min)
        kMatchBlockSize=kMatchBlockSize_min;
    if (oldData->streamSize<kMatchBlockSize) return;
    m_kMatchBlockSize=kMatchBlockSize;
    
    hpatch_StreamPos_t _blockCount=getBlockCount(m_kMatchBlockSize,m_oldData->streamSize);
    m_isUseLargeSorted=(_blockCount>=((hpatch_StreamPos_t)1<<32));
    const size_t blockCount=(size_t)_blockCount;
    if (blockCount!=_blockCount)
        throw std::runtime_error("TDigestMatcher() oldData->streamSize/MatchBlockSize too big error!");
    m_blocks.resize(blockCount);
    if (m_isUseLargeSorted)
        m_sorted_larger.resize(blockCount);
    else
        m_sorted_limit.resize(blockCount);
    
    m_backupCacheSize=getBackupSize(m_kMatchBlockSize);
    m_newCacheSize=upperCount(m_kMatchBlockSize*2+m_backupCacheSize,kBestReadSize)*kBestReadSize;
    m_oldCacheSize=upperCount(m_kMatchBlockSize+m_backupCacheSize,kBestReadSize)*kBestReadSize;
    m_oldMinCacheSize=upperCount(m_kMatchBlockSize+m_backupCacheSize,kMinReadSize)*kMinReadSize;
    assert(m_oldMinCacheSize<=m_oldCacheSize);
    m_mem.realloc((m_newCacheSize+m_oldCacheSize)*getSearchThreadNum());
    getDigests();
}

struct TIndex_comp{
    inline TIndex_comp(const adler_uint_t* _blocks,size_t _n,size_t _kMaxDeep)
        :blocks(_blocks),n(_n),kMaxDeep(_kMaxDeep){ }
    template<class TIndex>
    inline bool operator()(TIndex x,TIndex y)const {
        adler_uint_t xd=blocks[x];
        adler_uint_t yd=blocks[y];
        if (xd!=yd)
            return xd<yd;
        else
            return _cmp(x+1,y+1,n,kMaxDeep,blocks);
    }
private:
    const adler_uint_t* blocks;
    const size_t        n;
    const size_t        kMaxDeep;
    
    template<class TIndex>
    static bool _cmp(TIndex x,TIndex y,size_t n,size_t kMaxDeep,
                     const adler_uint_t* blocks) {
        if (x!=y) {} else{ return false; }
        size_t deep=n-((x>y)?x:y);
        deep=(deep<=kMaxDeep)?deep:kMaxDeep;
        for (size_t i=0;i<deep;++i){
            adler_uint_t xd=blocks[x++];
            adler_uint_t yd=blocks[y++];
            if (xd!=yd)
                return xd<yd;
        }
        return x>y;
    }
};

template<bool isMT>
static void _filter_insert(TBloomFilter<adler_hash_t>* filter,const adler_uint_t* begin,const adler_uint_t* end){
    while (begin!=end){
        adler_hash_t h=adler_to_hash(*begin++);
#if (_IS_USED_MULTITHREAD)
        if (isMT)
            filter->insert_MT(h);
        else
#endif
            filter->insert(h);
    }
}

static void filter_insert_parallel(TBloomFilter<adler_hash_t>& filter,const adler_uint_t* begin,
                                   const adler_uint_t* end,size_t threadNum){
#if (_IS_USED_MULTITHREAD)
    const size_t kInsertMinParallelSize=4096;
    const size_t size=end-begin;
    if ((threadNum>1)&&(size>=kInsertMinParallelSize)) {
        const size_t maxThreanNum=size/(kInsertMinParallelSize/2);
        threadNum=(threadNum<=maxThreanNum)?threadNum:maxThreanNum;

        const size_t step=size/threadNum;
        const size_t threadCount=threadNum-1;
        std::vector<std::thread> threads(threadCount);
        for (size_t i=0;i<threadCount;i++,begin+=step)
            threads[i]=std::thread(_filter_insert<true>,&filter,begin,begin+step);
        _filter_insert<true>(&filter,begin,end);
        for (size_t i=0;i<threadCount;i++)
            threads[i].join();
    }else
#endif
    {
        _filter_insert<false>(&filter,begin,end);
    }
}


#define __sort_indexs(TIndex,indexs,comp,m_threadNum) sort_parallel<TIndex,TIndex_comp,1024*8,137> \
                            (indexs.data(),indexs.data()+indexs.size(),comp,m_threadNum)

void TDigestMatcher::getDigests(){
    if (m_blocks.empty()) return;
    
    const size_t blockCount=m_blocks.size();
    TStreamCache streamCache(m_oldData,m_mem.data(),m_newCacheSize+m_oldCacheSize,0);
    for (size_t i=0;i<blockCount;++i) {
        hpatch_StreamPos_t readPos=blockIndexToPos(i,m_kMatchBlockSize,m_oldData->streamSize);
        streamCache.resetPos(0,readPos,m_kMatchBlockSize);
        adler_uint_t adler=adler_start(streamCache.data(),m_kMatchBlockSize);
        m_blocks[i]=adler;
        if (m_isUseLargeSorted)
            m_sorted_larger[i]=i;
        else
            m_sorted_limit[i]=(uint32_t)i;
    }
    m_filter.init(blockCount);
    filter_insert_parallel(m_filter,m_blocks.data(),m_blocks.data()+blockCount,m_mtsets.threadNum);

    size_t kMaxCmpDeep= 1 + upperCount(kMinTrustMatchedLength,m_kMatchBlockSize);
    TIndex_comp comp(m_blocks.data(),m_blocks.size(),kMaxCmpDeep);
    if (m_isUseLargeSorted)
        __sort_indexs(std::vector<size_t>::value_type,m_sorted_larger,comp,m_mtsets.threadNum);
    else
        __sort_indexs(uint32_t,m_sorted_limit,comp,m_mtsets.threadNum);
}

struct TBlockStreamCache:public TStreamCache{
    TBlockStreamCache(const hpatch_TStreamInput* _stream,unsigned char* _cache,size_t _cacheSize,
                      size_t _backupCacheSize, size_t _kMatchBlockSize,void* _locker)
    :TStreamCache(_stream,_cache,_cacheSize,_locker),
    backupCacheSize(_backupCacheSize),kMatchBlockSize(_kMatchBlockSize){
        assert(cacheSize>=(backupCacheSize+kMatchBlockSize)); }
    inline bool resetPos(hpatch_StreamPos_t streamPos){
        return TStreamCache::resetPos(backupCacheSize,streamPos,kMatchBlockSize);
    }
public:
    inline bool _loop_backward_cache(size_t skipLen,size_t needDataSize){
        return TStreamCache::resetPos(0,pos()+skipLen,needDataSize);
    }
    size_t forward_equal_length(const TBlockStreamCache& y)const{
        size_t y_len=(size_t)(y.pos()-y.m_readPos);
        size_t len=(size_t)(pos()-m_readPos);
        len=(len<y_len)?len:y_len;
        const unsigned char* px=data();
        const unsigned char* py=y.data();
        size_t i=0;
        for (;i<len;++i){
            if (*(--px) != *(--py)) break;
        }
        return i;
    }
public:
    const size_t    backupCacheSize;
    const size_t    kMatchBlockSize;
};

struct TOldStreamCache:public TBlockStreamCache{
    TOldStreamCache(const hpatch_TStreamInput* _stream,unsigned char* _cache,
                    size_t _minCacheSize,size_t _maxCacheSize,
                    size_t _backupCacheSize,size_t _kMatchBlockSize,void* _locker)
    :TBlockStreamCache(_stream,_cache+_maxCacheSize-_minCacheSize,
                       _minCacheSize, _backupCacheSize,_kMatchBlockSize,_locker),
    minCacheSize(_minCacheSize),maxCacheSize(_maxCacheSize){ }
    
    inline bool resetPos(hpatch_StreamPos_t streamPos){
        if (!_is_hit_cache(backupCacheSize,streamPos,kMatchBlockSize))
            _doMinCacheSize();
        return TBlockStreamCache::resetPos(streamPos);
    }
    hpatch_StreamPos_t loop_backward_equal_length(TBlockStreamCache& y){
        if (!_loop_backward_cache(kMatchBlockSize,1)) return 0;
        if (!y._loop_backward_cache(kMatchBlockSize,1)) return 0;
        hpatch_StreamPos_t eq_len=0;
        while(true){
            const size_t xLen=dataLength();
            size_t len=y.dataLength();
            len=(len>=xLen)?xLen:len;
            const unsigned char* px=data();
            const unsigned char* py=y.data();
            for (size_t i=0;i<len;++i){
                if (px[i]!=py[i])
                    return eq_len+i;
            }
            eq_len+=len;
            if ((len==xLen)&(cacheSize<maxCacheSize))
                _doGrowCacheSize();
            if (!_loop_backward_cache(len,1)) break;
            if (!y._loop_backward_cache(len,1)) break;
        }
        return eq_len;
    }
private:
    const size_t minCacheSize;
    const size_t maxCacheSize;
    inline void _doMinCacheSize(){
        cache    +=cacheSize-minCacheSize;
        cacheSize=minCacheSize;
        cachePos =minCacheSize;
        m_readPos=(cachedLength()<=cacheSize)?m_readPos:m_readPosEnd-cacheSize;
    }
    inline void _doGrowCacheSize(){
        const size_t growSize=(cacheSize*2<=maxCacheSize)?cacheSize:(maxCacheSize-cacheSize);
        cache    -=growSize;
        cacheSize+=growSize;
        cachePos +=growSize;
    }
};

struct TNewStreamCache:public TBlockStreamCache{
    TNewStreamCache(const hpatch_TStreamInput* _stream,unsigned char* _cache,size_t _cacheSize,
                    size_t _backupCacheSize,size_t _kMatchBlockSize,void* _locker)
    :TBlockStreamCache(_stream,_cache,_cacheSize,_backupCacheSize,_kMatchBlockSize,_locker){
        resetPos(0);
    }
    void toBestDataLength(){
        if (dataLength()*2>=cacheSize) return;
        bool result=_resetPos_continue(backupCacheSize,pos(),kMatchBlockSize);
        assert(result);
    }
    
    inline bool resetPos(hpatch_StreamPos_t streamPos){
        if (!TBlockStreamCache::resetPos(streamPos)) return false;
        roll_digest=adler_start(data(),kMatchBlockSize);
        return true;
    }
    inline bool roll(){
        //warning: after running _loop_backward_cache(),cache roll logic is failure
        if (dataLength()>kMatchBlockSize){
            const unsigned char* cur_datas=data();
            roll_digest=adler_roll(roll_digest,kMatchBlockSize,cur_datas[0],cur_datas[kMatchBlockSize]);
            ++cachePos;
            return true;
        }else{
            if (!TBlockStreamCache::resetPos(pos()+1)) return false;
            --cachePos;
            return roll();
        }
    }
    inline adler_uint_t rollDigest()const{ return roll_digest; }
private:
    adler_uint_t               roll_digest;
};


struct TDigest_comp{
    inline explicit TDigest_comp(const adler_uint_t* _blocks):blocks(_blocks){ }
    struct TDigest{
        adler_uint_t value;
        inline explicit TDigest(adler_uint_t _value):value(_value){}
    };
    template<class TIndex> inline
    bool operator()(const TIndex& x,const TDigest& y)const { return blocks[x]<y.value; }
    template<class TIndex> inline
    bool operator()(const TDigest& x,const TIndex& y)const { return x.value<blocks[y]; }
    template<class TIndex> inline
    bool operator()(const TIndex& x, const TIndex& y)const { return blocks[x]<blocks[y]; }
protected:
    const adler_uint_t* blocks;
};

struct TDigest_comp_i:public TDigest_comp{
    inline TDigest_comp_i(const adler_uint_t* _blocks,size_t _n):TDigest_comp(_blocks),n(_n),i(0){ }
    template<class TIndex> inline
    bool operator()(const TIndex& x,const TDigest& y)const { return (x+i<n)?(blocks[x+i]<y.value):true; }
    template<class TIndex> inline
    bool operator()(const TDigest& x,const TIndex& y)const { return (y+i<n)?(x.value<blocks[y+i]):false; }
    template<class TIndex> inline
    bool operator()(const TIndex& x, const TIndex& y)const {
        if ((x+i<n)&(y+i<n)) return blocks[x+i]<blocks[y+i];
        if (x+i<n) return false;
        if (y+i<n) return true;
        return x>y;//n-x<n-y
    }
private:
    const size_t        n;
public:
    size_t              i;
};

    static hpatch_StreamPos_t getMatchLength(TOldStreamCache& oldStream,TNewStreamCache& newStream,
                                             hpatch_StreamPos_t* pOldPos,size_t kMatchBlockSize,
                                             const TCover& lastCover){
        if (oldStream.resetPos(*pOldPos)&&
            (0==memcmp(oldStream.data(),newStream.data(),kMatchBlockSize))){
            const hpatch_StreamPos_t newPos=newStream.pos();
            size_t feq_len=oldStream.forward_equal_length(newStream);
            if (newPos-feq_len<lastCover.newPos+lastCover.length)
                feq_len=(size_t)(newPos-(lastCover.newPos+lastCover.length));
            hpatch_StreamPos_t beq_len=oldStream.loop_backward_equal_length(newStream);
            *pOldPos=(*pOldPos)-feq_len;
            return feq_len+kMatchBlockSize+beq_len;
        }else{
            return 0;
        }
    }

template <class TIndex>
static const TIndex* getBestMatchi(const adler_uint_t* blocksBase,size_t blocksSize,
                                   const TIndex** _left,const TIndex** _right,
                                   TNewStreamCache& newStream,const TCover& lastCover,
                                   size_t* _digests_eq_n){
    const size_t kMatchBlockSize=newStream.kMatchBlockSize;
    size_t max_digests_n=upperCount(kMinTrustMatchedLength,kMatchBlockSize);
    size_t _data_max_digests_n=newStream.dataLength()/kMatchBlockSize;
    if (max_digests_n>_data_max_digests_n) max_digests_n=_data_max_digests_n;
    
    const TIndex* best=0;
    const TIndex*& left=*_left;
    const TIndex*& right=*_right;
    size_t& digests_eq_n=*_digests_eq_n;
    digests_eq_n=1;
    //缩小[left best right)范围,留下最多2个(因为签名匹配并不保证一定相等,2个的话应该就够了?);
    if (right-left>1){
        //寻找最长的签名匹配位置(也就是最有可能的最长匹配位置);
        TDigest_comp_i comp_i(blocksBase,blocksSize);
        newStream.toBestDataLength();
        const unsigned char* bdata=newStream.data()+kMatchBlockSize;
        for (; (digests_eq_n<max_digests_n)&&(right-left>1);++digests_eq_n,bdata+=kMatchBlockSize){
            adler_uint_t digest=adler_start(bdata,kMatchBlockSize);
            typename TDigest_comp::TDigest digest_value(digest);
            comp_i.i=digests_eq_n;
            std::pair<const TIndex*,const TIndex*> i_range=
                std::equal_range(left,right,digest_value,comp_i);
            size_t rn=i_range.second-i_range.first;
            if (rn==0){
                break;
            }else if (rn==1){
                best=i_range.first;
                break;
            }else{
                left=i_range.first;
                right=i_range.second;
            }
        }
    }else{
        best=left;
    }
    //best==0 说明有>2个位置都是最好位置,还需要继续寻找;
    
    //assert(newStream.pos()>lastCover.newPos);
    hpatch_StreamPos_t linkOldPos=newStream.pos()+lastCover.oldPos-lastCover.newPos;
    TIndex linkIndex=(TIndex)posToBlockIndex(linkOldPos,kMatchBlockSize,blocksSize);
    //找到lastCover附近的位置当作比较好的best默认值,以利于link或压缩;
    if (best==0){
        TIndex_comp comp(blocksBase,blocksSize,max_digests_n);
        size_t findCount=(right-left)*2+1;
        if (findCount>kMaxLinkIndexFindCount) findCount=kMaxLinkIndexFindCount;
        for (TIndex inc=1;(inc<=findCount);++inc) { //linkIndex附近找;
            TIndex fi;  TIndex s=(inc>>1);
            if (inc&1){
                if (linkIndex<s) continue;
                fi=linkIndex-s;
            }else{
                if (linkIndex+s>=blocksSize) continue;
                fi=linkIndex+s;
            }
            std::pair<const TIndex*,const TIndex*> i_range=std::equal_range(left,right,fi,comp);
            if (i_range.first!=i_range.second){
                best=i_range.first+(i_range.second-i_range.first)/2;
                for (const TIndex* ci=best;ci<i_range.second; ++ci) {
                    if (*ci==fi) { best=ci; break;  } //找到;
                }
                break;
            }
        }
    }
    if(best==0){ //继续找;
        best=left+(right-left)/2;
        hpatch_StreamPos_t _best_distance=hpatch_kNullStreamPos;
        const TIndex* end=(left+kBestMatchRange>=right)?right:(left+kBestMatchRange);
        for (const TIndex* it=left;it<end; ++it) {
            hpatch_StreamPos_t oldIndex=(*it);
            hpatch_StreamPos_t distance=(oldIndex<linkIndex)?(linkIndex-oldIndex):(oldIndex-linkIndex);
            if (distance<_best_distance){ //找最近;
                best=it;
                _best_distance=distance;
            }
        }
    }
    return best;
}

template <class TIndex>
static bool getBestMatch(const TIndex* left,const TIndex* right,const TIndex* best,
                         TOldStreamCache& oldStream,TNewStreamCache& newStream,
                         const TCover& lastCover,size_t digests_eq_n,TCover* out_curCover){
    assert(best!=0);
    const size_t kMatchBlockSize=newStream.kMatchBlockSize;
    const hpatch_StreamPos_t newPos=newStream.pos();
    bool isMatched=false;
    hpatch_StreamPos_t  bestLen=0;
    const size_t kMaxFindCount=5; //周围距离2;
    size_t findCount=(right-left)*2+1;
    if (findCount>kMaxFindCount) findCount=kMaxFindCount;
    for (size_t inc=1;(inc<=findCount);++inc) { //best附近找;
        const TIndex* fi;  size_t s=(inc>>1);
        if (inc&1){
            if (best<left+s) continue;
            fi=best-s;
        }else{
            if (best+s>=right) continue;
            fi=best+s;
        }
        if (inc>1)
            newStream.TBlockStreamCache::resetPos(newPos);
        
        hpatch_StreamPos_t oldPos=blockIndexToPos(*fi,kMatchBlockSize,oldStream.streamSize());
        hpatch_StreamPos_t matchedOldPos=oldPos;
        hpatch_StreamPos_t curEqLen=getMatchLength(oldStream,newStream,
                                                   &matchedOldPos,kMatchBlockSize,lastCover);
        if (curEqLen>bestLen){
            isMatched=true;
            bestLen=curEqLen;
            out_curCover->length=curEqLen;
            out_curCover->oldPos=matchedOldPos;
            out_curCover->newPos=newPos-(oldPos-matchedOldPos);
            if (curEqLen>=digests_eq_n*kMatchBlockSize)
                break;//matched maybe best
        }
    }
    return isMatched;
}
    
    static const size_t getOldPosCost(hpatch_StreamPos_t oldPos,const TCover& lastCover){
        hpatch_StreamPos_t oldPosEnd=lastCover.oldPos+lastCover.length;
        hpatch_StreamPos_t dis=(oldPos>=oldPosEnd)?(oldPos-oldPosEnd):(oldPosEnd-oldPos);
        return _getUIntCost(dis*2);
    }
    
    //尝试共线;
    static void tryLink(const TCover& lastCover,TCover& matchCover,
                        TOldStreamCache& oldStream,TNewStreamCache& newStream){
        if (lastCover.length<=0) return;
        hpatch_StreamPos_t linkOldPos=matchCover.newPos+lastCover.oldPos-lastCover.newPos;
        if (linkOldPos==matchCover.oldPos) return;
        newStream.TBlockStreamCache::resetPos(matchCover.newPos);
        hpatch_StreamPos_t matchedOldPos=linkOldPos;
        hpatch_StreamPos_t curEqLen=getMatchLength(oldStream,newStream,&matchedOldPos,
                                                   newStream.kMatchBlockSize,lastCover);
        size_t unlinkCost=getOldPosCost(matchCover.oldPos,lastCover);
        size_t unlinkCost_link=getOldPosCost(matchedOldPos,lastCover);
        if ((curEqLen>=kMinMatchedLength)
            &&(curEqLen+unlinkCost>=matchCover.length+unlinkCost_link)){
            matchCover.length=curEqLen;
            matchCover.oldPos=matchedOldPos;
            matchCover.newPos=matchCover.newPos-(linkOldPos-matchedOldPos);
        }
    }

template <class TIndex>
static void tm_search_cover(const adler_uint_t* blocksBase,
                            const TIndex* iblocks,const TIndex* iblocks_end,
                            TOldStreamCache& oldStream,TNewStreamCache& newStream,
                            const TBloomFilter<adler_hash_t>& filter,
                            hpatch_TOutputCovers* out_covers,
                            hpatch_StreamPos_t _coverNewOffset,
                            void* _newLocker,void* _dataLocker,const hdiff_TMTSets_s& _mtsets) {
    const size_t blocksSize=iblocks_end-iblocks;
    TDigest_comp comp(blocksBase);
    TCover  lastCover={0,0,0};
    while (true) {
        adler_uint_t digest=newStream.rollDigest();
        if (!filter.is_hit(adler_to_hash(digest)))
            { if (newStream.roll()) continue; else break; }//finish
        typename TDigest_comp::TDigest digest_value(digest);
        std::pair<const TIndex*,const TIndex*>
        range=std::equal_range(iblocks,iblocks_end,digest_value,comp);
        if (range.first==range.second)
            { if (newStream.roll()) continue; else break; }//finish
        
        hpatch_StreamPos_t newPosNext=newStream.pos()+1;
        size_t digests_eq_n;
        const TIndex* besti=getBestMatchi(blocksBase,blocksSize,&range.first,&range.second,
                                          newStream,lastCover,&digests_eq_n);
        {
    #if (_IS_USED_MULTITHREAD)
            CAutoLocker _autoDataLocker(_mtsets.oldDataIsMTSafe?0:
                                        (_mtsets.newAndOldDataIsMTSameRes?_newLocker:_dataLocker));
    #endif
            TCover  curCover;
            if (getBestMatch(range.first,range.second,besti,oldStream,newStream,
                             lastCover,digests_eq_n,&curCover)){
                tryLink(lastCover,curCover,oldStream,newStream);
                if (curCover.length>=kMinMatchedLength){//matched
                    TCover _cover;
                    setCover(_cover,curCover.oldPos,curCover.newPos+_coverNewOffset,curCover.length);
                    {
    #if (_IS_USED_MULTITHREAD)
                        CAutoLocker _autoCoverLocker(_mtsets.oldDataIsMTSafe?_dataLocker:0);
    #endif
                        if (!out_covers->push_cover(out_covers,&_cover))
                            throw std::runtime_error("TDigestMatcher::search_cover() push_cover error!");
                    }
                    lastCover=curCover;
                    newPosNext=curCover.newPos+curCover.length;//next
                }
            }
        }
        //next
        if (!newStream.resetPos(newPosNext)) break;//finish
    }
}

#define __search_cover(indexs,coverNewOffset,newLocker,dataLocker)  \
            tm_search_cover(m_blocks.data(),indexs.data(),indexs.data()+indexs.size(), \
                            oldStream,newStream,m_filter,out_covers,coverNewOffset,newLocker,dataLocker,m_mtsets)

void TDigestMatcher::_search_cover(const hpatch_TStreamInput* newData,hpatch_StreamPos_t newOffset,
                                   hpatch_TOutputCovers* out_covers,unsigned char* pmem,
                                   void* newDataLocker,void* dataLocker){
    TNewStreamCache newStream(newData,pmem,m_newCacheSize,m_backupCacheSize,
                              m_kMatchBlockSize,m_mtsets.newDataIsMTSafe?0:newDataLocker);
    TOldStreamCache oldStream(m_oldData,pmem+m_newCacheSize,m_oldMinCacheSize,
                              m_oldCacheSize,m_backupCacheSize,m_kMatchBlockSize,0);    
    if (m_isUseLargeSorted)
        __search_cover(m_sorted_larger,newOffset,newDataLocker,dataLocker);
    else
        __search_cover(m_sorted_limit,newOffset,newDataLocker,dataLocker);
}

#if (_IS_USED_MULTITHREAD)
# if (_IS_NO_ATOMIC_U64)
#   define uint_work size_t 
# else
#   define uint_work hpatch_StreamPos_t 
# endif
struct mt_data_t{
    CHLocker    newDataLocker;
    CHLocker    dataLocker;
    uint_work          workCount;
    volatile uint_work workIndex;
};
#endif

void TDigestMatcher::_search_cover_thread(hpatch_TOutputCovers* out_covers,
                                          unsigned char* pmem,void* mt_data){
#if (_IS_USED_MULTITHREAD)
    const size_t kPartPepeatSize=m_kMatchBlockSize-1;
    mt_data_t& mt=*(mt_data_t*)mt_data;
    const uint_work workCount=mt.workCount;
    const hpatch_StreamPos_t rollCount=m_newData->streamSize-(m_kMatchBlockSize-1);
    std::atomic<uint_work>& workIndex=*(std::atomic<uint_work>*)&mt.workIndex;
    while (true){
        uint_work curWorkIndex=workIndex++;
        if (curWorkIndex>=workCount) break;
        hpatch_StreamPos_t new_begin=rollCount*curWorkIndex/workCount;
        hpatch_StreamPos_t new_end=(curWorkIndex+1<workCount)?rollCount*(curWorkIndex+1)/workCount:rollCount;
        assert(new_end+kPartPepeatSize<=m_newData->streamSize);
        TStreamInputClip newClip;
        TStreamInputClip_init(&newClip,m_newData,new_begin,new_end+kPartPepeatSize);
        _search_cover(&newClip.base,new_begin,out_covers,pmem,
                      mt.newDataLocker.locker,mt.dataLocker.locker);
    }
#endif 
}

static inline void __search_cover_mt(TDigestMatcher* self,hpatch_TOutputCovers* out_covers,
                                     unsigned char* pmem,void* mt_data){
    self->_search_cover_thread(out_covers,pmem,mt_data);
}

void TDigestMatcher::search_cover(hpatch_TOutputCovers* out_covers){
    if (m_blocks.empty()) return;
    if (m_newData->streamSize<m_kMatchBlockSize) return;
#if (_IS_USED_MULTITHREAD)
    size_t threadNum=getSearchThreadNum();
    if (threadNum>1){
        const hpatch_StreamPos_t rollCount=m_newData->streamSize-(m_kMatchBlockSize-1);
        size_t bestStep=(kBestParallelSize/2>m_kMatchBlockSize)?kBestParallelSize:2*m_kMatchBlockSize;
        hpatch_StreamPos_t workCount=(rollCount+bestStep-1)/bestStep;
        workCount=(threadNum>workCount)?threadNum:workCount;
        mt_data_t mt_data;
        mt_data.workCount=(uint_work)workCount;
        if ((sizeof(mt_data.workCount)<sizeof(workCount))&&(mt_data.workCount!=workCount))
            throw std::runtime_error("TDigestMatcher::search_cover() multi-thread workCount error!");
        mt_data.workIndex=0;
        const size_t threadCount=threadNum-1;
        std::vector<std::thread> threads(threadCount);
        unsigned char* pmem=m_mem.data();
        for (size_t i=0;i<threadCount;i++,pmem+=(m_newCacheSize+m_oldCacheSize))
            threads[i]=std::thread(__search_cover_mt,this,out_covers,pmem,&mt_data);
        __search_cover_mt(this,out_covers,pmem,&mt_data);
        for (size_t i=0;i<threadCount;i++)
            threads[i].join();
        out_covers->collate_covers(out_covers);
    }else
#endif
    {
        _search_cover(m_newData,0,out_covers,m_mem.data());
    }
}

}//namespace hdiff_private
