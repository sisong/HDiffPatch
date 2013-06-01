//
//  diff_base.h
//  HDiff
//
/*
 This is the HDiff copyright.
 
 Copyright (c) 2012-2013 HouSisong All Rights Reserved.
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

#ifndef HDiff_diff_base_h
#define HDiff_diff_base_h

#include "assert.h"
#include <vector>


namespace hdiffpatch {

    #include "../HPatch/patch_base.h"

//变长32bit正整数编码方案(x bit额外类型标志位,x<=3),从高位开始输出1-5byte:
// x0*  7-x bit
// x1* 0*  7+7-x bit
// x1* 1* 0*  7+7+7-x bit
// x1* 1* 1* 0*  7+7+7+7-x bit
// x1* 1* 1* 1* 0*  7+7+7+7+7-x bit

static void pack32BitWithTag(std::vector<TByte>& out_code,TUInt32 iValue,int highBit,const int kTagBit){//写入并前进指针.
    const int kMaxPack32BitTagBit=3;
    assert((0<=kTagBit)&&(kTagBit<=kMaxPack32BitTagBit));
    assert((highBit>>kTagBit)==0);
    const int kMaxPack32BitSize=5;
    const unsigned int kMaxValueWithTag=(1<<(7-kTagBit))-1;

    TByte codeBuf[kMaxPack32BitSize];
    TByte* codeEnd=codeBuf;
    while (iValue>kMaxValueWithTag) {
        *codeEnd=iValue&((1<<7)-1); ++codeEnd;
        iValue>>=7;
    }
    out_code.push_back( (highBit<<(8-kTagBit)) | iValue | (((codeBuf!=codeEnd)?1:0)<<(7-kTagBit)));
    while (codeBuf!=codeEnd) {
        --codeEnd;
        out_code.push_back((*codeEnd) | (((codeBuf!=codeEnd)?1:0)<<7));
    }
}

inline static void pack32Bit(std::vector<TByte>& out_code,TUInt32 iValue){
    pack32BitWithTag(out_code,iValue,0,0);
}

}//end namespac hdiffpatch

#endif
