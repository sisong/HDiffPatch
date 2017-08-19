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


template <class T,class Tn>
inline static T upperCount(T size,Tn nsize){
    return (size+nsize-1)/nsize;
}

TDigestMatcher::TDigestMatcher(const hpatch_TStreamInput* oldData,
                               size_t kMatchBlockSize,bool isSearchBestMatch)
:m_oldData(oldData),m_isUseLargeSorted(true),m_isSearchBestMatch(isSearchBestMatch),
m_kMatchBlockSize(kMatchBlockSize),m_newCacheSize(0){
    if ((kMatchBlockSize==0)||(kMatchBlockSize>adler32_roll_kMaxBlockSize))
        throw std::runtime_error("TDataDigest() kMatchBlockSize value error.");
    while ((m_kMatchBlockSize>1)&&(m_kMatchBlockSize>=m_oldData->streamSize))
        m_kMatchBlockSize>>=1;
    hpatch_StreamPos_t _blockCount=upperCount(m_oldData->streamSize,m_kMatchBlockSize);
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
    
    const size_t kBestReadSize=1024*256;
    m_newCacheSize=upperCount(m_kMatchBlockSize*3,kBestReadSize)*kBestReadSize; //sequence read
    const size_t kMinReadSize=1024*4;
    size_t oldCacheSize=upperCount(m_kMatchBlockSize*2,kMinReadSize)*kMinReadSize; //random read
    m_buf.resize(m_newCacheSize+oldCacheSize);
    getDigests();
}

#define readStream(stream,pos,dst,n) { \
    if ((long)(n)!=(stream)->read((stream)->streamHandle,pos,dst,dst+(n))) \
        throw std::runtime_error("TDataDigest stream->read() error!"); \
}

struct TIndex_comp{
    inline TIndex_comp(const adler_uint_t* _blocks,size_t _n):blocks(_blocks),n(_n){ }
    template<class TIndex>
    inline bool operator()(TIndex x,TIndex y)const {
        adler_uint_t xd=blocks[x];
        adler_uint_t yd=blocks[y];
        if (xd!=yd)
            return xd<yd;
        else
            return _cmp(x+1,y+1,n,blocks);
    }
private:
    const adler_uint_t* blocks;
    const size_t        n;
    
    template<class TIndex>
    static bool _cmp(TIndex x,TIndex y,size_t n,const adler_uint_t* blocks) {
        if (x!=y) {} else{ return false; }
        //const size_t kMaxDeep=128;
        //for (size_t i=0;(x<n)&(y<n)&(i<kMaxDeep);++x,++y,++i){
        for (;(x<n)&(y<n);++x,++y){
            adler_uint_t xd=blocks[x];
            adler_uint_t yd=blocks[y];
            if (xd!=yd)
                return xd<yd;
        }
        return x<y;
    }
};

void TDigestMatcher::getDigests(){
    if (m_blocks.empty()) return;
    
    const size_t blockCount=m_blocks.size();
    m_filter.init(blockCount);
    unsigned char* buf=m_buf.data();
    for (size_t i=0; i<blockCount; ++i) {
        hpatch_StreamPos_t readPos=(hpatch_StreamPos_t)i*m_kMatchBlockSize;
        if (i==blockCount-1)
            readPos=m_oldData->streamSize-m_kMatchBlockSize;
        readStream(m_oldData,readPos,buf,m_kMatchBlockSize);
        adler_uint_t adler32=adler_roll_start(buf,m_kMatchBlockSize);
        m_filter.insert(adler32);
        m_blocks[i]=adler32;
        if (m_isUseLargeSorted)
            m_sorted_larger[i]=i;
        else
            m_sorted_limit[i]=(uint32_t)i;
    }
    TIndex_comp comp(m_blocks.data(),m_blocks.size());
    if (m_isUseLargeSorted)
        std::sort(m_sorted_larger.begin(),m_sorted_larger.end(),comp);
    else
        std::sort(m_sorted_limit.begin(),m_sorted_limit.end(),comp);
}

struct TStreamCache{
    TStreamCache(const hpatch_TStreamInput* _stream,size_t _kMatchBlockSize,
               unsigned char* _cache,size_t _cacheSize)
    :kMatchBlockSize(_kMatchBlockSize),stream(_stream),m_readPos(0),m_readPosEnd(0),
    cache(_cache),cacheSize(_cacheSize),cachePos(_cacheSize){
        assert(cacheSize>=2*kMatchBlockSize);
    }
    inline hpatch_StreamPos_t streamSize()const{ return stream->streamSize; }
    
    inline hpatch_StreamPos_t pos()const {
        return m_readPosEnd-dataLength();
    }
    inline bool resetPos(hpatch_StreamPos_t streamPos){
        return _resetPos(kMatchBlockSize,streamPos,kMatchBlockSize);
    }
    inline const size_t   dataLength()const{ return (size_t)(cacheSize-cachePos); }
    inline const unsigned char* data()const { return cache+cachePos; }
private:
    inline unsigned char* cachedData(){ return cache+cacheSize-(size_t)(m_readPosEnd-m_readPos); }
    inline bool _resetPos(size_t kBackupCacheSize,hpatch_StreamPos_t streamPos,size_t kMinCacheDataSize){
        //stream:[      |            |                  |                      |        ]
        //           readPos     streamPos  (streamPos+kMinCacheDataSize)  (readPosEnd)
        //cache:   [    |            |                  |                      ]
        //         0           (init cachePos)                             cacheSize
        if ((streamPos+kMinCacheDataSize<=m_readPosEnd)&&(streamPos>=m_readPos+kBackupCacheSize)){
            cachePos=cacheSize-(size_t)(m_readPosEnd-streamPos);
            return true; //hit cache
        }
        return _resetPos_continue(kBackupCacheSize,streamPos,kMinCacheDataSize);
    }
    bool _resetPos_continue(size_t kBackupCacheSize,hpatch_StreamPos_t streamPos,size_t kMinCacheDataSize){
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
    inline bool _roll_backward_cache(size_t skipLen,size_t needDataSize){
        return _resetPos(0,pos()+skipLen,needDataSize);
    }
public:
    size_t forward_equal_length(const TStreamCache& y)const{
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
    hpatch_StreamPos_t roll_backward_equal_length(TStreamCache& y,
                                                  std::vector<adler_uint_t>* xBackwardDigests){
        if (!_roll_backward_cache(kMatchBlockSize,1)) return 0;
        if (!y._roll_backward_cache(kMatchBlockSize,1)) return 0;
        hpatch_StreamPos_t eq_len=0;
        size_t xDigest_index=0;
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
            if ((xBackwardDigests)&&(len==kMatchBlockSize)){
                if (xDigest_index==xBackwardDigests->size())
                    xBackwardDigests->push_back(adler_roll_start(px,kMatchBlockSize));
                ++xDigest_index;
            }
            if (!_roll_backward_cache(len,1)) break;
            if (!y._roll_backward_cache(len,1)) break;
        }
        return eq_len;
    }
    
public:
    const size_t               kMatchBlockSize;
private:
    const hpatch_TStreamInput* stream;
    hpatch_StreamPos_t         m_readPos;
    hpatch_StreamPos_t         m_readPosEnd;
    unsigned char*  cache;
    const size_t    cacheSize;
protected:
    size_t          cachePos;
};

struct TOldStreamCache:public TStreamCache{
    TOldStreamCache(const hpatch_TStreamInput* _stream,size_t _kMatchBlockSize,
                    unsigned char* _cache,size_t _cacheSize)
    :TStreamCache(_stream,_kMatchBlockSize,_cache,_cacheSize){ }
};

struct TNewStreamCache:public TStreamCache{
    TNewStreamCache(const hpatch_TStreamInput* _stream,size_t _kMatchBlockSize,
               unsigned char* _cache,size_t _cacheSize)
    :TStreamCache(_stream,_kMatchBlockSize,_cache,_cacheSize),
    kBlockSizeBM(adler_roll_kBlockSizeBM((adler_uint_t)_kMatchBlockSize)){
        resetPos(0);
    }
    
    inline bool resetPos(hpatch_StreamPos_t oldPos){
        if (!TStreamCache::resetPos(oldPos)) return false;
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
            if (!TStreamCache::resetPos(pos()+1)) return false;
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

template <class TIndex>
static bool getBestMatch(const adler_uint_t* blocksBase,size_t blocksSize,
                         const TIndex* iblocks,const TIndex* iblocks_end,
                        TOldStreamCache& oldStream,TNewStreamCache& newStream,
                         const TCover& lastCover,TCover* out_curCover,
                         std::vector<adler_uint_t>* newBackwardDigests){
    const size_t kMinTrustLength=64*1024;
    const size_t kMatchBlockSize=newStream.kMatchBlockSize;
    const hpatch_StreamPos_t newPos=newStream.pos();
    bool isMatched=false;
    TCover  bestCover={0,0,0};
    if (newBackwardDigests) newBackwardDigests->clear();
    
    size_t n=iblocks_end-iblocks;
    //*
    for (size_t i=0; i<n; ++i) {
        const TIndex* cur_pi=iblocks+(n>>1)+(i>>1)*((i&1)*2-1);
        assert((cur_pi>=iblocks)&&(cur_pi<iblocks_end));//*/
    //for (const TIndex* cur_pi=iblocks; cur_pi!=iblocks_end; ++cur_pi) {
        size_t cur_ibk=(*cur_pi);
        bool is_digest_matched=true;
        if (newBackwardDigests){
            for (size_t i=0; i<newBackwardDigests->size(); ++i) {
                size_t cur_ibk_next=cur_ibk+1+i;
                if ((cur_ibk_next>=blocksSize)||((*newBackwardDigests)[i]!=blocksBase[cur_ibk_next])) {
                    is_digest_matched=false;
                    break;
                }
            }
            if (!is_digest_matched) continue;
        }
        const hpatch_StreamPos_t oldPos=(cur_ibk+1==blocksSize)?(oldStream.streamSize()-kMatchBlockSize):
                                            (hpatch_StreamPos_t)cur_ibk*kMatchBlockSize;
        if (!oldStream.resetPos(oldPos)) continue;
        if (0!=memcmp(oldStream.data(),newStream.data(),kMatchBlockSize)) continue;
        isMatched=true;
        size_t feq_len=oldStream.forward_equal_length(newStream);
        if (newPos-feq_len<lastCover.newPos+lastCover.length)
            feq_len=(size_t)(newPos-(lastCover.newPos+lastCover.length));
        hpatch_StreamPos_t beq_len=newStream.roll_backward_equal_length(oldStream,newBackwardDigests);
        if (i+1!=n)//(cur_pi+1!=iblocks_end)//
            newStream.TStreamCache::resetPos(newPos);
        hpatch_StreamPos_t curEqLen=feq_len+kMatchBlockSize+beq_len;
        if (curEqLen>bestCover.length){
            bestCover.length=curEqLen;
            bestCover.oldPos=oldPos-feq_len;
            bestCover.newPos=newPos-feq_len;
            if (!newBackwardDigests) break;
            if (curEqLen>=kMinTrustLength) break;
        }
    }
    *out_curCover=bestCover;
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



struct TDigest_comp{
    inline TDigest_comp(const adler_uint_t* _blocks,size_t _n):blocks(_blocks),n(_n){ }
    struct TDigest{
        adler_uint_t value;
        inline explicit TDigest(adler_uint_t _value):value(_value){}
    };
    template<class TIndex>
    inline bool operator()(const TIndex& x,const TDigest& y)const { return blocks[x]<y.value; }
    template<class TIndex>
    inline bool operator()(const TDigest& x,const TIndex& y)const { return x.value<blocks[y]; }
private:
    const adler_uint_t* blocks;
    const size_t        n;
};


template <class TIndex>
static void tm_search_cover(const adler_uint_t* blocksBase,size_t blocksSize,
                            const TIndex* iblocks,const TIndex* iblocks_end,
                            TOldStreamCache& oldStream,TNewStreamCache& newStream,
                            const TBloomFilter& filter,size_t kMatchBlockSize,
                            TCovers* out_covers,bool isSearchBestMatch) {
    TDigest_comp comp(blocksBase,blocksSize);
    TCover  lastCover={0,0,0};
    //hpatch_StreamPos_t sumLen=0;
    std::vector<adler_uint_t> bestMatchDigests;
    std::vector<adler_uint_t>* pbestMatchDigests=isSearchBestMatch?&bestMatchDigests:0;
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
                                 oldStream,newStream,lastCover,&curCover,pbestMatchDigests)){
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
    TNewStreamCache newStream(newData,m_kMatchBlockSize,&m_buf[0],m_newCacheSize);
    TOldStreamCache oldStream(m_oldData,m_kMatchBlockSize,&m_buf[m_newCacheSize],m_buf.size()-m_newCacheSize);
    if (m_isUseLargeSorted)
        tm_search_cover(&m_blocks[0],m_blocks.size(),&m_sorted_larger[0],&m_sorted_larger[0]+m_blocks.size(),
                        oldStream,newStream,m_filter,m_kMatchBlockSize,out_covers,m_isSearchBestMatch);
    else
        tm_search_cover(&m_blocks[0],m_blocks.size(),&m_sorted_limit[0],&m_sorted_limit[0]+m_blocks.size(),
                        oldStream,newStream,m_filter,m_kMatchBlockSize,out_covers,m_isSearchBestMatch);
}


