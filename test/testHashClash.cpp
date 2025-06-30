// testHashClash.cpp
// tool for HDiff
// An estimation method for detecting hash clashs
/*
 The MIT License (MIT)
 Copyright (c) 2012-2019 HouSisong
 
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

#include <iostream>
#include <stdlib.h>
#include <vector>
#include <assert.h>
#include <unordered_map>
#include "../libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.h"
#include "../_clock_for_demo.h"
#define _IS_NEED_ZLIB   1
#define _IS_NEED_MD5    0
#if (_IS_NEED_ZLIB)
#include "zlib.h"
#endif
#if (_IS_NEED_MD5)
#include "md5.h" // https://sourceforge.net/projects/libmd5-rfc
#endif

typedef unsigned char   TByte;
typedef ptrdiff_t       TInt;
typedef size_t          TUInt;

#define _IS_USES_MY_RAND
#ifdef _IS_USES_MY_RAND
class  CMyRand {
public:
    unsigned int _my_holdrand;
public:
    inline CMyRand() :_my_holdrand(1) {}
    inline int _my_rand() {
        unsigned int result = _my_holdrand * 214013 + 2531011;
        _my_holdrand = result;
        return (result >> 16) & RAND_MAX;
    }
};
static CMyRand _MyRand;
inline int  _rand() { return  _MyRand._my_rand(); }
inline void _srand(unsigned int seed) { _MyRand._my_holdrand = seed; }
#else
#define  _rand  rand
#define  _srand srand
#endif

/* //interface
struct THash{
    typename TValue;
    static const char* name() const;
    void hash_begin();
    void hash(const TByte* pdata,const TByte* pdataEnd);
    void hash_finish(TValue* hv);
};*/

#if (_IS_NEED_MD5)
struct THash_md5_128{
    typedef std::pair<uint64_t,uint64_t> TValue;
    inline static const char* name() { return "md5_128"; }
    md5_state_t _hv;
    inline void hash_begin() { md5_init(&_hv); }
    inline void hash(const TByte* pdata,const TByte* pdata_end)
        { md5_append(&_hv,pdata,(int)(pdata_end-pdata)); }
    inline void hash_end(TValue* hv) { md5_finish(&_hv,(TByte*)hv); }
};
namespace std{
    template<> struct hash<THash_md5_128::TValue>{
        inline size_t operator()(const THash_md5_128::TValue& v) const{
            return v.first^v.second; }
    };
    template<> struct less<THash_md5_128::TValue>{
        inline bool operator()(const THash_md5_128::TValue& x,const THash_md5_128::TValue& y)const{
            return (x.first^x.second) < (y.first^y.second); }
    };
}
#endif

#if (_IS_NEED_ZLIB)
struct THash_crc32{
    typedef uint32_t TValue;
    inline static const char* name() { return "crc32"; }
    TValue _hv;
    inline void hash_begin() { _hv=(TValue)crc32(0,0,0); }
    inline void hash(const TByte* pdata,const TByte* pdata_end)
                        { _hv=(TValue)crc32(_hv,pdata,(uInt)(pdata_end-pdata)); }
    inline void hash_end(TValue* hv) { *hv=_hv; }
};

struct THash_adler32{
    typedef uint32_t TValue;
    inline static const char* name() { return "adler32"; }
    TValue _hv;
    inline void hash_begin() { _hv=(TValue)adler32(0,0,0); }
    inline void hash(const TByte* pdata,const TByte* pdata_end)
                        { //TValue _tv=(TValue)adler32_append(_hv,pdata,(pdata_end-pdata));
                          _hv=(TValue)adler32(_hv,pdata,(uInt)(pdata_end-pdata));
                          //assert(_hv==_tv);
                        }
    inline void hash_end(TValue* hv) { *hv=_hv; }
};
#endif


struct THash_adler32h{
    typedef uint32_t TValue;
    inline static const char* name() { return "adler32h"; }
    TValue _hv;
    inline void hash_begin() { _hv=adler32_start(0,0); }
    inline void hash(const TByte* pdata,const TByte* pdata_end)
                        { _hv=adler32_append(_hv,pdata,(pdata_end-pdata));}
    inline void hash_end(TValue* hv) { *hv=_hv; }
};

template<size_t kTestBit>
struct THash_adler32h_bit{
    typedef uint32_t TValue;
    inline static const char* name() { return "adler32h_bit"; }
    TValue _hv;
    inline void hash_begin() { _hv=adler32_start(0,0); }
    inline void hash(const TByte* pdata,const TByte* pdata_end)
                        { _hv=adler32_append(_hv,pdata,(pdata_end-pdata)); }
    inline void hash_end(TValue* hv) { *hv=_hv&((1<<kTestBit)-1); }
};


struct THash_fadler32{
    typedef uint32_t TValue;
    inline static const char* name() { return "fadler32"; }
    TValue _hv;
    inline void hash_begin() { _hv=fast_adler32_start(0,0); }
    inline void hash(const TByte* pdata,const TByte* pdata_end)
                    { _hv=fast_adler32_append(_hv,pdata,(pdata_end-pdata)); }
    inline void hash_end(TValue* hv) { *hv=_hv; }
};

struct THash_adler64h{
    typedef uint64_t TValue;
    inline static const char* name() { return "adler64h"; }
    TValue _hv;
    inline void hash_begin() { _hv=adler64_start(0,0); }
    inline void hash(const TByte* pdata,const TByte* pdata_end)
                    { _hv=adler64_append(_hv,pdata,(pdata_end-pdata)); }
    inline void hash_end(TValue* hv) { *hv=_hv; }
};

struct THash_fadler64{
    typedef uint64_t TValue;
    inline static const char* name() { return "fadler64"; }
    TValue _hv;
    inline void hash_begin() { _hv=fast_adler64_start(0,0); }
    inline void hash(const TByte* pdata,const TByte* pdata_end)
                    { _hv=fast_adler64_append(_hv,pdata,(pdata_end-pdata)); }
    inline void hash_end(TValue* hv) { *hv=_hv; }
};

#if (_IS_NEED_FAST_ADLER128)
struct THash_fadler128{
    typedef adler128_t TValue;
    inline static const char* name() { return "fadler128"; }
    TValue _hv;
    inline void hash_begin() { _hv=fast_adler128_start(0,0); }
    inline void hash(const TByte* pdata,const TByte* pdata_end)
                    { _hv=fast_adler128_append(_hv,pdata,(pdata_end-pdata)); }
    inline void hash_end(TValue* hv) { *hv=_hv; }
};
struct _adler128_t_hash{
    inline size_t operator()(const adler128_t& c)const{
        return c.adler ^ c.sum;
    }
};
struct _adler128_t_cmp{
    inline bool operator()(const adler128_t& x, const adler128_t& y)const{
        return (x.adler==y.adler) & (x.sum==y.sum);
    }
};
#endif

template <class THash>
void test_speed(const TByte* data,const TByte* data_end){
    const size_t kMinHashDataSize=0;
    const size_t kMaxHashDataSize=1024*1024;
    const size_t kTestCount = 1000000;
    
    printf("%s%s ",THash::name(),std::string(12-strlen(THash::name()),' ').c_str());
    
    THash th;
    static typename THash::TValue hv;
    unsigned int rand_seed=1;
    _srand(rand_seed);
    
    double time0=clock_s();
    size_t total_size = 0;
    
    for(size_t i = 0; i < kTestCount; i++) {
        size_t dlen =kMinHashDataSize+(_rand() % (kMaxHashDataSize+1-kMinHashDataSize));
        size_t dstrat=((uint64_t)_rand()+((uint64_t)_rand()*(RAND_MAX+1))) % ((data_end-data)-dlen);
        assert(dstrat+dlen<=(size_t)(data_end-data));
        const TByte* pv=data+dstrat;
        th.hash_begin();
        
        th.hash(pv,pv+dlen);
        total_size+=dlen;
        
        th.hash_end(&hv);
    }
    
    double time_used = clock_s() - time0;
    double speed = total_size / (time_used * 1024.0 * 1024.0); // MB/s
    
    printf("speed: %.1f MB/s\n", speed);
}

const uint64_t kMaxMapNodeSize=80000000ull; //run test memory ctrl
const size_t   kRandTestMaxSize=1024*1024*1024;//test rand data size
    size_t   kMinHashDataSize=0;
    size_t   kMaxHashDataSize=256;
    size_t   kMinClash=0; //run test max time ctrl

template <class THash,class TUInt,int kTestBit=sizeof(TUInt)*8,TUInt kTestMask=(TUInt)~(TUInt)0>
void test(const TByte* data,const TByte* data_end){
    typedef typename THash::TValue                TValue;
    typedef std::pair<const TByte*,const TByte*>  TPair;
    typedef std::unordered_map<TUInt,TPair> TMap;
    double time0=clock_s();
    const size_t clip_count=sizeof(TValue)/sizeof(TUInt);
    assert(clip_count*sizeof(TUInt)==sizeof(TValue)); //unsupport other bit
    TMap maps[clip_count];
    for (size_t m=0;m<clip_count;++m)
        maps[m].reserve(kMaxMapNodeSize*3/clip_count);
    unsigned int rand_seed=7;
    _srand(rand_seed);
    if (kTestMask!=(TUInt)~(TUInt)0) printf("mask[%08X %08X] ",(int)(((uint64_t)kTestMask)>>32),(int)kTestMask);
    printf("%s%s ",THash::name(),std::string(12-strlen(THash::name()),' ').c_str());
    
    uint64_t    curClashMin=0;
    uint64_t    clashs[clip_count]={0};
    double clashBases[clip_count]={0};
    size_t i=0;
    while (curClashMin<kMinClash) {
        uint64_t    clashMin=~(uint64_t)0;
        for (size_t m=0;m<clip_count;++m){
            if (clashs[m]<clashMin) clashMin=clashs[m];
        }
        curClashMin=clashMin;
        
        size_t dlen =kMinHashDataSize+(_rand() % (kMaxHashDataSize+1-kMinHashDataSize));
        size_t dstrat=((uint64_t)_rand()+((uint64_t)_rand()*(RAND_MAX+1))) % ((data_end-data)-dlen);
        assert(dstrat+dlen<=(size_t)(data_end-data));
        const TByte* pv    =data+dstrat;
        const TByte* pv_end=pv+dlen;
        
        THash th;
        typename THash::TValue hvs;
        th.hash_begin();
        th.hash(pv,pv_end);
        th.hash_end(&hvs);
        for (size_t m=0;m<clip_count;++m){
            TMap&     map=maps[m];
            uint64_t& clash=clashs[m];
            double& clashBase=clashBases[m];
            TUInt hv=((TUInt*)&hvs)[m];
            hv&=kTestMask;
            auto it=map.find(hv);
            if (it==map.end()){
                if (map.size()*clip_count<kMaxMapNodeSize)
                    map[hv]=TPair(pv,pv_end);
                clashBase+=map.size();
                ++i;
            }else{
                const TPair& v=it->second;
                const TByte* vf=v.first;
                if ((pv_end-pv)!=(v.second-vf)){
                    ++clash;
                    clashBase+=map.size();
                    ++i;
                } else if (pv==vf){
                    //same i
                }else{
                    bool isEq=true;
                    for (size_t e=0; e<dlen; ++e) {
                        if (pv[e]==vf[e]){
                            continue;
                        }else{
                            isEq=false;
                            break;
                        }
                    }
                    if (isEq){
                        //same i
                    }else{
                        ++clash;
                        clashBase+=map.size();
                        ++i;
                    }
                }
            }
        }
    }
    for (size_t m=0;m<clip_count;++m)
        maps[m].clear();
    
    printf("test %dbitx%d, clash rate: ",kTestBit,(int)clip_count);
    if (clip_count>0) printf("[");
    for (size_t m=0;m<clip_count; ++m) {
        double clashR=clashs[m]/clashBases[m];
        if (m>0) printf(" ");
        if (clashR>0){
            //printf("%.3e(%.1fbit)",clashR,log2(1/clashR));
            printf("%.2fbit",log2(1/clashR));
        }else{
            printf("0/%.3e",clashBases[m]);
        }
    }
    if (clip_count>0) printf("]");
    printf(" \ttime:%.1fs\n",(clock_s()-time0));
}

#if (_IS_NEED_FAST_ADLER128)
template <uint64_t kTestMask0,uint64_t kTestMask1>
void test_fadler128(const TByte* data,const TByte* data_end){
    typedef THash_fadler128 THash;
    typedef uint64_t TUInt;
    typedef typename THash::TValue                TValue;
    typedef std::pair<const TByte*,const TByte*>  TPair;
    typedef std::unordered_map<TValue,TPair,_adler128_t_hash,_adler128_t_cmp> TMap;
    double time0=clock_s();
    const size_t clip_count=1;
    TMap maps[clip_count];
    for (size_t m=0;m<clip_count;++m)
        maps[m].reserve(kMaxMapNodeSize*3/clip_count);
    unsigned int rand_seed=7;
    _srand(rand_seed);
    printf("mask[%08X %08X ",(int)(kTestMask1>>32),(int)kTestMask1);
    printf("%08X %08X] ",(int)(kTestMask0>>32),(int)kTestMask0);
    printf("%s%s ",THash::name(),std::string(10-strlen(THash::name()),' ').c_str());
    
    uint64_t    curClashMin=0;
    uint64_t    clashs[clip_count]={0};
    double clashBases[clip_count]={0};
    size_t i=0;
    while (curClashMin<kMinClash) {
        uint64_t    clashMin=~(uint64_t)0;
        for (size_t m=0;m<clip_count;++m){
            if (clashs[m]<clashMin) clashMin=clashs[m];
        }
        curClashMin=clashMin;
        
        size_t dlen =kMinHashDataSize+(_rand() % (kMaxHashDataSize+1-kMinHashDataSize));
        size_t dstrat=_rand() % ((data_end-data) - dlen);
        assert(dstrat+dlen<=(data_end-data));
        const TByte* pv    =data+dstrat;
        const TByte* pv_end=pv+dlen;
        
        THash th;
        typename THash::TValue hvs;
        th.hash_begin();
        th.hash(pv,pv_end);
        th.hash_end(&hvs);
        for (size_t m=0;m<clip_count;++m){
            TMap&     map=maps[m];
            uint64_t& clash=clashs[m];
            double& clashBase=clashBases[m];
            TValue hv=hvs;
            hv.adler&=kTestMask0;
            hv.sum&=kTestMask1;
            auto it=map.find(hv);
            if (it==map.end()){
                if (map.size()*clip_count<kMaxMapNodeSize)
                    map[hv]=TPair(pv,pv_end);
                clashBase+=map.size();
                ++i;
            }else{
                const TPair& v=it->second;
                const TByte* vf=v.first;
                if ((pv_end-pv)!=(v.second-vf)){
                    ++clash;
                    clashBase+=map.size();
                    ++i;
                } else if (pv==vf){
                    //same i
                }else{
                    bool isEq=true;
                    for (size_t e=0; e<dlen; ++e) {
                        if (pv[e]==vf[e]){
                            continue;
                        }else{
                            isEq=false;
                            break;
                        }
                    }
                    if (isEq){
                        //same i
                    }else{
                        ++clash;
                        clashBase+=map.size();
                        ++i;
                    }
                }
            }
        }
    }
    for (size_t m=0;m<clip_count;++m)
        maps[m].clear();
    
    printf("clash rate: ");
    if (clip_count>0) printf("[");
    for (size_t m=0;m<clip_count; ++m) {
        double clashR=clashs[m]/clashBases[m];
        if (m>0) printf(" ");
        if (clashR>0){
            //printf("%.3e(%.1fbit)",clashR,log2(1/clashR));
            printf("%.2fbit",log2(1/clashR));
        }else{
            printf("0/%.3e",clashBases[m]);
        }
    }
    if (clip_count>0) printf("]");
    printf(" \ttime:%.1fs\n",(clock_s()-time0));
}
#endif

int main() {
    
    printf("init test data ...\n");
    std::vector<TByte> data(kRandTestMaxSize);
    unsigned int rand_seed=0;
    _srand(rand_seed);
    for (size_t i=0; i<data.size(); ++i) {
        data[i]=(TByte)_rand();
    }
    printf("\n");

    //test speed
    printf("test hash speed ...\n");
#if (_IS_NEED_ZLIB)
    test_speed<THash_adler32>(data.data(),data.data()+data.size());
    test_speed<THash_crc32>(data.data(),data.data()+data.size());
#endif
#if (_IS_NEED_MD5)
    test_speed<THash_md5_128>(data.data(),data.data()+data.size());
#endif
    test_speed<THash_adler32h>(data.data(),data.data()+data.size());
    test_speed<THash_adler64h>(data.data(),data.data()+data.size());
    test_speed<THash_fadler32>(data.data(),data.data()+data.size());
    test_speed<THash_fadler64>(data.data(),data.data()+data.size());
#if (_IS_NEED_FAST_ADLER128)
    test_speed<THash_fadler128>(data.data(),data.data()+data.size());
#endif
    printf("\n");

    //test clash
    printf("test hash clash ...\n");

    double bestCR_32bit =1.0/(((uint64_t)1)<<32);
    double bestCR_64bit =bestCR_32bit*bestCR_32bit;
    double bestCR_128bit=bestCR_64bit*bestCR_64bit;
    printf("32bit  hash  best clash rate: %.3e (1/%llu) \n",
           bestCR_32bit,(((uint64_t)1)<<32));
    printf("48bit  hash  best clash rate: %.3e (1/%llu) \n",
           1.0/(((uint64_t)1)<<48),(((uint64_t)1)<<48));
    printf("64bit  hash  best clash rate: %.3e (1/%llu%llu) \n",
           bestCR_64bit,(((uint64_t)(~(uint64_t)0)))/10,(((uint64_t)(~(uint64_t)0)))%10+1);
    printf("128bit hash  best clash rate: %.3e (1/%.3e) \n\n",
           bestCR_128bit,1/bestCR_128bit);
    const uint64_t kTestMask8=((((uint64_t)1)<<8)-1);
    const uint64_t kTestMask16=((((uint64_t)1)<<16)-1);
    const uint64_t kTestMask32=((((uint64_t)1)<<32)-1);
    const uint64_t kTestMask64=~(uint64_t)0;

    /* //only test adler32h by bit
    kMinHashDataSize=1024;
    kMaxHashDataSize=1024;
    kMinClash=10000;
    test<THash_adler32h_bit<1>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<2>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<3>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<4>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<5>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<6>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<7>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<8>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<9>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<10>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<11>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<12>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<13>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<14>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<15>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<16>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<17>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<18>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<19>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<20>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<21>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<22>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<23>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<24>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<25>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<26>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<27>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<28>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<29>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<30>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<31>,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler32h_bit<32>,uint32_t>(data.data(),data.data()+data.size());
    return 0;
    //*/

    //*
    kMinClash=100000;
    test<THash_adler32h,uint8_t>(data.data(),data.data()+data.size());
    test<THash_adler32h,uint16_t>(data.data(),data.data()+data.size());
    test<THash_adler32h,uint32_t>(data.data(),data.data()+data.size());
#if (_IS_NEED_ZLIB)
    test<THash_adler32,uint32_t>(data.data(),data.data()+data.size());
    test<THash_crc32,uint16_t>(data.data(),data.data()+data.size());
    test<THash_crc32,uint32_t>(data.data(),data.data()+data.size());
#endif
#if (_IS_NEED_MD5)
    test<THash_md5_128,uint32_t>(data.data(),data.data()+data.size());
#endif
    test<THash_adler64h,uint8_t>(data.data(),data.data()+data.size());
    test<THash_adler64h,uint16_t>(data.data(),data.data()+data.size());
    test<THash_adler64h,uint32_t>(data.data(),data.data()+data.size());
    test<THash_adler64h,uint64_t>(data.data(),data.data()+data.size());
    test<THash_fadler32,uint8_t>(data.data(),data.data()+data.size());
    test<THash_fadler32,uint16_t>(data.data(),data.data()+data.size());
    test<THash_fadler32,uint32_t>(data.data(),data.data()+data.size());
    test<THash_fadler64,uint8_t>(data.data(),data.data()+data.size());
    test<THash_fadler64,uint16_t>(data.data(),data.data()+data.size());
    test<THash_fadler64,uint32_t>(data.data(),data.data()+data.size());
    //test<THash_fadler64,uint64_t>(data.data(),data.data()+data.size());
#if (_IS_NEED_FAST_ADLER128)
    test<THash_fadler128,uint8_t>(data.data(),data.data()+data.size());
    test<THash_fadler128,uint16_t>(data.data(),data.data()+data.size());
    test<THash_fadler128,uint32_t>(data.data(),data.data()+data.size());
    kMinClash=50;
    test<THash_fadler128,uint64_t>(data.data(),data.data()+data.size());
#endif
    printf("\n");
    //*/
    //*
    printf("NOTE: test fadler64 32bit ...\n");
    kMinClash=100000;
    test<THash_fadler64,uint64_t,32,kTestMask32>(data.data(),data.data()+data.size());
    test<THash_fadler64,uint64_t,32,kTestMask32<<32>(data.data(),data.data()+data.size());
    test<THash_fadler64,uint64_t,32,kTestMask32<<16>(data.data(),data.data()+data.size());
    test<THash_fadler64,uint64_t,32,kTestMask16|kTestMask16<<32>(data.data(),data.data()+data.size());
    test<THash_fadler64,uint64_t,32,kTestMask16<<16|kTestMask16<<48>(data.data(),data.data()+data.size());
    test<THash_fadler64,uint64_t,32,kTestMask16|kTestMask16<<48>(data.data(),data.data()+data.size());
    printf("\n");
    //*/
    /*
#if (_IS_NEED_FAST_ADLER128)
    printf("NOTE: not enough time to get next test results ...\n");
    kMinClash=1;
    test_fadler128<kTestMask32,kTestMask32>(data.data(),data.data()+data.size());
    test_fadler128<kTestMask32,kTestMask32<<32>(data.data(),data.data()+data.size());
    test_fadler128<kTestMask32<<32,kTestMask32>(data.data(),data.data()+data.size());
    test_fadler128<kTestMask32<<32,kTestMask32<<32>(data.data(),data.data()+data.size());
    printf("\n");
#endif
    //*/
    //*
    printf("NOTE: not enough time to get next test results ...\n");
    kMinClash=1; // for timesaving but increase deviation
    test<THash_fadler64,uint64_t>(data.data(),data.data()+data.size());
#if (_IS_NEED_MD5)
    test<THash_md5_128,uint64_t>(data.data(),data.data()+data.size());
#endif
    //*/
    return 0;
}

