//  mem_buf.h
//
/*
 The MIT License (MIT)
 Copyright (c) 2012-2019 HouSisong
 
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

#ifndef __mem_buf_h
#define __mem_buf_h
#include <stdlib.h> //malloc free
#include <string.h> //size_t
#include <stdexcept>  //std::runtime_error
namespace hdiff_private{

    struct TAutoMem{
        inline explicit TAutoMem(size_t size=0) :_data(0),_data_end(0){ realloc(size); }
        inline ~TAutoMem(){ clear(); }
        inline unsigned char* data(){ return _data; }
        inline unsigned char* data_end(){ return _data_end; }
        inline size_t size()const{ return (size_t)(_data_end-_data); }
        inline void clear(){ if (_data) { free(_data); _data=0; _data_end=0; } }
        inline void realloc(size_t newSize){
            if (newSize!=size()){
                clear();
                if (newSize>0){
                    _data=(unsigned char*)malloc(newSize);
                    if (0==_data) throw std::runtime_error("TAutoMem::TAutoMem() malloc() error!");
                    _data_end=_data+newSize;
                }
            }
        }
    private:
        unsigned char*  _data;
        unsigned char*  _data_end;
    };

}//namespace hdiff_private
#endif //__mem_buf_h
