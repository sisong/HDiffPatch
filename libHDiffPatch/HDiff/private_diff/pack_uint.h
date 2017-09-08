//pack_uint.h
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
#include "../../HPatch/patch_types.h" //hpatch_packUIntWithTag
namespace hdiff_private{

template<class _UInt>
inline static void packUIntWithTag(std::vector<unsigned char>& out_code,_UInt uValue,
                                   int highTag,const int kTagBit){
    unsigned char  codeBuf[hpatch_kMaxPackedUIntBytes];
    unsigned char* codeEnd=codeBuf;
    if (!hpatch_packUIntWithTag(&codeEnd,codeBuf+hpatch_kMaxPackedUIntBytes,
                                uValue,highTag,kTagBit)) throw uValue;
    out_code.insert(out_code.end(),codeBuf,codeEnd);
}

template<class _UInt>
inline static void packUInt(std::vector<unsigned char>& out_code,_UInt uValue){
    packUIntWithTag(out_code,uValue,0,0);
}

}//namespace hdiff_private
#endif //__PACK_UINT_H_
