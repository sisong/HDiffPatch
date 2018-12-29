//file_for_dir.h
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
#ifndef DirDiffPatch_file_for_dir_h
#define DirDiffPatch_file_for_dir_h
#include <stdio.h>
#include "../libHDiffPatch/HPatch/patch_types.h"

#include <sys/stat.h> //stat
#include <dirent.h> //opendir ...

#ifdef WIN32
    const char   kPatch_dirTag = '\';
#else
    const char   kPatch_dirTag = '/';
#endif
const char kPatch_dirTag_saved = '/';

typedef enum TPathType{
    kPathType_file,
    kPathType_dir
} TPathType;

static inline
hpatch_BOOL getPathType(const char* path,TPathType* out_type){
    assert(out_type!=0);
    struct stat s;
    memset(&s,0,sizeof(s));
    int res = stat(path,&s);
    if(res!=0){
        return hpatch_FALSE;
    }else if (S_ISREG(s.st_mode)){
        *out_type=kPathType_file;
        return hpatch_TRUE;
    }else if (S_ISDIR(s.st_mode)){
        *out_type=kPathType_dir;
        return hpatch_TRUE;
    }else{
        return hpatch_FALSE;
    }
}


typedef void* TDirHandle;

static inline
TDirHandle dirOpenForRead(const char* dir){
    return opendir(dir);
}

static inline
const char* dirNext(TDirHandle dirHandle,TPathType *out_type){
    assert(dirHandle!=0);
    assert(out_type!=0);
    DIR* pdir =(DIR*)dirHandle;
    struct dirent* pdirent = readdir(pdir);
    if (pdirent==0) return 0;
    
    const char* subName=pdirent->d_name;
    if (pdirent->d_type==DT_DIR){
        *out_type=kPathType_dir;
        return subName;
    }else if (pdirent->d_type==DT_REG){
        *out_type=kPathType_file;
        return subName;
    }else{
        return dirNext(dirHandle,out_type);
    }
}

static inline
void dirClose(TDirHandle dirHandle){
    if (dirHandle)
        closedir((DIR*)dirHandle);
}

#endif
