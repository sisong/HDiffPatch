// vcpatch_wrapper.h
// HDiffPatch
/*
 The MIT License (MIT)
 Copyright (c) 2022 HouSisong
 
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
#ifndef hpatch_vcpatch_wrapper_h
#define hpatch_vcpatch_wrapper_h
#include "../libHDiffPatch/HPatch/patch_types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum{
    kVcDiff_compressorID_no   = 0, // no compress
    kVcDiff_compressorID_7zXZ = 2, // compress by 7zXZ, compatible with $xdelta -S lzma ...
} vcdiff_compressType;

typedef struct hpatch_VcDiffInfo{
    hpatch_StreamPos_t  appHeadDataOffset;
    hpatch_StreamPos_t  appHeadDataLen;
    hpatch_StreamPos_t  windowOffset;
    hpatch_BOOL         isGoogleVersion;
    vcdiff_compressType compressorID;
} hpatch_VcDiffInfo;

hpatch_BOOL getVcDiffInfo(hpatch_VcDiffInfo* out_diffinfo,const hpatch_TStreamInput* diffStream);
hpatch_BOOL getVcDiffInfo_mem(hpatch_VcDiffInfo* out_diffinfo,const unsigned char* diffData,const unsigned char* diffData_end);

hpatch_BOOL vcpatch_with_cache(const hpatch_TStreamOutput* out_newData,
                               const hpatch_TStreamInput*  oldData,
                               const hpatch_TStreamInput*  compressedDiff, //create by vcdiff or hdiffz -VCD
                               hpatch_TDecompress* decompressPlugin,hpatch_BOOL isNeedChecksum,
                               unsigned char* temp_cache,unsigned char* temp_cache_end);

#ifdef __cplusplus
}
#endif
#endif