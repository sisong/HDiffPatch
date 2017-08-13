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

#define _adler32_FAST_BASE

#ifdef _adler32_FAST_BASE
#   define _adler32_BASE (1<<16)
#   define _adler32_mod(v)       ((uint32_t)((v)&(_adler32_BASE-1)))
#   define _adler32_to_border(v) { (v)=_adler32_mod(v); }
#else
#   define _adler32_BASE 65521
//# define _adler32_to_border(v) { if ((v) >= _adler32_BASE) (v) -= _adler32_BASE; }
#   define _adler32_to_border(v) { (v) -= _adler32_BASE & (uint32_t)( ((_adler32_BASE-1)-(int32_t)(v))>>31 ); }
#   define _adler32_mod(v)       ((v)%_adler32_BASE)
#endif


#define _adler32_add1(adler,sum,byteData){ \
    (adler) += (byteData); \
    (sum)   += (adler);    \
}
#define _adler32_add4(adler,sum,data,i){ \
    _adler32_add1(adler,sum,data[i  ]); \
    _adler32_add1(adler,sum,data[i+1]); \
    _adler32_add1(adler,sum,data[i+2]); \
    _adler32_add1(adler,sum,data[i+3]); \
}

static uint32_t adler32(uint32_t adler,unsigned char* data,size_t n){
    uint32_t sum=adler>>16;
    adler&=0xFFFF;
    while (n>=16) {
        _adler32_add4(adler,sum,data,0);
        _adler32_add4(adler,sum,data,4);
        _adler32_add4(adler,sum,data,8);
        _adler32_add4(adler,sum,data,12);
        sum=_adler32_mod(sum);
        _adler32_to_border(adler);
        data+=16;
        n-=16;
    }
    while (n>0) {
        adler += (*data++);
        _adler32_to_border(adler);
        sum+=adler;
        _adler32_to_border(sum);
        --n;
    }
    return adler | (sum<<16);
}


hpatch_inline static
uint32_t adler32_roll_blockSizeBM(uint32_t blockSize){
    //limit: sum + adler + kBlockSizeBM >= blockSize*out
    //  [0..B-1] + [0..B-1] + m*B >= blockSize*[0..255]
    // => 0 + 0 + m*B>=blockSize*255
    // => min(m)=(blockSize*255+(B-1))/B
    // => blockSizeBM=B*min(m)
    uint32_t min_m=(blockSize*255+(_adler32_BASE-1))/_adler32_BASE;
    uint32_t blockSizeBM=_adler32_BASE*min_m;
    return blockSizeBM;
}

//limit: all result in uint32_t
//blockSize*255+(B-1)<2^32 && [0..B-1]+[0..B-1]+(blockSize*255+(B-1))/B*B<2^32
// => blockSize*255<=(2^32-1)-(B-1) && (B-1)+(B-1)+(blockSize*255+(B-1))/B*B<=(2^32-1)
// => ~~~ && (blockSize*255+B-1)/B*B<=(2^32-1)-2*(B-1)
// => ~~~ && (blockSize*255+B-1)<=(2^32-1)-2*(B-1)
// => ~~~ && blockSize*255<=(2^32-1)-3*(B-1)
// => blockSize*255<=(2^32-1)-3*(B-1)
// => blockSize<=((2^32-1)-3*(B-1))/255
// => max(blockSize)=((2^32-1)-3*(B-1))/255
const static uint32_t adler32_roll_kMaxBlockSize=(0xFFFFFFFF-3*(_adler32_BASE-1))/255;
//assert(adler32_roll_kMaxBlockSize > (1<<24))

hpatch_inline static
uint32_t adler32_roll(uint32_t adler,uint32_t blockSize,uint32_t kBlockSizeBM,
                      unsigned char out,unsigned char in){
    uint32_t sum=adler>>16;
    adler&=0xFFFF;
#ifdef _adler32_FAST_BASE
    adler =_adler32_mod(adler + in - out);
    sum +=adler - blockSize*out;
#else
    //  [0..B-1] + [0..255] + B - [0..255]        => [0+0+B-255..B-1+255+B-0]
    adler += in +(uint32_t)(_adler32_BASE - out);// => [B-255..B*2-1+255]
    _adler32_to_border(adler);//[0..B-1+255]
    _adler32_to_border(adler);//[0..B-1]
    //sum + adler + kBlockSizeBM >= blockSize*out => in adler32_roll_blockSizeBM()
    sum = _adler32_mod(sum + adler + kBlockSizeBM - blockSize*out);
#endif
    return adler | (sum<<16);
}


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
        m_blocks[i]=adler32(0,buf,m_kMatchBlockSize);
    }
}

struct TOldStream{
    TOldStream(const hpatch_TStreamInput* _stream,uint32_t _kMatchBlockSize,
               unsigned char* _cache,size_t _cacheSize)
    :stream(_stream),streamPos(0),kMatchBlockSize(_kMatchBlockSize),
    kBlockSizeBM(adler32_roll_blockSizeBM(_kMatchBlockSize)),
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
        digest=adler32(0,cache,kMatchBlockSize);
        cachePos=0;
        streamPos=oldPos+cacheSize;
        return true;
    }
    inline bool roll(){
        if (cachePos+kMatchBlockSize!=cacheSize){
            digest=adler32_roll(digest,kMatchBlockSize,kBlockSizeBM,
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
