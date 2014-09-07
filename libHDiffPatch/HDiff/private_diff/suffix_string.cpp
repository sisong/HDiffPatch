//suffix_string.cpp
//create by housisong
//后缀字符串的一个实现.
//
/*
 Copyright (c) 2012-2014 HouSisong All Rights Reserved.
 
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
#include <stdio.h>
#include <algorithm>
#include <stdexcept>
#include "sais.hxx"

//#define IS_TEST_USE_STD_SORT

namespace {
    typedef TSuffixString::TInt TInt;

    static bool getStringIsLess(const char* str0,const char* str0End,const char* str1,const char* str1End){
        TInt L0=(TInt)(str0End-str0);
        TInt L1=(TInt)(str1End-str1);
    #ifdef IS_TEST_USE_STD_SORT
        const int kMaxCmpLength=1024*4;
        if (L0>kMaxCmpLength) L0=kMaxCmpLength;
        if (L1>kMaxCmpLength) L1=kMaxCmpLength;
    #endif
        TInt LMin;
        if (L0<L1) LMin=L0; else LMin=L1;
        for (int i=0; i<LMin; ++i){
            TInt sub=TInt(((unsigned char*)str0)[i])-TInt(((unsigned char*)str1)[i]);
            if (sub==0) continue;
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
        out_sstring.resize(size);
        if (size<=0) return;
        
    #ifdef IS_TEST_USE_STD_SORT //test uses std::sort, but slow
        for (TSAInt i=0;i<size;++i) out_sstring[i]=i;
        std::sort<TSAInt*,const TSuffixString_compare&>(&out_sstring[0],&out_sstring[0]+size,TSuffixString_compare(src,src_end));
    #else
        if (saisxx((const unsigned char*)src, &out_sstring[0],size) !=0)
            throw std::runtime_error("suffixString_create() error.");
    #endif
    }
    
    template<class TSAInt>
    static TInt _lower_bound(const std::vector<TSAInt>& SA, const StringToken& value,const TSuffixString_compare& comp){
        if (SA.empty()) return 0;
        const TSAInt* begin=&SA[0];
        const TSAInt* pos=std::lower_bound<const TSAInt*,StringToken,const TSuffixString_compare&>
            (begin,begin+SA.size(),value,comp);
        return (TInt)(pos-begin);
    }
    

}//end namespace


TSuffixString::TSuffixString()
:m_src_begin(0),m_src_end(0){
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
        m_SA_small.clear();
        _suffixString_create(m_src_begin,m_src_end,m_SA_large);
    }else{
        m_SA_large.clear();
        _suffixString_create(m_src_begin,m_src_end,m_SA_small);
    }
}

TInt TSuffixString::lower_bound(const char* str,const char* str_end)const{
    if (isUseLargeSA())
        return _lower_bound(m_SA_large,StringToken(str,str_end),TSuffixString_compare(m_src_begin,m_src_end));
    else
        return _lower_bound(m_SA_small,StringToken(str,str_end),TSuffixString_compare(m_src_begin,m_src_end));
}
