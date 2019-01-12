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
#include <stdio.h>
#include <stdlib.h> // malloc free
#include <locale.h> // setlocale
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
#if (_WIN32)
#   include <wchar.h>
#   include <windows.h> //for file API, character encoding API
#endif
#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char TByte;
#define kFileIOBestMaxSize  (1<<20)
#define kPathMaxSize  1024

#ifndef PRIu64
#   ifdef _MSC_VER
#       define PRIu64 "I64u"
#   else
#       define PRIu64 "llu"
#   endif
#endif

typedef FILE* hpatch_FileHandle;

static hpatch_BOOL _import_fileTell64(hpatch_FileHandle file,hpatch_StreamPos_t* out_pos){
#ifdef _MSC_VER
    __int64 fpos=_ftelli64(file);
#else
    off_t fpos=ftello(file);
#endif
    hpatch_BOOL result=(fpos>=0);
    if (result) *out_pos=fpos;
    return result;
}

static hpatch_BOOL _import_fileSeek64(hpatch_FileHandle file,hpatch_StreamPos_t seekPos,int whence){
#ifdef _MSC_VER
    int ret=_fseeki64(file,seekPos,whence);
#else
    off_t fpos=seekPos;
    if ((fpos<0)||((hpatch_StreamPos_t)fpos!=seekPos)) return hpatch_FALSE;
    int ret=fseeko(file,fpos,whence);
#endif
    return (ret==0);
}

static hpatch_BOOL _import_fileClose(hpatch_FileHandle* pfile){
    hpatch_FileHandle file=*pfile;
    if (file){
        *pfile=0;
        if (0!=fclose(file))
            return hpatch_FALSE;
    }
    return hpatch_TRUE;
}

hpatch_inline static
hpatch_BOOL _import_fileRead(hpatch_FileHandle file,TByte* buf,TByte* buf_end){
    while (buf<buf_end) {
        size_t readLen=(size_t)(buf_end-buf);
        if (readLen>kFileIOBestMaxSize) readLen=kFileIOBestMaxSize;
        if (readLen!=fread(buf,1,readLen,file)) return hpatch_FALSE;
        buf+=readLen;
    }
    return buf==buf_end;
}

hpatch_inline static
hpatch_BOOL _import_fileWrite(hpatch_FileHandle file,const TByte* data,const TByte* data_end){
    while (data<data_end) {
        size_t writeLen=(size_t)(data_end-data);
        if (writeLen>kFileIOBestMaxSize) writeLen=kFileIOBestMaxSize;
        if (writeLen!=fwrite(data,1,writeLen,file)) return hpatch_FALSE;
        data+=writeLen;
    }
    return data==data_end;
}


hpatch_inline static
hpatch_BOOL import_fileFlush(hpatch_FileHandle writedFile){
    return (0==fflush(writedFile));
}

#define _file_error(fileHandle){ \
    if (fileHandle) _import_fileClose(&fileHandle); \
    return hpatch_FALSE; \
}

#if (_WIN32)
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
#   define _kFileReadMode  L"rb"
#   define _kFileWriteMode L"wb"
#else
#   define _kFileReadMode  "rb"
#   define _kFileWriteMode "wb"
#endif

#if (_IS_USE_WIN32_UTF8_WAPI)
    static hpatch_FileHandle _import_fileOpenByMode(const char* fileName_utf8,const wchar_t* mode_w){
        wchar_t fileName_w[kPathMaxSize];
        int wsize=_utf8FileName_to_w(fileName_utf8,fileName_w,kPathMaxSize);
        if (wsize>0) {
    # if (_MSC_VER>=1400) // VC2005
            hpatch_FileHandle file=0;
            errno_t err=_wfopen_s(&file,fileName_w,mode_w);
            return (err==0)?file:0;
    # else
            return _wfopen(fileName_w,mode_w);
    # endif
        }else{
            return 0;
        }
    }
#else
    static hpatch_FileHandle _import_fileOpenByMode(const char* fileName_utf8,const char* mode){
        return fopen(fileName_utf8,mode); }
#endif

hpatch_inline static
hpatch_BOOL _import_fileOpenRead(const char* fileName_utf8,hpatch_FileHandle* out_fileHandle,
                                 hpatch_StreamPos_t* out_fileLength){
    hpatch_FileHandle file=0;
    assert(out_fileHandle!=0);
    if (out_fileHandle==0) _file_error(file);
    file=_import_fileOpenByMode(fileName_utf8,_kFileReadMode);
    if (file==0) _file_error(file);
    if (out_fileLength!=0){
        hpatch_StreamPos_t file_length=0;
        if (!_import_fileSeek64(file,0,SEEK_END)) _file_error(file);
        if (!_import_fileTell64(file,&file_length)) _file_error(file);
        if (!_import_fileSeek64(file,0,SEEK_SET)) _file_error(file);
        *out_fileLength=file_length;
    }
    *out_fileHandle=file;
    return hpatch_TRUE;
}

hpatch_inline static
hpatch_BOOL _import_fileOpenCreateOrReWrite(const char* fileName_utf8,hpatch_FileHandle* out_fileHandle){
    hpatch_FileHandle file=0;
    assert(out_fileHandle!=0);
    if (out_fileHandle==0) _file_error(file);
    file=_import_fileOpenByMode(fileName_utf8,_kFileWriteMode);
    if (file==0) _file_error(file);
    *out_fileHandle=file;
    return hpatch_TRUE;
}

#undef _file_error

#if (_IS_USE_WIN32_UTF8_WAPI)
hpatch_inline static
void SetDefaultStringLocale(){ //for some locale Path character encoding view
    setlocale(LC_CTYPE,"");
}
#endif

hpatch_inline static
hpatch_BOOL isSamePath(const char* xPath_utf8,const char* yPath_utf8){
    if (0==strcmp(xPath_utf8,yPath_utf8)){
        return hpatch_TRUE;
    }else{
        // WARING!!! better return getCanonicalPath(xPath_utf8)==getCanonicalPath(yPath_utf8);
        return hpatch_FALSE;
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

typedef struct TFileStreamInput{
    hpatch_TStreamInput base;
    hpatch_FileHandle   m_file;
    hpatch_StreamPos_t  m_fpos;
    size_t              m_offset;
    hpatch_BOOL         fileError;
} TFileStreamInput;

hpatch_inline
static void TFileStreamInput_init(TFileStreamInput* self){
    memset(self,0,sizeof(TFileStreamInput));
}
hpatch_BOOL TFileStreamInput_open(TFileStreamInput* self,const char* fileName_utf8);
void TFileStreamInput_setOffset(TFileStreamInput* self,size_t offset);
hpatch_BOOL TFileStreamInput_close(TFileStreamInput* self);

typedef struct TFileStreamOutput{
    hpatch_TStreamOutput base;
    hpatch_FileHandle   m_file;
    hpatch_StreamPos_t  out_pos;
    hpatch_StreamPos_t  out_length;
    hpatch_BOOL         fileError;
    hpatch_BOOL         is_random_out;
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
