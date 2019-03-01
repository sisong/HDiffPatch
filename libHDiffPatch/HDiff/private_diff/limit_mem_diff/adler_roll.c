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

static const uint64_t __fast_adler_table[256]=/*create by _gen_fast_adler_table*/{
    0xe9834671b5e6a199ull,0x0914fc228c715a7full,0x9d31fb3962abb5afull,0xa3d3c03569f00344ull,
    0x399a5b7264db3b1cull,0x0c036c4f959bb01eull,0x4dc75b77344fbc39ull,0x0ab026b325be7198ull,
    0xe6a1281ebab2d5e6ull,0x53346d230eb13df7ull,0xb363ede943437647ull,0xdea89be55783a9ebull,
    0x7b58e5534a2e1c06ull,0xc1fafc95d3979639ull,0xb0db4075b5ec388cull,0x4cde0617c573fb3dull,
    0xd77edb524b86ee95ull,0x6527c8e8100140c5ull,0x3bb6faa98b8e01cbull,0xd11925ac9f77583eull,
    0x59380e038ae986f0ull,0xad9eb6013a9b560bull,0x36b3f8940f16648eull,0x14a50768fe25d3b7ull,
    0x06cd0894aaed25fdull,0x5833a3c9667abac1ull,0xd9674e5e92bfd5cfull,0xa36fa10b00b7f263ull,
    0xcb11edb612338782ull,0xd10e177fca15bc13ull,0x762153c14c60b16cull,0x6f8d3213059207f3ull,
    0xdba6f8b4ef298d69ull,0x409f815015056856ull,0xf15a0a043b7609d6ull,0x8ad571be53b2fcc9ull,
    0xe127ab04a40dcc34ull,0xfe33ae6bdd1a7d81ull,0x45791ae110e26f52ull,0xc52818409208538cull,
    0xa7b050a1444fee94ull,0xaa159ddd0e053633ull,0x70e88e7ac3c99a7bull,0x5627f1ae40f5c699ull,
    0x90550ccc7c216685ull,0x22ca3af35b565b88ull,0x856d11ad3f5c8372ull,0x70e5fc79d749378cull,
    0x5fbea93e0ad88005ull,0xb2108f9f9119cf5eull,0xd01b096bd069b0e6ull,0x9d2e92ef3bbbef86ull,
    0xd853a1a88b0b3b0bull,0x314507667773f1f2ull,0x935a1c0796cefd47ull,0x4d75b877b6f5245aull,
    0xf291709dc4e0ae38ull,0xeaf6717b5d4a7395ull,0x14ab729d353013a2ull,0x8d2f277620560099ull,
    0x4832d5cd7ce6ccb9ull,0x0cac0dd3d4519fa3ull,0x0360ed98008ba27dull,0x12f6b6ad6fb2f7c0ull,
    0x82441585694823ddull,0x09aa5ce12317f193ull,0x4a06eddb92dffce9ull,0x9561318180095313ull,
    0x2dd5ab173a4b5d4bull,0xd4297df4b4ad20beull,0x61036b5e881cb3c5ull,0x5ca6be400d271952ull,
    0xd1a392881c13ed9aull,0x76bd0c5785d5c7ffull,0x8ee9e2bc900595daull,0x77b940bfadeaeb12ull,
    0x1e1a09de6589001cull,0x3bd372be4c31dc7full,0x37279e1b86af13ebull,0xb9e4d0d09329fcc0ull,
    0x00800dd5d0d7f7b6ull,0xd9b49a97a9cd017full,0x107a9974c6b9318cull,0x1e030a540e3d65c5ull,
    0x298db8ca71a2ae9aull,0x7c7e43c4dd7e2ea6ull,0x5f35be6141d5b0d5ull,0x33b43e0cbc4a4198ull,
    0xd2cfb061a0e21f24ull,0x8c91798460963539ull,0x08f07c80c86fc710ull,0x1287cd98044d34fbull,
    0xa0aeab0bbebea104ull,0xd75dc20eb775e218ull,0x88cab942c79211cbull,0x69a032d119caa6e3ull,
    0x3658c36c3d9db18bull,0xaa2bd1e88deb277cull,0xebd02f0ca6ea28e4ull,0x0d6e86659de0ccd9ull,
    0x68189e89ca028b95ull,0xf4c654ee5b18e944ull,0x26d7a9ea03824598ull,0xc6b26613bbb5de59ull,
    0x5abd6d3388152a10ull,0x33a9fe838fb51b5cull,0x4cc280948b82720eull,0x5442aac86687f674ull,
    0x99f94f7d1369f26full,0xe3b5bb2549dd656eull,0xceab6332e9d7524aull,0x5e730186a8d417d5ull,
    0x38cda2e23ff82ff0ull,0xb1eb6f6f9da9e9f2ull,0xc3fe12ade2643584ull,0x18054b2e6a6621ceull,
    0x55c725b1880c75bbull,0x04747a86d6e226a9ull,0xde31e4b63ba442afull,0x90be6f0656b265b8ull,
    0xe6004b58d491b721ull,0xb13531527e737445ull,0xd9ae2437689bee09ull,0x761828fc1e4160bcull,
    0xdae6bc729e290dc5ull,0x5198300db64afb06ull,0x0800732342d6d3f1ull,0xa1606c218cf682e4ull,
    0x1838e333280f674eull,0xf32f3622c8b9929full,0x6faa5847b2a5e48bull,0x9bc9239301e76beaull,
    0x6af6da13021eaf55ull,0xb41a06eb9c2568f1ull,0x2d2b1e3bf2355c05ull,0x23392705f4be24f2ull,
    0x2fa3b21fdfb99b28ull,0x4c63f66993a13011ull,0x461b744d68e5f030ull,0x6b2c411820684fffull,
    0xfec175cf5e25963aull,0xb2b9843c5ab1cd22ull,0xd093bb54d4dd0d58ull,0xc2d82a015f47006eull,
    0xb8bda6068a424df4ull,0xc2da2f437df54d84ull,0x420ba10e8cfe1791ull,0x79bb721e68d9f0daull,
    0x19503b244e702ff8ull,0x580545e65e50ca27ull,0x9e87d8215f4982dbull,0x932033429b61963dull,
    0x5d0dad04736a072eull,0x99a6b1bf9450b7abull,0xa1ea44d7ad94fa9aull,0xd8a3f135f31c000bull,
    0xa5d03bd92fe1da41ull,0x8f9e79ca39ddfb4bull,0x6584ff87614bde5cull,0x792d069e7b4a784bull,
    0x778b05310f6d0f8dull,0xb5e5dbc7ea76bbb7ull,0x2218383ba0377ffaull,0x9f2f7c072313b362ull,
    0x9d1685de8a0964cfull,0x84682b60bcce4d80ull,0x81d3ec4549ae98deull,0x20e39c141f8119f9ull,
    0xfa621384bce8e620ull,0x55b71475a3fa41ddull,0x6e9edf35257df71full,0x189fd107e19ec182ull,
    0xaad8659b07716f2dull,0x0f3ac47780f8039cull,0xc7742a5a925db153ull,0x809b118230253df4ull,
    0xd82d45081d351f76ull,0x65701b3e7eabc10aull,0xbca8d9efa9708035ull,0x163061807a1e2829ull,
    0x8c6275a12ad461e8ull,0xfb2bc26167e58ce9ull,0x7b8556c19061dadeull,0x7e2bf8d6fb047568ull,
    0x66863c313613fa7cull,0x1564597faca39945ull,0xb58e9c23d4ad6bbfull,0xeba2bfdc1563e12dull,
    0xdfeed88c67234d07ull,0xce23a19b5a995781ull,0x0ea1fb79123c03cfull,0x67e694c0befec0bfull,
    0xee5d2bbedcb6ef2aull,0x8a5c162274a3ff1aull,0xd7f55f1f338fa292ull,0xdf2b314f6aa8c581ull,
    0x6767ff9557aa37bbull,0xbf3a8e66b9ada0c2ull,0x354522f82cdf9a5cull,0x44b1e3f9ff6ee3aaull,
    0xf5571375ca4067bcull,0x93dd20c8d9382754ull,0xc8cc026c489d6ba0ull,0xd8afbb930cf35324ull,
    0xf6b1e4d4c3cdbaafull,0x6a8db0a2af31b927ull,0xcd93545ebd2bfdd4ull,0xe988dc92a3076071ull,
    0x3d835895a5a99432ull,0x3e6db869fcb9f96dull,0x9324623048594b04ull,0x17aee587ded33cadull,
    0x9a4f7e29b3cf7654ull,0xb6e38f02a2db00a4ull,0x9a7f4482033c93b9ull,0xdbaf95093bad1701ull,
    0x25c5a2eae9fafbc4ull,0x778f84c6b2a945f0ull,0x220b78ed985ba402ull,0xa856b484ef3f2728ull,
    0x02ed74be46e39db2ull,0x4bba27d36b9a61cfull,0x3be8c7cef4403efdull,0xaa45492dda915d43ull,
    0xf26cd77c5e31fb24ull,0xdd194a89fc88f48eull,0x3a5eb8dbb47c0dceull,0x6d1828851df3e8c9ull,
    0xc47572193ec6ac80ull,0xd0f0c6fa90b43371ull,0x42746c648581fb76ull,0xd0b052bbd93f2866ull,
    0x20194469aa76e6aeull,0x0d4bff2c846cd9a0ull,0x58101dfee696b5f4ull,0xb09bb5eb0f6797ecull,
    0x179613f4316af61eull,0xf25438934a674e5dull,0x566e87958450c6bfull,0xe09e404ca452bcf5ull,
    0x10d604d7d5acf7d9ull,0x5f1ee02d80e62ec4ull,0x094272f2b03a3adcull,0x7c53663013c68fe6ull,
    0xdfc16f8c66e87e96ull,0x513669ffcc854cbcull,0x7b6e2e4729891f10ull,0x366d835f1e25842eull,
    0xb59420af3feb2f8aull,0xc880ce98aab484c7ull,0xf27daa46241716e1ull,0x167b40b03178b385ull,
    0xfa1ba16937e78784ull,0x21e6ec6ca0b50cf4ull,0xe984c351976999a1ull,0x84d3c2fbb222e66dull };
/*
#include <set>
//gen table better than CRC32Table:{0,0x77073096,..,0x2D02EF8D} or CRC64Table
static bool _gen_fast_adler_table(unsigned int rand_seed,bool isPrint=true){
    std::set<uint32_t> check_set;
    if (isPrint) printf("{\n");
    for (int y=64-1; y>=0; --y) {
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
            if (isPrint){
                if (x%2==1){
                    printf("0x%08x",v);
                }else{
                    printf("%08xull",v);
                    if (x>0) printf(",");
                }
            }
        }
        if (isPrint) printf((y>0)?",\n":" };\n\n");
    }
    return true;
}
static bool _gen_fast_adler_table(){
    bool result=false;
    for (unsigned int i=0; i<1000000000;++i) {
        if (!_gen_fast_adler_table(i,false)) continue;
        printf("rand_seed: %u\n",i);
        result=_gen_fast_adler_table(i,true);
    }
    assert(result);
    return result;
}
static bool _null=_gen_fast_adler_table(10735);
*/
const uint16_t* _private_fast_adler32_table =(const uint16_t*)&__fast_adler_table[0];
const uint32_t* _private_fast_adler64_table =(const uint32_t*)&__fast_adler_table[0];
const uint64_t* _private_fast_adler128_table =(const uint64_t*)&__fast_adler_table[0];

#define fast_adler_add1(_table, adler,sum,pdata){ \
    (adler) += _table[(unsigned char)(*pdata++)]; \
    (sum)   += (adler);    \
}

#define fast_adler_add2(_c_t,_table,adler,sum,pdata){ \
    _c_t a0=_table[(unsigned char)*pdata++]; \
    _c_t a1=_table[(unsigned char)*pdata++]; \
    a0+=adler;      \
    adler=a0+a1;    \
    sum+=a0*2+a1;   \
}

#define adler_add1(adler,sum,pdata){ \
    (adler) += *pdata++; \
    (sum)   += (adler);    \
}

#define adler_add2(_c_t,adler,sum,pdata){ \
    _c_t a0=*pdata++; \
    _c_t a1=*pdata++; \
    a0+=adler;      \
    adler=a0+a1;    \
    sum+=a0*2+a1;   \
}

#define _adler_add8(_c_t,adler,sum,pdata){  \
    adler_add2(_c_t,adler,sum,pdata);    \
    adler_add2(_c_t,adler,sum,pdata);    \
    adler_add2(_c_t,adler,sum,pdata);    \
    adler_add2(_c_t,adler,sum,pdata);    \
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
        case  8: { adler_add1(adler,sum,pdata); } \
        case  7: { adler_add1(adler,sum,pdata); } \
        case  6: { adler_add1(adler,sum,pdata); } \
        case  5: { adler_add1(adler,sum,pdata); } \
        case  4: { adler_add1(adler,sum,pdata); } \
        case  3: { adler_add1(adler,sum,pdata); } \
        case  2: { adler_add1(adler,sum,pdata); } \
        case  1: { adler_add1(adler,sum,pdata); } \
        case  0: {  sum  =mod(sum,BASE);          \
                    border1(adler,BASE);          \
                    return adler | (sum<<half_bit); } \
        default: { /* continue */ } \
    } \
    while(n>=kFNBest){  \
        size_t fn;      \
        n-=kFNBest;     \
        fn=(kFNBest>>3);\
        do{ \
            _adler_add8(uint_t,adler,sum,pdata);\
        }while(--fn);   \
        sum  =mod(sum,BASE);   \
        adler=mod(adler,BASE); \
    }       \
    if (n>8) {         \
        do{ \
            _adler_add8(uint_t,adler,sum,pdata);\
            n-=8;       \
        } while(n>=8);  \
        adler=mod(adler,BASE); \
    }       \
    goto _case8; \
}


#define _fast_adler_append(SUM,ADLER,return_SUMADLER,_c_t,_table, _adler,pdata,n){ \
    _c_t sum  =(_c_t)SUM(_adler);   \
    _c_t adler=(_c_t)ADLER(_adler); \
_case8:  \
    switch(n){ \
        case  8: { fast_adler_add1(_table,adler,sum,pdata); } \
        case  7: { fast_adler_add1(_table,adler,sum,pdata); } \
        case  6: { fast_adler_add1(_table,adler,sum,pdata); } \
        case  5: { fast_adler_add1(_table,adler,sum,pdata); } \
        case  4: { fast_adler_add1(_table,adler,sum,pdata); } \
        case  3: { fast_adler_add1(_table,adler,sum,pdata); } \
        case  2: { fast_adler_add1(_table,adler,sum,pdata); } \
        case  1: { fast_adler_add1(_table,adler,sum,pdata); } \
        case  0: {  return_SUMADLER(sum,adler); }             \
        default:{ /* continue */} \
    }   \
    do{ \
        fast_adler_add2(_c_t,_table,adler,sum,pdata); \
        fast_adler_add2(_c_t,_table,adler,sum,pdata); \
        fast_adler_add2(_c_t,_table,adler,sum,pdata); \
        fast_adler_add2(_c_t,_table,adler,sum,pdata); \
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

#define  _fast_adler_by_combine(SUM,ADLER,return_SUMADLER,_c_t, \
                                adler_left,adler_right,len_right){ \
    _c_t rem  = (_c_t)len_right;         \
    _c_t sum  = rem * (_c_t)ADLER(adler_left); \
    _c_t adler= (_c_t)ADLER(adler_left)  \
              + (_c_t)ADLER(adler_right) \
              - ADLER_INITIAL;   \
    sum += (_c_t)SUM(adler_left)+(_c_t)SUM(adler_right) \
           - rem*ADLER_INITIAL;  \
    return_SUMADLER(sum,adler);  \
}


//limit: if all result in uint32_t
//blockSize*255 <= 2^32-1
// => max(blockSize)=(2^32-1)/255   ( =16843009 =(1<<24)+65793 )
static const size_t   adler_roll_kBestBlockSize=((size_t)(~(size_t)0))/MAX_DATA;
static const uint64_t adler64_roll_kBestBlockSize=((uint64_t)(~(uint64_t)0))/MAX_DATA;

uint32_t adler32_append(uint32_t adler,const adler_data_t* pdata,size_t n)
    _adler_append(uint32_t,16,_adler32_BASE,_adler_mod,_adler_border1, adler,pdata,n)
uint32_t adler32_roll(uint32_t adler,size_t blockSize,adler_data_t out_data,adler_data_t in_data)
    _adler_roll(uint32_t,16,_adler32_BASE,_adler_mod,_adler_border2,
                adler_roll_kBestBlockSize, adler,blockSize, out_data,in_data)
uint32_t adler32_by_combine(uint32_t adler_left,uint32_t adler_right,size_t len_right)
    _adler_by_combine(uint32_t,16,_adler32_BASE,_adler_mod,_adler_border2,_adler_border3,
                      adler_left,adler_right,len_right)

uint64_t adler64_append(uint64_t adler,const adler_data_t* pdata,size_t n)
    _adler_append(uint64_t,32,_adler64_BASE,_adler_mod,_adler_border1, adler,pdata,n)
uint64_t adler64_roll(uint64_t adler,uint64_t blockSize,adler_data_t out_data,adler_data_t in_data)
    _adler_roll(uint64_t,32,_adler64_BASE,_adler_mod,_adler_border2,
                adler64_roll_kBestBlockSize,adler,blockSize, out_data,in_data)
uint64_t adler64_by_combine(uint64_t adler_left,uint64_t adler_right,uint64_t len_right)
    _adler_by_combine(uint64_t,32,_adler64_BASE,_adler_mod,_adler_border2,_adler_border3,
                      adler_left,adler_right,len_right)

uint32_t fast_adler32_append(uint32_t _adler,const adler_data_t* pdata,size_t n)
    _fast_adler_append(__private_fast32_SUM,__private_fast32_ADLER,__private_fast32_SUMADLER,
                       uint32_t,_private_fast_adler32_table, _adler,pdata,n)
uint32_t fast_adler32_by_combine(uint32_t adler_left,uint32_t adler_right,size_t len_right)
    _fast_adler_by_combine(__private_fast32_SUM,__private_fast32_ADLER,__private_fast32_SUMADLER,
                           uint32_t, adler_left,adler_right,len_right)

uint64_t fast_adler64_append(uint64_t _adler,const adler_data_t* pdata,size_t n)
    _fast_adler_append(__private_fast64_SUM,__private_fast64_ADLER,__private_fast64_SUMADLER,
                       size_t, _private_fast_adler64_table, _adler,pdata,n)
uint64_t fast_adler64_by_combine(uint64_t adler_left,uint64_t adler_right,uint64_t len_right)
    _fast_adler_by_combine(__private_fast64_SUM,__private_fast64_ADLER,__private_fast64_SUMADLER,
                           uint32_t, adler_left,adler_right,len_right)

adler128_t fast_adler128_append(adler128_t _adler,const adler_data_t* pdata,size_t n)
    _fast_adler_append(__private_fast128_SUM,__private_fast128_ADLER,__private_fast128_SUMADLER,
                       uint64_t, _private_fast_adler128_table, _adler,pdata,n)
adler128_t fast_adler128_by_combine(adler128_t adler_left,adler128_t adler_right,uint64_t len_right)
    _fast_adler_by_combine(__private_fast128_SUM,__private_fast128_ADLER,__private_fast128_SUMADLER,
                           uint64_t, adler_left,adler_right,len_right)
