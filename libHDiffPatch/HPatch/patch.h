//patch.h
//
/*
 The MIT License (MIT)
 Copyright (c) 2012-2018 HouSisong
 
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

//  all patch*() functions do not allocate memory

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
//  if use patch_stream_with_cache(), can passing more memory for I/O cache
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


//get compressedDiff info
//  compressedDiff created by create_compressed_diff() or create_compressed_diff_stream()
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
//  compressedDiff create by create_compressed_diff() or create_compressed_diff_stream()
//  decompressPlugin can null when no compressed data in compressedDiff
//  if use patch_decompress_with_cache(), can passing larger memory cache to optimize speed;
//   or recommended load oldData in memory(and use mem_as_hStreamInput()) to optimize speed.
hpatch_BOOL patch_decompress(const hpatch_TStreamOutput* out_newData,
                             const hpatch_TStreamInput*  oldData,
                             const hpatch_TStreamInput*  compressedDiff,
                             hpatch_TDecompress* decompressPlugin);

    
//ON: for patch_decompress_with_cache(), preparatory load part of oldData into cache,
//  cache memory size (temp_cache_end-temp_cache) the larger the better for large oldData file
#ifndef _IS_NEED_CACHE_OLD_BY_COVERS
#   define _IS_NEED_CACHE_OLD_BY_COVERS 1
#endif

//see patch_decompress()
//  use larger memory cache to optimize speed
//  limit (temp_cache_end-temp_cache)>=2048
hpatch_BOOL patch_decompress_with_cache(const hpatch_TStreamOutput* out_newData,
                                        const hpatch_TStreamInput*  oldData,
                                        const hpatch_TStreamInput*  compressedDiff,
                                        hpatch_TDecompress* decompressPlugin,
                                        unsigned char* temp_cache,unsigned char* temp_cache_end);

//patch_decompress_repeat_out DEPRECATED
//  will be remove in a future release version
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
        hpatch_TStreamInput  oldStream;
        hpatch_TStreamInput  diffStream;
        mem_as_hStreamOutput(&out_newStream,out_newData,out_newData_end);
        mem_as_hStreamInput(&oldStream,oldData,oldData_end);
        mem_as_hStreamInput(&diffStream,compressedDiff,compressedDiff_end);
        return patch_decompress(&out_newStream,&oldStream,&diffStream,decompressPlugin);
    }
    
    typedef struct hpatch_TCoverList{
        hpatch_TCovers* ICovers;
    //private:
        unsigned char _buf[hpatch_kStreamCacheSize*4];
    } hpatch_TCoverList;
    
hpatch_inline static
void        hpatch_coverList_init(hpatch_TCoverList* coverList) {
                                  assert(coverList!=0); memset(coverList,0,sizeof(*coverList)-sizeof(coverList->_buf)); }
hpatch_BOOL hpatch_coverList_open_serializedDiff(hpatch_TCoverList*         out_coverList,
                                                 const hpatch_TStreamInput* serializedDiff);
hpatch_BOOL hpatch_coverList_open_compressedDiff(hpatch_TCoverList*         out_coverList,
                                                 const hpatch_TStreamInput* compressedDiff,
                                                 hpatch_TDecompress*        decompressPlugin);
hpatch_inline static
hpatch_BOOL hpatch_coverList_close(hpatch_TCoverList* coverList) {
                                   hpatch_BOOL result=hpatch_TRUE;
                                   if ((coverList!=0)&&(coverList->ICovers)){
                                       result=coverList->ICovers->close(coverList->ICovers);
                                       hpatch_coverList_init(coverList); } return result; }

#ifdef __cplusplus
}
#endif

#endif
