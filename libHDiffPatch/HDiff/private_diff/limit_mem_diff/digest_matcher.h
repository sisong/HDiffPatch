//digest_matcher.h
//用摘要匹配的办法代替后缀数组的匹配,匹配效果比后缀数差,但内存占用少;
//用adler计算数据的摘要信息,以便于滚动匹配.
//
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

#ifndef digest_matcher_h
#define digest_matcher_h
#include "bloom_filter.h"
#include "covers.h"
#include "adler_roll.h"
#include "../mem_buf.h"

namespace hdiff_private{

typedef uint64_t        adler_uint_t;
#define adler_start     fast_adler64_start
#define adler_roll      fast_adler64_roll
typedef uint64_t        adler_hash_t;
static inline adler_hash_t adler_to_hash(const uint64_t x){ return x; }

//typedef adler128_t      adler_uint_t;
//#define adler_start     fast_adler128_start
//#define adler_roll      fast_adler128_roll
//typedef uint64_t        adler_hash_t;
//static inline adler_hash_t adler_to_hash(const adler128_t& x){ return x.adler^x.sum; }
//static inline bool operator !=(const adler128_t& x,const adler128_t& y){
//    return (x.adler!=y.adler)||(x.sum!=y.sum);}
//static inline bool operator <(const adler128_t& x,const adler128_t& y){
//    if (x.adler!=y.adler) return (x.adler<y.adler); else return (x.sum<y.sum); }

//typedef uint32_t        adler_uint_t;
//#define adler_start     fast_adler32_start
//#define adler_roll      fast_adler32_roll
//typedef uint32_t        adler_hash_t;
//static inline adler_hash_t adler_to_hash(const uint32_t x){ return x; }

class TDigestMatcher{
public:
    //throw std::runtime_error when data->read error or kMatchBlockSize error;
    TDigestMatcher(const hpatch_TStreamInput* oldData,size_t kMatchBlockSize,bool kIsSkipSameRange);
    void search_cover(const hpatch_TStreamInput* newData,TCovers* out_covers);
    ~TDigestMatcher();
private:
    const hpatch_TStreamInput*  m_oldData;
    std::vector<adler_uint_t>   m_blocks;
    TBloomFilter<adler_hash_t>  m_filter;
    std::vector<uint32_t>       m_sorted_limit;
    std::vector<size_t>         m_sorted_larger;
    bool                        m_isUseLargeSorted;
    bool                        m_kIsSkipSameRange;
    TAutoMem                    m_mem;
    size_t                      m_newCacheSize;
    size_t                      m_oldCacheSize;
    size_t                      m_oldMinCacheSize;
    size_t                      m_backupCacheSize;
    size_t                      m_kMatchBlockSize;
    
    void getDigests();
};

}//namespace hdiff_private
#endif
