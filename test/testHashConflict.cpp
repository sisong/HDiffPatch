//  testHashConflict.cpp
// tool for HDiff
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
#include <unordered_map>
#include "zlib.h"
#include "md5c.h"
#include "../libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.h"
#include "../_clock_for_demo.h"
typedef unsigned char   TByte;
typedef ptrdiff_t       TInt;
typedef size_t          TUInt;

/*
struct THash{
    typename TValue;
    static const char* name() const;
    void hash_begin();
    void hash(const TByte* pdata,const TByte* pdataEnd);
    void hash_finish(TValue* hv);
};*/

struct THash_md5_128{
    typedef std::pair<uint64_t,uint64_t> TValue;
    inline static const char* name() { return "md5_128"; }
    md5_state_t _hv;
    inline void hash_begin() { md5_init_c(&_hv); }
    inline void hash(const TByte* pdata,const TByte* pdata_end)
    { md5_append_c(&_hv,pdata,(int)(pdata_end-pdata)); }
    inline void hash_end(TValue* hv) { md5_finish_c(&_hv,(TByte*)hv); }
};

namespace std{
    template<>
    struct hash<THash_md5_128::TValue>
    {
        size_t operator()(const THash_md5_128::TValue& v) const{
            return v.first^v.second;
        }
    };
}

struct THash_md5_64:public THash_md5_128{
    typedef uint64_t TValue;
    inline static const char* name() { return "md5_64"; }
    inline void hash_end(TValue* hv) { THash_md5_128::TValue v; THash_md5_128::hash_end(&v); *hv=v.first^v.second; }
};

struct THash_md5_32:public THash_md5_64{
    typedef uint32_t TValue;
    inline static const char* name() { return "md5_32"; }
    inline void hash_end(TValue* hv) { THash_md5_64::TValue v; THash_md5_64::hash_end(&v); *hv=(TValue)(v^(v>>32)); }
};

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
                        { _hv=(TValue)adler32(_hv,pdata,(uInt)(pdata_end-pdata)); }
    inline void hash_end(TValue* hv) { *hv=_hv; }
};


struct THash_adler32h{
    typedef uint32_t TValue;
    inline static const char* name() { return "adler32h"; }
    TValue _hv;
    inline void hash_begin() { _hv=(TValue)adler32_start(0,0); }
    inline void hash(const TByte* pdata,const TByte* pdata_end)
                        { _hv=(TValue)adler32_append(_hv,pdata,(pdata_end-pdata));
                            //assert(_hv==(TValue)adler32(1,pdata,(pdata_end-pdata)));
                        }
    inline void hash_end(TValue* hv) { *hv=_hv; }
};

struct THash_adler32f{
    typedef uint32_t TValue;
    inline static const char* name() { return "adler32f"; }
    TValue _hv;
    inline void hash_begin() { _hv=(TValue)fast_adler32_start(0,0); }
    inline void hash(const TByte* pdata,const TByte* pdata_end)
                    { _hv=(TValue)fast_adler32_append(_hv,pdata,(pdata_end-pdata)); }
    inline void hash_end(TValue* hv) { *hv=_hv; }
};

struct THash_adler64h{
    typedef uint64_t TValue;
    inline static const char* name() { return "adler64h"; }
    TValue _hv;
    inline void hash_begin() { _hv=(TValue)adler64_start(0,0); }
    inline void hash(const TByte* pdata,const TByte* pdata_end)
                    { _hv=(TValue)adler64_append(_hv,pdata,(pdata_end-pdata)); }
    inline void hash_end(TValue* hv) { *hv=_hv; }
};

struct THash_adler64f{
    typedef uint64_t TValue;
    inline static const char* name() { return "adler64f"; }
    TValue _hv;
    inline void hash_begin() { _hv=(TValue)fast_adler64_start(0,0); }
    inline void hash(const TByte* pdata,const TByte* pdata_end)
                    { _hv=(TValue)fast_adler64_append(_hv,pdata,(pdata_end-pdata)); }
    inline void hash_end(TValue* hv) { *hv=_hv; }
};



const long kRandTestMaxSize=1024*1024*1024;
const long kMaxHashSize=1024;
const uint64_t kRandTestCount=100000000;

template <class THash>
void test(const TByte* data,const TByte* data_end){
    typedef std::pair<const TByte*,const TByte*>  TPair;
    std::unordered_map<typename THash::TValue,TPair> map;
    map.reserve(kRandTestCount*2);
    unsigned int rand_seed=3;
    printf("%s       \t",THash::name());
    double time0=clock_s();
    
    uint64_t conflict=0;
    size_t i=0;
    while (i<kRandTestCount) {
        size_t dlen=rand_r(&rand_seed) % kMaxHashSize;
        size_t dstrat=rand_r(&rand_seed) % ((data_end-data) - dlen);
        assert(dstrat+dlen<=(data_end-data));
        const TByte* pv    =data+dstrat;
        const TByte* pv_end=pv+dlen;
        
        THash th;
        typename THash::TValue hv;
        th.hash_begin();
        th.hash(pv,pv_end);
        th.hash_end(&hv);
        
        auto it=map.find(hv);
        if (it==map.end()){
            map[hv]=TPair(pv,pv_end);
            ++i;
        }else{
            const TPair& v=it->second;
            const TByte* vf=v.first;
            if ((pv_end-pv)!=(v.second-vf)){
                ++conflict;
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
                    ++conflict;
                    ++i;
                }
            }
        }
    }
    double conflictR=conflict*1.0/kRandTestCount;
    printf("conflict: %.3f%%%% (%lld/%lld)   \ttime: %.3f s\n",
           conflictR*10000,conflict,kRandTestCount,(clock_s()-time0));
}

int main() {
    std::vector<TByte> data(kRandTestMaxSize);
    unsigned int rand_seed=0;
    for (size_t i=0; i<data.size(); ++i) {
        data[i]=(TByte)rand_r(&rand_seed);
    }
    
    test<THash_adler32h>(data.data(),data.data()+data.size());
    test<THash_adler32f>(data.data(),data.data()+data.size());
    test<THash_adler64h>(data.data(),data.data()+data.size());
    test<THash_adler64f>(data.data(),data.data()+data.size());
    test<THash_adler32>(data.data(),data.data()+data.size());
    test<THash_crc32>(data.data(),data.data()+data.size());
    test<THash_md5_32>(data.data(),data.data()+data.size());
    test<THash_md5_64>(data.data(),data.data()+data.size());
    test<THash_md5_128>(data.data(),data.data()+data.size());
    return 0;
}
