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

static const int kMinSingleMatchScore_default = 6;

//create a diff data between oldData and newData
//  out_diff is uncompressed, you can use create_compressed_diff()
//       or create_compressed_diff_stream() create compressed diff data
//  recommended always use create_compressed_diff() replace create_diff()
//  kMinSingleMatchScore: default 6, bin: 0--4  text: 4--9
void create_diff(const unsigned char* newData,const unsigned char* newData_end,
                 const unsigned char* oldData,const unsigned char* oldData_end,
                 std::vector<unsigned char>& out_diff,
                 int kMinSingleMatchScore=kMinSingleMatchScore_default);

//return patch(oldData+diff)==newData?
bool check_diff(const unsigned char* newData,const unsigned char* newData_end,
                const unsigned char* oldData,const unsigned char* oldData_end,
                const unsigned char* diff,const unsigned char* diff_end);


#ifdef __cplusplus
extern "C"
{
#endif
    
    //compress plugin
    typedef struct hdiff_TCompress{
        //return type tag; strlen(result)<=hpatch_kMaxCompressTypeLength;（Note:result lifetime）
        const char*  (*compressType)(const hdiff_TCompress* compressPlugin);
        //return the max compressed size, if input dataSize data;
        size_t  (*maxCompressedSize)(const hdiff_TCompress* compressPlugin,size_t dataSize);
        //compress data to out_code; return compressed size, if error or not need compress then return 0;
        size_t           (*compress)(const hdiff_TCompress* compressPlugin,
                                     unsigned char* out_code,unsigned char* out_code_end,
                                     const unsigned char* data,const unsigned char* data_end);
    } hdiff_TCompress;
    
#ifdef __cplusplus
}
#endif

//create a compressed diffData between oldData and newData
//  out_diff compressed by compressPlugin
//  kMinSingleMatchScore: default 6, bin: 0--4  text: 4--9
void create_compressed_diff(const unsigned char* newData,const unsigned char* newData_end,
                            const unsigned char* oldData,const unsigned char* oldData_end,
                            std::vector<unsigned char>& out_diff,
                            const hdiff_TCompress* compressPlugin=0,
                            int kMinSingleMatchScore=kMinSingleMatchScore_default);
//return patch_decompress(oldData+diff)==newData?
bool check_compressed_diff(const unsigned char* newData,const unsigned char* newData_end,
                           const unsigned char* oldData,const unsigned char* oldData_end,
                           const unsigned char* diff,const unsigned char* diff_end,
                           hpatch_TDecompress* decompressPlugin);

//see check_compressed_diff
bool check_compressed_diff_stream(const hpatch_TStreamInput*  newData,
                                  const hpatch_TStreamInput*  oldData,
                                  const hpatch_TStreamInput*  compressed_diff,
                                  hpatch_TDecompress* decompressPlugin);

#ifdef __cplusplus
extern "C"
{
#endif

typedef void*  hdiff_compressHandle;
typedef hpatch_TStreamOutput hdiff_TStreamOutput;
typedef hpatch_TStreamInput  hdiff_TStreamInput;
#define hdiff_kStreamOutputCancel 0
//stream compress plugin
typedef struct hdiff_TStreamCompress{
    //return type tag; strlen(result)<=hpatch_kMaxCompressTypeLength;（Note:result lifetime）
    const char*           (*compressType)(const hdiff_TStreamCompress* compressPlugin);
    
    //compress data to out_code; return compressed size, if error or not need compress then return 0;
    //if out_code->write() return hdiff_stream_kCancelCompress then return 0;
    hpatch_StreamPos_t (*compress_stream)(const hdiff_TStreamCompress* compressPlugin,
                                          const hdiff_TStreamOutput*   out_code,
                                          const hdiff_TStreamInput*    in_data);
} hdiff_TStreamCompress;

#ifdef __cplusplus
}
#endif

//diff by stream:
//  can control memory requires and run speed by different kMatchBlockSize value,
//      but out_diff size is larger than create_compressed_diff()
//  recommended used in limited environment or support large file
//  kMatchBlockSize: recommended (1<<3)--(1<<14)
//    if increase kMatchBlockSize then run faster and require less memory, but out_diff size increase
//  NOTICE: out_diff->write()'s writeToPos may be back to update headData!
//  throw std::runtime_error when I/O error,etc.
static const int kMatchBlockSize_default = (1<<7);
void create_compressed_diff_stream(const hpatch_TStreamInput* newData,
                                   const hpatch_TStreamInput* oldData,
                                   hpatch_TStreamOutput*      out_diff,
                                   hdiff_TStreamCompress* compressPlugin=0,
                                   size_t kMatchBlockSize=kMatchBlockSize_default);

#endif
