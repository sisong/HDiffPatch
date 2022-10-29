//suffix_string.cpp
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

#include "suffix_string.h"
#include <assert.h>
#include <string.h> //memset
#include <stdexcept> //std::runtime_error
#include "../../../libParallel/parallel_import.h"
#if (_IS_USED_MULTITHREAD)
#include <thread>   //if used vc++, need >= vc2012
#endif
//排序方法选择.
#ifndef _SA_SORTBY
#define _SA_SORTBY
//#  define _SA_SORTBY_STD_SORT
//#  define _SA_SORTBY_SAIS
#   define _SA_SORTBY_DIVSUFSORT
#endif//_SA_SORTBY

//匹配查找方法选择,是否用std::lower_bound,否则用自定义的实现.
//#define _SA_MATCHBY_STD_LOWER_BOUND

#if (defined _SA_SORTBY_STD_SORT) || (defined _SA_MATCHBY_STD_LOWER_BOUND)
    #include <algorithm> //sort,lower_bound
#endif
#ifdef _SA_SORTBY_SAIS
    #include "sais.hxx"
#endif
#ifdef _SA_SORTBY_DIVSUFSORT
    #include "libdivsufsort/divsufsort.h"
    #include "libdivsufsort/divsufsort64.h"
#endif

namespace hdiff_private{

template<class T>
static void _clearVector(std::vector<T>& v){
    std::vector<T> _tmp;
    v.swap(_tmp);
}

namespace {
    typedef TSuffixString::TInt   TInt;
    typedef TSuffixString::TInt32 TInt32;
    typedef TSuffixString::TChar  TChar;

    static bool getStringIsLess(const TChar* str0,const TChar* str0End,
                                const TChar* str1,const TChar* str1End){
        TInt L0=(TInt)(str0End-str0);
        TInt L1=(TInt)(str1End-str1);
    #ifdef _SA_SORTBY_STD_SORT
        const int kMaxCmpLength_sort=1024*4; //警告:这是一个特殊的处理手段,用以避免_suffixString_create在使用std::sort时
                                        //  某些情况退化到O(n*n)复杂度(运行时间无法接受),设置最大比较长度从而控制算法在
                                        //  O(kMaxCmpLength_sort*n)复杂度以内,这时排序的结果并不是标准的后缀数组;
        if (L0>kMaxCmpLength_sort) L0=kMaxCmpLength_sort;
        if (L1>kMaxCmpLength_sort) L1=kMaxCmpLength_sort;
    #endif
        TInt LMin;
        if (L0<L1) LMin=L0; else LMin=L1;
        for (int i=0; i<LMin; ++i){
            TInt sub=str0[i]-str1[i];
            if (!sub) continue;
            return sub<0;
        }
        return (L0<L1);
    }

    struct StringToken{
        const TChar* begin;
        const TChar* end;
        inline explicit StringToken(const TChar* _begin,const TChar* _end):begin(_begin),end(_end){}
    };

    class TSuffixString_compare{
    public:
        inline TSuffixString_compare(const TChar* begin,const TChar* end):m_begin(begin),m_end(end){}
        inline bool operator()(const TInt s0,const StringToken& s1)const{
            return getStringIsLess(m_begin+s0,m_end,s1.begin,s1.end);
        }
        inline bool operator()(const StringToken& s0,const TInt s1)const{
            return getStringIsLess(s0.begin,s0.end,m_begin+s1,m_end);
        }
        inline bool operator()(const TInt s0,const TInt s1)const{
            return getStringIsLess(m_begin+s0,m_end,m_begin+s1,m_end);
        }
    private:
        const TChar* m_begin;
        const TChar* m_end;
    };

    template<class TSAInt>
    static void _suffixString_create(const TChar* src,const TChar* src_end,
                                     std::vector<TSAInt>& out_sstring,size_t threadNum){
        size_t size=(size_t)(src_end-src);
        if (size<0)
            throw std::runtime_error("suffixString_create() error.");
        out_sstring.resize(size);
        if (size<=0) return;
    #ifdef _SA_SORTBY_STD_SORT
        for (TSAInt i=0;i<size;++i)
            out_sstring[i]=i;
        int rt=0;
        try {
            std::sort<TSAInt*,const TSuffixString_compare&>(&out_sstring[0],&out_sstring[0]+size,
                                                            TSuffixString_compare(src,src_end));
        } catch (...) {
            rt=-1;
        }
    #endif
    #ifdef _SA_SORTBY_SAIS
        TSAInt rt=saisxx(src,&out_sstring[0],size);
    #endif
    #ifdef _SA_SORTBY_DIVSUFSORT
        saint_t rt=-1;
        if (sizeof(TSAInt)==8)
            rt=divsufsort64(src,(saidx64_t*)&out_sstring[0],(saidx64_t)size,(int)threadNum);
        else if (sizeof(TSAInt)==4)
            rt=divsufsort(src,(saidx32_t*)&out_sstring[0],(saidx32_t)size,(int)threadNum);
    #endif
       if (rt!=0)
            throw std::runtime_error("suffixString_create() error.");
    }
    
#ifdef _SA_MATCHBY_STD_LOWER_BOUND
#else
    template <class T> inline static T* __select_mid(T*& p,size_t n) { return p+(n>>1); }
            //'&' for hack cpu cache speed for xcode, somebody know way?
#endif
    template <class T>
    inline static const T* _lower_bound(const T* rbegin,const T* rend,
                                        const TChar* str,const TChar* str_end,
                                        const TChar* src_begin,const TChar* src_end,
                                        size_t min_eq=0){
#ifdef _SA_MATCHBY_STD_LOWER_BOUND
        return std::lower_bound<const T*,StringToken,const TSuffixString_compare&>
                    (rbegin,rend,StringToken(str,str_end),TSuffixString_compare(src_begin,src_end));
#else
        size_t left_eq=min_eq;
        size_t right_eq=min_eq;
        while (size_t len=(size_t)(rend-rbegin)) {
            const T* mid=__select_mid(rbegin,len);
            size_t eq_len=(left_eq<=right_eq)?left_eq:right_eq;
            const TChar* vs=str+eq_len;
            const TChar* ss=src_begin+(*mid)+eq_len;
            bool is_less;
            while (true) {
                if (vs==str_end) { is_less=false; break; };
                if (ss==src_end) { is_less=true;  break; };
                TInt sub=(*ss)-(*vs);
                if (!sub) {
                    ++vs;
                    ++ss;
                    ++eq_len;
                    const int kMaxCmpLength_forLimitRangeDiff=1024*8; 
                    if (eq_len<kMaxCmpLength_forLimitRangeDiff) //only for optimize limitRange match speed
                        continue;
                    else
                        return mid;
                }else{
                    is_less=(sub<0);
                    break;
                }
            }
            if (is_less){
                left_eq=eq_len;
                rbegin=mid+1;
            }else{
                right_eq=eq_len;
                rend=mid;
            }
        }
        return rbegin;
#endif
    }
    
    static TInt _lower_bound_TInt(const TInt* rbegin,const TInt* rend,
                                  const TChar* str,const TChar* str_end,
                                  const TChar* src_begin,const TChar* src_end,
                                  const TInt* SA_begin,size_t min_eq){
        return _lower_bound(rbegin,rend,str,str_end,src_begin,src_end,min_eq) - SA_begin;
    }
    
    static TInt _lower_bound_TInt32(const TInt32* rbegin,const TInt32* rend,
                                  const TChar* str,const TChar* str_end,
                                  const TChar* src_begin,const TChar* src_end,
                                  const TInt32* SA_begin,size_t min_eq){
        return _lower_bound(rbegin,rend,str,str_end,src_begin,src_end,min_eq) - SA_begin;
    }
    
    template<class T>
    static void _build_range256(const T* SA_begin,const T* SA_end,
                                const TChar* src_begin,const TChar* src_end,
                                const T** range){
        TChar str[1];
        const T* pos=SA_begin;
        for (size_t c=0;c<256;++c){
            str[0]=(TChar)(c);
            pos=_lower_bound(pos,SA_end,str,str+1,src_begin,src_end);
            range[c]=pos;
        }
        range[256]=SA_end;
    }

    
    template<class T>
    static void _build_range(const T* SA_begin,const T* SA_end,
                             const TChar* src_begin,const TChar* src_end,
                             T* range){
        TChar str[2];
        str[0]=0;
        str[1]=0;
        const T* pos=SA_begin;
        for (size_t cc=0;cc<256*256;++cc){
            //cc is [c0,c1]
            str[0]=(TChar)(cc>>8);
            str[1]=(TChar)(cc&255);
            pos=_lower_bound(pos,SA_end,str,str+2,src_begin,src_end);
            range[cc]=(T)(pos-SA_begin);
        }
        range[256*256]=(T)(SA_end-SA_begin);
    }

}//end namespace


TSuffixString::TSuffixString(bool isUsedFastMatch)
:m_src_begin(0),m_src_end(0),m_isUsedFastMatch(isUsedFastMatch),m_cached2char_range(0){
     clear_cache();
}

TSuffixString::TSuffixString(const TChar* src_begin,const TChar* src_end,bool isUsedFastMatch,size_t threadNum)
:m_src_begin(0),m_src_end(0),m_isUsedFastMatch(isUsedFastMatch),m_cached2char_range(0){
    clear_cache();
    resetSuffixString(src_begin,src_end,threadNum);
}

TSuffixString::~TSuffixString(){
    clear();
}

void TSuffixString::clear(){
    clear_cache();
    m_src_begin=0;
    m_src_end=0;
    _clearVector(m_SA_limit);
    _clearVector(m_SA_large);
}


void TSuffixString::resetSuffixString(const TChar* src_begin,const TChar* src_end,size_t threadNum){
    assert(src_begin<=src_end);
    m_src_begin=src_begin;
    m_src_end=src_end;
    if (isUseLargeSA()){
        _clearVector(m_SA_limit);
        _suffixString_create(m_src_begin,m_src_end,m_SA_large,threadNum);
    }else{
        assert(sizeof(TInt32)==4);
        _clearVector(m_SA_large);
        _suffixString_create(m_src_begin,m_src_end,m_SA_limit,threadNum);
    }
    build_cache(threadNum);
}

TInt TSuffixString::lower_bound(const TChar* str,const TChar* str_end)const{
    //not use any cached range table
    //return m_lower_bound(m_cached_SA_begin,m_cached_SA_end,
    //                     str,str_end,m_src_begin,m_src_end,m_cached_SA_begin,0);
#if (_SSTRING_FAST_MATCH>0)
    if (m_isUsedFastMatch&&(!m_fastMatch.isHit(TFastMatchForSString::getHash(str))))
        return -1;
    #define kMinStrLen _SSTRING_FAST_MATCH
#else
    //assert(str_end-str>=2);
    #define kMinStrLen 2
#endif
    if ((kMinStrLen>=2)&(m_cached2char_range!=0)){
        size_t cc=((size_t)str[1]) | (((size_t)str[0])<<8);
        size_t r0,r1;
        if (isUseLargeSA()){
            r0=((TInt*)m_cached2char_range)[cc]*sizeof(TInt);
            r1=((TInt*)m_cached2char_range)[cc+1]*sizeof(TInt);
        }else{
            r0=((TInt32*)m_cached2char_range)[cc]*sizeof(TInt32);
            r1=((TInt32*)m_cached2char_range)[cc+1]*sizeof(TInt32);
        }
        return m_lower_bound((TChar*)m_cached_SA_begin+r0,(TChar*)m_cached_SA_begin+r1,
                             str,str_end,m_src_begin,m_src_end,m_cached_SA_begin,2);
    }else if (kMinStrLen>0){
        size_t c=str[0];
        return m_lower_bound(m_cached1char_range[c],m_cached1char_range[c+1],
                             str,str_end,m_src_begin,m_src_end,m_cached_SA_begin,1);
    }else{
        return -1;
    }
}

void TSuffixString::clear_cache(){
#if (_SSTRING_FAST_MATCH>0)
    if (m_isUsedFastMatch) m_fastMatch.clear();
#endif
    if (m_cached2char_range){
        delete [](TChar*)m_cached2char_range;
        m_cached2char_range=0;
    }
    memset(&m_cached1char_range[0],0,sizeof(void*)*(256+1));
    m_cached_SA_begin=0;
    m_cached_SA_end=0;
    m_lower_bound=(t_lower_bound_func)_lower_bound_TInt32;//safe
}

void TSuffixString::build_cache(size_t threadNum){
    clear_cache();
#if (_SSTRING_FAST_MATCH>0)
    if (m_isUsedFastMatch) m_fastMatch.buildMatchCache(m_src_begin,m_src_end,threadNum);
#endif
    const size_t kUsedCacheMinSASize =2*(1<<20); //当字符串较大时再启用大缓存表.
    if (SASize()>kUsedCacheMinSASize){
        m_cached2char_range=new TChar[(256*256+1)*(isUseLargeSA()?sizeof(size_t):sizeof(TInt32))];
    }
    
    if (isUseLargeSA()){
        m_lower_bound=(t_lower_bound_func)_lower_bound_TInt;
        if (m_SA_large.empty()) return;
        m_cached_SA_begin=&m_SA_large[0];
        m_cached_SA_end=&m_SA_large[0]+m_SA_large.size();
        _build_range256((TInt*)m_cached_SA_begin,(TInt*)m_cached_SA_end,
                        m_src_begin,m_src_end,(const TInt**)&m_cached1char_range[0]);
        if (m_cached2char_range){
            _build_range((TInt*)m_cached_SA_begin,(TInt*)m_cached_SA_end,
                         m_src_begin,m_src_end,(TInt*)m_cached2char_range);
        }
    }else{
        m_lower_bound=(t_lower_bound_func)_lower_bound_TInt32;
        if (m_SA_limit.empty()) return;
        m_cached_SA_begin=&m_SA_limit[0];
        m_cached_SA_end=&m_SA_limit[0]+m_SA_limit.size();
        _build_range256((TInt32*)m_cached_SA_begin,(TInt32*)m_cached_SA_end,
                        m_src_begin,m_src_end,(const TInt32**)&m_cached1char_range[0]);
        if (m_cached2char_range){
            _build_range((TInt32*)m_cached_SA_begin,(TInt32*)m_cached_SA_end,
                         m_src_begin,m_src_end,(TInt32*)m_cached2char_range);
        }
    }
}


#if (_SSTRING_FAST_MATCH>0)

    template<bool isMT>
    static void _filter_insert(TBloomFilter<TFastMatchForSString::THash>* filter,
                               const TChar* src_begin,const TChar* src_end){
        const TChar* cur = src_begin;
        TFastMatchForSString::THash h=TFastMatchForSString::getHash(cur);
        cur+=TFastMatchForSString::kFMMinStrSize;
        do {
    #if (_IS_USED_MULTITHREAD)
            if (isMT)
                filter->insert_MT(h);
            else
    #endif
                filter->insert(h);
            if (cur<src_end)
                h=TFastMatchForSString::rollHash(h,cur++);
            else
                break;
        } while (true);
    }

    void TFastMatchForSString::buildMatchCache(const TChar* src_begin,const TChar* src_end,size_t threadNum){
        #define kFMZoom 4  //ctrl memory size & match speed
        size_t srcSize=src_end-src_begin;
        if (srcSize>=kFMMinStrSize){
            const size_t rollSize=srcSize-(kFMMinStrSize-1);
            bf.init(rollSize,kFMZoom); //alloc large memory
#if (_IS_USED_MULTITHREAD)
            const size_t kInsertMinParallelSize=4096;
            if ((threadNum>1)&&(rollSize>=kInsertMinParallelSize)) {
                const size_t maxThreanNum=rollSize/(kInsertMinParallelSize/2);
                threadNum=(threadNum<=maxThreanNum)?threadNum:maxThreanNum;

                const size_t step=rollSize/threadNum;
                const size_t threadCount=threadNum-1;
                std::vector<std::thread> threads(threadCount);
                for (size_t i=0;i<threadCount;i++,src_begin+=step)
                    threads[i]=std::thread(_filter_insert<true>,&bf,src_begin,src_begin+step+(kFMMinStrSize-1));
                _filter_insert<true>(&bf,src_begin,src_end);
                for (size_t i=0;i<threadCount;i++)
                    threads[i].join();
            }else
#endif
            {
                _filter_insert<false>(&bf,src_begin,src_end);
            }
        }else if ((srcSize>0)||(src_begin!=0))
            bf.init(0,kFMZoom);
        else{
            bf.clear();
        }
    }
#endif
    
}//namespace hdiff_private
