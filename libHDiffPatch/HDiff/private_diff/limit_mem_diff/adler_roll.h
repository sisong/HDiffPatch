//adler_roll.h
//support: adler32,adler64,roll,fast roll,combine
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

#if defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
#   include <stdint.h> //for uint16_t,uint32_t,uint64_t
#else
#   ifdef _MSC_VER
#       if (_MSC_VER >= 1300)
        typedef unsigned __int16    uint16_t;
        typedef unsigned __int32    uint32_t;
#       else
        typedef unsigned short      uint16_t;
        typedef unsigned int        uint32_t;
#       endif
        typedef unsigned __int64    uint64_t;
#   else
        typedef unsigned short      uint16_t;
        typedef unsigned int        uint32_t;
        typedef unsigned long long  uint64_t;
#   endif
#endif
    
#ifdef _MSC_VER
#   define __adler_inline _inline
#else
#   define __adler_inline inline
#endif

#ifndef ADLER_INITIAL
#define ADLER_INITIAL 1 //must 0 or 1
#endif

#define  __private_fast_adler_roll(SUM,ADLER,return_SUMADLER,_c_t,_table,_tb_t, \
                                   adler,blockSize,out_data,in_data){ \
    _tb_t  out_v = _table[(unsigned char)(out_data)];  \
    _c_t   sum   = (_c_t)SUM(adler);                   \
    _c_t   radler= (_c_t)ADLER(adler) + _table[(unsigned char)(in_data)] -out_v;  \
    sum  = sum + radler - ADLER_INITIAL-((_c_t)blockSize)*out_v; \
    return_SUMADLER(sum,radler); \
}

#define  adler32_start(pdata,n) adler32_append(ADLER_INITIAL,pdata,n)
uint32_t adler32_append(uint32_t adler,const adler_data_t* pdata,size_t n);
uint32_t adler32_roll(uint32_t adler,size_t blockSize,adler_data_t out_data,adler_data_t in_data);
uint32_t adler32_by_combine(uint32_t adler_left,uint32_t adler_right,size_t len_right);

#define  adler64_start(pdata,n) adler64_append(ADLER_INITIAL,pdata,n)
uint64_t adler64_append(uint64_t adler,const adler_data_t* pdata,size_t n);
uint64_t adler64_roll(uint64_t adler,uint64_t blockSize,adler_data_t out_data,adler_data_t in_data);
uint64_t adler64_by_combine(uint64_t adler_left,uint64_t adler_right,uint64_t len_right);
    
#define  fast_adler32_start(pdata,n) fast_adler32_append(ADLER_INITIAL,pdata,n)
uint32_t fast_adler32_append(uint32_t adler,const adler_data_t* pdata,size_t n);
                        extern const uint16_t* _private_fast_adler32_table;
#   define __private_fast32_SUM(xadler)             ((xadler)>>16)
#   define __private_fast32_ADLER(xadler)           (xadler)
#   define __private_fast32_SUMADLER(xsum,xadler)   return (((xadler)&(((uint32_t)1<<16)-1)) | ((xsum)<<16))
__adler_inline static
uint32_t fast_adler32_roll(uint32_t adler,size_t blockSize,adler_data_t out_data,adler_data_t in_data)
    __private_fast_adler_roll(__private_fast32_SUM,__private_fast32_ADLER,__private_fast32_SUMADLER,
                              uint32_t, _private_fast_adler32_table,uint16_t, adler,blockSize,out_data,in_data)
uint32_t fast_adler32_by_combine(uint32_t adler_left,uint32_t adler_right,size_t len_right);

#define  fast_adler64_start(pdata,n) fast_adler64_append(ADLER_INITIAL,pdata,n)
uint64_t fast_adler64_append(uint64_t adler,const adler_data_t* pdata,size_t n);
                        extern const uint32_t* _private_fast_adler64_table;
#   define __private_fast64_SUM(xadler)             ((xadler)>>32)
#   define __private_fast64_ADLER(xadler)           (xadler)
#   define __private_fast64_SUMADLER(xsum,xadler)   return ((uint32_t)(xadler) | ((uint64_t)(xsum)<<32))
__adler_inline static
uint64_t fast_adler64_roll(uint64_t adler,uint64_t blockSize,adler_data_t out_data,adler_data_t in_data)
    __private_fast_adler_roll(__private_fast64_SUM,__private_fast64_ADLER,__private_fast64_SUMADLER,
                              uint32_t, _private_fast_adler64_table,uint32_t, adler,blockSize,out_data,in_data)
uint64_t fast_adler64_by_combine(uint64_t adler_left,uint64_t adler_right,uint64_t len_right);

typedef struct adler128_t{
    uint64_t adler;
    uint64_t sum;
} adler128_t;
static const adler128_t ADLER128_INITIAL = {ADLER_INITIAL,0};

#define    fast_adler128_start(pdata,n) fast_adler128_append(ADLER128_INITIAL,pdata,n)
adler128_t fast_adler128_append(adler128_t adler,const adler_data_t* pdata,size_t n);
                        extern const uint64_t* _private_fast_adler128_table;
#   define __private_fast128_SUM(xadler)            (xadler.sum)
#   define __private_fast128_ADLER(xadler)          (xadler.adler)
#   define __private_fast128_SUMADLER(xsum,xadler)  { adler128_t _rt ={xadler,xsum}; return _rt; }
__adler_inline static
adler128_t fast_adler128_roll(adler128_t adler,uint64_t blockSize,adler_data_t out_data,adler_data_t in_data)
    __private_fast_adler_roll(__private_fast128_SUM,__private_fast128_ADLER,__private_fast128_SUMADLER,
                              uint64_t, _private_fast_adler128_table,uint64_t, adler,blockSize,out_data,in_data)
adler128_t fast_adler128_by_combine(adler128_t adler_left,adler128_t adler_right,uint64_t len_right);


#ifdef __cplusplus
}
#endif

#endif
