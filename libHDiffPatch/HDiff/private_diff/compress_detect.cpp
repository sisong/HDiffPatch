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
#include <string.h> //memset
#include <stdexcept> //std::runtime_error
namespace hdiff_private{

    template<bool is_sub_on,bool is_out_on>
    size_t _getRegionRleCost(const unsigned char* d,size_t n,const unsigned char* sub,
                             unsigned char* out_nocompress,size_t* nocompressSize){
        size_t bufi=0;
        size_t i=0;
        size_t cost=0;
        while ((i<n)&&(d[i]==(is_sub_on?sub[i]:0)))
            ++i;
        while(i<n){
            unsigned char v=(is_sub_on?(unsigned char)(d[i]-sub[i]):d[i]);
            size_t i0=i; ++i;
            while ((i<n)&&(v==(is_sub_on?(unsigned char)(d[i]-sub[i]):d[i])))
                ++i;
            if ((v==0)|(v==255)){
                ++cost;
            }else{
                cost+=(i-i0<=1)?1:2;
                if (is_out_on)
                    out_nocompress[bufi++]=v;
            }
        }
        if (nocompressSize) *nocompressSize=bufi;
        return cost;
    }

    
size_t getRegionRleCost(const unsigned char* d,size_t n,const unsigned char* sub,
                        unsigned char* out_nocompress,size_t* nocompressSize){
    assert((nocompressSize==0)||(*nocompressSize>=n));
    if (sub){
        if (out_nocompress)
            return _getRegionRleCost<true,true>(d,n,sub,out_nocompress,nocompressSize);
        else
            return _getRegionRleCost<true,false>(d,n,sub,0,nocompressSize);
    }else{
        if (out_nocompress)
            return _getRegionRleCost<false,true>(d,n,0,out_nocompress,nocompressSize);
        else
            return _getRegionRleCost<false,false>(d,n,0,0,nocompressSize);
    }
}

static const size_t kCacheSize=(1<<19);
TCompressDetect::TCompressDetect()
:m_table(0),m_lastChar(-1),
m_lastPopChar(-1),m_cacheBegin(0),m_cacheEnd(0){
    m_mem.realloc(sizeof(TCharConvTable)+kCacheSize);
    m_table=(TCharConvTable*)m_mem.data();
    memset(m_table,0,sizeof(TCharConvTable));
}
TCompressDetect::~TCompressDetect(){
}

void TCompressDetect::_add_rle(const unsigned char* d,size_t n){
    if (m_lastChar<0){
        if (n==0) return;
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

size_t TCompressDetect::_cost_rle(const unsigned char* d,size_t n)const{
    //assert(kCacheSize*2^(kCompressDetectMaxBit-1)<2^32);
    const unsigned int kCompressDetectDivBit=12;
    const unsigned int kCompressDetectMaxBit=12;
    if (n==0) return 0;
    if (m_lastChar<0) return n;
    size_t        codeSize=0;
    unsigned char last=m_lastChar;
    for (size_t i=0;i<n;++i) {
        unsigned char cur=d[i];
        size_t rab=m_table->sum2char[last*256+cur];
        size_t ra =1+m_table->sum1char[last];
        last=cur;
        if ((rab<<(kCompressDetectMaxBit-1))>ra){
            if ((rab<<8)<ra) { rab<<=8; codeSize+=8; }
            if ((rab<<4)<ra) { rab<<=4; codeSize+=4; }
            if ((rab<<2)<ra) { rab<<=2; codeSize+=2; }
            if ((rab<<1)<ra) { codeSize+=1; }
            ++codeSize;
        }else{
            codeSize+=kCompressDetectMaxBit;
        }
    }
    return (codeSize+((kCompressDetectDivBit>>1)+1))/kCompressDetectDivBit;
}

static const size_t _kBufSize=1024;
#define by_step(call)     \
    size_t rleCtrlCost=0; \
    unsigned char rcode[_kBufSize]; \
    while (n>0) {         \
        size_t readLen=_kBufSize;   \
        if (readLen>n) readLen=n;   \
        size_t rcodeLen=readLen;\
        rleCtrlCost+=getRegionRleCost(d,readLen,sub,rcode,&rcodeLen);\
        rleCtrlCost-=rcodeLen;  \
        call(rcode,rcodeLen);   \
        d+=readLen;             \
        if (sub) sub+=readLen;  \
        n-=readLen;             \
    }  \


void TCompressDetect::add_chars(const unsigned char* d,size_t n,const unsigned char* sub){
    by_step(this->_add_rle);
}

size_t TCompressDetect::cost(const unsigned char* d,size_t n,const unsigned char* sub)const{
    size_t result=0;
    by_step(result+=this->_cost_rle);
    return result+rleCtrlCost;
}

}//namespace hdiff_private
