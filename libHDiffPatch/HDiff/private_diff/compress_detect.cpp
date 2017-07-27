//compress_detect.cpp
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

#include "compress_detect.h"
#include <assert.h>
#include <string.h> //memset
#include <stdlib.h> //malloc free

size_t getRegionRelCost(const unsigned char* d,size_t n,const unsigned char* sub,
                        unsigned char* out_nocompress,size_t* nocompressSize){
    assert((nocompressSize==0)||(*nocompressSize>=n));
    size_t bufi=0;
    size_t i=0;
    while ((i<n)&&(d[i]==(sub?sub[i]:0)))
        ++i;
    size_t cost=0;
    while(i<n){
        unsigned char v=(unsigned char)(d[i]-(sub?sub[i]:0));
        size_t i0=i; ++i;
        while ((i<n)&&(v==(unsigned char)(d[i]-(sub?sub[i]:0))))
            ++i;
        if ((v==0)|(v==255)){
            ++cost;
        }else{
            cost+=(i-i0<=1)?1:2;
            if (out_nocompress)
                out_nocompress[bufi++]=v;
        }
    }
    if (nocompressSize) *nocompressSize=bufi;
    return cost;
}

static const size_t kCacheSize=1024*1024*2;
TCompressDetect::TCompressDetect()
:m_table(0),m_lastChar(-1),
m_lastPopChar(-1),m_cacheBegin(0),m_cacheEnd(0){
    m_table=(TCharConvTable*)malloc(sizeof(TCharConvTable)+kCacheSize);
    memset(m_table,0,sizeof(TCharConvTable));
}
TCompressDetect::~TCompressDetect(){
    free(m_table);
    m_table=0;
}


void TCompressDetect::_add_rle(const unsigned char* d,size_t n){
    if (n==0) return;
    if (m_lastChar<0){
        m_lastChar=d[0];
        ++m_table->sum1char[m_lastChar];
        ++d;
        --n;
    }
    for (size_t i=0;i<n;++i) {
        unsigned char cur=d[i];
        ++m_table->sum1char[cur];
        ++m_table->sum2char[m_lastChar*256+cur];
        m_lastChar=cur;
        
        if (m_table->sum==kCacheSize){
            --m_table->sum;
            unsigned char pop=m_table->cache[m_cacheBegin++];
            if (m_cacheBegin==kCacheSize) m_cacheBegin=0;
            --m_table->sum1char[pop];
            if (m_lastPopChar>=0){
                --m_table->sum2char[m_lastPopChar*256+pop];
            }
            m_lastPopChar=pop;
        }
        ++m_table->sum;
        m_table->cache[m_cacheEnd++]=cur;
        if (m_cacheEnd==kCacheSize) m_cacheEnd=0;
    }
}

#include <stdio.h>
int kCompressDetectDivBit=12;
int kCompressDetectMaxBit=8;
size_t TCompressDetect::_cost_rle(const unsigned char* d,size_t n)const{
    if (n==0) return 0;
    if (m_lastChar<0) return n;
    size_t        codeSize=0;
    unsigned char last=m_lastChar;
    for (size_t i=0;i<n;++i) {
        unsigned char cur=d[i];
        size_t rab=m_table->sum2char[last*256+cur];
        int bit=kCompressDetectMaxBit;
        if (rab>0){
            bit=1;
            size_t ra=m_table->sum1char[last];
            while(rab<(ra>>=1))
                ++bit;
            //printf("%ld,%d:%d ",rab,m_table->sum1char[last],bit);
        }
        codeSize+=(bit<=kCompressDetectMaxBit)?bit:kCompressDetectMaxBit;
        last=cur;
    }
    return (codeSize+(kCompressDetectDivBit>>1))/kCompressDetectDivBit;
}

static const size_t kBufSize=1024*4;
#define by_step(call)  {\
    unsigned char rcode[kBufSize];  \
    while (n>0) {   \
        size_t readLen=kBufSize;    \
        if (readLen>n) readLen=n;   \
        size_t rcodeLen=readLen;\
        getRegionRelCost(d,readLen,sub,rcode,&rcodeLen);\
        call(rcode,rcodeLen);   \
        d+=readLen;             \
        if (sub) sub+=readLen;  \
        n-=readLen;             \
    }  \
}

void TCompressDetect::add_chars(const unsigned char* d,size_t n,const unsigned char* sub){
    by_step(this->_add_rle);
}

size_t TCompressDetect::cost(const unsigned char* d,size_t n,const unsigned char* sub)const{
    size_t result=0;
    by_step(result+=this->_cost_rle);
    return result;
}



