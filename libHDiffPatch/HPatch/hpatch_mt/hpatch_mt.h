//  hpatch_mt.h
//  hpatch
/*
 The MIT License (MIT)
 Copyright (c) 2025 HouSisong
 
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
#ifndef hpatch_mt_h
#define hpatch_mt_h
#include "../patch_types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct{
    hpatch_byte     writeNew_isMT;          //0 or 1; if 1, start a thread for write to newData
    hpatch_byte     readOld_isMT;           //0 or 1; if 1, start a thread for read from oldData
    hpatch_byte     readDiff_isMT;          //0 or 1; if 1, start a thread for read from diffData
    hpatch_byte     decompressDiff_isMT;    //0 or 1; if 1, start a thread for decompress diffData
    //patch func run in main thread
} hpatchMTSets_t;

static const hpatchMTSets_t hpatchMTSets_no  ={0,0,0,0};
static const hpatchMTSets_t hpatchMTSets_full={1,1,1,1};
static hpatch_force_inline
size_t _hpatchMTSets_threadNum(const hpatchMTSets_t mtsets){
            return mtsets.writeNew_isMT+mtsets.readOld_isMT+mtsets.readDiff_isMT+mtsets.decompressDiff_isMT+1; }

static hpatch_force_inline 
hpatchMTSets_t hpatch_getMTSets(hpatch_StreamPos_t newSize,hpatch_StreamPos_t oldSize,hpatch_StreamPos_t diffSize,
                               const hpatch_TDecompress* decompressPlugin,size_t kCacheCount,size_t stepMemSize,
                               size_t temp_cacheSumSize,size_t maxThreadNum,hpatchMTSets_t mtsets){
#if (_IS_USED_MULTITHREAD)
    hpatchMTSets_t _hpatch_getMTSets(hpatch_StreamPos_t newSize,hpatch_StreamPos_t oldSize,hpatch_StreamPos_t diffSize,
                                    const hpatch_TDecompress* decompressPlugin,size_t kCacheCount,size_t stepMemSize,
                                    size_t temp_cacheSumSize,size_t maxThreadNum,hpatchMTSets_t mtsets);
    if (maxThreadNum<=1) return hpatchMTSets_no;
    return _hpatch_getMTSets(newSize,oldSize,diffSize,decompressPlugin,kCacheCount,stepMemSize,temp_cacheSumSize,maxThreadNum,mtsets);
#else
    return hpatchMTSets_no;
#endif
}


#define     _kCacheSgCount  3  //patch for singleCompressedDiff need cache count
hpatch_BOOL _patch_single_stream_mt(sspatch_listener_t* listener,
                                    const hpatch_TStreamOutput* __out_newData,
                                    const hpatch_TStreamInput*  oldData,
                                    const hpatch_TStreamInput*  singleCompressedDiff, 
                                    hpatch_StreamPos_t  diffInfo_pos,
                                    sspatch_coversListener_t* coversListener,
                                    size_t maxThreadNum,hpatchMTSets_t hpatchMTSets);
hpatch_BOOL _patch_single_compressed_diff_mt(const hpatch_TStreamOutput* out_newData,
                                             const hpatch_TStreamInput*  oldData,
                                             const hpatch_TStreamInput*  singleCompressedDiff,
                                             hpatch_StreamPos_t          diffData_pos,
                                             hpatch_StreamPos_t          uncompressedSize,
                                             hpatch_StreamPos_t          compressedSize,
                                             hpatch_TDecompress*         decompressPlugin,
                                             hpatch_StreamPos_t coverCount,hpatch_size_t stepMemSize,
                                             unsigned char* temp_cache,unsigned char* temp_cache_end,
                                             sspatch_coversListener_t* coversListener,
                                             size_t maxThreadNum,hpatchMTSets_t mtsets);

#if (_IS_USED_MULTITHREAD)

struct hpatch_mt_manager_t;

struct hpatch_mt_manager_t* hpatch_mt_manager_open(const hpatch_TStreamOutput** pout_newData,
                                                   const hpatch_TStreamInput**  poldData,
                                                   const hpatch_TStreamInput**  psingleCompressedDiff,
                                                   hpatch_StreamPos_t*          pdiffData_pos,
                                                   hpatch_StreamPos_t*          pdiffData_posEnd,
                                                   hpatch_StreamPos_t           uncompressedSize,
                                                   hpatch_TDecompress**         pdecompressPlugin,
                                                   size_t                       stepMemSize,
                                                   unsigned char** ptemp_cache,unsigned char** ptemp_cache_end,
                                                   sspatch_coversListener_t**   pcoversListener,
                                                   hpatch_BOOL                  isOnStepCoversInThread,
                                                   size_t                       kCacheCount,
                                                   hpatchMTSets_t               mtsets);

hpatch_BOOL                 hpatch_mt_manager_close(struct hpatch_mt_manager_t* self,hpatch_BOOL isOnError);

#endif // _IS_USED_MULTITHREAD

#ifdef __cplusplus
}
#endif
#endif //hpatch_mt_h
