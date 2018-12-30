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
    
typedef struct TDirDiffInfo{
    hpatch_BOOL                 isDirDiff;
    hpatch_BOOL                 newPathIsDir;
    hpatch_BOOL                 oldPathIsDir;
    hpatch_BOOL                 dirDataIsCompressed;
    hpatch_StreamPos_t          externDataOffset;
    hpatch_StreamPos_t          externDataSize;
    hpatch_compressedDiffInfo   hdiffInfo;
} TDirDiffInfo;


hpatch_BOOL getDirDiffInfo(const hpatch_TStreamInput* diffFile,TDirDiffInfo* out_info);
hpatch_BOOL getDirDiffInfoByFile(const char* diffFileName,TDirDiffInfo* out_info);
hpatch_inline static hpatch_BOOL getIsDirDiffFile(const char* diffFileName,hpatch_BOOL* out_isDirDiffFile){
    TDirDiffInfo info;
    hpatch_BOOL result=getDirDiffInfoByFile(diffFileName,&info);
    if (result) *out_isDirDiffFile=info.isDirDiff;
    return result;
}

typedef enum TDirPatchResult{
    DIRPATCH_SUCCESS=0,
} TDirPatchResult;
    
    
    typedef struct _TDirDiffHead {
        hpatch_StreamPos_t  newPathCount;
        hpatch_StreamPos_t  oldPathCount;
        hpatch_StreamPos_t  sameFileCount;
        hpatch_StreamPos_t  newRefFileCount;
        hpatch_StreamPos_t  oldRefFileCount;
        hpatch_StreamPos_t  headDataOffset;
        hpatch_StreamPos_t  headDataSize;
        hpatch_StreamPos_t  headDataCompressedSize;
        hpatch_StreamPos_t  hdiffDataOffset;
        hpatch_StreamPos_t  hdiffDataSize;
    } _TDirDiffHead;

typedef struct TDirPatch{
    
//private:
    const hpatch_TStreamInput*  _dirDiffData;
    hpatch_TDecompress*         _decompressPlugin;
    
    TDirDiffInfo                _dirDiffInfo;
    _TDirDiffHead               _dirDiffHead;
} TDirPatch;

void          dirPatch_init(TDirPatch* self);
TDirDiffInfo* dirPatch_open(TDirPatch* self,const hpatch_TStreamInput* dirDiffData,
                            hpatch_TDecompress* decompressPlugin);


#ifdef __cplusplus
}
#endif
#endif //DirPatch_dir_patch_h
