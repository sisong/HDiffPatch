//  patch_private.h
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

#ifndef HPatch_patch_private_h
#define HPatch_patch_private_h

#include "patch_types.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct _THDiffzHead{
    hpatch_StreamPos_t coverCount;
    
    hpatch_StreamPos_t cover_buf_size;
    hpatch_StreamPos_t compress_cover_buf_size;
    hpatch_StreamPos_t rle_ctrlBuf_size;
    hpatch_StreamPos_t compress_rle_ctrlBuf_size;
    hpatch_StreamPos_t rle_codeBuf_size;
    hpatch_StreamPos_t compress_rle_codeBuf_size;
    hpatch_StreamPos_t newDataDiff_size;
    hpatch_StreamPos_t compress_newDataDiff_size;
    
    hpatch_StreamPos_t headEndPos;
    hpatch_StreamPos_t coverEndPos;
} _THDiffzHead;
    
hpatch_BOOL read_diffz_head(hpatch_compressedDiffInfo* out_diffInfo,_THDiffzHead* out_head,
                            const hpatch_TStreamInput* compressedDiff);
    
#ifdef __cplusplus
}
#endif
#endif
