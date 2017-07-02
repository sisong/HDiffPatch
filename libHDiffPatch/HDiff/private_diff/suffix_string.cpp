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
#include <algorithm>
#include <stdexcept>

//排序方法选择.
#ifndef _SA_SORTBY
#define _SA_SORTBY
//#  define _SA_SORTBY_STD_SORT
//#  define _SA_SORTBY_SAIS
#   define _SA_SORTBY_DIVSUFSORT
#endif//_SA_SORTBY

#ifdef _SA_SORTBY_SAIS
#include "sais.hxx"
#endif
#ifdef _SA_SORTBY_DIVSUFSORT
#include "libdivsufsort/divsufsort.h"
#include "libdivsufsort/divsufsort64.h"
#endif


namespace {
    typedef TSuffixString::TInt   TInt;
    typedef TSuffixString::TInt32 TInt32;

    static bool getStringIsLess(const char* str0,const char* str0End,const char* str1,const char* str1End){
        TInt L0=(TInt)(str0End-str0);
        TInt L1=(TInt)(str1End-str1);
    #ifdef _SA_SORTBY_STD_SORT
        const int kMaxCmpLength=1024*4; //警告:这是一个特殊的处理手段,用以避免_suffixString_create在使用std::sort时
                                        //  某些情况退化到O(n*n)复杂度(运行时间无法接受),设置最大比较长度从而控制算法在
                                        //  O(kMaxCmpLength*n)复杂度以内,这时排序的结果并不是标准的后缀数组;
        if (L0>kMaxCmpLength) L0=kMaxCmpLength;
        if (L1>kMaxCmpLength) L1=kMaxCmpLength;
    #endif
        TInt LMin;
        if (L0<L1) LMin=L0; else LMin=L1;
        for (int i=0; i<LMin; ++i){
            TInt sub=((const unsigned char*)str0)[i]-((const unsigned char*)str1)[i];
            if (!sub) continue;
            return sub<0;
        }
        return (L0<L1);
    }

    struct StringToken{
        const char* begin;
        const char* end;
        inline explicit StringToken(const char* _begin,const char* _end):begin(_begin),end(_end){}
    };

    class TSuffixString_compare{
    public:
        inline TSuffixString_compare(const char* begin,const char* end):m_begin(begin),m_end(end){}
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
        const char* m_begin;
        const char* m_end;
    };

    template<class TSAInt>
    static void _suffixString_create(const char* src,const char* src_end,std::vector<TSAInt>& out_sstring){
        TSAInt size=(TSAInt)(src_end-src);
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
        TSAInt rt=saisxx((const unsigned char*)src, &out_sstring[0],size);
    #endif
    #ifdef _SA_SORTBY_DIVSUFSORT
        saint_t rt=-1;
        if (sizeof(TSAInt)==8)
            rt=divsufsort64((const unsigned char*)src,(saidx64_t*)&out_sstring[0],(saidx64_t)size);
        else if (sizeof(TSAInt)==4)
            rt=divsufsort((const unsigned char*)src,(saidx_t*)&out_sstring[0],(saidx_t)size);
    #endif
       if (rt!=0)
            throw std::runtime_error("suffixString_create() error.");
    }
    
    template<class TSAInt>
    static void _build_range256(TSAInt* SA_begin,TSAInt* SA_end,
                                TSAInt** range256,const TSuffixString_compare& comp){
        char str[1];
        StringToken value(&str[0],&str[0]+1);
        TSAInt* pos=SA_begin;
        for (int c=0;c<255;++c){
            range256[c*2+0]=pos;
            str[0]=(char)(c+1);
            pos=std::lower_bound(pos,SA_end,value,comp);
            range256[c*2+1]=pos;
        }
        range256[255*2+0]=pos;
        range256[255*2+1]=SA_end;
    }

    
//#define _SA_MATCHBY_STD_LOWER_BOUND
#ifdef _SA_MATCHBY_STD_LOWER_BOUND
#else
    template <class T> inline static T* __iterator_next(T*& p,TInt n)
                                            { return p+n; }//? hack cpu cache speed for xcode.
#endif
    
    template <class T>
    inline static const T* _lower_bound(const T* rbegin,const T* rend,
                                        const char* str,const char* str_end,
                                        const char* src_begin,const char* src_end){
#ifdef _SA_MATCHBY_STD_LOWER_BOUND
        return std::lower_bound<const T*,StringToken,const TSuffixString_compare&>
                    (rbegin,rend,StringToken(str,str_end),TSuffixString_compare(src_begin,src_end));
#else
        TInt left_eq=1;
        TInt right_eq=1;
        while (size_t len=(size_t)(rend-rbegin)) {
            const T* m=__iterator_next(rbegin,len>>1);
            //const T* m=rbegin+(len>>1);
            TInt eq_len=(left_eq<=right_eq)?left_eq:right_eq;
            const char* vs=str+eq_len;
            const char* ss=src_begin+eq_len+(*m);
            bool is_less;
            while (true) {
                if (vs==str_end) { is_less=false; break; };
                if (ss==src_end) { is_less=true;  break; };
                TInt sub=(*(const unsigned char*)ss)-(*(const unsigned char*)vs);
                if (!sub) {
                    ++vs;
                    ++ss;
                    ++eq_len;
                    continue;
                }else{
                    is_less=(sub<0);
                    break;
                }
            }
            if (is_less){
                left_eq=eq_len;
                rbegin=m+1;
            }else{
                right_eq=eq_len;
                rend=m;
            }
        }
        return rbegin;
#endif
    }
    
    static TInt _lower_bound_TInt(const TInt* rbegin,const TInt* rend,
                                  const char* str,const char* str_end,
                                  const char* src_begin,const char* src_end,
                                  const TInt* SA_begin){
        return _lower_bound(rbegin,rend,str,str_end,src_begin,src_end) - SA_begin;
    }
    
    static TInt _lower_bound_TInt32(const TInt32* rbegin,const TInt32* rend,
                                  const char* str,const char* str_end,
                                  const char* src_begin,const char* src_end,
                                  const TInt32* SA_begin){
        return _lower_bound(rbegin,rend,str,str_end,src_begin,src_end) - SA_begin;
    }

}//end namespace


TSuffixString::TSuffixString()
:m_src_begin(0),m_src_end(0){
}

void TSuffixString::clear(){
    clear_cache();
    m_src_begin=0;
    m_src_end=0;
    std::vector<TInt32> _tmp_m;
    m_SA_limit.swap(_tmp_m);
    std::vector<TInt> _tmp_g;
    m_SA_large.swap(_tmp_g);
}

TSuffixString::TSuffixString(const char* src_begin,const char* src_end)
:m_src_begin(0),m_src_end(0){
    resetSuffixString(src_begin,src_end);
}

void TSuffixString::resetSuffixString(const char* src_begin,const char* src_end){
    assert(src_begin<=src_end);
    m_src_begin=src_begin;
    m_src_end=src_end;
    if (isUseLargeSA()){
        m_SA_limit.clear();
        _suffixString_create(m_src_begin,m_src_end,m_SA_large);
    }else{
        assert(sizeof(TInt32)==4);
        m_SA_large.clear();
        _suffixString_create(m_src_begin,m_src_end,m_SA_limit);
    }
    build_cache();
}

void TSuffixString::clear_cache(){
    memset(&m_cached_range256[0], 0, sizeof(void*)*256*2);
    m_cached_SA_begin=0;
    m_lower_bound=0;
}

void TSuffixString::build_cache(){
    clear_cache();
    if (isUseLargeSA()){
        m_cached_SA_begin=m_SA_large.empty()?0:&m_SA_large[0];
        _build_range256((TInt*)m_cached_SA_begin,(TInt*)m_cached_SA_begin+m_SA_large.size(),
                        (TInt**)&m_cached_range256[0],TSuffixString_compare(m_src_begin,m_src_end));
        m_lower_bound=(t_lower_bound_func)_lower_bound_TInt;
    }else{
        m_cached_SA_begin=m_SA_limit.empty()?0:&m_SA_limit[0];
        _build_range256((TInt32*)m_cached_SA_begin,(TInt32*)m_cached_SA_begin+m_SA_limit.size(),
                        (TInt32**)&m_cached_range256[0],TSuffixString_compare(m_src_begin,m_src_end));
        m_lower_bound=(t_lower_bound_func)_lower_bound_TInt32;
    }
}
