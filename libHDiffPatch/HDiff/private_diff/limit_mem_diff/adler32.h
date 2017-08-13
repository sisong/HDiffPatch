//adler32.h
//计算adler32摘要信息,支持滚动校验.
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

#ifndef adler32_h
#define adler32_h

#ifdef __cplusplus
extern "C" {
#endif
    
#ifdef _MSC_VER
#   if (_MSC_VER < 1300)
typedef unsigned int      uint32_t;
#   else
typedef unsigned __int32  uint32_t;
#   endif
#else
#   include <stdint.h> //for int32_t uint32_t
#endif
    
//support #define _adler32_FAST_BASE

uint32_t adler32_append(uint32_t adler,unsigned char* pdata,size_t n);

extern const uint32_t adler32_roll_kMaxBlockSize; // > (1<<24)
uint32_t              adler32_roll_kBlockSizeBM(uint32_t blockSize);

#define  adler32_roll_start(pdata,n) adler32_append(0,pdata,n)
uint32_t adler32_roll_step(uint32_t adler,uint32_t blockSize,uint32_t kBlockSizeBM,
                           unsigned char out_data,unsigned char in_data);

#ifdef __cplusplus
}
#endif
#endif
