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
#include <assert.h>
#include <stdexcept>  //std::runtime_error
#include <algorithm>  //std::sort,std::equal_range

static  const size_t kMinTrustMatchedLength=16*1024;
static  const size_t kBestReadSize=1024*256; //for sequence read
static  const size_t kMinReadSize=1024;      //for random read
static  const size_t kMinBackupReadSize=256; //assert(kMinBackupReadSize<kMinReadSize)

#define readStream(stream,pos,dst,n) { \
    if ((long)(n)!=(stream)->read((stream)->streamHandle,pos,dst,dst+(n))) \
        throw std::runtime_error("TDataDigest stream->read() error!"); }

struct TStreamCache{
    TStreamCache(const hpatch_TStreamInput* _stream,
                 unsigned char* _cache,size_t _cacheSize)
    :stream(_stream),cache(_cache),m_readPos(0),m_readPosEnd(0),
        cacheSize(_cacheSize),cachePos(_cacheSize){ }
    inline hpatch_StreamPos_t streamSize()const{ return stream->streamSize; }
    inline hpatch_StreamPos_t pos()const { return m_readPosEnd-dataLength(); }
    inline const size_t   dataLength()const{ return (size_t)(cacheSize-cachePos); }
    inline const unsigned char* data()const { return cache+cachePos; }
    inline bool resetPos(size_t kBackupCacheSize,hpatch_StreamPos_t streamPos,size_t kMinCacheDataSize){
        if ((streamPos+kMinCacheDataSize<=m_readPosEnd)&&(streamPos>=m_readPos+kBackupCacheSize)){
            cachePos=cacheSize-(size_t)(m_readPosEnd-streamPos);
            return true; //hit cache
        }
        return _resetPos_continue(kBackupCacheSize,streamPos,kMinCacheDataSize);
    }
protected:
    inline unsigned char* cachedData(){ return cache+cacheSize-(size_t)(m_readPosEnd-m_readPos); }
    bool _resetPos_continue(size_t kBackupCacheSize,hpatch_StreamPos_t streamPos,size_t kMinCacheDataSize){
        //stream:[...     |                |                   |                  |                      |      ...]
        //             readPos (streamPos-kBackupCacheSize) streamPos (streamPos+kMinCacheDataSize) (readPosEnd)
        //cache:     [    |                                    |                  |                      ]
        //           0                                   (init cachePos)                              cacheSize
        hpatch_StreamPos_t streamSize=stream->streamSize;
        if (streamPos+kMinCacheDataSize>streamSize) return false;
        hpatch_StreamPos_t readPos=(streamPos>=kBackupCacheSize)?(streamPos-kBackupCacheSize):0;
        size_t readLen=((streamSize-readPos)>=cacheSize)?cacheSize:(size_t)(streamSize-readPos);
        
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
        m_readPos=readPos;
        m_readPosEnd=readPos+readLen;
        cachePos=cacheSize-(size_t)(m_readPosEnd-streamPos);
        return true;
    }
private:
    const hpatch_TStreamInput* stream;
    unsigned char*             cache;
protected:
    hpatch_StreamPos_t         m_readPos;
    hpatch_StreamPos_t         m_readPosEnd;
    const size_t               cacheSize;
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


TDigestMatcher::TDigestMatcher(const hpatch_TStreamInput* oldData,size_t kMatchBlockSize)
:m_oldData(oldData),m_isUseLargeSorted(true),m_kMatchBlockSize(kMatchBlockSize),m_newCacheSize(0){
    if ((kMatchBlockSize==0)||(kMatchBlockSize>adler32_roll_kMaxBlockSize))
        throw std::runtime_error("TDataDigest() kMatchBlockSize value error.");
    while ((m_kMatchBlockSize>1)&&(m_kMatchBlockSize>=m_oldData->streamSize))
        m_kMatchBlockSize>>=1;
    hpatch_StreamPos_t _blockCount=getBlockCount(m_kMatchBlockSize,m_oldData->streamSize);
    if (m_oldData->streamSize<m_kMatchBlockSize) _blockCount=0;
    m_isUseLargeSorted=(_blockCount>=((hpatch_StreamPos_t)1<<32));
    const size_t blockCount=(size_t)_blockCount;
    if (blockCount!=_blockCount)
        throw std::runtime_error("TDataDigest() oldData->streamSize/MatchBlockSize too big error.");
    m_blocks.resize(blockCount);
    if (m_isUseLargeSorted)
        m_sorted_larger.resize(blockCount);
    else
        m_sorted_limit.resize(blockCount);
    
    const size_t kBackupSize=getBackupSize(m_kMatchBlockSize);
    m_newCacheSize=upperCount(m_kMatchBlockSize*2+kBackupSize,kBestReadSize)*kBestReadSize;
    size_t oldCacheSize=upperCount(m_kMatchBlockSize+kBackupSize,kMinReadSize)*kMinReadSize;
    m_buf.resize(m_newCacheSize+oldCacheSize);
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

void TDigestMatcher::getDigests(){
    if (m_blocks.empty()) return;
    
    const size_t blockCount=m_blocks.size();
    m_filter.init(blockCount);
    TStreamCache streamCache(m_oldData,m_buf.data(),m_buf.size());
    for (size_t i=0; i<blockCount; ++i) {
        hpatch_StreamPos_t readPos=blockIndexToPos(i,m_kMatchBlockSize,m_oldData->streamSize);
        streamCache.resetPos(0,readPos,m_kMatchBlockSize);
        adler_uint_t adler32=adler_roll_start(streamCache.data(),m_kMatchBlockSize);
        m_filter.insert(adler32);
        m_blocks[i]=adler32;
        if (m_isUseLargeSorted)
            m_sorted_larger[i]=i;
        else
            m_sorted_limit[i]=(uint32_t)i;
    }
    size_t kMaxCmpDeep=upperCount(kMinTrustMatchedLength,m_kMatchBlockSize);
    TIndex_comp comp(m_blocks.data(),m_blocks.size(),kMaxCmpDeep);
    if (m_isUseLargeSorted)
        std::sort(m_sorted_larger.begin(),m_sorted_larger.end(),comp);
    else
        std::sort(m_sorted_limit.begin(),m_sorted_limit.end(),comp);
}

struct TBlockStreamCache:public TStreamCache{
    TBlockStreamCache(const hpatch_TStreamInput* _stream,unsigned char* _cache,
                      size_t _cacheSize, size_t _kMatchBlockSize)
    :TStreamCache(_stream,_cache,_cacheSize),kMatchBlockSize(_kMatchBlockSize){
        assert(cacheSize>=2*kMatchBlockSize); }
    inline bool resetPos(hpatch_StreamPos_t streamPos){
        return TStreamCache::resetPos(getBackupSize(kMatchBlockSize),streamPos,kMatchBlockSize);
    }
    void toBestDataLength(){
        if (dataLength()*2>=cacheSize) return;
        bool result=_resetPos_continue(getBackupSize(kMatchBlockSize),pos(),kMatchBlockSize);
        assert(result);
    }
private:
    inline bool _roll_backward_cache(size_t skipLen,size_t needDataSize){
        return TStreamCache::resetPos(0,pos()+skipLen,needDataSize);
    }
public:
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
    hpatch_StreamPos_t roll_backward_equal_length(TBlockStreamCache& y){
        if (!_roll_backward_cache(kMatchBlockSize,1)) return 0;
        if (!y._roll_backward_cache(kMatchBlockSize,1)) return 0;
        hpatch_StreamPos_t eq_len=0;
        while(true){
            size_t len;
            if (_roll_backward_cache(0,kMatchBlockSize)&&y._roll_backward_cache(0,kMatchBlockSize)){
                len=kMatchBlockSize;
            }else{
                len=dataLength();
                size_t yLen=y.dataLength();
                len=(len<=yLen)?len:yLen;
                //assert(len<kMatchBlockSize);
            }
            const unsigned char* px=data();
            const unsigned char* py=y.data();
            for (size_t i=0;i<len;++i){
                if (px[i]!=py[i])
                    return eq_len+i;
            }
            eq_len+=len;
            if (!_roll_backward_cache(len,1)) break;
            if (!y._roll_backward_cache(len,1)) break;
        }
        return eq_len;
    }
public:
    const size_t               kMatchBlockSize;
};

struct TOldStreamCache:public TBlockStreamCache{
    TOldStreamCache(const hpatch_TStreamInput* _stream,unsigned char* _cache,
                    size_t _cacheSize,size_t _kMatchBlockSize)
    :TBlockStreamCache(_stream,_cache,_cacheSize,_kMatchBlockSize){ }
};

struct TNewStreamCache:public TBlockStreamCache{
    TNewStreamCache(const hpatch_TStreamInput* _stream,unsigned char* _cache,
                    size_t _cacheSize,size_t _kMatchBlockSize)
    :TBlockStreamCache(_stream,_cache,_cacheSize,_kMatchBlockSize),
    kBlockSizeBM(adler_roll_kBlockSizeBM((adler_uint_t)_kMatchBlockSize)){
        resetPos(0);
    }
    
    inline bool resetPos(hpatch_StreamPos_t oldPos){
        if (!TBlockStreamCache::resetPos(oldPos)) return false;
        roll_digest=adler_roll_start(data(),kMatchBlockSize);
        return true;
    }
    inline bool roll(){
        //warning: after running _roll_backward_cache(),cache roll logic is failure
        if (dataLength()>kMatchBlockSize){
            const unsigned char* cur_datas=data();
            roll_digest=adler_roll_step(roll_digest,(adler_uint_t)kMatchBlockSize,kBlockSizeBM,
                                        cur_datas[0],cur_datas[kMatchBlockSize]);
            ++cachePos;
            return true;
        }else{
            if (!TBlockStreamCache::resetPos(pos()+1)) return false;
            --cachePos;
            return roll();
        }
    }
    bool skip_same(unsigned char same){
        if (!resetPos(pos()+kMatchBlockSize)) return false;
        while(data()[0]==same){
            if (!roll()) return false;
        }
        return true;
    }
    inline adler_uint_t rollDigest()const{ return roll_digest; }
private:
    adler_uint_t               kBlockSizeBM;
    adler_uint_t               roll_digest;
};


struct TDigest_comp{
    inline explicit TDigest_comp(const adler_uint_t* _blocks):blocks(_blocks){ }
    struct TDigest{
        adler_uint_t value;
        inline explicit TDigest(adler_uint_t _value):value(_value){}
    };
    template<class TIndex>
    inline bool operator()(const TIndex& x,const TDigest& y)const { return blocks[x]<y.value; }
    template<class TIndex>
    inline bool operator()(const TDigest& x,const TIndex& y)const { return x.value<blocks[y]; }
protected:
    const adler_uint_t* blocks;
};

struct TDigest_comp_i:public TDigest_comp{
    inline TDigest_comp_i(const adler_uint_t* _blocks,size_t _n):TDigest_comp(_blocks),n(_n),i(0){ }
    template<class TIndex>
    inline bool operator()(const TIndex& x,const TDigest& y)const {
        return (x+i<n)?(blocks[x+i]<y.value):true; }
    template<class TIndex>
    inline bool operator()(const TDigest& x,const TIndex& y)const {
        return (y+i<n)?(x.value<blocks[y+i]):false; }
private:
    const size_t        n;
public:
    size_t              i;
};

template <class TIndex>
static bool getBestMatch(const adler_uint_t* blocksBase,size_t blocksSize,
                         const TIndex* left,const TIndex* right,
                         TOldStreamCache& oldStream,TNewStreamCache& newStream,
                         const TCover& lastCover,TCover* out_curCover){
    const size_t kMatchBlockSize=newStream.kMatchBlockSize;
    TDigest_comp_i comp_i(blocksBase,blocksSize);
    const size_t max_bdigests_n=upperCount(kMinTrustMatchedLength,kMatchBlockSize);
    
    const TIndex* best=left;
    size_t bdigests_n=0;
    if (right-left>1){
        //缩小[left best right)范围,留下最多2个;
        newStream.toBestDataLength();
        size_t bmaxn=newStream.dataLength()/kMatchBlockSize-1;
        if (bmaxn>max_bdigests_n) bmaxn=max_bdigests_n;
        const unsigned char* bdata=newStream.data()+kMatchBlockSize;
        for (; (bdigests_n<bmaxn)&&(right-left>1);++bdigests_n,bdata+=kMatchBlockSize){
            adler_uint_t digest=adler_roll_start(bdata,kMatchBlockSize);
            comp_i.i=bdigests_n+1;
            std::pair<const TIndex*,const TIndex*>
            i_range=std::equal_range(left,right,TDigest_comp::TDigest(digest),comp_i);
            size_t rn=i_range.second-i_range.first;
            if (rn==0){
                break;
            }else if (rn==1){
                best=i_range.first;
                break;
            }else{
                best=left;
                left=i_range.first;
                right=i_range.second;
            }
        }
    }
    if (best>left)
        right=0;
    else
        left=0;

    const hpatch_StreamPos_t newPos=newStream.pos();
    bool isMatched=false;
    hpatch_StreamPos_t  bestLen=0;
    for (const TIndex* cur_pi=best; cur_pi!=0; ) {
        const hpatch_StreamPos_t oldPos=blockIndexToPos(*cur_pi,kMatchBlockSize,
                                                        oldStream.streamSize());
        if ((oldStream.resetPos(oldPos))&&(0==memcmp(oldStream.data(),newStream.data(),
                                                     kMatchBlockSize))){
            size_t feq_len=oldStream.forward_equal_length(newStream);
            if (newPos-feq_len<lastCover.newPos+lastCover.length)
                feq_len=(size_t)(newPos-(lastCover.newPos+lastCover.length));
            hpatch_StreamPos_t beq_len=newStream.roll_backward_equal_length(oldStream);
            hpatch_StreamPos_t curEqLen=feq_len+kMatchBlockSize+beq_len;
            if (curEqLen>bestLen){
                isMatched=true;
                bestLen=curEqLen;
                out_curCover->length=curEqLen;
                out_curCover->oldPos=oldPos-feq_len;
                out_curCover->newPos=newPos-feq_len;
                if (curEqLen>=bdigests_n*kMatchBlockSize)
                    break;//matched best
            }
        }
        //next cur_pi
        if (left!=0){
            cur_pi=best-1;
            left=0;
        }else if (best+1<right){
            cur_pi=best+1;
            right=0;
        }else{
            cur_pi=0;
        }
        if (cur_pi)
            newStream.TBlockStreamCache::resetPos(newPos);
    }
    return isMatched;
}


static bool is_same_data(const unsigned char* s,size_t n){
    if (n==0) return true;
    unsigned char d=s[0];
    for (size_t i=1; i<n; ++i) {
        if(s[i]!=d)
            return false;
    }
    return true;
}

template <class TIndex>
static void tm_search_cover(const adler_uint_t* blocksBase,size_t blocksSize,
                            const TIndex* iblocks,const TIndex* iblocks_end,
                            TOldStreamCache& oldStream,TNewStreamCache& newStream,
                            const TBloomFilter& filter,TCovers* out_covers) {
    const size_t kMatchBlockSize=newStream.kMatchBlockSize;
    TDigest_comp comp(blocksBase);
    TCover  lastCover={0,0,0};
    //hpatch_StreamPos_t sumLen=0;
    while (true) {
        if (filter.is_hit(newStream.rollDigest())){
            std::pair<const TIndex*,const TIndex*>
            range=std::equal_range(iblocks,iblocks_end,
                                   TDigest_comp::TDigest(newStream.rollDigest()),comp);
            if (range.first!=range.second){
                const unsigned char* pnew=newStream.data();
                if (is_same_data(pnew,kMatchBlockSize)){
                    if (!newStream.skip_same(pnew[0])) break;//finish
                    continue;
                }
                TCover  curCover;
                if (getBestMatch(blocksBase,blocksSize,range.first,range.second,
                                 oldStream,newStream,lastCover,&curCover)){
                    //todo: + link cover
                    out_covers->addCover(curCover);
                    //sumLen+=curCover.length;
                    lastCover=curCover;
                    if (!newStream.resetPos(curCover.newPos+curCover.length)) break;//finish
                    continue;
                }//else roll
            }//else roll
        }
        if (!newStream.roll()) break;//finish
    }
    //printf("%zu /%lld\n",out_covers->coverCount(),upperCount(newStream.streamSize(),kMatchBlockSize));
    //printf("%lld / %lld\n",sumLen,newStream.streamSize());
}

void TDigestMatcher::search_cover(const hpatch_TStreamInput* newData,TCovers* out_covers){
    if (m_blocks.empty()) return;
    if (newData->streamSize<m_kMatchBlockSize) return;
    TNewStreamCache newStream(newData,&m_buf[0],m_newCacheSize,m_kMatchBlockSize);
    TOldStreamCache oldStream(m_oldData,&m_buf[m_newCacheSize],m_buf.size()-m_newCacheSize,m_kMatchBlockSize);
    if (m_isUseLargeSorted)
        tm_search_cover(&m_blocks[0],m_blocks.size(),&m_sorted_larger[0],&m_sorted_larger[0]+m_blocks.size(),
                        oldStream,newStream,m_filter,out_covers);
    else
        tm_search_cover(&m_blocks[0],m_blocks.size(),&m_sorted_limit[0],&m_sorted_limit[0]+m_blocks.size(),
                        oldStream,newStream,m_filter,out_covers);
}


