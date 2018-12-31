//file_for_patch.h
// patch demo file tool
//
/*
 This is the HDiffPatch copyright.

 Copyright (c) 2012-2017 HouSisong All Rights Reserved.

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
#include <stdlib.h> //malloc free
#include "libHDiffPatch/HPatch/patch_types.h"
typedef unsigned char TByte;
#define kFileIOBestMaxSize  (1024*1024)

static hpatch_BOOL fileTell64(FILE* file,hpatch_StreamPos_t* out_pos){
#ifdef _MSC_VER
    __int64 fpos=_ftelli64(file);
#else
    off_t fpos=ftello(file);
#endif
    hpatch_BOOL result=(fpos>=0);
    if (result) *out_pos=fpos;
    return result;
}

static hpatch_BOOL fileSeek64(FILE* file,hpatch_StreamPos_t seekPos,int whence){
#ifdef _MSC_VER
    int ret=_fseeki64(file,seekPos,whence);
#else
    off_t fpos=seekPos;
    if ((fpos<0)||((hpatch_StreamPos_t)fpos!=seekPos)) return hpatch_FALSE;
    int ret=fseeko(file,fpos,whence);
#endif
    return (ret==0);
}

static hpatch_BOOL fileClose(FILE** pfile){
    FILE* file=*pfile;
    if (file){
        *pfile=0;
        if (0!=fclose(file))
            return hpatch_FALSE;
    }
    return hpatch_TRUE;
}

static hpatch_BOOL fileRead(FILE* file,TByte* buf,TByte* buf_end){
    while (buf<buf_end) {
        size_t readLen=(size_t)(buf_end-buf);
        if (readLen>kFileIOBestMaxSize) readLen=kFileIOBestMaxSize;
        if (readLen!=fread(buf,1,readLen,file)) return hpatch_FALSE;
        buf+=readLen;
    }
    return buf==buf_end;
}

static hpatch_BOOL fileWrite(FILE* file,const TByte* data,const TByte* data_end){
    while (data<data_end) {
        size_t writeLen=(size_t)(data_end-data);
        if (writeLen>kFileIOBestMaxSize) writeLen=kFileIOBestMaxSize;
        if (writeLen!=fwrite(data,1,writeLen,file)) return hpatch_FALSE;
        data+=writeLen;
    }
    return data==data_end;
}

#define _file_error(fileHandle){ \
    if (fileHandle) fileClose(&fileHandle); \
    return hpatch_FALSE; \
}

#if defined(_MSC_VER)&&(_MSC_VER>=1400) //VC2005
#   define _fileOpenByMode(ppFile,fileName,mode) { \
        errno_t err=fopen_s(ppFile,fileName,mode);  \
        if (err!=0) *(ppFile)=0; }
#else
#   define _fileOpenByMode(ppFile,fileName,mode) { \
        *(ppFile)=fopen(fileName,mode); }
#endif

hpatch_inline static
hpatch_BOOL fileOpenForRead(const char* fileName,FILE** out_fileHandle,hpatch_StreamPos_t* out_fileLength){
    FILE* file=0;
    assert(out_fileHandle!=0);
    if (out_fileHandle==0) _file_error(file);
    _fileOpenByMode(&file,fileName,"rb");
    if (file==0) _file_error(file);
    if (out_fileLength!=0){
        hpatch_StreamPos_t file_length=0;
        if (!fileSeek64(file,0,SEEK_END)) _file_error(file);
        if (!fileTell64(file,&file_length)) _file_error(file);
        if (!fileSeek64(file,0,SEEK_SET)) _file_error(file);
        *out_fileLength=file_length;
    }
    *out_fileHandle=file;
    return hpatch_TRUE;
}

hpatch_inline static
hpatch_BOOL fileOpenForCreateOrReWrite(const char* fileName,FILE** out_fileHandle){
    FILE* file=0;
    assert(out_fileHandle!=0);
    if (out_fileHandle==0) _file_error(file);
    _fileOpenByMode(&file,fileName,"wb");
    if (file==0) _file_error(file);
    *out_fileHandle=file;
    return hpatch_TRUE;
}

typedef struct TFileStreamInput{
    hpatch_TStreamInput base;
    FILE*               m_file;
    hpatch_StreamPos_t  m_fpos;
    size_t              m_offset;
    hpatch_BOOL         fileError;
} TFileStreamInput;

hpatch_inline
static void TFileStreamInput_init(TFileStreamInput* self){
    memset(self,0,sizeof(TFileStreamInput));
}

    #define _fileError_return { self->fileError=hpatch_TRUE; return hpatch_FALSE; }

    static hpatch_BOOL _read_file(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                  TByte* out_data,TByte* out_data_end){
        size_t readLen;
        TFileStreamInput* self=(TFileStreamInput*)stream->streamImport;
        assert(out_data<=out_data_end);
        readLen=(size_t)(out_data_end-out_data);
        if (readLen==0) return hpatch_TRUE;
        if ((readLen>self->base.streamSize)
            ||(readFromPos>self->base.streamSize-readLen)) _fileError_return;
        if (self->m_fpos!=readFromPos+self->m_offset){
            if (!fileSeek64(self->m_file,readFromPos+self->m_offset,SEEK_SET)) _fileError_return;
        }
        if (!fileRead(self->m_file,out_data,out_data+readLen)) _fileError_return;
        self->m_fpos=readFromPos+self->m_offset+readLen;
        return hpatch_TRUE;
    }

hpatch_inline static
hpatch_BOOL TFileStreamInput_open(TFileStreamInput* self,const char* fileName){
    assert(self->m_file==0);
    if (self->m_file) return hpatch_FALSE;
    if (!fileOpenForRead(fileName,&self->m_file,&self->base.streamSize)) return hpatch_FALSE;
    
    self->base.streamImport=self;
    self->base.read=_read_file;
    self->m_fpos=0;
    self->m_offset=0;
    self->fileError=hpatch_FALSE;
    return hpatch_TRUE;
}

hpatch_inline static
void TFileStreamInput_setOffset(TFileStreamInput* self,size_t offset){
    assert(self->m_offset==0);
    assert(self->base.streamSize>=offset);
    self->m_offset=offset;
    self->base.streamSize-=offset;
}

hpatch_inline
static hpatch_BOOL TFileStreamInput_close(TFileStreamInput* self){
    return fileClose(&self->m_file);
}

typedef struct TFileStreamOutput{
    hpatch_TStreamOutput base;
    FILE*               m_file;
    hpatch_StreamPos_t  out_pos;
    hpatch_StreamPos_t  out_length;
    hpatch_BOOL         fileError;
    hpatch_BOOL         is_random_out;
} TFileStreamOutput;

hpatch_inline
static void TFileStreamOutput_init(TFileStreamOutput* self){
    memset(self,0,sizeof(TFileStreamOutput));
}

    static hpatch_BOOL _write_file(const hpatch_TStreamOutput* stream,hpatch_StreamPos_t writeToPos,
                                   const TByte* data,const TByte* data_end){
        size_t writeLen;
        TFileStreamOutput* self=(TFileStreamOutput*)stream->streamImport;
        assert(data<=data_end);
        writeLen=(size_t)(data_end-data);
        if (writeLen==0) return hpatch_TRUE;
        if ((writeLen>self->base.streamSize)
            ||(writeToPos>self->base.streamSize-writeLen)) _fileError_return;
        if (writeToPos!=self->out_pos){
            if (self->is_random_out){
                if (!fileSeek64(self->m_file,writeToPos,SEEK_SET)) _fileError_return;
                self->out_pos=writeToPos;
            }else{
                _fileError_return;
            }
        }
        if (!fileWrite(self->m_file,data,data+writeLen)) _fileError_return;
        self->out_pos=writeToPos+writeLen;
        self->out_length=(self->out_length>=self->out_pos)?self->out_length:self->out_pos;
        return hpatch_TRUE;
    }
hpatch_inline static
hpatch_BOOL TFileStreamOutput_open(TFileStreamOutput* self,const char* fileName,
                                          hpatch_StreamPos_t max_file_length){
    assert(self->m_file==0);
    if (self->m_file) return hpatch_FALSE;
    if (!fileOpenForCreateOrReWrite(fileName,&self->m_file)) return hpatch_FALSE;
    
    self->base.streamImport=self;
    self->base.streamSize=max_file_length;
    self->base.write=_write_file;
    self->out_pos=0;
    self->out_length=0;
    self->is_random_out=hpatch_FALSE;
    self->fileError=hpatch_FALSE;
    return hpatch_TRUE;
}

hpatch_inline static
void TFileStreamOutput_setRandomOut(TFileStreamOutput* self,hpatch_BOOL is_random_out){
    self->is_random_out=is_random_out;
}

hpatch_inline static
hpatch_BOOL TFileStreamOutput_flush(TFileStreamOutput* self){
    return (0!=fflush(self->m_file));
}

hpatch_inline
static hpatch_BOOL TFileStreamOutput_close(TFileStreamOutput* self){
    return fileClose(&self->m_file);
}

#endif
