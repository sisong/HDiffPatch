//file_for_dirPatch.h
// file dir tool
//
/*
 This is the HDiffPatch copyright.

 Copyright (c) 2012-2019 HouSisong All Rights Reserved.

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
#ifndef DirDiffPatch_file_for_dirPatch_h
#define DirDiffPatch_file_for_dirPatch_h
#include <stdio.h>
#include "../libHDiffPatch/HPatch/patch_types.h"
#include "../file_for_patch.h"

#include <sys/stat.h> //stat

#ifdef _WIN32
      static const char kPatch_dirSeparator = '\\';
#else
      static const char kPatch_dirSeparator = '/';
#endif
static const char kPatch_dirSeparator_saved = '/';

#define kMaxOpenFileCount_min       8
#define kMaxOpenFileCount_default   (128-16)

hpatch_inline static
hpatch_BOOL getIsDirName(const char* path_utf8){
    size_t len=strlen(path_utf8);
    return (len>0)&&(path_utf8[len-1]==kPatch_dirSeparator);
}

typedef enum TPathType{
    kPathType_file,
    kPathType_dir
} TPathType;

hpatch_inline static
hpatch_BOOL getPathType(const char* path_utf8,TPathType* out_type){
#if (_IS_USE_WIN32_UTF8_WAPI)
    int            wsize;
    wchar_t        path_w[kPathMaxSize];
    struct _stat64 s;
#else
    struct stat    s;
#endif
    int         res;
    hpatch_BOOL isDirName=getIsDirName(path_utf8);
    assert(out_type!=0);
    if (isDirName){ *out_type=kPathType_dir; return hpatch_TRUE; }
    
    memset(&s,0,sizeof(s));
#if (_IS_USE_WIN32_UTF8_WAPI)
    wsize=_utf8FileName_to_w(path_utf8,path_w,kPathMaxSize);
    if (wsize<=0) return hpatch_FALSE;
    res= _wstat64(path_w,&s);
#else
    res = stat(path_utf8,&s);
#endif
    if(res!=0){
        return hpatch_FALSE;
    }else if ((s.st_mode&S_IFMT)==S_IFREG){
        *out_type=kPathType_file;
        return hpatch_TRUE;
    }else if ((s.st_mode&S_IFMT)==S_IFDIR){
        *out_type=kPathType_dir;
        return hpatch_TRUE;
    }else{
        return hpatch_FALSE;
    }
}

#endif
