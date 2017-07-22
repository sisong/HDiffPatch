//diff.h
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

#ifndef HDiff_diff_h
#define HDiff_diff_h

#include <vector>
#include "../HPatch/patch_types.h"

//生成diff数据.
void create_diff(const unsigned char* newData,const unsigned char* newData_end,
                 const unsigned char* oldData,const unsigned char* oldData_end,
                 std::vector<unsigned char>& out_diff);
//检查生成的序列化的diff数据是否正确.
bool check_diff(const unsigned char* newData,const unsigned char* newData_end,
                const unsigned char* oldData,const unsigned char* oldData_end,
                const unsigned char* diff,const unsigned char* diff_end);


//create_compressed_diff() diff with compress plugin
#ifdef __cplusplus
extern "C"
{
#endif
    
    //压缩插件接口定义.
    typedef struct hdiff_TCompress{
        const char*  (*compressType)(const hdiff_TCompress* compressPlugin);
        size_t  (*maxCompressedSize)(const hdiff_TCompress* compressPlugin,size_t dataSize);
        //压缩成功返回实际后压缩数据大小,失败返回0.
        size_t           (*compress)(const hdiff_TCompress* compressPlugin,
                                     unsigned char* out_code,unsigned char* out_code_end,
                                     const unsigned char* data,const unsigned char* data_end);
    } hdiff_TCompress;
    
    #define  hdiff_kNocompressPlugin ((const hdiff_TCompress*)0)  //不压缩数据的“压缩”插件.
    
#ifdef __cplusplus
}
#endif
//支持压缩插件的diff; 需要对应的支持解压缩的patch配合.
void create_compressed_diff(const unsigned char* newData,const unsigned char* newData_end,
                            const unsigned char* oldData,const unsigned char* oldData_end,
                            std::vector<unsigned char>& out_diff,
                            const hdiff_TCompress* compressPlugin);
//检查生成的压缩的diff数据是否正确.
bool check_compressed_diff(const unsigned char* newData,const unsigned char* newData_end,
                           const unsigned char* oldData,const unsigned char* oldData_end,
                           const unsigned char* diff,const unsigned char* diff_end,
                           hpatch_TDecompress* decompressPlugin);

#endif
