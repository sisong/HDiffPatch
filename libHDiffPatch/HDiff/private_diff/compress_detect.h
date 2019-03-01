//compress_detect.h
//粗略估算数据的可压缩性 for diff.
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

#ifndef compress_detect_h
#define compress_detect_h
#include <stddef.h> //for size_t
#include "../../HPatch/patch_types.h" //for hpatch_uint32_t
#include "mem_buf.h"
namespace hdiff_private{

template<class _UInt>
static unsigned int _getUIntCost(_UInt v){
    if ((sizeof(_UInt)<8)||((v>>28)>>28)==0) {
        int cost=1;
        _UInt t;
        if ((t=(v>>28))) { v=t; cost+=4; }
        if ((t=(v>>14))) { v=t; cost+=2; }
        if ((t=(v>> 7))) { v=t; ++cost; }
        return cost;
    }else{
        return 9;
    }
}

template<class _Int,class _UInt>
inline static unsigned _getIntCost(_Int v){
    return _getUIntCost((_UInt)(2*((v>=0)?(_UInt)v:(_UInt)(-v))));
}

//粗略估算该区域存储成本.
size_t getRegionRleCost(const unsigned char* d,size_t n,const unsigned char* sub=0,
                        unsigned char* out_nocompress=0,size_t* nocompressSize=0);

class TCompressDetect{
public:
    TCompressDetect();
    ~TCompressDetect();
    void   add_chars(const unsigned char* d,size_t n,const unsigned char* sub=0);
    size_t cost(const unsigned char* d,size_t n,const unsigned char* sub=0)const;
private:
    struct TCharConvTable{
        hpatch_uint32_t sum;
        hpatch_uint32_t sum1char[256];
        hpatch_uint32_t sum2char[256*256];//用相邻字符转换几率来近似估计数据的可压缩性.
        unsigned char cache[1];//实际大小为kCacheSize,超出该距离的旧数据会被清除.
    };
    TAutoMem        m_mem;
    TCharConvTable* m_table;
    int             m_lastChar;
    int             m_lastPopChar;
    hpatch_uint32_t m_cacheBegin;
    hpatch_uint32_t m_cacheEnd;
    void _add_rle(const unsigned char* d,size_t n);
    size_t _cost_rle(const unsigned char* d,size_t n)const;
};

}//namespace hdiff_private

#endif
