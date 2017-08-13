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
#include "adler32_roll.h"

TDigestMatcher::TDigestMatcher(const hpatch_TStreamInput* oldData,size_t kMatchBlockSize)
:m_oldData(oldData),m_kMatchBlockSize(kMatchBlockSize),m_oldCacheSize(0){
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
    
    size_t kDataBufSize=1024*64;
    if (kDataBufSize<m_kMatchBlockSize) kDataBufSize=m_kMatchBlockSize;
    m_oldCacheSize=m_kMatchBlockSize*2+kDataBufSize;
    size_t newCacheSize=m_oldCacheSize;
    m_buf.resize(m_oldCacheSize+newCacheSize);
    getDigests();
}

#define readStream(stream,pos,dst,n) { \
    if (n!=(stream)->read((stream)->streamHandle,pos,dst,dst+n)) \
    throw std::runtime_error("TDataDigest stream->read() error!"); \
}


void TDigestMatcher::getDigests(){
    const size_t blockCount=m_blocks.size();
    unsigned char* buf=&m_buf[0];
    for (size_t i=0; i<blockCount; ++i) {
        hpatch_StreamPos_t readPos=(hpatch_StreamPos_t)i*m_kMatchBlockSize;
        if (i==blockCount-1)
            readPos=m_oldData->streamSize-m_kMatchBlockSize;
        readStream(m_oldData,readPos,buf,m_kMatchBlockSize);
        m_blocks[i]=adler32_roll_start(buf,m_kMatchBlockSize);
    }
}

struct TOldStream{
    TOldStream(const hpatch_TStreamInput* _stream,uint32_t _kMatchBlockSize,
               unsigned char* _cache,size_t _cacheSize)
    :stream(_stream),streamPos(0),kMatchBlockSize(_kMatchBlockSize),
    kBlockSizeBM(adler32_roll_kBlockSizeBM(_kMatchBlockSize)),
    cache(_cache),cacheSize(_cacheSize),cachePos(_cacheSize){
        resetPos(0);
    }
    
    inline hpatch_StreamPos_t pos()const {
        return streamPos-cacheSize+cachePos;
    }
    bool resetPos(hpatch_StreamPos_t oldPos){
        if (oldPos+kMatchBlockSize>stream->streamSize) return false;
        //todo: move old;
        if (cacheSize>stream->streamSize-oldPos) cacheSize=stream->streamSize-oldPos;
        readStream(stream,oldPos,cache,cacheSize);
        digest=adler32_roll_start(cache,kMatchBlockSize);
        cachePos=0;
        streamPos=oldPos+cacheSize;
        return true;
    }
    inline bool roll(){
        if (cachePos+kMatchBlockSize!=cacheSize){
            digest=adler32_roll_step(digest,kMatchBlockSize,kBlockSizeBM,
                                     cache[cachePos],cache[cachePos+kMatchBlockSize]);
            ++cachePos;
            return true;
        }else{
            return _update_roll();
        }
    }
    bool _update_roll(){
        if (stream->streamSize!=streamPos){
            _update_cache();
            return roll();
        }else
            return false; //roll finish
    }
    void _update_cache(){
        assert(cachePos>0);
        assert(cachePos+kMatchBlockSize==cacheSize);
        assert(stream->streamSize>streamPos);
        memmove(cache,cache+cachePos,kMatchBlockSize);
        if (cacheSize-kMatchBlockSize>(stream->streamSize-streamPos))
            cacheSize=(stream->streamSize-streamPos)+kMatchBlockSize;
        cachePos=0;
        readStream(stream,streamPos,cache+kMatchBlockSize,cacheSize-kMatchBlockSize);
        streamPos+=cacheSize-kMatchBlockSize;
    }
    hpatch_inline const unsigned char* matchData()const{ return cache+cachePos; }
    const hpatch_TStreamInput* stream;
    hpatch_StreamPos_t         streamPos;
    uint32_t                   kMatchBlockSize;
    uint32_t                   kBlockSizeBM;
    uint32_t        digest;
    unsigned char*  cache;
    size_t          cacheSize;
    size_t          cachePos;
};

void TDigestMatcher::search_cover(const hpatch_TStreamInput* newData,ICovers* out_covers){
    if (newData->streamSize<m_kMatchBlockSize) return;
    if (m_blocks.empty()) return;
    TOldStream oldStream(m_oldData,(uint32_t)m_kMatchBlockSize,&m_buf[0],m_oldCacheSize);
    TCover  lastCover={0,0,0};
    TCover  curCover;
    while (true) {
        /*if (getBestMatch(oldStream,lastCover,&curCover)) {
            out_covers->addCover(curCover);
            if (!oldStream.resetPos(curCover.oldPos+curCover.length)) break;
            lastCover=curCover;
        }else*/{
            if (!oldStream.roll())
                break;
        }
    }
}
