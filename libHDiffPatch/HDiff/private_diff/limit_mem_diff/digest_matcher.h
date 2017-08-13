//digest_matcher.h
//用摘要匹配的办法代替后缀数组的匹配,匹配效果比后缀数差,但内存占用少;
//用adler32计算数据的摘要信息,以便于滚动匹配.
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

#ifdef _MSC_VER
#   if (_MSC_VER < 1300)
typedef signed   int      int32_t;
typedef unsigned int      uint32_t;
#   else
typedef signed   __int32  int32_t;
typedef unsigned __int32  uint32_t;
#   endif
#else
#   include <stdint.h> //for int32_t uint32_t
#endif


struct TCover{
    hpatch_StreamPos_t oldPos;
    hpatch_StreamPos_t newPos;
    hpatch_StreamPos_t length;
};
struct ICovers{
    virtual void addCover(const TCover& cover)=0;
};

class TDigestMatcher{
public:
    //throw std::runtime_error when data->read error or kMatchBlockSize error;
    TDigestMatcher(const hpatch_TStreamInput* oldData,size_t kMatchBlockSize);
    void search_cover(const hpatch_TStreamInput* newData,ICovers* out_covers);
private:
    typedef uint32_t TDigest;
    const hpatch_TStreamInput* m_oldData;
    std::vector<unsigned char> m_buf;
    std::vector<TDigest>       m_blocks;
    size_t     m_kMatchBlockSize;
    size_t     m_oldCacheSize;
    void getDigests();
};


#endif
