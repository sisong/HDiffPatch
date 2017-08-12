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
#include <assert.h>
#include <stdlib.h> //malloc free
#include "libHDiffPatch/HPatch/patch_types.h"
typedef unsigned char TByte;

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

static hpatch_BOOL _close_file(FILE** pfile){
    FILE* file=*pfile;
    if (file){
        *pfile=0;
        if (0!=fclose(file))
            return hpatch_FALSE;
    }
    return hpatch_TRUE;
}

static hpatch_BOOL fileRead(FILE* file,TByte* buf,TByte* buf_end){
    static const size_t kBestSize=1<<20;
    while (buf<buf_end) {
        size_t readLen=(size_t)(buf_end-buf);
        if (readLen>kBestSize) readLen=kBestSize;
        if (readLen!=fread(buf,1,readLen,file)) return hpatch_FALSE;
        buf+=readLen;
    }
    return buf==buf_end;
}

#define _file_error(fileHandle){ \
    if (fileHandle) _close_file(&fileHandle); \
    return hpatch_FALSE; \
}

hpatch_BOOL readFileAll(TByte** out_pdata,size_t* out_dataSize,const char* fileName){
    hpatch_StreamPos_t file_length=0;
    size_t dataSize;
    FILE* file=fopen(fileName,"rb");
    if (file==0) _file_error(file);
    if (!fileSeek64(file,0,SEEK_END)) _file_error(file);
    if (!fileTell64(file,&file_length)) _file_error(file);
    if (!fileSeek64(file,0,SEEK_SET)) _file_error(file);
    
    assert((*out_pdata)==0);
    dataSize=(size_t)file_length;
    if (dataSize!=file_length) _file_error(file);
    *out_pdata=(TByte*)malloc(dataSize);
    if (*out_pdata==0) _file_error(file);
    *out_dataSize=dataSize;
    
    if (!fileRead(file,*out_pdata,(*out_pdata)+dataSize)) _file_error(file);
    return _close_file(&file);
}

typedef struct TFileStreamInput{
    hpatch_TStreamInput base;
    FILE*               m_file;
    hpatch_StreamPos_t  m_fpos;
    size_t              m_offset;
    hpatch_BOOL         fileError;
} TFileStreamInput;

static void TFileStreamInput_init(TFileStreamInput* self){
    memset(self,0,sizeof(TFileStreamInput));
}

    #define _fileError_return { self->fileError=hpatch_TRUE; return -1; }

    static long _read_file(hpatch_TStreamInputHandle streamHandle,const hpatch_StreamPos_t readFromPos,
                           TByte* out_data,TByte* out_data_end){
        size_t readLen;
        TFileStreamInput* self=(TFileStreamInput*)streamHandle;
        assert(out_data<=out_data_end);
        readLen=(size_t)(out_data_end-out_data);
        if ((readFromPos+readLen<readFromPos)
            ||(readFromPos+readLen>self->base.streamSize)) _fileError_return;
        if (self->m_fpos!=readFromPos+self->m_offset){
            if (!fileSeek64(self->m_file,readFromPos+self->m_offset,SEEK_SET)) _fileError_return;
        }
        if (!fileRead(self->m_file,out_data,out_data+readLen)) _fileError_return;
        self->m_fpos=readFromPos+self->m_offset+readLen;
        return (long)readLen;
    }
static hpatch_BOOL TFileStreamInput_open(TFileStreamInput* self,const char* fileName){
    assert(self->m_file==0);
    if (self->m_file) return hpatch_FALSE;
    
    self->m_file=fopen(fileName, "rb");
    if (self->m_file==0) return hpatch_FALSE;
    if (!fileSeek64(self->m_file, 0, SEEK_END)) return hpatch_FALSE;
    if (!fileTell64(self->m_file,&self->base.streamSize)) return hpatch_FALSE;
    if (!fileSeek64(self->m_file, 0, SEEK_SET)) return hpatch_FALSE;
    
    self->base.streamHandle=self;
    self->base.read=_read_file;
    self->m_fpos=0;
    self->m_offset=0;
    self->fileError=hpatch_FALSE;
    return hpatch_TRUE;
}

void TFileStreamInput_setOffset(TFileStreamInput* self,size_t offset){
    assert(self->m_offset==0);
    assert(self->base.streamSize>=offset);
    self->m_offset=offset;
    self->base.streamSize-=offset;
}

static hpatch_BOOL TFileStreamInput_close(TFileStreamInput* self){
    return _close_file(&self->m_file);
}

typedef struct TFileStreamOutput{
    hpatch_TStreamOutput base;
    FILE*               m_file;
    hpatch_StreamPos_t  out_length;
    hpatch_BOOL         fileError;
    hpatch_BOOL         is_repeat_out;
} TFileStreamOutput;

static void TFileStreamOutput_init(TFileStreamOutput* self){
    memset(self,0,sizeof(TFileStreamOutput));
}

    static long _write_file(hpatch_TStreamInputHandle streamHandle,const hpatch_StreamPos_t writeToPos,
                            const TByte* data,const TByte* data_end){
        unsigned long writeLen,writed;
        TFileStreamOutput* self=(TFileStreamOutput*)streamHandle;
        assert(data<=data_end);
        if (writeToPos!=self->out_length){
            if (self->is_repeat_out&&(writeToPos==0)
                &&(self->out_length==self->base.streamSize)){//rewrite
                if (!fileSeek64(self->m_file,0,SEEK_SET)) _fileError_return;
                self->out_length=0;
            }else{
                _fileError_return;
            }
        }
        writeLen=(unsigned long)(data_end-data);
        if ((writeToPos+writeLen<writeToPos)
            ||(writeToPos+writeLen>self->base.streamSize)) _fileError_return;
        writed=(unsigned long)fwrite(data,1,writeLen,self->m_file);
        if (writed!=writeLen)  _fileError_return;
        self->out_length+=writed;
        if ((self->out_length==self->base.streamSize)&&(self->is_repeat_out)){
            if (0!=fflush(self->m_file)) _fileError_return;
        }
        return (long)writed;
    }
static hpatch_BOOL TFileStreamOutput_open(TFileStreamOutput* self,const char* fileName,
                                          hpatch_StreamPos_t file_length){
    assert(self->m_file==0);
    if (self->m_file) return hpatch_FALSE;
    
    self->m_file=fopen(fileName, "wb");
    if (self->m_file==0) return hpatch_FALSE;
    self->base.streamHandle=self;
    self->base.streamSize=file_length;
    self->base.write=_write_file;
    self->out_length=0;
    self->is_repeat_out=hpatch_FALSE;
    self->fileError=hpatch_FALSE;
    return hpatch_TRUE;
}

void TFileStreamOutput_setRepeatOut(TFileStreamOutput* self,hpatch_BOOL is_repeat_out){
    self->is_repeat_out=is_repeat_out;
}

static hpatch_BOOL TFileStreamOutput_close(TFileStreamOutput* self){
    return _close_file(&self->m_file);
}

#endif
