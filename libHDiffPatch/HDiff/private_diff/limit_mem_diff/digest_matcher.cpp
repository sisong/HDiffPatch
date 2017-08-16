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

TDigestMatcher::TDigestMatcher(const hpatch_TStreamInput* oldData,size_t kMatchBlockSize)
:m_oldData(oldData),m_isUseLargeBlocks(true),
m_kMatchBlockSize(kMatchBlockSize),m_newCacheSize(0){
    if ((kMatchBlockSize==0)||(kMatchBlockSize>adler32_roll_kMaxBlockSize))
        throw std::runtime_error("TDataDigest() kMatchBlockSize value error.");
    while ((m_kMatchBlockSize>1)&&(m_kMatchBlockSize>=m_oldData->streamSize))
        m_kMatchBlockSize>>=1;
    const hpatch_StreamPos_t _blockCount=upperCount(m_oldData->streamSize,m_kMatchBlockSize);
    m_isUseLargeBlocks=(_blockCount>=((uint64_t)1<<32));
    const size_t blockCount=(size_t)_blockCount;
    if (blockCount!=_blockCount)
        throw std::runtime_error("TDataDigest() oldData->streamSize/MatchBlockSize too big error.");
    if (m_isUseLargeBlocks)
        m_blocks_lager.resize(blockCount);
    else
        m_blocks_limit.resize(blockCount);
    
    const size_t kBestReadSize=1024*256;
    m_newCacheSize=upperCount(m_kMatchBlockSize*3,kBestReadSize)*kBestReadSize; //sequence read
    const size_t kMinReadSize=1024*4;
    size_t oldCacheSize=upperCount(m_kMatchBlockSize*2,kMinReadSize)*kMinReadSize; //random read
    m_buf.resize(m_newCacheSize+oldCacheSize);
    getDigests();
}

#define readStream(stream,pos,dst,n) { \
    if (n!=(stream)->read((stream)->streamHandle,pos,dst,dst+n)) \
    throw std::runtime_error("TDataDigest stream->read() error!"); \
}


void TDigestMatcher::getDigests(){
    const size_t blockCount=m_isUseLargeBlocks?m_blocks_lager.size():m_blocks_limit.size();
    m_filter.init(blockCount);
    unsigned char* buf=&m_buf[0];
    for (size_t i=0; i<blockCount; ++i) {
        hpatch_StreamPos_t readPos=(hpatch_StreamPos_t)i*m_kMatchBlockSize;
        if (i==blockCount-1)
            readPos=m_oldData->streamSize-m_kMatchBlockSize;
        readStream(m_oldData,readPos,buf,m_kMatchBlockSize);
        adler_uint_t adler32=adler_roll_start(buf,m_kMatchBlockSize);
        m_filter.insert(adler32);
        if (m_isUseLargeBlocks)
            m_blocks_lager[i]=TDigest<uint64_t>(adler32,i);
        else
            m_blocks_limit[i]=TDigest<adler_uint_t>(adler32,(adler_uint_t)i);
    }
    if (m_isUseLargeBlocks)
        std::sort(m_blocks_lager.begin(),m_blocks_lager.end());
    else
        std::sort(m_blocks_limit.begin(),m_blocks_limit.end());
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
        return m_readPosEnd-(cacheSize-cachePos);
    }
    inline unsigned char* datas(){ return cache+cachePos; }
    inline const unsigned char* datas()const { return cache+cachePos; }
    inline const size_t   dataLength()const{ return (size_t)(cacheSize-cachePos); }
    inline unsigned char* cachedDatas(){ return cache+cacheSize-cachedLength(); }
    inline const size_t   cachedLength()const{ return (size_t)(m_readPosEnd-m_readPos); }
    inline bool resetPos(hpatch_StreamPos_t oldPos){
        return _resetPos(kMatchBlockSize,oldPos,kMatchBlockSize);
    }
private:
    inline bool _resetPos(size_t kBackupCacheSize,hpatch_StreamPos_t oldPos,size_t kMinCacheDataSize){
        //stream:[      |            |                  |                      |        ]
        //           readPos       oldPos    (oldPos+kMinCacheDataSize)  (readPosEnd)
        //cache:   [    |            |                  |                      ]
        //         0           (init cachePos)                             cacheSize
        if ((oldPos+kMinCacheDataSize<=m_readPosEnd)&&(oldPos>=m_readPos)){
            cachePos=cacheSize-(size_t)(m_readPosEnd-oldPos);
            return true; //hit cache
        }
        return _resetPos_continue(kBackupCacheSize,oldPos,kMinCacheDataSize);
    }
    bool _resetPos_continue(size_t kBackupCacheSize,hpatch_StreamPos_t oldPos,size_t kMinCacheDataSize){
        hpatch_StreamPos_t streamSize=stream->streamSize;
        if (oldPos+kMinCacheDataSize>streamSize) return false;
        hpatch_StreamPos_t readPos=(oldPos>=kBackupCacheSize)?(oldPos-kBackupCacheSize):0;
        size_t readLen=((streamSize-readPos)>=cacheSize)?cacheSize:(size_t)(streamSize-readPos);

        unsigned char* dst=cache+cacheSize-readLen;
        if ((m_readPosEnd>readPos)&&(m_readPos<=readPos)){
            size_t moveLen=(size_t)(m_readPosEnd-readPos);
            memmove(dst,cache+cacheSize-moveLen,moveLen);
            readStream(stream,readPos+moveLen,dst+moveLen,readLen-moveLen);
        }else if ((m_readPos<readPos+readLen)&&(m_readPosEnd>=readPos+readLen)){
            size_t moveLen=(size_t)(readPos+readLen-m_readPos);
            memmove(dst+readLen-moveLen,cachedDatas(),moveLen);
            readStream(stream,readPos,dst,readLen-moveLen);
        }else{
            readStream(stream,readPos,dst,readLen);
        }
        m_readPos=readPos;
        m_readPosEnd=readPos+readLen;
        cachePos=cacheSize-(size_t)(m_readPosEnd-oldPos);
        return true;
    }
    inline bool _roll_backward_cache(size_t skipLen){
        return _resetPos(0,pos()+skipLen,1);
    }
public:
    size_t forward_equal_length(const TStreamCache& y)const{
        size_t y_len=(size_t)(y.pos()-y.m_readPos);
        size_t len=(size_t)(pos()-m_readPos);
        len=(len<y_len)?len:y_len;
        const unsigned char* px=datas();
        const unsigned char* py=y.datas();
        size_t i=1;
        for (;i<=len;++i){
            if (px[-i]!=py[-i]) break;
        }
        return i-1;
    }
    hpatch_StreamPos_t roll_backward_equal_length(TStreamCache& y){
        if (!_roll_backward_cache(kMatchBlockSize)) return 0;
        if (!y._roll_backward_cache(kMatchBlockSize)) return 0;
        hpatch_StreamPos_t eq_len=0;
        while(true){
            size_t yLen=y.dataLength();
            size_t len=dataLength();
            len=(len<=yLen)?len:yLen;
            const unsigned char* px=datas();
            const unsigned char* py=y.datas();
            for (size_t i=0;i<len;++i){
                if (px[i]==py[i])
                    ++eq_len;
                else
                    return eq_len;
            }
            if (!_roll_backward_cache(len)) break;
            if (!y._roll_backward_cache(len)) break;
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
    :TStreamCache(_stream,_kMatchBlockSize,_cache,_cacheSize){
    }
    
    
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
        roll_digest=adler_roll_start(datas(),kMatchBlockSize);
        return true;
    }
    inline bool roll(){
        //warning: after running _roll_backward_cache(),cache roll logic is failure
        if (dataLength()>kMatchBlockSize){
            const unsigned char* cur_datas=datas();
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
        while(datas()[0]==same){
            if (!roll()) return false;
        }
        return true;
    }
    inline adler_uint_t rollDigest()const{ return roll_digest; }
private:
    adler_uint_t               kBlockSizeBM;
    adler_uint_t               roll_digest;
};

template <class tm_TDigest>
static bool getBestMatch(const tm_TDigest* pblocks,const tm_TDigest* pblocks_end,
                        TOldStreamCache& oldStream,TNewStreamCache& newStream,
                         const TCover& lastCover,TCover* out_curCover){
    
    const size_t kMatchBlockSize=newStream.kMatchBlockSize;
    bool isMatched=false;
    for (const tm_TDigest* curBlock=pblocks; curBlock!=pblocks_end; ++curBlock) {
        hpatch_StreamPos_t oldPos=(hpatch_StreamPos_t)curBlock->block_index*kMatchBlockSize;
        if (!oldStream.resetPos(oldPos)) continue;
        if (0==memcmp(oldStream.datas(),newStream.datas(),kMatchBlockSize)){
            isMatched=true;
            hpatch_StreamPos_t newPos=newStream.pos();
            size_t feq_len=oldStream.forward_equal_length(newStream);
            if (newPos-feq_len<lastCover.newPos+lastCover.length)
                feq_len=(size_t)(newPos-(lastCover.newPos+lastCover.length));
            hpatch_StreamPos_t beq_len=oldStream.roll_backward_equal_length(newStream);
            out_curCover->oldPos=oldPos-feq_len;
            out_curCover->newPos=newPos-feq_len;
            out_curCover->length=feq_len+kMatchBlockSize+beq_len;
            break; //todo: continue get max coverLength?
            //如果pblocks_end-pblocks较长(>6)，在sort阶段可以按后续校验值排序，这里就可以用二分查找？
        }
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



template <class tm_TDigest>
static void tm_search_cover(const tm_TDigest* pblocks,const tm_TDigest* pblocks_end,
                            TOldStreamCache& oldStream,TNewStreamCache& newStream,
                            const TBloomFilter& filter,size_t kMatchBlockSize,TCovers* out_covers) {
    TCover  lastCover={0,0,0};
    uint64_t sumlen=0;
    while (true) {
        if (filter.is_hit(newStream.rollDigest())){
            std::pair<const tm_TDigest*,const tm_TDigest*>
            range=std::equal_range(pblocks,pblocks_end,newStream.rollDigest(),typename tm_TDigest::T_comp());
            if (range.first!=range.second){
                const unsigned char* pnew=newStream.datas();
                if (is_same_data(pnew,kMatchBlockSize)){
                    if (!newStream.skip_same(pnew[0])) break;//finish
                    continue;
                }
                TCover  curCover;
                if (getBestMatch(range.first,range.second,oldStream,newStream,lastCover,&curCover)){
                    out_covers->addCover(curCover);
                    sumlen+=curCover.length;
                    lastCover=curCover;
                    if (!newStream.resetPos(curCover.newPos+curCover.length)) break;//finish
                    continue;
                }//else roll
            }//else roll
        }
        if (!newStream.roll()) break;//finish
    }
    printf("%zu/ %lld\n",out_covers->coverCount(),upperCount(newStream.streamSize(),kMatchBlockSize));
    printf("%lld / %lld\n",sumlen,newStream.streamSize());
}

void TDigestMatcher::search_cover(const hpatch_TStreamInput* newData,TCovers* out_covers){
    if (m_blocks_lager.empty()&&m_blocks_limit.empty()) return;
    if (newData->streamSize<m_kMatchBlockSize) return;
    TNewStreamCache newStream(newData,m_kMatchBlockSize,&m_buf[0],m_newCacheSize);
    TOldStreamCache oldStream(m_oldData,m_kMatchBlockSize,&m_buf[m_newCacheSize],m_buf.size()-m_newCacheSize);
    if (m_isUseLargeBlocks)
        tm_search_cover(&m_blocks_lager[0],&m_blocks_lager[0]+m_blocks_lager.size(),
                        oldStream,newStream,m_filter,m_kMatchBlockSize,out_covers);
    else
        tm_search_cover(&m_blocks_limit[0],&m_blocks_limit[0]+m_blocks_limit.size(),
                        oldStream,newStream,m_filter,m_kMatchBlockSize,out_covers);
}


