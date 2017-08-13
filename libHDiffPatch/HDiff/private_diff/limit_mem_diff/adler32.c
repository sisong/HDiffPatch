//adler32.c
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

#include "adler32.h"
#include <assert.h>

//#define _adler32_FAST_BASE

#ifdef _adler32_FAST_BASE
#   define _adler32_BASE (1<<16)
#   define _adler32_mod(v)       ((uint32_t)((v)&(_adler32_BASE-1)))
#   define _adler32_to_border(v) { (v)=_adler32_mod(v); }
#else
#   define _adler32_BASE 65521
//# define _adler32_to_border(v) { if ((v) >= _adler32_BASE) (v) -= _adler32_BASE; }
#   define _adler32_to_border(v) { (v) -= _adler32_BASE & (uint32_t)( ((_adler32_BASE-1)-(int32_t)(v))>>31 ); }
#   define _adler32_mod(v)       ((v)%_adler32_BASE)
#endif


#define _adler32_add1(adler,sum,byteData){ \
    (adler) += (byteData); \
    (sum)   += (adler);    \
}
#define _adler32_add4(adler,sum,pdata,i){ \
    _adler32_add1(adler,sum,pdata[i  ]); \
    _adler32_add1(adler,sum,pdata[i+1]); \
    _adler32_add1(adler,sum,pdata[i+2]); \
    _adler32_add1(adler,sum,pdata[i+3]); \
}

uint32_t adler32_append(uint32_t adler,unsigned char* pdata,size_t n){
    uint32_t sum=adler>>16;
    adler&=0xFFFF;
    while (n>=16) {
        _adler32_add4(adler,sum,pdata,0);
        _adler32_add4(adler,sum,pdata,4);
        _adler32_add4(adler,sum,pdata,8);
        _adler32_add4(adler,sum,pdata,12);
        sum=_adler32_mod(sum);
        _adler32_to_border(adler);
        pdata+=16;
        n-=16;
    }
    while (n>0) {
        adler += (*pdata++);
        _adler32_to_border(adler);
        sum+=adler;
        _adler32_to_border(sum);
        --n;
    }
    return adler | (sum<<16);
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
// assert(adler32_roll_kMaxBlockSize > (1<<24))
const uint32_t adler32_roll_kMaxBlockSize=(0xFFFFFFFF-3*(_adler32_BASE-1))/255;

uint32_t adler32_roll_kBlockSizeBM(uint32_t blockSize){
    //limit: sum + adler + kBlockSizeBM >= blockSize*out
    //  [0..B-1] + [0..B-1] + m*B >= blockSize*[0..255]
    // => 0 + 0 + m*B>=blockSize*255
    // => min(m)=(blockSize*255+(B-1))/B
    // => blockSizeBM=B*min(m)
    assert(blockSize>0);
    assert(blockSize<=adler32_roll_kMaxBlockSize);
    uint32_t min_m=(blockSize*255+(_adler32_BASE-1))/_adler32_BASE;
    uint32_t blockSizeBM=_adler32_BASE*min_m;
    return blockSizeBM;
}

uint32_t adler32_roll_step(uint32_t adler,uint32_t blockSize,uint32_t kBlockSizeBM,
                           unsigned char out_data,unsigned char in_data){
    uint32_t sum=adler>>16;
#ifdef _adler32_FAST_BASE
    adler =_adler32_mod(adler + in_data - out_data);
    sum += adler - blockSize*out_data;
#else
    adler&=0xFFFF;
    //    [0..B-1] + [0..255] + B - [0..255]          =>  [0+0+B-255..B-1+255+B-0]
    adler += in_data +(uint32_t)(_adler32_BASE - out_data);// => [B-255..B*2-1+255]
    _adler32_to_border(adler); // [0..B-1+255]
    _adler32_to_border(adler); // [0..B-1]
    //sum + adler + kBlockSizeBM >= blockSize*out => in adler32_roll_blockSizeBM()
    sum = _adler32_mod(sum + adler + kBlockSizeBM - blockSize*out_data);
#endif
    return adler | (sum<<16);
}

