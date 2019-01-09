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

#define MAX_DATA    255

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

#   define _adler64_BASE                0xFFFFFFFBull
#   define _fast_adler64_BASE           ((uint64_t)1<<32)

 static const uint32_t __CRC32_Table[256] ={
     0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,
     0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,
     0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
     0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
     0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,
     0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
     0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,
     0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,
     0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
     0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,
     0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,
     0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
     0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,
     0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
     0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
     0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
     0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,
     0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
     0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,
     0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
     0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
     0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,
     0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,
     0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
     0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,
     0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
     0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
     0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
     0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,
     0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
     0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,0x54DE5729,0x23D967BF,
     0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D
 };
const uint32_t* _private_fast_adler_table =__CRC32_Table;

#define fast_adler_add1(adler,sum,byteData){ \
    (adler) += _private_fast_adler_table[(unsigned char)(byteData)]; \
    (sum)   += (adler);    \
}

#define adler_add1(adler,sum,byteData){ \
    (adler) += (byteData); \
    (sum)   += (adler);    \
}

#define _adler_add8(adler,sum,pdata){ \
    adler_add1(adler,sum,pdata[0]); \
    adler_add1(adler,sum,pdata[1]); \
    adler_add1(adler,sum,pdata[2]); \
    adler_add1(adler,sum,pdata[3]); \
    adler_add1(adler,sum,pdata[4]); \
    adler_add1(adler,sum,pdata[5]); \
    adler_add1(adler,sum,pdata[6]); \
    adler_add1(adler,sum,pdata[7]); \
}

//limit: 255*n*(n+1)/2 + (n+1)(65521-1) <= 2^32-1
// => max(n)=5552
//limit: 255*255*n*(n+1)/2 + (n+1)(0xFFFFFFFB-1) <= 2^64-1
// => max(n) >>> 5552
#define kFNBest 5552  // ==(8*2*347)
#define _adler_append(uint_t,half_bit,BASE,mod,border1, adler,pdata,n){ \
    uint_t sum=adler>>half_bit;      \
    adler&=(((uint_t)1<<half_bit)-1);\
_case8:        \
    switch(n){ \
        case  8: { adler_add1(adler,sum,*pdata++); } \
        case  7: { adler_add1(adler,sum,*pdata++); } \
        case  6: { adler_add1(adler,sum,*pdata++); } \
        case  5: { adler_add1(adler,sum,*pdata++); } \
        case  4: { adler_add1(adler,sum,*pdata++); } \
        case  3: { adler_add1(adler,sum,*pdata++); } \
        case  2: { adler_add1(adler,sum,*pdata++); } \
        case  1: { adler_add1(adler,sum,*pdata++); } \
        case  0: {  sum  =mod(sum,BASE);             \
                    border1(adler,BASE);             \
                    return adler | (sum<<half_bit); }\
        default: { /* continue */ }                  \
    } \
    while(n>=kFNBest){  \
        n-=kFNBest;     \
        size_t fn=(kFNBest>>3);\
        do{ \
            _adler_add8(adler,sum,pdata);\
            pdata+=8;   \
        }while(--fn);   \
        sum  =mod(sum,BASE);   \
        adler=mod(adler,BASE); \
    }       \
    if (n>8) {         \
        do{ \
            _adler_add8(adler,sum,pdata);\
            pdata+=8;   \
            n-=8;       \
        } while(n>=8);  \
        adler=mod(adler,BASE); \
    }       \
    goto _case8; \
}

#define _fast_adler_append(uint_t,half_bit,BASE,mod, _adler,pdata,n){ \
    size_t sum  =(size_t)((_adler)>>half_bit);                \
    size_t adler=(size_t)((_adler)&(((uint_t)1<<half_bit)-1));\
_case8:  \
    switch(n){ \
        case  8: { fast_adler_add1(adler,sum,*pdata++); } \
        case  7: { fast_adler_add1(adler,sum,*pdata++); } \
        case  6: { fast_adler_add1(adler,sum,*pdata++); } \
        case  5: { fast_adler_add1(adler,sum,*pdata++); } \
        case  4: { fast_adler_add1(adler,sum,*pdata++); } \
        case  3: { fast_adler_add1(adler,sum,*pdata++); } \
        case  2: { fast_adler_add1(adler,sum,*pdata++); } \
        case  1: { fast_adler_add1(adler,sum,*pdata++); } \
        case  0: { return mod(adler,BASE) | (((uint_t)sum)<<half_bit); } \
        default:{ /* continue */} \
    }   \
    do{ \
        fast_adler_add1(adler,sum,pdata[0]); \
        fast_adler_add1(adler,sum,pdata[1]); \
        fast_adler_add1(adler,sum,pdata[2]); \
        fast_adler_add1(adler,sum,pdata[3]); \
        fast_adler_add1(adler,sum,pdata[4]); \
        fast_adler_add1(adler,sum,pdata[5]); \
        fast_adler_add1(adler,sum,pdata[6]); \
        fast_adler_add1(adler,sum,pdata[7]); \
        pdata+=8;  \
        n-=8;      \
    } while(n>=8); \
    goto _case8;   \
}

#define  _adler_roll(uint_t,half_bit,BASE,mod,border2,kBestBlockSize,\
                     adler,blockSize,out_data,in_data){ \
    uint_t sum=adler>>half_bit;       \
    adler&=(((uint_t)1<<half_bit)-1); \
    /*  [0..B-1] + [0..255] + B - [0..255]   =>  [0+0+B-255..B-1+255+B-0]*/ \
    adler+=in_data+(uint_t)(BASE-out_data);  /* => [B-255..B*2-1+255] */    \
    border2(adler,BASE);  \
    /* [0..B-1] + [0..B-1] + B-1 - [0..B-1] => [(B-1)-(B-1)..B-1+B-1+B-1]*/ \
    blockSize=(blockSize<=kBestBlockSize)?blockSize:mod(blockSize,BASE);    \
    sum=sum+adler+(uint_t)((BASE-ADLER_INITIAL) - mod(blockSize*out_data,BASE)); \
    border2(sum,BASE);    \
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
// => max(blockSize)=(2^32-1)/255   ( =16843009 =(1<<24)+65793 )
static const size_t   adler_roll_kBestBlockSize=((size_t)(~(size_t)0))/MAX_DATA;
static const uint64_t adler64_roll_kBestBlockSize=((uint64_t)(~(uint64_t)0))/MAX_DATA;

uint32_t adler32_append(uint32_t adler,const adler_data_t* pdata,size_t n)
    _adler_append(uint32_t,16,_adler32_BASE,_adler_mod,_adler_border1, adler,pdata,n)
uint32_t fast_adler32_append(uint32_t _adler,const adler_data_t* pdata,size_t n)
    _fast_adler_append(uint32_t,16,_fast_adler32_BASE,_fast_adler_mod, _adler,pdata,n)

uint32_t adler32_roll(uint32_t adler,size_t blockSize,adler_data_t out_data,adler_data_t in_data)
    _adler_roll(uint32_t,16,_adler32_BASE,_adler_mod,_adler_border2,
                adler_roll_kBestBlockSize,adler,blockSize, out_data,in_data)

uint32_t adler32_by_combine(uint32_t adler_left,uint32_t adler_right,size_t len_right)
    _adler_by_combine(uint32_t,16,_adler32_BASE,_adler_mod,_adler_border2,_adler_border3,
                      adler_left,adler_right,len_right)
uint32_t fast_adler32_by_combine(uint32_t adler_left,uint32_t adler_right,size_t len_right)
    _adler_by_combine(uint32_t,16,_fast_adler32_BASE,_fast_adler_mod,_fast_adler_border2,_fast_adler_border3,
                      adler_left,adler_right,len_right)

uint64_t adler64_append(uint64_t adler,const adler_data_t* pdata,size_t n)
    _adler_append(uint64_t,32,_adler64_BASE,_adler_mod,_adler_border1, adler,pdata,n)
uint64_t fast_adler64_append(uint64_t _adler,const adler_data_t* pdata,size_t n)
    _fast_adler_append(uint64_t,32,_fast_adler64_BASE,_fast_adler_mod, _adler,pdata,n)

uint64_t adler64_roll(uint64_t adler,uint64_t blockSize,adler_data_t out_data,adler_data_t in_data)
    _adler_roll(uint64_t,32,_adler64_BASE,_adler_mod,_adler_border2,
                adler64_roll_kBestBlockSize,adler,blockSize, out_data,in_data)

uint64_t adler64_by_combine(uint64_t adler_left,uint64_t adler_right,uint64_t len_right)
    _adler_by_combine(uint64_t,32,_adler64_BASE,_adler_mod,_adler_border2,_adler_border3,
                      adler_left,adler_right,len_right)
uint64_t fast_adler64_by_combine(uint64_t adler_left,uint64_t adler_right,uint64_t len_right)
    _adler_by_combine(uint64_t,32,_fast_adler64_BASE,_fast_adler_mod,_fast_adler_border2,_fast_adler_border3,
                      adler_left,adler_right,len_right)


