//bytes_rle.h
//快速解压的一个通用字节流压缩rle算法.
//
/*
 The MIT License (MIT)
 Copyright (c) 2012-2021 HouSisong
 
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

#ifndef __BYTES_RLE_H_
#define __BYTES_RLE_H_
#include <string.h>
#include <vector>
#include "../../HPatch/patch_types.h" //hpatch_TStreamInput
namespace hdiff_private{

#ifndef kMaxBytesRleLen
    static const size_t kMaxBytesRleLen =(size_t)(((size_t)1<<31)-1);
#endif

enum TRleParameter{ kRle_bestSize=1, kRle_default=3, kRle_bestUnRleSpeed=32 };

void bytesRLE_save(std::vector<unsigned char>& out_code,
                   const unsigned char* src,const unsigned char* src_end,
                   int rle_parameter=kRle_default);
void bytesRLE_save(std::vector<unsigned char>& out_code,
                   const hpatch_TStreamInput* src,//sequential read
                   int rle_parameter=kRle_default);

void bytesRLE_save(std::vector<unsigned char>& out_ctrlBuf,std::vector<unsigned char>& out_codeBuf,
                   const unsigned char* src,const unsigned char* src_end,int rle_parameter);
void bytesRLE_save(std::vector<unsigned char>& out_ctrlBuf,std::vector<unsigned char>& out_codeBuf,
                   const hpatch_TStreamInput* src,//sequential read
                   int rle_parameter);
    
    struct TSingleStreamRLE0{
        std::vector<unsigned char>  fixed_code;
        std::vector<unsigned char>  uncompressData;
        hpatch_StreamPos_t          len0;
        inline TSingleStreamRLE0():len0(0){}
        inline hpatch_StreamPos_t curCodeSize() const { return maxCodeSize(0,0); }
        hpatch_StreamPos_t maxCodeSize(const unsigned char* appendData,const unsigned char* appendData_end) const;
        hpatch_StreamPos_t maxCodeSize(const hpatch_TStreamInput* appendData) const;//sequential read
        hpatch_StreamPos_t maxCodeSizeByZeroLen(hpatch_StreamPos_t appendZeroLen) const;
        void append(const unsigned char* appendData,const unsigned char* appendData_end);
        void append(const hpatch_TStreamInput* appendData);//sequential read
        void appendByZeroLen(hpatch_StreamPos_t appendZeroLen);
        void finishAppend();
        inline void clear() { fixed_code.clear(); uncompressData.clear(); len0=0; }
    };
    
}//namespace hdiff_private
#endif //__BYTES_RLE_H_
