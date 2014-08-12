//
//  pack_uint.h
//  create by housisong
//
/*
 Copyright (c) 2012-2014 HouSisong All Rights Reserved.
 (The MIT License)

 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation
 files (the "Software"), to deal in the Software without
 restriction, including without limitation the rights to use,
 copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following
 conditions:

 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef __PACK_UINT_H_
#define __PACK_UINT_H_

#include <vector>

//变长正整数编码方案(x bit额外类型标志位,x<=7),从高位开始输出1--n byte:
// x0*  7-x bit
// x1* 0*  7+7-x bit
// x1* 1* 0*  7+7+7-x bit
// x1* 1* 1* 0*  7+7+7+7-x bit
// x1* 1* 1* 1* 0*  7+7+7+7+7-x bit
// ...
template<class TUInt>
static void packUIntWithTag(std::vector<unsigned char>& out_code,TUInt iValue,int highBit,const int kTagBit){//写入并前进指针.
    const int kPackMaxTagBit=7;
    assert((0<=kTagBit)&&(kTagBit<=kPackMaxTagBit));
    assert((highBit>>kTagBit)==0);
    const int kMaxPackUIntByteSize=(kPackMaxTagBit+sizeof(TUInt)*8+6)/7;
    const TUInt kMaxValueWithTag=(1<<(7-kTagBit))-1;

    unsigned char codeBuf[kMaxPackUIntByteSize];
    unsigned char* codeEnd=codeBuf;
    while (iValue>kMaxValueWithTag) {
        *codeEnd=iValue&((1<<7)-1); ++codeEnd;
        iValue>>=7;
    }
    out_code.push_back((unsigned char)( (highBit<<(8-kTagBit)) | iValue | (((codeBuf!=codeEnd)?1:0)<<(7-kTagBit))  ));
    while (codeBuf!=codeEnd) {
        --codeEnd;
        out_code.push_back((*codeEnd) | (((codeBuf!=codeEnd)?1:0)<<7));
    }
}

template<class TUInt>
inline static void packUInt(std::vector<unsigned char>& out_code,TUInt iValue){
    packUIntWithTag(out_code,iValue,0,0);
}

#endif //__PACK_UINT_H_
