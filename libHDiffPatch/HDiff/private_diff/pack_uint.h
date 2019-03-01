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
#include <stdexcept>  //std::runtime_error
#include <string>
namespace hdiff_private{

template<class _UInt>
inline static void packUIntWithTag(std::vector<unsigned char>& out_code,_UInt uValue,
                                   int highTag,const int kTagBit){
    unsigned char  codeBuf[hpatch_kMaxPackedUIntBytes];
    unsigned char* codeEnd=codeBuf;
    if (!hpatch_packUIntWithTag(&codeEnd,codeBuf+hpatch_kMaxPackedUIntBytes,uValue,highTag,kTagBit))
        throw std::runtime_error("packUIntWithTag<_UInt>() hpatch_packUIntWithTag() error!");
    out_code.insert(out_code.end(),codeBuf,codeEnd);
}

template<class _UInt>
inline static void packUInt(std::vector<unsigned char>& out_code,_UInt uValue){
    packUIntWithTag(out_code,uValue,0,0);
}

    
inline static void pushBack(std::vector<unsigned char>& out_buf,
                            const unsigned char* data,const unsigned char* data_end){
    out_buf.insert(out_buf.end(),data,data_end);
}
inline static void pushBack(std::vector<unsigned char>& out_buf,const std::vector<unsigned char>& data){
    out_buf.insert(out_buf.end(),data.begin(),data.end());
}
inline static void pushCStr(std::vector<unsigned char>& out_buf,const char* cstr){
    const unsigned char* data=(const unsigned char*)cstr;
    pushBack(out_buf,data,data+strlen(cstr));
}
inline static void pushString(std::vector<unsigned char>& out_buf,const std::string& str){
    const unsigned char* data=(const unsigned char*)str.c_str();
    pushBack(out_buf,data,data+str.size());
}
    
struct TPlaceholder{
    hpatch_StreamPos_t pos;
    hpatch_StreamPos_t pos_end;
    inline TPlaceholder(hpatch_StreamPos_t _pos,hpatch_StreamPos_t _pos_end)
    :pos(_pos),pos_end(_pos_end){ assert(_pos<=_pos_end); }
    inline hpatch_StreamPos_t size()const{ return pos_end-pos; }
};

hpatch_inline static
void packUInt_fixSize(unsigned char* out_code,unsigned char* out_code_fixEnd,
                      hpatch_StreamPos_t uValue){
    if (out_code>=out_code_fixEnd)
        throw std::runtime_error("packUInt_fixSize() out_code size error!");
    --out_code_fixEnd;
    *out_code_fixEnd=uValue&((1<<7)-1); uValue>>=7;
    while (out_code<out_code_fixEnd) {
        --out_code_fixEnd;
        *out_code_fixEnd=(uValue&((1<<7)-1)) | (1<<7);  uValue>>=7;
    }
    if (uValue!=0)
        throw std::runtime_error("packUInt_fixSize() out_code too small error!");
}

}//namespace hdiff_private
#endif //__PACK_UINT_H_
