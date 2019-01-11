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

static const uint32_t __fast_adler_table[256]=/*create by _gen_fast_adler_table*/{
        0xdf7aa77e,0x960998f4,0x61bd5b01,0xf3f24b83,0xd5995bf4,0xaa9f9a35,0x1fa1c38b,0xcb456928,
        0x8acb784b,0x907b4310,0x59b85f0e,0x5a6971aa,0x14e0078d,0xa67ab7b3,0x805c329f,0x821e118a,
        0x39f17b30,0xecbdde10,0x75bd05aa,0xb728af33,0x72ef55d3,0x8490a70a,0x3bd1cedd,0xdcf62525,
        0x6ff0385b,0xd96df9d5,0xaa13e8e5,0xead43975,0x8d0fe011,0xf88c68d,0x64ff4a0b,0x3d61944,
        0xcf44ad62,0xf3b2cac8,0x9bc9a4ac,0x431f876a,0x8013954,0x5182d87a,0x40905084,0xaa1d7605,
        0x35b6a073,0x76be7fe5,0xa6e97b19,0xba38419,0x38c06eef,0x545d4ff3,0xccdabe95,0x95b7484,
        0x742ae762,0x724892df,0x32cdba64,0xf3f1dbcd,0x15d05ed2,0x5c10b521,0x772f3739,0x76f02798,
        0xad7b8851,0xeddd8f58,0x1289f79d,0xc82bfbfd,0x875a9389,0x6e8f9ce5,0x7e783446,0x2e0b058c,
        0x31510c75,0xf966a1e4,0x714c785,0x4984e5c5,0x2b72223f,0x9e2f4110,0xd984cb1d,0xaaccdd24,
        0xc7ac1052,0x9658917,0xe1fffaf,0x33beeca3,0x58c11c17,0xebbb959,0x5185ec70,0x8648da5d,
        0x62e6cb29,0x290ed3f2,0x1b1fd7e4,0x88fcd293,0x1860de55,0x2e08df88,0x51ce64d6,0xe5152517,
        0x8495ca73,0x3b844db3,0x12269f70,0xb777bd85,0xbdc6477b,0xeb896b21,0x541df5b2,0x5d51fb96,
        0x5f7724c3,0x6dd03400,0x46944b32,0x5c834b3f,0xf8c35641,0x9a21d625,0xeebd4293,0x3964c3f3,
        0x2ebefe6e,0xf412742f,0x4cc9489e,0x1a142deb,0xadd096ba,0x50851bbd,0x1d5aa61,0x88bf54b4,
        0x43cbf5b,0x1ea1840c,0x78b8c5d3,0x6afd60d6,0x1d934578,0x3f52aa69,0x14a3a879,0x2d1bdf46,
        0x30b6a189,0xd9fb76a6,0xcbfa9ede,0x9e2a424b,0x6a145f22,0x88cbf559,0xd5881b5e,0x1095172d,
        0xf79bcfe3,0xaf9bbbcb,0x26c7f8fc,0x15350f3,0xa583b8b5,0xaf503911,0x6e515d0e,0x1160c6dc,
        0x544d465d,0x3486862b,0x9fa6ad6d,0x172cfb21,0x4a5dcf8a,0xee478515,0xbd90acec,0x7bb9ecd1,
        0x6486ccca,0xae7132e4,0x785a42bc,0x69f93d7e,0x8fca4e47,0xbcf52c10,0x776b4699,0x21003b46,
        0x4e19e43f,0x9c6b2c3c,0xbf80a266,0x57091370,0x4bccc5a0,0xaa8d6af8,0x30cbee6b,0xe606efb9,
        0xb54e7fcf,0x7591d25c,0x35ffa606,0x1746bca5,0xadc667b1,0x3316fe5b,0x2642d7cf,0x71e2eb57,
        0x1262edd0,0x4033cd84,0xcc51790b,0x8efcfdf9,0xa9062acc,0xcd451f5f,0x5e2a9f4a,0xe5fe76ec,
        0x6c25f042,0x396933c4,0xb82f602a,0xbaa9b2b4,0x3f7dccac,0x28f95a8e,0xb0b1557b,0x656b00af,
        0x2bd6f7,0xe620689a,0x1afc2562,0x367a4977,0x6d7fb346,0x356ec060,0xc7c484e7,0xb2e460d8,
        0xa658893a,0x40e1c4d8,0x66d8e91f,0x41972497,0xa806e58,0xec21f19,0xd88ed0c5,0xcb8212b8,
        0x96e9f268,0x535699cd,0xe3138457,0x1da6273f,0x71e49b04,0xaa916fcc,0x2b8bfd05,0x1d2d4a0,
        0xd8b2f1f,0xe4a5c23,0x3097e568,0x7518fd18,0xe64d2b1,0xa373c44f,0xf8cc6057,0x606729c6,
        0xe4bdc0a3,0x8e328068,0x3b571813,0x48273cf5,0x6d0c2a34,0x2fbee154,0xcea1b256,0x47fe9294,
        0x471b2cb8,0x69afbc8c,0x4a394b2d,0xffddc2db,0xbc5eea38,0x4b90f35d,0x62b47504,0xed48ad6,
        0xb53d0bc2,0xb0421110,0x27972702,0xf088ff1a,0xad5a8e0b,0x78000267,0x300470df,0x4d78226b,
        0xd95248d1,0x312a475a,0xe355d339,0xa469e3b4,0x769a1b4c,0x434618d1,0x23d7aeb0,0xfbe20cad,
        0x7a19b853,0x37b54c00,0x698a59e0,0xbff80bcf,0x6f17ff55,0x64a2eea9,0xe7b7ee82,0xc6446dc6 };
/*
#include <set>
static int _gen_fast_adler_table(){ // better than CRC32Table:{0,0x77073096,..,0x2D02EF8D}
    std::set<uint32_t> check_set;
    unsigned int rand_seed=9;
    printf("{\n");
    for (int y=32-1; y>=0; --y) {
        printf("    ");
        for (int x=8-1; x>=0; --x) {
            uint32_t v=0;
            v=(v<<8)|(unsigned char)rand_r(&rand_seed);
            v=(v<<8)|(unsigned char)rand_r(&rand_seed);
            v=(v<<8)|(unsigned char)rand_r(&rand_seed);
            v=(v<<8)|(unsigned char)rand_r(&rand_seed);
            {//check
                if (check_set.find(v&0xFFFF)!=check_set.end()) throw v;
                check_set.insert(v&0xFFFF);
                if (check_set.find(v>>16)!=check_set.end()) throw v;
                check_set.insert(v>>16);
            }
            printf("0x%x",v);
            if (x>0) printf(",");
        }
        printf((y>0)?",\n":" };\n");
    }
    return 0;
}
static int _null=_gen_fast_adler_table();
*/
const uint16_t* _private_fast_adler32_table =(const uint16_t*)__fast_adler_table;
const uint32_t* _private_fast_adler64_table =(const uint32_t*)__fast_adler_table;

#define fast_adler_add1(_table, adler,sum,byteData){ \
    (adler) += _table[(unsigned char)(byteData)];    \
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
        size_t fn;      \
        n-=kFNBest;     \
        fn=(kFNBest>>3);\
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

#define _fast_adler_append(uint_t,half_bit,BASE,mod,_table, _adler,pdata,n){ \
    size_t sum  =(size_t)((_adler)>>half_bit);                \
    size_t adler=(size_t)((_adler)&(((uint_t)1<<half_bit)-1));\
_case8:  \
    switch(n){ \
        case  8: { fast_adler_add1(_table,adler,sum,*pdata++); } \
        case  7: { fast_adler_add1(_table,adler,sum,*pdata++); } \
        case  6: { fast_adler_add1(_table,adler,sum,*pdata++); } \
        case  5: { fast_adler_add1(_table,adler,sum,*pdata++); } \
        case  4: { fast_adler_add1(_table,adler,sum,*pdata++); } \
        case  3: { fast_adler_add1(_table,adler,sum,*pdata++); } \
        case  2: { fast_adler_add1(_table,adler,sum,*pdata++); } \
        case  1: { fast_adler_add1(_table,adler,sum,*pdata++); } \
        case  0: { return mod((uint_t)adler,BASE) | (((uint_t)sum)<<half_bit); } \
        default:{ /* continue */} \
    }   \
    do{ \
        fast_adler_add1(_table,adler,sum,pdata[0]); \
        fast_adler_add1(_table,adler,sum,pdata[1]); \
        fast_adler_add1(_table,adler,sum,pdata[2]); \
        fast_adler_add1(_table,adler,sum,pdata[3]); \
        fast_adler_add1(_table,adler,sum,pdata[4]); \
        fast_adler_add1(_table,adler,sum,pdata[5]); \
        fast_adler_add1(_table,adler,sum,pdata[6]); \
        fast_adler_add1(_table,adler,sum,pdata[7]); \
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
    _fast_adler_append(uint32_t,16,_fast_adler32_BASE,_fast_adler_mod,
                       _private_fast_adler64_table, _adler,pdata,n)

uint32_t adler32_roll(uint32_t adler,size_t blockSize,adler_data_t out_data,adler_data_t in_data)
    _adler_roll(uint32_t,16,_adler32_BASE,_adler_mod,_adler_border2,
                adler_roll_kBestBlockSize, adler,blockSize, out_data,in_data)

uint32_t adler32_by_combine(uint32_t adler_left,uint32_t adler_right,size_t len_right)
    _adler_by_combine(uint32_t,16,_adler32_BASE,_adler_mod,_adler_border2,_adler_border3,
                      adler_left,adler_right,len_right)
uint32_t fast_adler32_by_combine(uint32_t adler_left,uint32_t adler_right,size_t len_right)
    _adler_by_combine(uint32_t,16,_fast_adler32_BASE,_fast_adler_mod,_fast_adler_border2,
                      _fast_adler_border3, adler_left,adler_right,len_right)

uint64_t adler64_append(uint64_t adler,const adler_data_t* pdata,size_t n)
    _adler_append(uint64_t,32,_adler64_BASE,_adler_mod,_adler_border1, adler,pdata,n)
uint64_t fast_adler64_append(uint64_t _adler,const adler_data_t* pdata,size_t n)
    _fast_adler_append(uint64_t,32,_fast_adler64_BASE,_fast_adler_mod,
                       _private_fast_adler64_table, _adler,pdata,n)

uint64_t adler64_roll(uint64_t adler,uint64_t blockSize,adler_data_t out_data,adler_data_t in_data)
    _adler_roll(uint64_t,32,_adler64_BASE,_adler_mod,_adler_border2,
                adler64_roll_kBestBlockSize,adler,blockSize, out_data,in_data)

uint64_t adler64_by_combine(uint64_t adler_left,uint64_t adler_right,uint64_t len_right)
    _adler_by_combine(uint64_t,32,_adler64_BASE,_adler_mod,_adler_border2,_adler_border3,
                      adler_left,adler_right,len_right)
uint64_t fast_adler64_by_combine(uint64_t adler_left,uint64_t adler_right,uint64_t len_right)
    _adler_by_combine(uint64_t,32,_fast_adler64_BASE,_fast_adler_mod,_fast_adler_border2,
                      _fast_adler_border3, adler_left,adler_right,len_right)


