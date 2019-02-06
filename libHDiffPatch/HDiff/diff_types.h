//diff_types.h
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

#ifndef HDiff_diff_types_h
#define HDiff_diff_types_h
#include "../HPatch/patch_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef hpatch_TStreamOutput hdiff_TStreamOutput;
    typedef hpatch_TStreamInput  hdiff_TStreamInput;
    //compress plugin
    typedef struct hdiff_TCompress{
        //return type tag; strlen(result)<=hpatch_kMaxPluginTypeLength; (Note:result lifetime)
        const char*             (*compressType)(void);//ascii cstring,cannot contain '&'
        //return the max compressed size, if input dataSize data;
        hpatch_StreamPos_t (*maxCompressedSize)(hpatch_StreamPos_t dataSize);
        //return support threadNumber
        int          (*setParallelThreadNumber)(hdiff_TCompress* compressPlugin,int threadNum);
        //compress data to out_code; return compressed size, if error or not need compress then return 0;
        //if out_code->write() return hdiff_stream_kCancelCompress(error) then return 0;
        //if memory I/O can use hdiff_compress_mem()
        hpatch_StreamPos_t          (*compress)(const hdiff_TCompress* compressPlugin,
                                                const hdiff_TStreamOutput*   out_code,
                                                const hdiff_TStreamInput*    in_data);
    } hdiff_TCompress;
    
    static hpatch_inline
    size_t hdiff_compress_mem(const hdiff_TCompress* compressPlugin,
                              unsigned char* out_code,unsigned char* out_code_end,
                              const unsigned char* data,const unsigned char* data_end){
        hdiff_TStreamOutput  codeStream;
        hdiff_TStreamInput   dataStream;
        mem_as_hStreamOutput(&codeStream,out_code,out_code_end);
        mem_as_hStreamInput(&dataStream,data,data_end);
        hpatch_StreamPos_t codeLen=compressPlugin->compress(compressPlugin,&codeStream,&dataStream);
        if (codeLen!=(size_t)codeLen) return 0; //error
        return (size_t)codeLen;
    }
    
#ifdef __cplusplus
}
#endif
#endif //HDiff_diff_types_h
