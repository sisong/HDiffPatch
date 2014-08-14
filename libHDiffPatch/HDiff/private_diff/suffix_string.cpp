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

static bool getStringIsLess(const char* str0,const char* str0End,const char* str1,const char* str1End){
    TSAInt L0=(TSAInt)(str0End-str0);
    TSAInt L1=(TSAInt)(str1End-str1);
#ifdef IS_TEST_USE_STD_SORT
    const int kMaxCmpLength=1024*4;
    if (L0>kMaxCmpLength) L0=kMaxCmpLength;
    if (L1>kMaxCmpLength) L1=kMaxCmpLength;
#endif
    TSAInt LMin;
    if (L0<L1) LMin=L0; else LMin=L1;
    for (int i=0; i<LMin; ++i){
        TSAInt sub=TSAInt(((unsigned char*)str0)[i])-TSAInt(((unsigned char*)str1)[i]);
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
     inline bool operator()(const TSAInt s0,const StringToken& s1)const{
         return getStringIsLess(m_begin+s0,m_end,s1.begin,s1.end);
    }
    inline bool operator()(const StringToken& s0,const TSAInt s1)const{
        return getStringIsLess(s0.begin,s0.end,m_begin+s1,m_end);
    }
    inline bool operator()(const TSAInt s0,const TSAInt s1)const{
        return getStringIsLess(m_begin+s0,m_end,m_begin+s1,m_end);
    }
private:
    const char* m_begin;
    const char* m_end;
};

static void _suffixString_create(const char* src,const char* src_end,TSuffixString::TSArray& out_sstring){
    TSAInt size=(TSAInt)(src_end-src);
    out_sstring.resize(size);
    if (size<=0) return;
    
#ifdef IS_TEST_USE_STD_SORT //test uses std::sort, but slow
    for (TSAInt i=0;i<size;++i) out_sstring[i]=i;
    std::sort<TSAInt*,const TSuffixString_compare&>(&out_sstring[0],&out_sstring[0]+size,TSuffixString_compare(src,src_end));
#else
    if (saisxx((const unsigned char*)src, &out_sstring[0], size) !=0)
        throw std::runtime_error("suffixString_create() error.");
#endif
}

}//end namespace

TSuffixString::TSuffixString(const char* _src_begin,const char* _src_end)
:src_begin(_src_begin),src_end(_src_end){
    _suffixString_create(src_begin,src_end,*(TSArray*)&SA);
}

TSAInt TSuffixString::lower_bound(const char* str,const char* str_end)const{
    if (SA.empty()) return 0;
    const TSAInt* SA0=&SA[0];
    const TSAInt* pos=std::lower_bound<const TSAInt*,StringToken,const TSuffixString_compare&>
            (SA0,SA0+SA.size(),StringToken(str,str_end),TSuffixString_compare(src_begin,src_end));
    return (TSAInt)(pos-SA0);
}

