//adler_roll.c
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

#include "adler_roll.h"
#include <assert.h>

//#define _IS_USE_ADLER_FAST_BASE
#define MAX_DATA ((1<<sizeof(adler_data_t)*8)-1)

#ifdef _IS_USE_ADLER_FAST_BASE
#   define _adler32_BASE (1<<16)
#   define _adler32_mod(v)       ((uint32_t)((v)&(_adler32_BASE-1)))
#   define _adler32_to_border(v) { (v)=_adler32_mod(v); }
#else
#   define _adler32_BASE 65521 //largest prime that is less than 2^32
//# define _adler32_to_border(v) { if ((v) >= _adler32_BASE) (v) -= _adler32_BASE; }
#   define _adler32_to_border(v) { (v) -= _adler32_BASE & (uint32_t)( \
                                          ((int32_t)(_adler32_BASE-1)-(int32_t)(v))>>31 ); }
#   define _adler32_mod(v)       ((v)%_adler32_BASE)
#endif


#ifdef _IS_NEED_ADLER64
#ifdef _IS_USE_ADLER_FAST_BASE
#   define _adler64_BASE ((uint64_t)1<<32)
#   define _adler64_mod(v)       ((uint64_t)((v)&(_adler64_BASE-1)))
#   define _adler64_to_border(v) { (v)=_adler64_mod(v); }
#else
#   define _adler64_BASE 0xFFFFFFFBull
//# define _adler64_to_border(v) { if ((v) >= _adler64_BASE) (v) -= _adler64_BASE; }
#   define _adler64_to_border(v) { (v) -= _adler64_BASE & (uint64_t)( \
                                          ((int64_t)(_adler64_BASE-1)-(int64_t)(v))>>63 ); }
#   define _adler64_mod(v)       ((v)%_adler64_BASE)
#endif
#endif


#define _adler_add1(adler,sum,byteData){ \
    (adler) += (byteData); \
    (sum)   += (adler);    \
}
#define _adler_add4(adler,sum,pdata,i){ \
    _adler_add1(adler,sum,pdata[i  ]); \
    _adler_add1(adler,sum,pdata[i+1]); \
    _adler_add1(adler,sum,pdata[i+2]); \
    _adler_add1(adler,sum,pdata[i+3]); \
}


#ifdef _IS_USE_ADLER_FAST_BASE
#   define _border_twice(border,v) border(v)
#else
#   define _border_twice(border,v) { border(v); border(v); }
#endif

//limit: 255*n*(n+1)/2 + (n+1)(B-1) <= 2^32-1
// => max(n)=5552
// => if (255 to (2^16-1)) then max(n) =360
#define _adler_append(uint_t,half_bit,mod,border, adler,pdata,n){ \
    uint_t sum=adler>>half_bit;      \
    adler&=(((uint_t)1<<half_bit)-1);\
    while (n>=32) { \
        _adler_add4(adler,sum,pdata, 0);\
        _adler_add4(adler,sum,pdata, 4);\
        _adler_add4(adler,sum,pdata, 8);\
        _adler_add4(adler,sum,pdata,12);\
        _adler_add4(adler,sum,pdata,16);\
        _adler_add4(adler,sum,pdata,20);\
        _adler_add4(adler,sum,pdata,24);\
        _adler_add4(adler,sum,pdata,28);\
        sum=mod(sum);   \
        border(adler);  \
        pdata+=32;      \
        n-=32;          \
    }               \
    while (n>0) {   \
        adler += (*pdata++);\
        border(adler);      \
        sum+=adler;         \
        border(sum);        \
        --n;                \
    }               \
    return adler | (sum<<half_bit);  \
}

#ifndef _IS_USE_ADLER_FAST_BASE
#   define  _adler_roll(uint_t,half_bit,mod,border,BASE,kMaxBlockSize,\
                        adler,blockSize,out_data,in_data){ \
        uint_t sum=adler>>half_bit;       \
        adler&=(((uint_t)1<<half_bit)-1); \
        /*  [0..B-1] + [0..255] + B - [0..255]   =>  [0+0+B-255..B-1+255+B-0]*/ \
        adler=adler+in_data+(uint_t)(BASE-out_data);/* => [B-255..B*2-1+255] */ \
        _border_twice(border,adler);      \
        /* [0..B-1] + [0..B-1] + B-1 - [0..B-1] => [(B-1)-(B-1)..B-1+B-1+B-1]*/ \
        blockSize=(blockSize<=kMaxBlockSize)?blockSize:mod(blockSize);          \
        sum=sum+adler+(uint_t)((BASE-ADLER_INITIAL) - mod(blockSize*out_data)); \
        _border_twice(border,sum);        \
        return adler | (sum<<half_bit);   \
    }
#endif

#define  _adler_combine(uint_t,half_bit,mod,border,BASE,   \
                        adler_left,adler_right,len_right){ \
    const uint_t kMask=((uint_t)1<<half_bit)-1; \
    uint_t rem= mod(len_right);       \
    uint_t adler= adler_left&kMask;   \
    uint_t sum  = rem * adler;        \
    adler+= (adler_right&kMask)       \
          + (BASE-ADLER_INITIAL);     \
    sum   = mod(sum);                 \
    sum  += (adler_left>>half_bit)    \
          + (adler_right>>half_bit)   \
          + (BASE-rem)*ADLER_INITIAL; \
    _border_twice(border,adler);      \
    _border_twice(border,sum);        \
    border(sum);                      \
    return adler | (sum<<half_bit);   \
}

//limit: all result in uint32_t
//blockSize*255 <= 2^32-1
// => max(blockSize)=(2^32-1)/255
// max(blockSize) =16843009 =(1<<24)+65793
// => if (255 to (2^16-1)) then max(blockSize) =65537 = (1<<16)+1
#ifndef _IS_USE_ADLER_FAST_BASE
static const size_t   adler_roll_kMaxBlockSize=((size_t)(~(size_t)0))/MAX_DATA;
#   ifdef _IS_NEED_ADLER64
static const uint64_t adler64_roll_kMaxBlockSize=0xFFFFFFFFFFFFFFFFull/MAX_DATA;
#   endif
#endif

uint32_t adler32_append(uint32_t adler,const adler_data_t* pdata,size_t n)
    _adler_append(uint32_t,16,_adler32_mod,_adler32_to_border, adler,pdata,n)

#ifndef _IS_USE_ADLER_FAST_BASE
uint32_t adler32_roll(uint32_t adler,size_t blockSize,adler_data_t out_data,adler_data_t in_data)
    _adler_roll(uint32_t,16,_adler32_mod,_adler32_to_border,_adler32_BASE,
                adler_roll_kMaxBlockSize,adler,blockSize, out_data,in_data)
#endif

uint32_t adler32_combine(uint32_t adler_left,uint32_t adler_right,size_t len_right)
    _adler_combine(uint32_t,16,_adler32_mod,_adler32_to_border,_adler32_BASE,
                   adler_left,adler_right,len_right)

#ifdef _IS_NEED_ADLER64
uint64_t adler64_append(uint64_t adler,const adler_data_t* pdata,size_t n)
    _adler_append(uint64_t,32,_adler64_mod,_adler64_to_border, adler,pdata,n)

#ifndef _IS_USE_ADLER_FAST_BASE
uint64_t adler64_roll(uint64_t adler,uint64_t blockSize,adler_data_t out_data,adler_data_t in_data)
    _adler_roll(uint64_t,32,_adler64_mod,_adler64_to_border,_adler64_BASE,
                adler64_roll_kMaxBlockSize,adler,blockSize, out_data,in_data)
#endif

uint64_t adler64_combine(uint64_t adler_left,uint64_t adler_right,uint64_t len_right)
    _adler_combine(uint64_t,32,_adler64_mod,_adler64_to_border,_adler64_BASE,
                   adler_left,adler_right,len_right)

#endif //_IS_NEED_ADLER64

