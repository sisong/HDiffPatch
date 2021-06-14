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

//all patch*() functions do not allocate memory

//optimize speed for patch_stream_with_cache() && patch_decompress_with_cache():
//  preload part of oldData into cache,
//  cache memory size (temp_cache_end-temp_cache) the larger the better for large oldData file
#ifndef _IS_NEED_CACHE_OLD_BY_COVERS
#   define _IS_NEED_CACHE_OLD_BY_COVERS 1
#endif


//generate newData by patch(oldData + serializedDiff)
//  serializedDiff create by create_diff()
hpatch_BOOL patch(unsigned char* out_newData,unsigned char* out_newData_end,
                  const unsigned char* oldData,const unsigned char* oldData_end,
                  const unsigned char* serializedDiff,const unsigned char* serializedDiff_end);

//patch by stream, see patch()
//  used (hpatch_kStreamCacheSize*8 stack memory) for I/O cache
//  if use patch_stream_with_cache(), can passing larger memory cache to optimize speed
//  serializedDiff create by create_diff()
hpatch_BOOL patch_stream(const hpatch_TStreamOutput* out_newData,       //sequential write
                         const hpatch_TStreamInput*  oldData,           //random read
                         const hpatch_TStreamInput*  serializedDiff);   //random read

//see patch_stream()
//  can passing more memory for I/O cache to optimize speed
//  note: (temp_cache_end-temp_cache)>=2048
hpatch_BOOL patch_stream_with_cache(const hpatch_TStreamOutput* out_newData,    //sequential write
                                    const hpatch_TStreamInput*  oldData,        //random read
                                    const hpatch_TStreamInput*  serializedDiff, //random read
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
    
//patch with decompress plugin
//  used (hpatch_kStreamCacheSize*6 stack memory) + (decompress buffer*4)
//  compressedDiff create by create_compressed_diff() or create_compressed_diff_stream()
//  decompressPlugin can null when no compressed data in compressedDiff
//  if use patch_decompress_with_cache(), can passing larger memory cache to optimize speed
hpatch_BOOL patch_decompress(const hpatch_TStreamOutput* out_newData,       //sequential write
                             const hpatch_TStreamInput*  oldData,           //random read
                             const hpatch_TStreamInput*  compressedDiff,    //random read
                             hpatch_TDecompress* decompressPlugin);

//see patch_decompress()
//  can passing larger memory cache to optimize speed
//  note: (temp_cache_end-temp_cache)>=2048
hpatch_BOOL patch_decompress_with_cache(const hpatch_TStreamOutput* out_newData,    //sequential write
                                        const hpatch_TStreamInput*  oldData,        //random read
                                        const hpatch_TStreamInput*  compressedDiff, //random read
                                        hpatch_TDecompress* decompressPlugin,
                                        unsigned char* temp_cache,unsigned char* temp_cache_end);

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




// hpatch_TCoverList: open diffData and read coverList
    typedef struct hpatch_TCoverList{
        hpatch_TCovers* ICovers;
    //private:
        unsigned char _buf[hpatch_kStreamCacheSize*4];
    } hpatch_TCoverList;

hpatch_inline static
void        hpatch_coverList_init(hpatch_TCoverList* coverList) {
                                  assert(coverList!=0); memset(coverList,0,sizeof(*coverList)-sizeof(coverList->_buf)); }
//  serializedDiff create by create_diff()
hpatch_BOOL hpatch_coverList_open_serializedDiff(hpatch_TCoverList*         out_coverList,
                                                 const hpatch_TStreamInput* serializedDiff);
//  compressedDiff create by create_compressed_diff() or create_compressed_diff_stream()
hpatch_BOOL hpatch_coverList_open_compressedDiff(hpatch_TCoverList*         out_coverList,
                                                 const hpatch_TStreamInput* compressedDiff,
                                                 hpatch_TDecompress*        decompressPlugin);
hpatch_inline static
hpatch_BOOL hpatch_coverList_close(hpatch_TCoverList* coverList) {
                                   hpatch_BOOL result=hpatch_TRUE;
                                   if ((coverList!=0)&&(coverList->ICovers)){
                                       result=coverList->ICovers->close(coverList->ICovers);
                                       hpatch_coverList_init(coverList); } return result; }




//patch singleCompressedDiff with listener
//	used (stepMemSize memory) + (I/O cache memory) + (decompress buffer*1)
//  every byte in singleCompressedDiff will only be read once in order
//  singleCompressedDiff create by create_single_compressed_diff()
//  you can download&patch diffData at the same time, without saving it to disk
//  same as call getSingleCompressedDiffInfo() + listener->onDiffInfo() + patch_single_compressed_diff()
hpatch_BOOL patch_single_stream(sspatch_listener_t* listener, //call back when got diffInfo
                                const hpatch_TStreamOutput* out_newData,          //sequential write
                                const hpatch_TStreamInput*  oldData,              //random read
                                const hpatch_TStreamInput*  singleCompressedDiff, //sequential read every byte
                                hpatch_StreamPos_t  diffInfo_pos, //default 0, begin pos in singleCompressedDiff
                                sspatch_coversListener_t* coversListener //default NULL, call by on got covers
                                );
// patch_single_stream_by rename to patch_single_stream
static hpatch_inline hpatch_BOOL
    patch_single_stream_mem(sspatch_listener_t* listener,
                            unsigned char* out_newData,unsigned char* out_newData_end,
                            const unsigned char* oldData,const unsigned char* oldData_end,
                            const unsigned char* diff,const unsigned char* diff_end,
                            sspatch_coversListener_t* coversListener){
        hpatch_TStreamOutput out_newStream;
        hpatch_TStreamInput  oldStream;
        hpatch_TStreamInput  diffStream;
        mem_as_hStreamOutput(&out_newStream,out_newData,out_newData_end);
        mem_as_hStreamInput(&oldStream,oldData,oldData_end);
        mem_as_hStreamInput(&diffStream,diff,diff_end);
        return patch_single_stream(listener,&out_newStream,&oldStream,&diffStream,0,coversListener);
    }
// patch_single_stream_by_mem rename to patch_single_stream_mem

//get singleCompressedDiff info
//  singleCompressedDiff create by create_single_compressed_diff()
hpatch_BOOL getSingleCompressedDiffInfo(hpatch_singleCompressedDiffInfo* out_diffInfo,
                                        const hpatch_TStreamInput*  singleCompressedDiff,   //sequential read
                                        hpatch_StreamPos_t diffInfo_pos//default 0, begin pos in singleCompressedDiff
                                        );
hpatch_inline static hpatch_BOOL
    getSingleCompressedDiffInfo_mem(hpatch_singleCompressedDiffInfo* out_diffInfo,
                                    const unsigned char* singleCompressedDiff,
                                    const unsigned char* singleCompressedDiff_end){
        hpatch_TStreamInput  diffStream;
        mem_as_hStreamInput(&diffStream,singleCompressedDiff,singleCompressedDiff_end);
        return getSingleCompressedDiffInfo(out_diffInfo,&diffStream,0);            
    }

//patch singleCompressedDiff with diffInfo
//	used (stepMemSize memory) + (I/O cache memory) + (decompress buffer*1)
//	note: (I/O cache memory) >= hpatch_kStreamCacheSize*3
//  temp_cache_end-temp_cache == stepMemSize + (I/O cache memory)
//  singleCompressedDiff create by create_single_compressed_diff()
//  decompressPlugin can null when no compressed data in singleCompressedDiff
//  same as call compressed_stream_as_uncompressed() + patch_single_stream_diff()
hpatch_BOOL patch_single_compressed_diff(const hpatch_TStreamOutput* out_newData,          //sequential write
                                         const hpatch_TStreamInput*  oldData,              //random read
                                         const hpatch_TStreamInput*  singleCompressedDiff, //sequential read
                                         hpatch_StreamPos_t          diffData_pos, //diffData begin pos in singleCompressedDiff
                                         hpatch_StreamPos_t          uncompressedSize,
                                         hpatch_StreamPos_t          compressedSize,
                                         hpatch_TDecompress*         decompressPlugin,
                                         hpatch_StreamPos_t coverCount,hpatch_size_t stepMemSize,
                                         unsigned char* temp_cache,unsigned char* temp_cache_end,
                                         sspatch_coversListener_t* coversListener //default NULL, call by on got covers
                                         );

hpatch_BOOL compressed_stream_as_uncompressed(hpatch_TUncompresser_t* uncompressedStream,hpatch_StreamPos_t uncompressedSize,
                                                hpatch_TDecompress* decompressPlugin,const hpatch_TStreamInput* compressedStream,
                                                hpatch_StreamPos_t compressed_pos,hpatch_StreamPos_t compressed_end);
void close_compressed_stream_as_uncompressed(hpatch_TUncompresser_t* uncompressedStream);

hpatch_BOOL patch_single_stream_diff(const hpatch_TStreamOutput*  out_newData,          //sequential write
                                     const hpatch_TStreamInput*   oldData,              //random read
                                     const hpatch_TStreamInput*   uncompressedDiffData, //sequential read
                                     hpatch_StreamPos_t           diffData_pos,//diffData begin pos in uncompressedDiffData
                                     hpatch_StreamPos_t coverCount,hpatch_size_t stepMemSize,
                                     unsigned char* temp_cache,unsigned char* temp_cache_end,
                                     sspatch_coversListener_t* coversListener);

#ifdef __cplusplus
}
#endif

#endif
