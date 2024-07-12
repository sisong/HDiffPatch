// bspatch_wrapper.h
// HDiffPatch
/*
 The MIT License (MIT)
 Copyright (c) 2021 HouSisong
 
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
#ifndef hpatch_bspatch_wrapper_h
#define hpatch_bspatch_wrapper_h
#include "../libHDiffPatch/HPatch/patch_types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct hpatch_BsDiffInfo{
    hpatch_StreamPos_t  headSize;
    hpatch_StreamPos_t  ctrlDataSize;
    hpatch_StreamPos_t  subDataSize;
    hpatch_StreamPos_t  newDataSize;
    hpatch_BOOL         isEndsleyBsdiff;
} hpatch_BsDiffInfo;

hpatch_BOOL getIsBsDiff(const hpatch_TStreamInput* diffData,hpatch_BOOL* out_isSingleCompressedDiff);
hpatch_BOOL getIsBsDiff_mem(const unsigned char* diffData,const unsigned char* diffData_end,hpatch_BOOL* out_isSingleCompressedDiff);

hpatch_BOOL getBsDiffInfo(hpatch_BsDiffInfo* out_diffinfo,const hpatch_TStreamInput* diffStream);
hpatch_BOOL getBsDiffInfo_mem(hpatch_BsDiffInfo* out_diffinfo,const unsigned char* diffData,const unsigned char* diffData_end);

hpatch_BOOL bspatch_with_cache(const hpatch_TStreamOutput* out_newData,
                               const hpatch_TStreamInput*  oldData,
                               const hpatch_TStreamInput*  compressedDiff, //create by bsdiff4 or hdiffz -BSD
                               hpatch_TDecompress* decompressPlugin, // ==&_bz2DecompressPlugin_unsz in "decompress_plugin_demo.h"
                               unsigned char* temp_cache,unsigned char* temp_cache_end);

#ifdef __cplusplus
}
#endif
#endif