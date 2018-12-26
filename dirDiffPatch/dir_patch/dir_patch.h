// dir_patch.h
// hdiffz dir diff
//
/*
 The MIT License (MIT)
 Copyright (c) 2012-2019 HouSisong
 
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
#ifndef DirPatch_dir_patch_h
#define DirPatch_dir_patch_h
#include "../../libHDiffPatch/HPatch/patch_types.h"
#ifdef __cplusplus
extern "C" {
#endif

hpatch_BOOL getDirDiffInfo(const hpatch_TStreamInput* diffFile,
                           char* out_compressType/*[hpatch_kMaxCompressTypeLength+1]*/,
                           hpatch_BOOL* out_newIsDir,hpatch_BOOL* out_oldIsDir);
hpatch_BOOL getDirDiffInfoByFile(const char* diffFileName,
                                 char* out_compressType/*[hpatch_kMaxCompressTypeLength+1]*/,
                                 hpatch_BOOL* out_newIsDir,hpatch_BOOL* out_oldIsDir);

typedef enum TDirPatchResult{
    DIRPATCH_SUCCESS=0,
} TDirPatchResult;

TDirPatchResult dir_patch(const hpatch_TStreamOutput* out_newData,
                          const char* oldPatch,const hpatch_TStreamInput*  diffData,
                          hpatch_TDecompress* decompressPlugin,
                          hpatch_BOOL isLoadOldAll,size_t patchCacheSize);
    
#ifdef __cplusplus
}
#endif
#endif //DirPatch_dir_patch_h
