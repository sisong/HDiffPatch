//patch_demo.cpp
// demo for HPatch
//NOTE: use HDiffZ+HPatchZ support compressed diff data
//
/*
 This is the HDiffPatch copyright.

 Copyright (c) 2012-2013 HouSisong All Rights Reserved.

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

#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "libHDiffPatch/HPatch/patch.h"
typedef unsigned char   TByte;

#define IS_USES_PATCH_STREAM

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

hpatch_BOOL fileSeek64(FILE* file,hpatch_StreamPos_t seekPos,int whence){
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

static int readSavedSize(const TByte* data,size_t dataSize,hpatch_StreamPos_t* outSize){
    size_t lsize;
    if (dataSize<4) return -1;
    lsize=data[0]|(data[1]<<8)|(data[2]<<16);
    if (data[3]!=0xFF){
        lsize|=data[3]<<24;
        *outSize=lsize;
        return 4;
    }else{
        size_t hsize;
        if (dataSize<9) return -1;
        lsize|=data[4]<<24;
        hsize=data[5]|(data[6]<<8)|(data[7]<<16)|(data[8]<<24);
        *outSize=lsize|(((hpatch_StreamPos_t)hsize)<<32);
        return 9;
    }
}

#define _clear_return(info){ \
    if (strlen(info)>0)      \
        printf("%s",(info)); \
    exitCode=1; \
    goto clear; \
}


#ifndef IS_USES_PATCH_STREAM

#define _file_error(fileHandle){ \
    if (fileHandle) _close_file(&fileHandle); \
    return hpatch_FALSE; \
}

hpatch_BOOL readFile(TByte** out_pdata,size_t* out_dataSize,const char* fileName){
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
    
    if (dataSize!=fread(*out_pdata,1,dataSize,file)) _file_error(file);
    return _close_file(&file);
}

hpatch_BOOL writeFile(const TByte* data,size_t dataSize,const char* fileName){
    FILE* file=fopen(fileName,"wb");
    if (file==0) _file_error(file);
    if (dataSize!=fwrite(data,1,dataSize,file)) _file_error(file);
    return _close_file(&file);
}

#define _free_mem(p){ \
    if (p) { free(p); p=0; } \
}

//diffFile need create by HDiff
int main(int argc, const char * argv[]){
    int     exitCode=0;
    clock_t time0=clock();
    clock_t time1,time2,time3;
    TByte*  oldData=0;
    TByte*  diffData=0;
    TByte*  newData=0;
    size_t  oldSize=0;
    size_t  diffSize=0;
    size_t  newSize=0;
    const char* outNewFileName=0;
    int kNewDataSizeSavedSize=0; //4 or 9
    
    if (argc!=4) {
        printf("patch command parameter:\n oldFileName diffFileName outNewFileName\n");
        return 1;
    }
    {//read file
        hpatch_StreamPos_t savedNewSize=0;
        const char* oldFileName=argv[1];
        const char* diffFileName=argv[2];
        outNewFileName=argv[3];
        printf("old :\"%s\"\ndiff:\"%s\"\nout :\"%s\"\n",oldFileName,diffFileName,outNewFileName);
        
        if (!readFile(&oldData,&oldSize,oldFileName)) _clear_return("\nread oldFile error!\n");
        if (!readFile(&diffData,&diffSize,diffFileName)) _clear_return("\nread diffFile error!\n");
        kNewDataSizeSavedSize=readSavedSize(diffData,diffSize,&savedNewSize);
        if (kNewDataSizeSavedSize<=0) _clear_return("\nread newSize from diffFile error!\n");
        newSize=(size_t)savedNewSize;
        if (newSize!=savedNewSize) _clear_return("\nmemroy not enough error!\n");
        newData=(TByte*)malloc(newSize);
        if (newData==0) _clear_return("\nmemroy alloc error!\n");
    }
    printf("oldDataSize : %ld\ndiffDataSize: %ld\nnewDataSize : %ld\n",
           oldSize,diffSize,newSize);
    
    time1=clock();
    if (!patch(newData,newData+newSize,oldData,oldData+oldSize,
               diffData+kNewDataSizeSavedSize,diffData+diffSize)){
        _clear_return("\npatch run error!!!\n");
    }
    time2=clock();
    if (!writeFile(newData,newSize,outNewFileName)) _clear_return("\nwrite newFile error!\n");
    printf("  patch ok!\n");
    printf("\npatch   time: %.0f ms\n",(time2-time1)*(1000.0/CLOCKS_PER_SEC));
clear:
    _free_mem(oldData);
    _free_mem(diffData);
    _free_mem(newData);
    time3=clock();
    printf("all run time: %.0f ms\n",(time3-time0)*(1000.0/CLOCKS_PER_SEC));
    return exitCode;
}

#else
//IS_USES_PATCH_STREAM


typedef struct TFileStreamInput{
    hpatch_TStreamInput base;
    FILE*               m_file;
    hpatch_StreamPos_t  m_fpos;
    hpatch_BOOL         fileError;
    unsigned long       m_offset;
} TFileStreamInput;

static void TFileStreamInput_init(TFileStreamInput* self){
    memset(self,0,sizeof(TFileStreamInput));
}

#define _fileError_return { self->fileError=hpatch_TRUE; return -1; }

static long _read_file(hpatch_TStreamInputHandle streamHandle,const hpatch_StreamPos_t readFromPos,
                       unsigned char* out_data,unsigned char* out_data_end){
    unsigned long readLen,readed;
    TFileStreamInput* self=(TFileStreamInput*)streamHandle;
    assert(out_data<=out_data_end);
    readLen=(unsigned long)(out_data_end-out_data);
    if ((readFromPos+readLen<readFromPos)
        ||(readFromPos+readLen>self->base.streamSize)) _fileError_return;
    if (self->m_fpos!=readFromPos+self->m_offset){
        if (!fileSeek64(self->m_file,readFromPos+self->m_offset,SEEK_SET)) _fileError_return;
    }
    readed=(unsigned long)fread(out_data,1,readLen,self->m_file);
    if (readed!=readLen) _fileError_return;
    self->m_fpos=readFromPos+self->m_offset+readed;
    return (long)readed;
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

static hpatch_BOOL TFileStreamInput_close(TFileStreamInput* self){
    return _close_file(&self->m_file);
}

static void TFileStreamInput_setOffset(TFileStreamInput* self,unsigned long offset){
    assert(self->m_offset==0);
    self->m_offset=offset;
    self->base.streamSize-=offset;
}


typedef struct TFileStreamOutput{
    hpatch_TStreamOutput base;
    FILE*               m_file;
    hpatch_StreamPos_t  out_length;
    hpatch_BOOL         fileError;
} TFileStreamOutput;

static void TFileStreamOutput_init(TFileStreamOutput* self){
    memset(self,0,sizeof(TFileStreamOutput));
}

static long _write_file(hpatch_TStreamInputHandle streamHandle,const hpatch_StreamPos_t writeToPos,
                        const unsigned char* data,const unsigned char* data_end){
    unsigned long writeLen,writed;
    TFileStreamOutput* self=(TFileStreamOutput*)streamHandle;
    assert(data<=data_end);
    writeLen=(unsigned long)(data_end-data);
    if ((writeToPos!=self->out_length)
        ||(writeToPos+writeLen<writeToPos)
        ||(writeToPos+writeLen>self->base.streamSize)) _fileError_return;
    writed=(unsigned long)fwrite(data,1,writeLen,self->m_file);
    if (writed!=writeLen)  _fileError_return;
    self->out_length+=writed;
    return (long)writed;
}
static hpatch_BOOL TFileStreamOutput_open(TFileStreamOutput* self,
                                          const char* fileName,hpatch_StreamPos_t file_length){
    assert(self->m_file==0);
    if (self->m_file) return hpatch_FALSE;
    
    self->m_file=fopen(fileName, "wb");
    if (self->m_file==0) return hpatch_FALSE;
    self->base.streamHandle=self;
    self->base.streamSize=file_length;
    self->base.write=_write_file;
    self->out_length=0;
    self->fileError=hpatch_FALSE;
    return hpatch_TRUE;
}

static hpatch_BOOL TFileStreamOutput_close(TFileStreamOutput* self){
    return _close_file(&self->m_file);
}


#define _check_error(is_error,errorInfo){ \
    if (is_error){  \
        exitCode=1; \
        printf("%s",(errorInfo)); \
    } \
}


//diffFile need create by HDiff
int main(int argc, const char * argv[]){
    int     exitCode=0;
    clock_t time0=clock();
    clock_t time1,time2,time3;
    TFileStreamInput  oldData;
    TFileStreamInput  diffData;
    TFileStreamOutput newData;
    if (argc!=4) {
        printf("patch command parameter:\n oldFileName diffFileName outNewFileName\n");
        return 1;
    }
    TFileStreamInput_init(&oldData);
    TFileStreamInput_init(&diffData);
    TFileStreamOutput_init(&newData);
    {//open file
        int kNewDataSizeSavedSize=9;
        TByte buf[9];
        hpatch_StreamPos_t savedNewSize=0;
        const char* oldFileName=argv[1];
        const char* diffFileName=argv[2];
        const char* outNewFileName=argv[3];
        printf("old :\"%s\"\ndiff:\"%s\"\nout :\"%s\"\n",oldFileName,diffFileName,outNewFileName);
        if (!TFileStreamInput_open(&oldData,oldFileName))
            _clear_return("\nopen oldFile error!\n");
        if (!TFileStreamInput_open(&diffData,diffFileName))
            _clear_return("\nopen diffFile error!\n");
        //read savedNewSize
        if (kNewDataSizeSavedSize>diffData.base.streamSize)
            kNewDataSizeSavedSize=(int)diffData.base.streamSize;
        if (kNewDataSizeSavedSize!=diffData.base.read(diffData.base.streamHandle,0,
                                                      buf,buf+kNewDataSizeSavedSize))
            _clear_return("\nread savedNewSize error!\n");
        kNewDataSizeSavedSize=readSavedSize(buf,kNewDataSizeSavedSize,&savedNewSize);
        if (kNewDataSizeSavedSize<=0) _clear_return("\nread savedNewSize error!\n");
        TFileStreamInput_setOffset(&diffData,kNewDataSizeSavedSize);
        
        if (!TFileStreamOutput_open(&newData, outNewFileName,savedNewSize))
            _clear_return("\nopen out newFile error!\n");
    }
    printf("oldDataSize : %lld\ndiffDataSize: %lld\nnewDataSize : %lld\n",
           oldData.base.streamSize,diffData.base.streamSize,newData.base.streamSize);
    
    time1=clock();
    if (!patch_stream(&newData.base,&oldData.base,&diffData.base)){
        _check_error(oldData.fileError,"\noldFile read error!\n");
        _check_error(diffData.fileError,"\ndiffFile read error!\n");
        _check_error(newData.fileError,"\nout newFile write error!\n");
        _clear_return("\npatch_stream() run error!\n");
    }
    if (newData.out_length!=newData.base.streamSize){
        printf("\nerror! out newFile dataSize %lld != saved newDataSize %lld\n",
               newData.out_length,newData.base.streamSize);
        _clear_return("");
    }
    time2=clock();
    printf("  patch ok!\n");
    printf("\npatch   time: %.0f ms\n",(time2-time1)*(1000.0/CLOCKS_PER_SEC));
    
clear:
    _check_error(!TFileStreamInput_close(&oldData),"\noldFile close error!\n");
    _check_error(!TFileStreamInput_close(&diffData),"\ndiffFile close error!\n");
    _check_error(!TFileStreamOutput_close(&newData),"\nout newFile close error!\n");
    time3=clock();
    printf("all run time: %.0f ms\n",(time3-time0)*(1000.0/CLOCKS_PER_SEC));
    return exitCode;
}

#endif

