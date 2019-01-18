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
    0xbf9edfcc,0xe71099c9,0xd67b67b2,0x8e33ad74,0xcab96776,0x98d54df3,0x3a522660,0xd93ba681,
    0x922cd076,0x0434c3bb,0x41d89626,0x0f8b0527,0x5d8e40bc,0x2d8342f8,0x0ff88f81,0xdf6ec295,
    0x79a1a333,0x8a602ebc,0xde7a3679,0xb52ae8ec,0xecb3f589,0x4004dfa1,0xbb2e5547,0x07473800,
    0xfc2b0169,0x8b9ce25b,0xcfe52186,0xbbe4786d,0xbf0b546c,0xd46fdd11,0xe7a88a18,0x64ed578b,
    0x958f8a17,0x3847b674,0x8bce9767,0x161ed88f,0x28ce3d46,0x395267d9,0x24771b78,0xce32aade,
    0x9d054cba,0x379a0d69,0x84b6bf76,0x6ee806e4,0x00a4675d,0x7f8b6971,0xc101d352,0xf1f78394,
    0x67a41f30,0xd1bc81f9,0xc8c31a03,0x8edacdfe,0xb97d904e,0x8b679198,0x90656f59,0x7381959a,
    0xa46bddc6,0x749aab74,0xe48d655d,0xaf19252e,0xa44982e3,0xd9563c6a,0xb2ecb406,0x47c270ba,
    0xa6c67350,0x7060c467,0x40088e0c,0x6cb0a483,0x377bb8e8,0xb7480fd9,0xdf0411dd,0xb2866151,
    0x1a124a77,0x49cd70fb,0xb57435c8,0xe9d6c177,0x5a5463c2,0x24fe320a,0x039837c6,0x2a2da939,
    0xdc178313,0x2d5ffda5,0x0cad7f10,0x350aa19f,0xe1634abd,0x435d50df,0xc8ac72b4,0x5043566d,
    0xb1a82ab8,0xf0af7fa4,0x8ce648e5,0x29742544,0x25c17d69,0x85a9b097,0x2b6b074a,0x1dc724ae,
    0x25573426,0xf2eded58,0xb4141d03,0xde755706,0xa4978cb8,0x6c0ad071,0x042782f7,0xea3d0b55,
    0xf06315f8,0x76e182a0,0x5ace1199,0xb0062216,0x4ff0e8fa,0x8c2627ed,0x93cd3f12,0x8983f31a,
    0x2f30129f,0x70e0afc7,0x97802de1,0xe4b928ef,0x1381adee,0x0279cd1b,0x94e91b47,0xd0d16b41,
    0x1495c2ac,0x89b4542e,0xd693eef6,0xb3bfdbac,0x717cb1d7,0xfcd75c40,0xea6dd303,0x11abdbeb,
    0x666fcc43,0x0252ab2d,0x9538517e,0xd0d3df1e,0xd33b9f3b,0x3b6e78f9,0x30558eb3,0xc264c108,
    0x82b85d6f,0xf09f81e1,0x2b84c0b8,0x893a0808,0x8dab3e93,0xb20e5e46,0xb2ccf3c3,0xa3856f7f,
    0xab7fdc0a,0x3607204a,0x977ebb5a,0x6f8cdfe9,0x389a4862,0x9360e584,0x58cceb12,0x4391b9ee,
    0xf53cba75,0xac930368,0x864790b5,0x73490171,0x3c4dbd13,0x24f87189,0xded8ceea,0xe10299be,
    0x7583eecd,0x6beba44e,0xbafda0ab,0xf8b14048,0xde86b90d,0x242b1611,0xe6680c5a,0x9306681f,
    0xfe1572d1,0x96ecc407,0xbf8eb719,0xdeb7ba8c,0x2057fa31,0x8d57c8d0,0x74bf821c,0xf913c6de,
    0x5f74d512,0xb08a95d5,0x8310286e,0xa65b65a8,0x3d76ddbb,0x9d6f47dd,0x3f2886a4,0x78945620,
    0x1e52434a,0xdf3dbd14,0xeb43a9be,0xbe2f6975,0xd69bdc25,0x65126eb6,0x55485dcb,0xa10f2a31,
    0x1023abec,0xcfe827f7,0x6268b539,0x465ca65c,0xd92c6b06,0x7b77c93b,0x126aed81,0x84a8c4b9,
    0x8f9a6e31,0xef8965a7,0xd491b2a9,0x15a3b105,0xe9bb3886,0x41eefa4f,0x7099a0fa,0x934c91f9,
    0x22074c8e,0xb13688d9,0x137a69f7,0x6cdf4a38,0x42392f7c,0x3f0cf232,0x88dbeec7,0xb865136a,
    0x6860dfb0,0x1591bb4f,0x7f1ec2e5,0xf77a6a41,0xb9ff1899,0x649e5781,0xc9fb2bc2,0x274d4789,
    0x9cb45863,0x353992f0,0x53aee797,0xc3f00258,0xe8afd500,0x74cb3772,0xa3c2c169,0x26cacacf,
    0xc6a619cd,0x41d96cd7,0x96230945,0xd7fa1a08,0x88643416,0x9673eed5,0xa4a21dc1,0x7bebba07,
    0xdd96be2c,0xc3e0479a,0x60db52ea,0xede0b66f,0x7150d241,0x4e978ebc,0xbf668b79,0x7a053a88,
    0xd9044944,0x517342c7,0xa73d5da8,0xdcdf50ea,0xc26b1a4a,0x911e03ac,0xc7ec2250,0xf8c5b8c2 };

/*
#include <set>
//gen table better than CRC32Table:{0,0x77073096,..,0x2D02EF8D}
static bool __gen_fast_adler_table(unsigned int rand_seed,bool isPrint){
    std::set<uint32_t> check_set;
    if (isPrint) printf("{\n");
    for (int y=32-1; y>=0; --y) {
        if (isPrint) printf("    ");
        for (int x=8-1; x>=0; --x) {
            uint32_t v=0;
            v=(v<<8)|(unsigned char)rand_r(&rand_seed);
            v=(v<<8)|(unsigned char)rand_r(&rand_seed);
            v=(v<<8)|(unsigned char)rand_r(&rand_seed);
            v=(v<<8)|(unsigned char)rand_r(&rand_seed);
            {//check
                if (check_set.find(v&0xFFFF)!=check_set.end()) return false;
                check_set.insert(v&0xFFFF);
                if (check_set.find(v>>16)!=check_set.end()) return false;
                check_set.insert(v>>16);
            }
            if (isPrint) printf("0x%08x",v);
            if (x>0) { if (isPrint) printf(","); }
        }
        if (isPrint) printf((y>0)?",\n":" };\n\n");
    }
    return true;
}
static bool _gen_fast_adler_table(){
    bool result=false;
    for (unsigned int i=0; i<100;++i) {
        if (!__gen_fast_adler_table(i,false)) continue;
        printf("rand_seed: %u\n",i);
        result=__gen_fast_adler_table(i,true);
    }
    return result;
}
static bool _null=__gen_fast_adler_table(41,true);
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


