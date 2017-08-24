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
#   define _adler32_to_border(v) { (v) -= _adler32_BASE & (uint32_t)( ((_adler32_BASE-1)-(int32_t)(v))>>31 ); }
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
#   define _adler64_to_border(v) { (v) -= _adler64_BASE & (uint64_t)( ((_adler64_BASE-1)-(int64_t)(v))>>63 ); }
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

//limit: sum + adler + kBlockSizeBM >= blockSize*out
//  [0..B-1] + [0..B-1] + m*B >= blockSize*[0..255]
// => 0 + 0 + m*B>=blockSize*255
// => min(m)=(blockSize*255+(B-1))/B
// => blockSizeBM=B*min(m)
#ifdef _IS_USE_ADLER_FAST_BASE
#   define _adler_roll_kBlockSizeBM(uint_t,kMaxBlockSize,BASE, blockSize) { return 0; } //no limit
#else
#   define _adler_roll_kBlockSizeBM(uint_t,kMaxBlockSize,BASE, blockSize) { \
        assert(blockSize>0);    \
        assert(blockSize<=kMaxBlockSize);   \
        uint_t min_m=(blockSize*MAX_DATA+(BASE-1))/BASE; \
        uint_t blockSizeBM=BASE*min_m;  \
        return blockSizeBM; \
    }
#endif

#ifndef _IS_USE_ADLER_FAST_BASE
#   define  _adler_roll_step(uint_t,half_bit,mod,border,BASE,\
                             adler,blockSize,kBlockSizeBM,out_data,in_data){ \
        uint_t sum=adler>>half_bit;       \
        adler&=(((uint_t)1<<half_bit)-1); \
        /*  [0..B-1] + [0..255] + B - [0..255]   =>  [0+0+B-255..B-1+255+B-0]*/ \
        adler += in_data +(uint_t)(BASE - out_data);/* => [B-255..B*2-1+255] */ \
        border(adler); /* [0..B-1+255] */   \
        border(adler); /* [0..B-1] */       \
        /*  limit by adler?_roll_blockSizeBM() and adler?_roll_kMaxBlockSize */ \
        sum = mod(sum + adler + kBlockSizeBM - blockSize*out_data); \
        return adler | (sum<<half_bit); \
    }
#endif

#define  _adler_combine(uint_t,half_bit,mod,border,BASE,\
                        adler1,adler2,len2){ \
    const uint_t kMask=((uint_t)1<<half_bit)-1; \
    uint_t rem= mod(len2);      \
    uint_t sum1 = adler1 & kMask;   \
    uint_t sum2 = rem * sum1;   \
    sum1 += (adler2 & kMask) + BASE - 1;    \
    sum2 += ((adler1 >> half_bit) & kMask)  \
          + ((adler2 >> half_bit) & kMask) + BASE - rem; \
    border(sum1); \
    border(sum1); \
    border(sum2); \
    border(sum2); \
    border(sum2); \
    return sum1 | (sum2 << half_bit); \
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
// max(blockSize) =16842238 =(1<<24)+65022
// => if (255 to (2^16-1)) then max(blockSize) =65534 = (1<<16)-2
#ifdef _IS_USE_ADLER_FAST_BASE
const uint32_t adler32_roll_kMaxBlockSize=0xFFFFFFFF; //no limit
#   ifdef _IS_NEED_ADLER64
const uint64_t adler64_roll_kMaxBlockSize=0xFFFFFFFFFFFFFFFFull;
#   endif
#else
const uint32_t adler32_roll_kMaxBlockSize=(0xFFFFFFFF-3*(_adler32_BASE-1))/MAX_DATA;
#   ifdef _IS_NEED_ADLER64
const uint64_t adler64_roll_kMaxBlockSize=(0xFFFFFFFFFFFFFFFFull-3*(_adler64_BASE-1))/MAX_DATA;
#   endif
#endif

uint32_t adler32_append(uint32_t adler,const adler_data_t* pdata,size_t n)
    _adler_append(uint32_t,16,_adler32_mod,_adler32_to_border,
                  adler,pdata,n)

uint32_t adler32_roll_kBlockSizeBM(uint32_t blockSize)
    _adler_roll_kBlockSizeBM(uint32_t,adler32_roll_kMaxBlockSize,_adler32_BASE,
                             blockSize)

#ifndef _IS_USE_ADLER_FAST_BASE
uint32_t adler32_roll_step(uint32_t adler,uint32_t blockSize,uint32_t kBlockSizeBM,
                           adler_data_t out_data,adler_data_t in_data)
    _adler_roll_step(uint32_t,16,_adler32_mod,_adler32_to_border,_adler32_BASE,
                     adler,blockSize,kBlockSizeBM, out_data,in_data)
#endif

uint32_t adler32_combine(uint32_t adler1,uint32_t adler2,size_t len2)
    _adler_combine(uint32_t,16,_adler32_mod,_adler32_to_border,_adler32_BASE,
                   adler1,adler2,len2)

#ifdef _IS_NEED_ADLER64
uint64_t adler64_append(uint64_t adler,const adler_data_t* pdata,size_t n)
    _adler_append(uint64_t,32,_adler64_mod,_adler64_to_border,
                  adler,pdata,n)

uint64_t adler64_roll_kBlockSizeBM(uint64_t blockSize)
    _adler_roll_kBlockSizeBM(uint64_t,adler64_roll_kMaxBlockSize,_adler64_BASE,
                             blockSize)

#ifndef _IS_USE_ADLER_FAST_BASE
uint64_t adler64_roll_step(uint64_t adler,uint64_t blockSize,uint64_t kBlockSizeBM,
                           adler_data_t out_data,adler_data_t in_data)
    _adler_roll_step(uint64_t,32,_adler64_mod,_adler64_to_border,_adler64_BASE,
                     adler,blockSize,kBlockSizeBM, out_data,in_data)
#endif

uint64_t adler64_combine(uint64_t adler1,uint64_t adler2,uint64_t len2)
    _adler_combine(uint64_t,32,_adler64_mod,_adler64_to_border,_adler64_BASE,
                   adler1,adler2,len2)

#endif //_IS_NEED_ADLER64

