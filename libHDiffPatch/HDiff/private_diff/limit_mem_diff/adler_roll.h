//adler_roll.h
//计算adler32、adler64摘要信息,支持滚动校验.
//https://github.com/madler/zlib/blob/master/adler32.c 没看到roll的实现
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

#ifndef adler_roll_h
#define adler_roll_h

#ifdef __cplusplus
extern "C" {
#endif
    
#define _IS_USE_ADLER_FAST_BASE //WARNING: non-standard for roll_step speed
//#define _IS_NEED_ADLER64
//#define _IS_DEFAULT_ADLER64

#ifdef _IS_DEFAULT_ADLER64
#   ifndef _IS_NEED_ADLER64
#       define _IS_NEED_ADLER64
#   endif
#   define adler_uint_t                 uint64_t
#   define adler_append                 adler64_append
#   define adler_roll_kMaxBlockSize     adler64_roll_kMaxBlockSize
#   define adler_roll_kBlockSizeBM      adler64_roll_kBlockSizeBM
#   define adler_roll_start             adler64_roll_start
#   define adler_roll_step              adler64_roll_step
#else
#   define adler_uint_t                 uint32_t
#   define adler_append                 adler32_append
#   define adler_roll_kMaxBlockSize     adler32_roll_kMaxBlockSize
#   define adler_roll_kBlockSizeBM      adler32_roll_kBlockSizeBM
#   define adler_roll_start             adler32_roll_start
#   define adler_roll_step              adler32_roll_step
#endif
    
#ifdef _MSC_VER
#   if (_MSC_VER < 1300)
typedef signed int        int32_t;
typedef unsigned int      uint32_t;
#   else
typedef signed __int32    int32_t;
typedef unsigned __int32  uint32_t;
#   endif
#   ifdef _IS_NEED_ADLER64
typedef signed   __int64  int64_t;
typedef unsigned __int64  uint64_t;
#   endif
#else
#   include <stdint.h> //for int32_t uint32_t int64_t uint64_t
#endif

uint32_t adler32_append(uint32_t adler,unsigned char* pdata,size_t n);

extern const uint32_t adler32_roll_kMaxBlockSize; // > (1<<24)
uint32_t              adler32_roll_kBlockSizeBM(uint32_t blockSize);

#define  adler32_roll_start(pdata,n) adler32_append(0,pdata,n)
uint32_t adler32_roll_step(uint32_t adler,uint32_t blockSize,uint32_t kBlockSizeBM,
                           unsigned char out_data,unsigned char in_data);

#ifdef _IS_NEED_ADLER64
    
uint64_t adler64_append(uint64_t adler,unsigned char* pdata,size_t n);
    
extern const uint64_t adler64_roll_kMaxBlockSize; // > (1<<(32+24))
uint64_t              adler64_roll_kBlockSizeBM(uint64_t blockSize);
    
#define  adler64_roll_start(pdata,n) adler64_append(0,pdata,n)
uint64_t adler64_roll_step(uint64_t adler,uint64_t blockSize,uint64_t kBlockSizeBM,
                           unsigned char out_data,unsigned char in_data);

#endif //_IS_NEED_ADLER64
    
#ifdef __cplusplus
}
#endif
#endif
