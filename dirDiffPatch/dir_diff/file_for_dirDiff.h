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
#ifdef _WIN32
#else
#   include <dirent.h> //opendir ...
#endif

typedef void* hdiff_TDirHandle;

#ifdef _WIN32
struct _hdiff_TFindFileData{
    HANDLE              handle;
    char                subName_utf8[hpatch_kPathMaxSize];
};

static inline
hdiff_TDirHandle hdiff_dirOpenForRead(const char* dir_utf8){
    size_t      ucSize=strlen(dir_utf8);
    hpatch_BOOL isNeedDirSeparator=(ucSize>0)&&(dir_utf8[ucSize-1]!=kPatch_dirSeparator);
    wchar_t     dir_w[hpatch_kPathMaxSize];
    int wsize=_utf8FileName_to_w(dir_utf8,dir_w,hpatch_kPathMaxSize-3);
    if (wsize<=0) return 0; //error
    if (dir_w[wsize-1]=='\0') --wsize;
    if (isNeedDirSeparator)
        dir_w[wsize++]=kPatch_dirSeparator;
    dir_w[wsize++]='*';
    dir_w[wsize++]='\0';
    _hdiff_TFindFileData*  finder=(_hdiff_TFindFileData*)malloc(sizeof(_hdiff_TFindFileData));
    WIN32_FIND_DATAW findData;
    finder->handle=FindFirstFileW(dir_w,&findData);
    if (finder->handle!=INVALID_HANDLE_VALUE){
        return finder; //open dir ok
    }else{
        DWORD errcode = GetLastError();
        if (errcode==ERROR_FILE_NOT_FOUND) {
            return finder; //open dir ok
        }else{
            free(finder);
            return 0; //error
        }
    }
}

static inline
hpatch_BOOL hdiff_dirNext(hdiff_TDirHandle dirHandle,hpatch_TPathType *out_type,const char** out_subName_utf8){
    assert(dirHandle!=0);
    _hdiff_TFindFileData* finder=(_hdiff_TFindFileData*)dirHandle;
    if (finder->handle==INVALID_HANDLE_VALUE) { *out_subName_utf8=0; return hpatch_TRUE; }//finish
    WIN32_FIND_DATAW findData;
    if (!FindNextFileW(finder->handle,&findData)){
        DWORD errcode = GetLastError();
        if (errcode==ERROR_NO_MORE_FILES) { *out_subName_utf8=0; return hpatch_TRUE; }//finish
        return hpatch_FALSE; //error
    }
    //get name
    const wchar_t* subName_w=(const wchar_t*)findData.cFileName;
    int bsize=_wFileName_to_utf8(subName_w,finder->subName_utf8,hpatch_kPathMaxSize);
    if (bsize<=0) return hpatch_FALSE; //error
    *out_subName_utf8=finder->subName_utf8;
    //get type
    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
        *out_type=kPathType_dir;
    }else{
        *out_type=kPathType_file;
    }
    return hpatch_TRUE;
}

static inline
void hdiff_dirClose(hdiff_TDirHandle dirHandle){
    _hdiff_TFindFileData* finder=(_hdiff_TFindFileData*)dirHandle;
    if (finder!=0){
        if (finder->handle!=INVALID_HANDLE_VALUE)
            FindClose(finder->handle);
        free(finder);
    }
}

#else  // _WIN32

static inline
hdiff_TDirHandle hdiff_dirOpenForRead(const char* dir_utf8){
    hdiff_TDirHandle h=opendir(dir_utf8);
    if (!h) return 0; //error
    return h;
}

static inline
hpatch_BOOL hdiff_dirNext(hdiff_TDirHandle dirHandle,hpatch_TPathType *out_type,const char** out_subName_utf8){
    assert(dirHandle!=0);
    DIR* pdir =(DIR*)dirHandle;
    struct dirent* pdirent = readdir(pdir);
    if (pdirent==0){
        *out_subName_utf8=0; //finish
        return hpatch_TRUE;
    }
    
    if (pdirent->d_type==DT_DIR){
        *out_type=kPathType_dir;
        *out_subName_utf8=pdirent->d_name;
        return hpatch_TRUE;
    }else if (pdirent->d_type==DT_REG){
        *out_type=kPathType_file;
        *out_subName_utf8=pdirent->d_name;
        return hpatch_TRUE;
    }else{
        return hdiff_dirNext(dirHandle,out_type,out_subName_utf8);
    }
}

static inline
void hdiff_dirClose(hdiff_TDirHandle dirHandle){
    if (dirHandle)
        closedir((DIR*)dirHandle);
}

#endif //_WIN32
#endif
#endif
