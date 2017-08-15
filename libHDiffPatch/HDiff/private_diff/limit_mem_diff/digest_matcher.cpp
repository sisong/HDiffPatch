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

TDigestMatcher::TDigestMatcher(const hpatch_TStreamInput* oldData,size_t kMatchBlockSize)
:m_oldData(oldData),m_kMatchBlockSize(kMatchBlockSize),m_newCacheSize(0){
    if ((kMatchBlockSize==0)||(kMatchBlockSize>adler32_roll_kMaxBlockSize))
        throw std::runtime_error("TDataDigest() kMatchBlockSize value error.");
    
    while ((m_kMatchBlockSize>1)&&(m_kMatchBlockSize>=m_oldData->streamSize)) {
        m_kMatchBlockSize>>=1;
    }
    const hpatch_StreamPos_t _blockCount=(m_oldData->streamSize+m_kMatchBlockSize-1)/m_kMatchBlockSize;
    const size_t blockCount=(size_t)_blockCount;
    if (blockCount!=_blockCount)
        throw std::runtime_error("TDataDigest() oldData->streamSize/MatchBlockSize too big error.");
    m_blocks.resize(blockCount);
    
    const size_t kBestReadSize=1024*256;
    m_newCacheSize=(m_kMatchBlockSize*3+kBestReadSize-1)/kBestReadSize*kBestReadSize; //sequence read
    const size_t kMinReadSize=1024*4;
    size_t oldCacheSize=(m_kMatchBlockSize*2+kMinReadSize-1)/kMinReadSize*kMinReadSize; //random read
    m_buf.resize(m_newCacheSize+oldCacheSize);
    getDigests();
}

#define readStream(stream,pos,dst,n) { \
    if (n!=(stream)->read((stream)->streamHandle,pos,dst,dst+n)) \
    throw std::runtime_error("TDataDigest stream->read() error!"); \
}


void TDigestMatcher::getDigests(){
    const size_t blockCount=m_blocks.size();
    m_filter.init(blockCount);
    unsigned char* buf=&m_buf[0];
    for (size_t i=0; i<blockCount; ++i) {
        hpatch_StreamPos_t readPos=(hpatch_StreamPos_t)i*m_kMatchBlockSize;
        if (i==blockCount-1)
            readPos=m_oldData->streamSize-m_kMatchBlockSize;
        readStream(m_oldData,readPos,buf,m_kMatchBlockSize);
        adler_uint_t adler32=adler_roll_start(buf,m_kMatchBlockSize);
        m_filter.insert(adler32);
        m_blocks[i]=TDigest(adler32,i);
    }
    std::sort(m_blocks.begin(),m_blocks.end());
}

struct TStreamCache{
    TStreamCache(const hpatch_TStreamInput* _stream,size_t _kMatchBlockSize,
               unsigned char* _cache,size_t _cacheSize)
    :kMatchBlockSize(_kMatchBlockSize),stream(_stream),m_readPos(0),m_readPosEnd(0),
    cache(_cache),cacheSize(_cacheSize),cachePos(_cacheSize){
        assert(cacheSize>=2*kMatchBlockSize);
    }
    
    inline hpatch_StreamPos_t pos()const {
        return m_readPosEnd-(cacheSize-cachePos);
    }
    inline unsigned char* datas(){ return cache+cachePos; }
    inline const size_t   dataLength()const{ return (size_t)(cacheSize-cachePos); }
    inline unsigned char* cachedDatas(){ return cache+cacheSize-cachedLength(); }
    inline const size_t   cachedLength()const{ return (size_t)(m_readPosEnd-m_readPos); }
    bool resetPos(hpatch_StreamPos_t oldPos){
        //stream:[      |            |                  |                      |        ]
        //           readPos       oldPos    (oldPos+kMatchBlockSize)    (readPosEnd)
        //cache:   [    |            |                  |                      ]
        //         0           (init cachePos)                             cacheSize
        if ((oldPos+kMatchBlockSize<=m_readPosEnd)&&(oldPos>=m_readPos)){
            cachePos=cacheSize-(size_t)(m_readPosEnd-oldPos);
            return true; //hit cache
        }
        hpatch_StreamPos_t streamSize=stream->streamSize;
        if (oldPos+kMatchBlockSize>streamSize) return false;
        hpatch_StreamPos_t readPos=(oldPos>=kMatchBlockSize)?(oldPos-kMatchBlockSize):0;
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

static bool getBestMatch(const TDigestMatcher::TDigest*pblocks,const TDigestMatcher::TDigest* pblocks_end,
                        TOldStreamCache& oldStream,TNewStreamCache& newStream,
                         const TCover& lastCover,TCover* out_curCover){
    const size_t kMatchBlockSize=newStream.kMatchBlockSize;
    bool isMatched=false;
    //TCover bestCover={0,0,0};
    for (const TDigestMatcher::TDigest* curBlock=pblocks; curBlock!=pblocks_end; ++curBlock) {
        hpatch_StreamPos_t oldPos=curBlock->block_index*kMatchBlockSize;
        if (!oldStream.resetPos(oldPos)) continue;
        if (0==memcmp(oldStream.datas(),newStream.datas(),kMatchBlockSize)){
            isMatched=true;
            
            out_curCover->oldPos=oldPos;
            out_curCover->newPos=newStream.pos();
            out_curCover->length=kMatchBlockSize;
            break;
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

void TDigestMatcher::search_cover(const hpatch_TStreamInput* newData,ICovers* out_covers){
    if (m_blocks.empty()) return;
    const TDigest* pblocks=&m_blocks[0];
    const TDigest* pblocks_end=pblocks+m_blocks.size();
    if (newData->streamSize<m_kMatchBlockSize) return;
    TNewStreamCache newStream(newData,m_kMatchBlockSize,&m_buf[0],m_newCacheSize);
    TOldStreamCache oldStream(m_oldData,m_kMatchBlockSize,&m_buf[m_newCacheSize],m_buf.size()-m_newCacheSize);
    TCover  lastCover={0,0,0};
    TCover  curCover;
    while (true) {
        if (m_filter.is_hit(newStream.rollDigest())){
            std::pair<const TDigest*,const TDigest*>
            range=std::equal_range(pblocks,pblocks_end,newStream.rollDigest(),TDigest::T_comp());
            if (range.first!=range.second){
                const unsigned char* pnew=newStream.datas();
                if (is_same_data(pnew,m_kMatchBlockSize)){
                    if (!newStream.skip_same(pnew[0])) break;//finish
                    continue;
                }
                if (getBestMatch(range.first,range.second,oldStream,newStream,lastCover,&curCover)){
                    out_covers->addCover(curCover);
                    lastCover=curCover;
                    if (!newStream.resetPos(curCover.newPos+curCover.length)) break;//finish
                    continue;
                }//else roll
            }//else roll
        }
        if (!newStream.roll()) break;//finish
    }
    printf("%lld / %lld",out_covers->coverCount(),(newData->streamSize+m_kMatchBlockSize-1)/m_kMatchBlockSize);
}


