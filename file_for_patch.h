//file_for_patch.h
// patch demo file tool
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
#ifndef HPatch_file_for_patch_h
#define HPatch_file_for_patch_h
#include <stdio.h>  //fprintf
#include <stdlib.h> // malloc free
#include <locale.h> // setlocale
#include <unistd.h> // rmdir
#ifdef _MSC_VER
#   include <direct.h> // *mkdir *rmdir
#endif

#include "libHDiffPatch/HPatch/patch_types.h"

#ifndef _IS_USE_WIN32_UTF8_WAPI
#   if (defined(_WIN32) && defined(_MSC_VER))
#       define _IS_USE_WIN32_UTF8_WAPI 1 // used utf8 string + wchar_t API
#   endif
#endif
#if (_IS_USE_WIN32_UTF8_WAPI)
#   define _kMultiBytePage CP_UTF8
#elif defined(_WIN32)
#   define _kMultiBytePage CP_ACP
#endif
#ifdef _WIN32
#   include <wchar.h>
#   include <windows.h> //for file API, character encoding API
#endif
#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char TByte;
#define kFileIOBestMaxSize  (1<<20)
#define kPathMaxSize  512

#define kMaxOpenFileNumber_limit_min          3
#define kMaxOpenFileNumber_default_min        8 //must >= limit_min
#define kMaxOpenFileNumber_default_diff      48
#define kMaxOpenFileNumber_default_patch     24

    
#ifdef _WIN32
    static const char kPatch_dirSeparator = '\\';
#else
    static const char kPatch_dirSeparator = '/';
#endif
    static const char kPatch_dirSeparator_saved = '/';
    
hpatch_inline static
hpatch_BOOL getIsDirName(const char* path_utf8){
    size_t len=strlen(path_utf8);
    return (len>0)&&(path_utf8[len-1]==kPatch_dirSeparator);
}


#define _path_noEndDirSeparator(dst_path,src_path)  \
    char  dst_path[kPathMaxSize];      \
    {   size_t len=strlen(src_path);   \
        if (len>=kPathMaxSize) return hpatch_FALSE; /* error */ \
        if ((len>0)&&(src_path[len-1]==kPatch_dirSeparator)) --len; /* without '/' */\
        memcpy(dst_path,src_path,len); \
        dst_path[len]='\0';  } /* safe */

#ifndef PRIu64
#   ifdef _MSC_VER
#       define PRIu64 "I64u"
#   else
#       define PRIu64 "llu"
#   endif
#endif


#ifdef _WIN32
static int _utf8FileName_to_w(const char* fileName_utf8,wchar_t* out_fileName_w,size_t out_wSize){
    return MultiByteToWideChar(_kMultiBytePage,0,fileName_utf8,-1,out_fileName_w,(int)out_wSize); }

static int _wFileName_to_utf8(const wchar_t* fileName_w,char* out_fileName_utf8,size_t out_bSize){
    return WideCharToMultiByte(_kMultiBytePage,0,fileName_w,-1,out_fileName_utf8,(int)out_bSize,0,0); }

static hpatch_BOOL _wFileNames_to_utf8(const wchar_t** fileNames_w,size_t fileCount,
                                       char** out_fileNames_utf8,size_t out_byteSize){
    char*   _bufEnd=((char*)out_fileNames_utf8)+out_byteSize;
    char*   _bufCur=(char*)(&out_fileNames_utf8[fileCount]);
    size_t i;
    for (i=0; i<fileCount; ++i) {
        int csize;
        if (_bufCur>=_bufEnd) return hpatch_FALSE; //error
        csize=_wFileName_to_utf8(fileNames_w[i],_bufCur,_bufEnd-_bufCur);
        if (csize<=0) return hpatch_FALSE; //error
        out_fileNames_utf8[i]=_bufCur;
        _bufCur+=csize;
    }
    return hpatch_TRUE;
}
#endif

#if (_IS_USE_WIN32_UTF8_WAPI)
hpatch_inline static
void SetDefaultStringLocale(){ //for some locale Path character encoding view
    setlocale(LC_CTYPE,"");
}
#endif

hpatch_inline static
hpatch_BOOL getIsSamePath(const char* xPath_utf8,const char* yPath_utf8){
    _path_noEndDirSeparator(xPath,xPath_utf8);
    {   _path_noEndDirSeparator(yPath,yPath_utf8);
        if (0==strcmp(xPath,yPath)){
            return hpatch_TRUE;
        }else{
            // WARING!!! better return getCanonicalPath(xPath)==getCanonicalPath(yPath);
            return hpatch_FALSE;
        }
    }
}

hpatch_inline static
int printPath_utf8(const char* pathTxt_utf8){
#if (_IS_USE_WIN32_UTF8_WAPI)
    wchar_t pathTxt_w[kPathMaxSize];
    int wsize=_utf8FileName_to_w(pathTxt_utf8,pathTxt_w,kPathMaxSize);
    if (wsize>0)
        return printf("%ls",pathTxt_w);
    else //view unknow
        return printf("%s",pathTxt_utf8);
#else
    return printf("%s",pathTxt_utf8);
#endif
}

hpatch_inline static
int printStdErrPath_utf8(const char* pathTxt_utf8){
#if (_IS_USE_WIN32_UTF8_WAPI)
    wchar_t pathTxt_w[kPathMaxSize];
    int wsize=_utf8FileName_to_w(pathTxt_utf8,pathTxt_w,kPathMaxSize);
    if (wsize>0)
        return fprintf(stderr,"%ls",pathTxt_w);
    else //view unknow
        return fprintf(stderr,"%s",pathTxt_utf8);
#else
    return fprintf(stderr,"%s",pathTxt_utf8);
#endif
}
    

    typedef enum TPathType{
        kPathType_notExist,
        kPathType_file,
        kPathType_dir,
    } TPathType;
    
    
hpatch_BOOL _getPathStat_noEndDirSeparator(const char* path_utf8,TPathType* out_type,
                                           hpatch_StreamPos_t* out_fileSize);
    
hpatch_inline static
hpatch_BOOL getPathStat(const char* path_utf8,TPathType* out_type,
                        hpatch_StreamPos_t* out_fileSize){
    if (!getIsDirName(path_utf8)){
        return _getPathStat_noEndDirSeparator(path_utf8,out_type,out_fileSize);
    }else{//dir name
        _path_noEndDirSeparator(path,path_utf8);
        return _getPathStat_noEndDirSeparator(path,out_type,out_fileSize);
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
        return _getPathStat_noEndDirSeparator(path_utf8,out_type,out_fileSize);
    }
}

hpatch_BOOL getTempPathName(const char* path_utf8,char* out_tempPath_utf8,char* out_tempPath_end);

hpatch_BOOL renamePath(const char* oldPath_utf8,const char* newPath_utf8);
hpatch_BOOL moveFile(const char* oldPath_utf8,const char* newPath_utf8);

hpatch_BOOL removeFile(const char* fileName_utf8);
hpatch_BOOL removeDir(const char* dirName_utf8);

hpatch_BOOL makeNewDir(const char* dirName_utf8);
   

    
typedef FILE* hpatch_FileHandle;

typedef struct TFileStreamInput{
    hpatch_TStreamInput base;
    const void*         _null_write;
    hpatch_FileHandle   m_file;
    hpatch_StreamPos_t  m_fpos;
    hpatch_StreamPos_t  m_offset;
    hpatch_BOOL         fileError;
} TFileStreamInput;

hpatch_inline
static void TFileStreamInput_init(TFileStreamInput* self){
    memset(self,0,sizeof(TFileStreamInput));
}
hpatch_BOOL TFileStreamInput_open(TFileStreamInput* self,const char* fileName_utf8);
void TFileStreamInput_setOffset(TFileStreamInput* self,size_t offset);
hpatch_BOOL TFileStreamInput_close(TFileStreamInput* self);

typedef struct TFileStreamOutput{ //is TFileStreamInput !
    hpatch_TStreamOutput base; //is hpatch_TStreamInput + write
    hpatch_FileHandle   m_file;
    hpatch_StreamPos_t  m_fpos;
    hpatch_StreamPos_t  m_offset; //now not used
    hpatch_BOOL         fileError;
    //
    hpatch_BOOL         is_random_out;
    hpatch_BOOL         is_in_readModel;
    hpatch_StreamPos_t  out_length;
    
} TFileStreamOutput;

hpatch_inline
static void TFileStreamOutput_init(TFileStreamOutput* self){
    memset(self,0,sizeof(TFileStreamOutput));
}
hpatch_BOOL TFileStreamOutput_open(TFileStreamOutput* self,const char* fileName_utf8,
                                   hpatch_StreamPos_t max_file_length);
hpatch_inline static
void TFileStreamOutput_setRandomOut(TFileStreamOutput* self,hpatch_BOOL is_random_out){
    self->is_random_out=is_random_out;
}

hpatch_BOOL TFileStreamOutput_flush(TFileStreamOutput* self);
hpatch_BOOL TFileStreamOutput_close(TFileStreamOutput* self);

#ifdef __cplusplus
}
#endif
#endif
