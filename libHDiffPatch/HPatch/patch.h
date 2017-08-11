//patch.h
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

#ifndef HPatch_patch_h
#define HPatch_patch_h
#include "patch_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//generate newData by patch(oldData + diff)
//  serializedDiff create by create_diff()
hpatch_BOOL patch(unsigned char* out_newData,unsigned char* out_newData_end,
                  const unsigned char* oldData,const unsigned char* oldData_end,
                  const unsigned char* serializedDiff,const unsigned char* serializedDiff_end);

//default once I/O (read/write) max byte size
#ifndef hpatch_kStreamCacheSize
#define hpatch_kStreamCacheSize  (1024)
#endif

//patch by stream , used (hpatch_kStreamCacheSize*7 stack memory) for I/O cache
//  serializedDiff create by create_diff()
//  if use patch_stream_with_cache(), can passing your memory for I/O cache
//  recommended load oldData in memory(and use mem_as_hStreamInput()),random access faster
hpatch_BOOL patch_stream(const hpatch_TStreamOutput* out_newData,
                         const hpatch_TStreamInput*  oldData,
                         const hpatch_TStreamInput*  serializedDiff);

//see patch_stream()
//  limit (temp_cache_end-temp_cache)>=2048
hpatch_BOOL patch_stream_with_cache(const hpatch_TStreamOutput* out_newData,
                                    const hpatch_TStreamInput*  oldData,
                                    const hpatch_TStreamInput*  serializedDiff,
                                    unsigned char* temp_cache,unsigned char* temp_cache_end);

//NOTE:
// when your patch running environment resources are very limited,
//  you can use the following way to achieve the balance between memory usage and speed:
// . recommended use patch_decompress_with_cache(), wrap file handle as file stream,
//     and pass some memory cache to optimize file I/O speed;
// . if patch run slow, you can increase kMinSingleMatchScore, such as 64,256,1024,etc., when you create diff.
//     this can be optimize file random access speed,but diffData will becomes larger!
// . compress plugin recommended use zlib(zlibCompressPlugin\zlibDecompressPlugin),
//     its memory requires very small when decompress;


//get compressedDiff info
//  compressedDiff created by create_compressed_diff()
hpatch_BOOL getCompressedDiffInfo(hpatch_compressedDiffInfo* out_diffInfo,
                                  const hpatch_TStreamInput* compressedDiff);
//see getCompressedDiffInfo()
hpatch_inline static hpatch_BOOL
    getCompressedDiffInfo_mem(hpatch_compressedDiffInfo* out_diffInfo,
                              const unsigned char* compressedDiff,
                              const unsigned char* compressedDiff_end){
        hpatch_TStreamInput  diffStream;
        mem_as_hStreamInput(&diffStream,compressedDiff,compressedDiff_end);
        return getCompressedDiffInfo(out_diffInfo,&diffStream);
    }

    
//patch with decompress plugin, used (hpatch_kStreamCacheSize*5 stack memory) + (decompress*4 used memory)
//  compressedDiff create by create_compressed_diff()
//  decompressPlugin can null when no compressed data in compressedDiff
//  if use patch_decompress_with_cache(), can passing your memory for I/O cache
//  recommended load oldData in memory(and use mem_as_hStreamInput()),random access faster
hpatch_BOOL patch_decompress(const hpatch_TStreamOutput* out_newData,
                             const hpatch_TStreamInput*  oldData,
                             const hpatch_TStreamInput*  compressedDiff,
                             hpatch_TDecompress* decompressPlugin);

//see patch_decompress()
//  limit (temp_cache_end-temp_cache)>=2048
hpatch_BOOL patch_decompress_with_cache(const hpatch_TStreamOutput* out_newData,
                                        const hpatch_TStreamInput*  oldData,
                                        const hpatch_TStreamInput*  compressedDiff,
                                        hpatch_TDecompress* decompressPlugin,
                                        unsigned char* temp_cache,unsigned char* temp_cache_end);

//see patch_decompress(), used (hpatch_kStreamCacheSize*5 stack memory) + (decompress*2 used memory)
//  write newData twice and read newData once,slower than patch_decompress,but memroy requires to be halved.
//  recommended used in limited memory environment
hpatch_BOOL patch_decompress_repeat_out(const hpatch_TStreamOutput* repeat_out_newData,
                                        hpatch_TStreamInput*        in_newData,//streamSize can set 0
                                        const hpatch_TStreamInput*  oldData,
                                        const hpatch_TStreamInput*  compressedDiff,
                                        hpatch_TDecompress*         decompressPlugin);

//see patch_decompress()
hpatch_inline static hpatch_BOOL
    patch_decompress_mem(unsigned char* out_newData,unsigned char* out_newData_end,
                         const unsigned char* oldData,const unsigned char* oldData_end,
                         const unsigned char* compressedDiff,const unsigned char* compressedDiff_end,
                         hpatch_TDecompress* decompressPlugin){
        hpatch_TStreamOutput out_newStream;
        hpatch_TStreamInput  in_newStream;
        hpatch_TStreamInput  oldStream;
        hpatch_TStreamInput  diffStream;
        mem_as_hStreamOutput(&out_newStream,out_newData,out_newData_end);
        mem_as_hStreamInput(&in_newStream,out_newData,out_newData_end);
        mem_as_hStreamInput(&oldStream,oldData,oldData_end);
        mem_as_hStreamInput(&diffStream,compressedDiff,compressedDiff_end);
        return patch_decompress_repeat_out(&out_newStream,&in_newStream,
                                           &oldStream,&diffStream,decompressPlugin);
    }

#ifdef __cplusplus
}
#endif

#endif
