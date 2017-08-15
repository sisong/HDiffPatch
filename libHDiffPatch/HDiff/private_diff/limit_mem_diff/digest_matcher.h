//digest_matcher.h
//用摘要匹配的办法代替后缀数组的匹配,匹配效果比后缀数差,但内存占用少;
//用adler计算数据的摘要信息,以便于滚动匹配.
//
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

#ifndef digest_matcher_h
#define digest_matcher_h
#include "../../../HPatch/patch_types.h"
#include <vector>
#include "bloom_filter.h"
#include "adler_roll.h"

struct TCover{
    hpatch_StreamPos_t oldPos;
    hpatch_StreamPos_t newPos;
    hpatch_StreamPos_t length;
};
struct ICovers{
    virtual void   addCover(const TCover& cover)=0;
    virtual size_t coverCount()const=0;
};

class TDigestMatcher{
public:
    //throw std::runtime_error when data->read error or kMatchBlockSize error;
    TDigestMatcher(const hpatch_TStreamInput* oldData,size_t kMatchBlockSize);
    void search_cover(const hpatch_TStreamInput* newData,ICovers* out_covers);

    struct TDigest{
        adler_uint_t    digest;
        uint64_t        block_index;
        inline TDigest(){ }
        inline TDigest(adler_uint_t _digest,uint64_t _block_index)
            :digest(_digest),block_index(_block_index){ }
        inline bool operator < (const TDigest& y) const {
            return (digest!=y.digest)?(digest<y.digest):(block_index<y.block_index); }
        
        struct T_comp{
            inline bool operator()(const TDigest& x,const adler_uint_t y)const { return x.digest<y; }
            inline bool operator()(const adler_uint_t x,const TDigest& y)const { return x<y.digest; }
        };
    };
private:
    const hpatch_TStreamInput*  m_oldData;
    std::vector<unsigned char>  m_buf;
    std::vector<TDigest>        m_blocks;
    size_t                      m_kMatchBlockSize;
    size_t                      m_newCacheSize;
    TBloomFilter                m_filter;
    
    void getDigests();
};


#endif
