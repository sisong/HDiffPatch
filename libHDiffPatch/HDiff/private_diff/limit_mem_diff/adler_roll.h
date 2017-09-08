//adler_roll.h
//support: adler32,adler64,roll,fast adler,combine adler
//  adler32 support roll by uint8,uint16
//  adler64 support roll by uint8,uint16,uint32
//https://github.com/madler/zlib/blob/master/adler32.c not find roll
//
/*
 The MIT License (MIT)
 Copyright (c) 2017 HouSisong
 
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
#include <stddef.h> //for size_t
#ifdef __cplusplus
extern "C" {
#endif
    
#ifndef adler_data_t
#   define adler_data_t unsigned char
#endif
    
#define _IS_NEED_ADLER64

#if defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
#   include <stdint.h> //for uint32_t,uint64_t
#else
#   ifdef _MSC_VER
#       if (_MSC_VER >= 1300)
        typedef unsigned __int32    uint32_t;
#       else
        typedef unsigned int        uint32_t;
#       endif
#       ifdef _IS_NEED_ADLER64
        typedef unsigned __int64    uint64_t;
#       endif
#   else
        typedef unsigned int        uint32_t;
#       ifdef _IS_NEED_ADLER64
        typedef unsigned long long  uint64_t;
#       endif
#   endif
#endif
    
#ifdef _MSC_VER
#   define __adler_inline _inline
#else
#   define __adler_inline inline
#endif
    
#define ADLER_INITIAL 1 //must 0 or 1

#define  __private_adler_roll_fast(uint_t,half_bit,\
                                  adler,blockSize,out_data,in_data){ \
    uint_t sum=adler>>half_bit;        \
    adler= adler + in_data - out_data; \
    sum  = sum + adler - ADLER_INITIAL-(uint32_t)blockSize*out_data; \
    return (adler&(((uint_t)1<<half_bit)-1)) | (sum<<half_bit);      \
}

uint32_t adler32_append(uint32_t adler,const adler_data_t* pdata,size_t n);
#define  adler32_start(pdata,n) adler32_append(ADLER_INITIAL,pdata,n)
uint32_t adler32_roll(uint32_t adler,size_t blockSize,adler_data_t out_data,adler_data_t in_data);
uint32_t adler32_by_combine(uint32_t adler_left,uint32_t adler_right,size_t len_right);
    
uint32_t fast_adler32_append(uint32_t adler,const adler_data_t* pdata,size_t n);
#define  fast_adler32_start(pdata,n) fast_adler32_append(ADLER_INITIAL,pdata,n)
__adler_inline static
uint32_t fast_adler32_roll(uint32_t adler,size_t blockSize,adler_data_t out_data,adler_data_t in_data)
                __private_adler_roll_fast(uint32_t,16,adler,blockSize,out_data,in_data)
uint32_t fast_adler32_by_combine(uint32_t adler_left,uint32_t adler_right,size_t len_right);

#ifdef _IS_NEED_ADLER64
    
uint64_t adler64_append(uint64_t adler,const adler_data_t* pdata,size_t n);
#define  adler64_start(pdata,n) adler64_append(ADLER_INITIAL,pdata,n)
uint64_t adler64_roll(uint64_t adler,uint64_t blockSize,adler_data_t out_data,adler_data_t in_data);
uint64_t adler64_by_combine(uint64_t adler_left,uint64_t adler_right,uint64_t len_right);

uint64_t fast_adler64_append(uint64_t adler,const adler_data_t* pdata,size_t n);
#define  fast_adler64_start(pdata,n) fast_adler64_append(ADLER_INITIAL,pdata,n)
__adler_inline static
uint64_t fast_adler64_roll(uint64_t adler,uint64_t blockSize,adler_data_t out_data,adler_data_t in_data)
                __private_adler_roll_fast(uint64_t,32,adler,blockSize,out_data,in_data)
uint64_t fast_adler64_by_combine(uint64_t adler_left,uint64_t adler_right,uint64_t len_right);

#endif //_IS_NEED_ADLER64

#ifdef __cplusplus
}
#endif
#endif
