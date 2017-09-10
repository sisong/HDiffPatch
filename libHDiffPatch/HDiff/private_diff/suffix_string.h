//suffix_string.h
//后缀字符串的一个实现.
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

#ifndef __SUFFIX_STRING_H_
#define __SUFFIX_STRING_H_
#include <vector>
#include <stddef.h> //for ptrdiff_t,size_t
#if defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
#   include <stdint.h> //for int32_t
namespace hdiff_private{
#else
namespace hdiff_private{
#   if (_MSC_VER >= 1300)
    typedef signed __int32 int32_t;
#   else
    typedef signed int     int32_t;
#   endif
#endif

class TSuffixString{
public:
    typedef ptrdiff_t     TInt;
    typedef int32_t       TInt32;
    typedef unsigned char TChar;
    TSuffixString();
    ~TSuffixString();
    
    //throw std::runtime_error when create SA error
    TSuffixString(const TChar* src_begin,const TChar* src_end);
    void resetSuffixString(const TChar* src_begin,const TChar* src_end);

    inline const TChar* src_begin()const{ return m_src_begin; }
    inline const TChar* src_end()const{ return m_src_end; }
    inline size_t SASize()const{ return (size_t)(m_src_end-m_src_begin); }
    void clear();

    inline TInt SA(TInt i)const{//return m_SA[i];//排好序的后缀字符串数组.
        if (isUseLargeSA())
            return m_SA_large[i];
        else
            return (TInt)m_SA_limit[i];
    }
    TInt lower_bound(const TChar* str,const TChar* str_end)const;//return index in SA
private:
    const TChar*        m_src_begin;//原字符串.
    const TChar*        m_src_end;
    std::vector<TInt32> m_SA_limit;
    std::vector<TInt>   m_SA_large;
    enum{ kLimitSASize= (1<<30)-1 + (1<<30) };//2G-1
    inline bool isUseLargeSA()const{
        return (sizeof(TInt)>sizeof(TInt32)) && (SASize()>kLimitSASize);
    }
private:
    const void*         m_cached_SA_begin;
    const void*         m_cached_SA_end;
    const void*         m_cached1char_range[256*2];
    void**              m_cached2char_range;//[256*256*2]
    typedef TInt (*t_lower_bound_func)(const void* rbegin,const void* rend,
                                       const TChar* str,const TChar* str_end,
                                       const TChar* src_begin,const TChar* src_end,
                                       const void* SA_begin,size_t min_eq);
    t_lower_bound_func  m_lower_bound;
    void                build_cache();
    void                clear_cache();
};

}//namespace hdiff_private
#endif //__SUFFIX_STRING_H_
