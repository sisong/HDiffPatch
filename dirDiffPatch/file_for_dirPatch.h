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
#include <locale.h> // setlocale
#include "../libHDiffPatch/HPatch/patch_types.h"

#ifdef _WIN32
//todo:
#else //linux like
#   include <sys/stat.h> //stat
#endif

#ifdef _WIN32
      static const char kPatch_dirSeparator = '\';
#else
      static const char kPatch_dirSeparator = '/';
#endif
static const char kPatch_dirSeparator_saved = '/';

typedef enum TPathType{
    kPathType_file,
    kPathType_dir
} TPathType;

hpatch_inline static
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


hpatch_inline static
void SetDefaultLocale(){ //for some locale Path character encoding
    setlocale(LC_ALL, "" );
}

hpatch_inline static
hpatch_BOOL isSamePath(const char* xPath,const char* yPath){
    if (0==strcmp(xPath,yPath)){
        return hpatch_TRUE;
    }else{
        // WARING!!! better return getCanonicalPath(xPath)==getCanonicalPath(yPath);
        return hpatch_FALSE;
    }
}

hpatch_inline static //if error return 0 else return outCStringByteSize
size_t utf8String_to_utf8(const char* path,char* out_utf8,char* out_utf8BufEnd){
    //copy only
    size_t size=strlen(path)+1; // with '\0'
    if (out_utf8!=0){
        if ((out_utf8BufEnd-out_utf8)<size) return 0;//error
        memmove(out_utf8,path,size);
    }
    return size;
}

hpatch_inline static //if error return 0 else return outCStringByteSize
size_t utf8_to_localePath(const char* utf8Path,char* out_Path,char* out_PathBufEnd){
#if ( defined(__APPLE__) || defined(__ANDROID__) || defined(__linux__) )
    return utf8String_to_utf8(utf8Path,out_Path,out_PathBufEnd);
#else
    #warning Path unknown character encoding, probably can not cross-platform
    return utf8String_to_utf8(utf8Path,out_Path,out_PathBufEnd);
#endif
}


hpatch_inline static //if error return 0 else return outCStringBufSize
size_t localePath_to_utf8(const char* path,char* out_utf8,char* out_utf8BufEnd){
#if (defined(__APPLE__))
    return utf8String_to_utf8(path,out_utf8,out_utf8BufEnd);
#else
#warning Path unknown character encoding, probably can not cross-platform
    return utf8String_to_utf8(path,out_utf8,out_utf8BufEnd);
#endif
}


#endif
