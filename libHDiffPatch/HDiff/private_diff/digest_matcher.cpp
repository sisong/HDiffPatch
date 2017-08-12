//digest_macher.cpp
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

#include "digest_matcher.h"
#include <stdexcept>

#define _adler32_BASE 65521

//#define _adler32_border(v){ if ((v) >= _adler32_BASE) (v) -= _adler32_BASE; }
#define _adler32_border(v){ (v) -= _adler32_BASE & (uint32_t)( ((_adler32_BASE-1)-(int32_t)(v))>>31 ); }

#define _adler32_add1(adler,sum,byteData){ \
    (adler) += (byteData); \
    (sum)   += (adler);    \
}
#define _adler32_add4(adler,sum,data,i){ \
    _adler32_add1(adler,sum,data[i  ]); \
    _adler32_add1(adler,sum,data[i+1]); \
    _adler32_add1(adler,sum,data[i+2]); \
    _adler32_add1(adler,sum,data[i+3]); \
}

static uint32_t adler32(unsigned char* data,size_t n){
    uint32_t adler=1;
    uint32_t sum=0;
    while (n>=16) {
        _adler32_add4(adler,sum,data,0);
        _adler32_add4(adler,sum,data,4);
        _adler32_add4(adler,sum,data,8);
        _adler32_add4(adler,sum,data,12);
        sum%=_adler32_BASE;
        _adler32_border(adler);
        data+=16;
        n-=16;
    }
    for (size_t i=0; i<n; ++i) {
        _adler32_add1(adler,sum,data[i]);
        _adler32_border(sum);
    }
    _adler32_border(adler);
    return adler | (sum<<16);
}


TDigestMatcher::TDigestMatcher(const hpatch_TStreamInput* data,int kMatchNodeSizeBit)
:m_data(data),m_kMatchNodeSizeBit(kMatchNodeSizeBit){
    if ((m_kMatchNodeSizeBit<=0)||(m_kMatchNodeSizeBit>=31))
        throw std::runtime_error("TDataDigest() kMatchNodeSizeBit error.");
    
    size_t matchNodeSize=((size_t)1)<<m_kMatchNodeSizeBit;
    while ((matchNodeSize>0)&&(matchNodeSize>=m_data->streamSize)) {
        matchNodeSize>>=1;
        --m_kMatchNodeSizeBit;
    }
    const size_t nodeCount=(m_data->streamSize+matchNodeSize-1)/matchNodeSize;
    m_nodes.resize(nodeCount);
    
    size_t kDataBufSize=1024*64;
    if (kDataBufSize<matchNodeSize) kDataBufSize=matchNodeSize;
    m_buf.resize(kDataBufSize);
    getDigests();
}


void TDigestMatcher::getDigests(){
    const size_t nodeCount=m_nodes.size();
    size_t matchNodeSize=((size_t)1)<<m_kMatchNodeSizeBit;
    unsigned char* buf=&m_buf[0];
    for (size_t i=0; i<nodeCount; ++i) {
        size_t readLen=matchNodeSize;
        if (i==nodeCount-1){
            readLen=m_data->streamSize & (matchNodeSize-1);
            memset(buf+readLen,0,matchNodeSize-readLen);
        }
        if (readLen!=m_data->read(m_data->streamHandle,i<<m_kMatchNodeSizeBit,buf,buf+readLen))
            throw std::runtime_error("TDataDigest::getDigests() data->read() error.");
        m_nodes[i]=adler32(buf,matchNodeSize);
    }
}
