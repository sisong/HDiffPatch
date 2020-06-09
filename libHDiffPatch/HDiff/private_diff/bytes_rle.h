//bytes_rle.h
//快速解压的一个通用字节流压缩rle算法.
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

#ifndef __BYTES_RLE_H_
#define __BYTES_RLE_H_
#include <string.h>
#include <vector>
namespace hdiff_private{

enum TRleParameter{ kRle_bestSize=1, kRle_default=3, kRle_bestUnRleSpeed=32 };

void bytesRLE_save(std::vector<unsigned char>& out_code,
                   const unsigned char* src,const unsigned char* src_end,
                   int rle_parameter=kRle_default);


void bytesRLE_save(std::vector<unsigned char>& out_ctrlBuf,std::vector<unsigned char>& out_codeBuf,
                   const unsigned char* src,const unsigned char* src_end,int rle_parameter);

#ifndef kMaxBytesRle0Len
    static const size_t kMaxBytesRle0Len =(size_t)(((size_t)1<<31)-1);
#endif
    
    struct TSangileStreamRLE0{
        std::vector<unsigned char>  fixed_code;
        std::vector<unsigned char>  uncompressData;
        size_t                      len0;
        inline TSangileStreamRLE0():len0(0){}
        inline size_t curCodeSize() const { return maxCodeSize(0,0); }
        size_t maxCodeSize(const unsigned char* appendData,const unsigned char* appendData_end) const;
        void append(const unsigned char* appendData,const unsigned char* appendData_end);
        void finishAppend();
        inline void clear() { fixed_code.clear(); uncompressData.clear(); len0=0; }
    };
    
}//namespace hdiff_private
#endif //__BYTES_RLE_H_
