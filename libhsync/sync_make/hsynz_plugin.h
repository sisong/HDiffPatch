//  hsynz_plugin.h
//  sync_make
//  Created by housisong on 2022-12-15.
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
#ifndef hsynz_plugin_h
#define hsynz_plugin_h
#include "../../libHDiffPatch/HPatch/patch_types.h"
#ifdef __cplusplus
extern "C" {
#endif
    
    //hsynz plugin
    typedef struct hsync_THsynz{
        //return new curOutPos
        hpatch_StreamPos_t (*hsynz_write_head)(struct hsync_THsynz* zPlugin,
                                               const hpatch_TStreamOutput* out_stream,hpatch_StreamPos_t curOutPos,
                                               bool isDirSync,hpatch_StreamPos_t newDataSize,uint32_t kSyncBlockSize,
                                               hpatch_TChecksum* strongChecksumPlugin,hsync_TDictCompress* compressPlugin);
        //hsynz_readed_data can null
        void              (*hsynz_readed_data)(struct hsync_THsynz* zPlugin,
                                               const hpatch_byte* newData,size_t dataSize);
        //return new curOutPos
        hpatch_StreamPos_t (*hsynz_write_foot)(struct hsync_THsynz* zPlugin,
                                               const hpatch_TStreamOutput* out_stream,hpatch_StreamPos_t curOutPos,
                                               const hpatch_byte* newDataCheckChecksum,size_t checksumSize);
    } hsync_THsynz;

#ifdef __cplusplus
}
#endif
#endif // hsynz_plugin_h
