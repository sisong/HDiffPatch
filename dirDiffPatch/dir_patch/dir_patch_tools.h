// dir_patch_tools.h
// hdiffz dir patch
//
/*
 The MIT License (MIT)
 Copyright (c) 2018-2019 HouSisong
 
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
#ifndef DirPatch_dir_patch_tools_h
#define DirPatch_dir_patch_tools_h
#include "dir_patch_types.h"
#if (_IS_NEED_DIR_DIFF_PATCH)
#include "../../libHDiffPatch/HPatch/patch_private.h"
#ifdef __cplusplus
extern "C" {
#endif
    void formatDirTagForLoad(char* utf8_path,char* utf8_pathEnd);
    hpatch_BOOL clipCStrsTo(const char* cstrs,const char* cstrsEnd,
                            const char** out_cstrList,size_t cstrCount);
    hpatch_BOOL readListTo(TStreamCacheClip* sclip,hpatch_StreamPos_t* out_list,size_t count);
    hpatch_BOOL readIncListTo(TStreamCacheClip* sclip,size_t* out_list,
                              size_t count,size_t check_endValue);
    
    //return path end pos
    char* setPath(char* out_path,char* out_pathBufEnd,const char* fileName);
    char* setDirPath(char* out_path,char* out_pathBufEnd,const char* dirName);

    
#ifdef __cplusplus
}
#endif
#endif
#endif //DirPatch_dir_patch_tools_h
