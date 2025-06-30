//file_for_dirDiff.h
// file dir tool
//
/*
 This is the HDiffPatch copyright.

 Copyright (c) 2018-2019 HouSisong All Rights Reserved.

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
#ifndef DirDiffPatch_file_for_dirDiff_h
#define DirDiffPatch_file_for_dirDiff_h
#include "../../file_for_patch.h"
#include "../dir_patch/dir_patch_types.h"
#if (_IS_NEED_DIR_DIFF_PATCH)

typedef void* hdiff_TDirHandle;

hdiff_TDirHandle hdiff_dirOpenForRead(const char* dir_utf8);
hpatch_BOOL hdiff_dirNext(hdiff_TDirHandle dirHandle,hpatch_TPathType *out_type,const char** out_subName_utf8);
void hdiff_dirClose(hdiff_TDirHandle dirHandle);

#endif // _IS_NEED_DIR_DIFF_PATCH
#endif // DirDiffPatch_file_for_dirDiff_h
