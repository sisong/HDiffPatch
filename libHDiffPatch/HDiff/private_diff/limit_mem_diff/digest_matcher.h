//digest_matcher.h
//Use digest matching instead of suffix array matching - less effective but uses less memory;
//Use adler to calculate data digest information for rolling match.
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
#include "../../diff_types.h"
#include "bloom_filter.h"
#include "../mem_buf.h"
namespace hdiff_private{

class TDigestMatcher{
public:
    //throw std::runtime_error when data->read error or kMatchBlockSize error;
    TDigestMatcher(const hpatch_TStreamInput* oldData,const hpatch_TStreamInput* newData,
                   size_t kMatchBlockSize,const hdiff_TMTSets_s& mtsets);
    void search_cover(TOutputCovers* out_covers);
    ~TDigestMatcher();
private:
    TDigestMatcher(const TDigestMatcher &); //empty
    TDigestMatcher &operator=(const TDigestMatcher &); //empty
private:
    typedef uint64_t hash_uint_t;
    const hpatch_TStreamInput*  m_oldData;
    const hpatch_TStreamInput*  m_newData;
    std::vector<hash_uint_t>    m_blocks;
    TBloomFilter<hash_uint_t>   m_filter;
    std::vector<uint32_t>       m_sorted_limit;
    std::vector<size_t>         m_sorted_larger;
    bool                        m_isUseLargeSorted;
    const hdiff_TMTSets_s       m_mtsets;
    TAutoMem                    m_mem;
    size_t                      m_newCacheSize;
    size_t                      m_oldCacheSize;
    size_t                      m_oldMinCacheSize;
    size_t                      m_backupCacheSize;
    size_t                      m_kMatchBlockSize;
    
    void getDigests();
    size_t getSearchThreadNum()const;
    void _search_cover(const hpatch_TStreamInput* newData,hpatch_StreamPos_t newOffset,
                       TOutputCovers* out_covers,unsigned char* pmem,
                       void* oldDataLocker=0,void* newDataLocker=0,void* dataLocker=0);
public: //private for multi-thread
    void _search_cover_thread(TOutputCovers* out_covers,unsigned char* pmem,void* mt_data);
};

}//namespace hdiff_private
#endif
