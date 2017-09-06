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

#define MAX_DATA ((1<<sizeof(adler_data_t)*8)-1)

#   define _adler32_BASE                65521 //largest prime that is less than 2^32
#   define _adler_mod(v,BASE)           ((v)%(BASE))
#   define _adler_border1(v,BASE)       { (v)=((v)<(BASE))?(v):((v)-(BASE)); }
#   define _adler_border2(v,BASE)       { _adler_border1(v,BASE); _adler_border1(v,BASE); }
#   define _adler_border3(v,BASE)       { _adler_border1(v,((BASE)*2)); _adler_border1(v,BASE); }

#   define _fast_adler32_BASE           (1<<16)
#   define _fast_adler_mod(v,BASE)      ((v)&((BASE)-1))
#   define _fast_adler_border1(v,BASE)  { (v)=_fast_adler_mod(v,BASE); }
#   define _fast_adler_border2(v,BASE)  _fast_adler_border1(v,BASE)
#   define _fast_adler_border3(v,BASE)  _fast_adler_border1(v,BASE)

#ifdef _IS_NEED_ADLER64
#   define _adler64_BASE                0xFFFFFFFBull
#   define _fast_adler64_BASE           ((uint64_t)1<<32)
#endif //_IS_NEED_ADLER64


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
#define _adler_add16(adler,sum,pdata,i){ \
    _adler_add4(adler,sum,pdata,i   ); \
    _adler_add4(adler,sum,pdata,i+ 4); \
    _adler_add4(adler,sum,pdata,i+ 8); \
    _adler_add4(adler,sum,pdata,i+12); \
}

//limit: 255*n*(n+1)/2 + (n+1)(B-1) <= 2^32-1
// => max(n)=5552
// => if (255 to (2^16-1)) then max(n) =360
#define kFNBest (16*22) //<=360
#define _adler_append(uint_t,half_bit,BASE,mod,border1, adler,pdata,n){ \
    uint_t sum=adler>>half_bit;      \
    adler&=(((uint_t)1<<half_bit)-1);\
    while(n>=kFNBest){  \
        size_t fn=kFNBest>>4;  \
        n-=kFNBest;     \
        do{      \
            _adler_add16(adler,sum,pdata,0);\
            pdata+=16;  \
        }while(--fn);   \
        sum  =mod(sum,BASE);   \
        adler=mod(adler,BASE); \
    }            \
    if (n>=16){  \
        while (n>=16) { \
            n-=16;      \
            _adler_add16(adler,sum,pdata,0);\
            pdata+=16;  \
        }       \
        while (n--) {   \
            _adler_add1(adler,sum,*pdata);  \
            ++pdata;    \
        }       \
        sum  =mod(sum,BASE);   \
        adler=mod(adler,BASE); \
    }else{      \
        while (n--) {   \
            _adler_add1(adler,sum,*pdata);  \
            border1(sum,BASE); \
            ++pdata;    \
        }      \
        border1(adler,BASE);   \
    }          \
    return adler | (sum<<half_bit);  \
}

#define  _adler_roll(uint_t,half_bit,BASE,mod,border2,kBestBlockSize,\
                     adler,blockSize,out_data,in_data){ \
    uint_t sum=adler>>half_bit;       \
    adler&=(((uint_t)1<<half_bit)-1); \
    /*  [0..B-1] + [0..255] + B - [0..255]   =>  [0+0+B-255..B-1+255+B-0]*/ \
    adler=adler+in_data+(uint_t)(BASE-out_data);/* => [B-255..B*2-1+255] */ \
    border2(adler,BASE);      \
    /* [0..B-1] + [0..B-1] + B-1 - [0..B-1] => [(B-1)-(B-1)..B-1+B-1+B-1]*/ \
    blockSize=(blockSize<=kBestBlockSize)?blockSize:mod(blockSize,BASE);    \
    sum=sum+adler+(uint_t)((BASE-ADLER_INITIAL) - mod(blockSize*out_data,BASE)); \
    border2(sum,BASE);        \
    return adler | (sum<<half_bit);   \
}

#define  _adler_by_combine(uint_t,half_bit,BASE,mod,border2,border3,   \
                           adler_left,adler_right,len_right){ \
    const uint_t kMask=((uint_t)1<<half_bit)-1; \
    uint_t rem= mod(len_right,BASE);  \
    uint_t adler= adler_left&kMask;   \
    uint_t sum  = rem * adler;        \
    adler+= (adler_right&kMask)       \
          + (BASE-ADLER_INITIAL);     \
    sum   = mod(sum,BASE);            \
    sum  += (adler_left>>half_bit)    \
          + (adler_right>>half_bit)   \
          + (BASE-rem)*ADLER_INITIAL; \
    border2(adler,BASE);              \
    border3(sum,BASE);                \
    return adler | (sum<<half_bit);   \
}

//limit: if all result in uint32_t
//blockSize*255 <= 2^32-1
// => max(blockSize)=(2^32-1)/255
// max(blockSize) =16843009 =(1<<24)+65793
// => if (255 to (2^16-1)) then max(blockSize) =65537 = (1<<16)+1
static const size_t   adler_roll_kBestBlockSize=((size_t)(~(size_t)0))/MAX_DATA;
#ifdef _IS_NEED_ADLER64
static const uint64_t adler64_roll_kBestBlockSize=0xFFFFFFFFFFFFFFFFull/MAX_DATA;
#endif

uint32_t adler32_append(uint32_t adler,const adler_data_t* pdata,size_t n)
    _adler_append(uint32_t,16,_adler32_BASE,_adler_mod,_adler_border1, adler,pdata,n)
uint32_t fast_adler32_append(uint32_t adler,const adler_data_t* pdata,size_t n)
    _adler_append(uint32_t,16,_fast_adler32_BASE,_fast_adler_mod,_fast_adler_border1, adler,pdata,n)

uint32_t adler32_roll(uint32_t adler,size_t blockSize,adler_data_t out_data,adler_data_t in_data)
    _adler_roll(uint32_t,16,_adler32_BASE,_adler_mod,_adler_border2,
                adler_roll_kBestBlockSize,adler,blockSize, out_data,in_data)

uint32_t adler32_by_combine(uint32_t adler_left,uint32_t adler_right,size_t len_right)
    _adler_by_combine(uint32_t,16,_adler32_BASE,_adler_mod,_adler_border2,_adler_border3,
                      adler_left,adler_right,len_right)
uint32_t fast_adler32_by_combine(uint32_t adler_left,uint32_t adler_right,size_t len_right)
    _adler_by_combine(uint32_t,16,_fast_adler32_BASE,_fast_adler_mod,_fast_adler_border2,_fast_adler_border3,
                      adler_left,adler_right,len_right)

#ifdef _IS_NEED_ADLER64
uint64_t adler64_append(uint64_t adler,const adler_data_t* pdata,size_t n)
    _adler_append(uint64_t,32,_adler64_BASE,_adler_mod,_adler_border1, adler,pdata,n)
uint64_t fast_adler64_append(uint64_t adler,const adler_data_t* pdata,size_t n)
    _adler_append(uint64_t,32,_fast_adler64_BASE,_fast_adler_mod,_fast_adler_border1, adler,pdata,n)

uint64_t adler64_roll(uint64_t adler,uint64_t blockSize,adler_data_t out_data,adler_data_t in_data)
    _adler_roll(uint64_t,32,_adler64_BASE,_adler_mod,_adler_border2,
                adler64_roll_kBestBlockSize,adler,blockSize, out_data,in_data)

uint64_t adler64_by_combine(uint64_t adler_left,uint64_t adler_right,uint64_t len_right)
    _adler_by_combine(uint64_t,32,_adler64_BASE,_adler_mod,_adler_border2,_adler_border3,
                      adler_left,adler_right,len_right)
uint64_t fast_adler64_by_combine(uint64_t adler_left,uint64_t adler_right,uint64_t len_right)
    _adler_by_combine(uint64_t,32,_fast_adler64_BASE,_fast_adler_mod,_fast_adler_border2,_fast_adler_border3,
                      adler_left,adler_right,len_right)

#endif //_IS_NEED_ADLER64

