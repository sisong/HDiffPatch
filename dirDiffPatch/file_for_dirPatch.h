//file_for_dirPatch.h
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
#ifndef DirDiffPatch_file_for_dirPatch_h
#define DirDiffPatch_file_for_dirPatch_h
#include <stdio.h>
#include "../libHDiffPatch/HPatch/patch_types.h"
#include "../file_for_patch.h"

#include <sys/stat.h> //stat mkdir
#ifdef _MSC_VER
#   include <direct.h> // *mkdir
#endif

#ifdef _WIN32
      static const char kPatch_dirSeparator = '\\';
#else
      static const char kPatch_dirSeparator = '/';
#endif
static const char kPatch_dirSeparator_saved = '/';

#define kMaxOpenFileNumber_limit_min          3
#define kMaxOpenFileNumber_default_min        8 //must >= limit_min
#define kMaxOpenFileNumber_default_diff      48
#define kMaxOpenFileNumber_default_patch     24

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
hpatch_BOOL getPathStat(const char* path_utf8,TPathType* out_type,hpatch_StreamPos_t* out_fileSize){
#if (_IS_USE_WIN32_UTF8_WAPI)
    int            wsize;
    wchar_t        path_w[kPathMaxSize];
    struct _stat64 s;
#else
    struct stat  s;
#endif
    
    int          rt;
    assert(out_type!=0);
    memset(&s,0,sizeof(s));
#if (_IS_USE_WIN32_UTF8_WAPI)
    wsize=_utf8FileName_to_w(path_utf8,path_w,kPathMaxSize);
    if (wsize<=0) return hpatch_FALSE;
    rt = _wstat64(path_w,&s);
#else
    rt = stat(path_utf8,&s);
#endif
    
    if(rt!=0){
        return hpatch_FALSE;
    }else if ((s.st_mode&S_IFMT)==S_IFREG){
        *out_type=kPathType_file;
        if (out_fileSize) *out_fileSize=s.st_size;
        return hpatch_TRUE;
    }else if ((s.st_mode&S_IFMT)==S_IFDIR){
        *out_type=kPathType_dir;
        if (out_fileSize) *out_fileSize=0;
        return hpatch_TRUE;
    }else{
        return hpatch_FALSE;
    }
}

hpatch_inline static
hpatch_BOOL getPathTypeByName(const char* path_utf8,TPathType* out_type,hpatch_StreamPos_t* out_fileSize){
    assert(out_type!=0);
    if (getIsDirName(path_utf8)){
        *out_type=kPathType_dir;
        if (out_fileSize) *out_fileSize=0;
        return hpatch_TRUE;
    }else{
        return getPathStat(path_utf8,out_type,out_fileSize);
    }
}

hpatch_inline static
hpatch_BOOL makeNewDir(const char* dirName_utf8){
    TPathType type;
    if (getPathStat(dirName_utf8,&type,0)){
        return type==kPathType_dir;
    }else{
#if (_IS_USE_WIN32_UTF8_WAPI)
        int     rt;
        int     wsize;
        wchar_t path_w[kPathMaxSize];
        wsize=_utf8FileName_to_w(dirName_utf8,path_w,kPathMaxSize);
        if (wsize<=0) return hpatch_FALSE;
        rt = _wmkdir(path_w);
#else
#   ifdef _MSC_VER
        int rt=_mkdir(dirName_utf8);
#   else
        const mode_t kDefalutMode=S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
        int rt=mkdir(dirName_utf8,kDefalutMode);
#   endif
#endif
        return rt==0;
    }
}

#endif
