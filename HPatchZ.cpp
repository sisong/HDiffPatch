//HPatchZ.cpp
// patch with decompress plugin
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

#include <stdio.h>
#include "assert.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "libHDiffPatch/HPatch/patch.h"

static hpatch_BOOL getFilePos64(FILE* file,hpatch_StreamPos_t* out_pos){
    fpos_t fpos;
    int ret=fgetpos(file, &fpos); //support 64bit?
    hpatch_BOOL result=(ret==0);
    if (result){
#if defined(__linux) || defined(__linux__)
        *out_pos=fpos.__pos;
#else //windows macosx
        *out_pos=fpos;
#endif
    }
    return result;
}

hpatch_BOOL setFilePos64(FILE* file,hpatch_StreamPos_t seekPos){
    fpos_t fpos;
#if defined(__linux) || defined(__linux__)
    memset(&fpos,0,sizeof(fpos));
    fpos.__pos=seekPos;
#else //windows macosx
    fpos=seekPos;
#endif
    int ret=fsetpos(file,&fpos);
    return (ret==0);
}


static hpatch_BOOL _close_file(FILE** pfile){
    hpatch_BOOL result=hpatch_TRUE;
    FILE* file=*pfile;
    if (file!=0){
        if (0!=fclose(file))
            result=hpatch_FALSE;
        *pfile=0;
    }
    return result;
}



typedef struct TFileStreamInput{
    hpatch_TStreamInput base;
    FILE*               m_file;
    hpatch_StreamPos_t  m_fpos;
    hpatch_BOOL         fileError;
} TFileStreamInput;

static void TFileStreamInput_init(TFileStreamInput* self){
    memset(self,0,sizeof(TFileStreamInput));
}

    #define _fileError_return { self->fileError=hpatch_TRUE; return -1; }

    static long _read_file(hpatch_TStreamInputHandle streamHandle,const hpatch_StreamPos_t readFromPos,
                           unsigned char* out_data,unsigned char* out_data_end){
        TFileStreamInput* self=(TFileStreamInput*)streamHandle;
        size_t readLen=(size_t)(out_data_end-out_data);
        if ((readFromPos+readLen<readFromPos)
            ||(readFromPos+readLen>self->base.streamSize)) _fileError_return;
        if (self->m_fpos!=readFromPos){
            if (!setFilePos64(self->m_file,readFromPos)) _fileError_return;
        }
        size_t readed=fread(out_data,1,readLen,self->m_file);
        if (readed!=readLen) _fileError_return;
        self->m_fpos=readFromPos+readed;
        return (long)readed;
    }
static hpatch_BOOL TFileStreamInput_open(TFileStreamInput* self,const char* fileName){
    assert(self->m_file==0);
    if (self->m_file) return hpatch_FALSE;
    
    self->m_file=fopen(fileName, "rb");
    if (self->m_file==0) return hpatch_FALSE;
    if (0!=fseek(self->m_file, 0, SEEK_END)) return hpatch_FALSE;
    if (!getFilePos64(self->m_file,&self->base.streamSize)) return hpatch_FALSE;
    if (0!=fseek(self->m_file, 0, SEEK_SET)) return hpatch_FALSE;
    
    self->base.streamHandle=self;
    self->base.read=_read_file;
    self->m_fpos=0;
    self->fileError=hpatch_FALSE;
    return hpatch_TRUE;
}

static hpatch_BOOL TFileStreamInput_close(TFileStreamInput* self){
    return _close_file(&self->m_file);
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
        TFileStreamOutput* self=(TFileStreamOutput*)streamHandle;
        size_t writeLen=(size_t)(data_end-data);
        if ((writeToPos!=self->out_length)
            ||(writeToPos+writeLen<writeToPos)
            ||(writeToPos+writeLen>self->base.streamSize)) _fileError_return;
        size_t writed=fwrite(data,1,writeLen,self->m_file);
        if (writed!=writeLen)  _fileError_return;
        self->out_length+=writed;
        return (long)writed;
    }
static hpatch_BOOL TFileStreamOutput_open(TFileStreamOutput* self,
                                          const char* fileName,hpatch_StreamPos_t file_length){
    assert(self->m_file==0);
    if (self->m_file) return hpatch_FALSE;
    
    self->m_file=fopen(fileName, "wb+");
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

static void check_error(hpatch_BOOL error,int* outExitCode,const char* errorInfo){
    if (!error) return;
    if (outExitCode) *outExitCode=10;
    printf("%s", errorInfo);
}

//===== select decompress plugin =====
#define _CompressPlugin_zlib
#define _CompressPlugin_bz2
//#define _CompressPlugin_lzma

#include "decompress_plugin_demo.h"

#define _clear_return(info,exitCode) {\
    if (strlen(info)>0) printf(info); \
    result=exitCode; \
    goto clear;      \
}

//diffFile need create by HDiffZ
int main(int argc, const char * argv[]){
    if (argc!=4) {
        printf("HPatchZ command parameter:\n oldFileName diffFileName outNewFileName\n");
        return 1;
    }
    
    clock_t time0=clock();
    clock_t time1;
    clock_t time2;
    clock_t time3;
    int                 result=0;
    hpatch_TDecompress* decompressPlugin=0;
    TFileStreamInput  oldData;
    TFileStreamInput  diffData;
    TFileStreamOutput newData;
    TFileStreamInput_init(&oldData);
    TFileStreamInput_init(&diffData);
    TFileStreamOutput_init(&newData);
    {//open file
        const char* oldFileName=argv[1];
        const char* diffFileName=argv[2];
        const char* outNewFileName=argv[3];
        printf("old :\"%s\"\ndiff:\"%s\"\nout :\"%s\"\n",oldFileName,diffFileName,outNewFileName);
        if (!TFileStreamInput_open(&oldData,oldFileName))
            _clear_return("\nopen oldFile error!\n",2);
        if (!TFileStreamInput_open(&diffData,diffFileName))
            _clear_return("\nopen diffFile error!\n",3);
        hpatch_compressedDiffInfo diffInfo;
        if (!getCompressedDiffInfo(&diffInfo,&diffData.base)){
            check_error(diffData.fileError,0,"\ndiffFile read error!\n");
            _clear_return("\ngetCompressedDiffInfo() run error!\n",4);
        }
        if (oldData.base.streamSize!=diffInfo.oldDataSize){
            printf("\nerror! oldFile dataSize %lld != saved oldDataSize %lld\n",
                   oldData.base.streamSize,diffInfo.oldDataSize);
            _clear_return("",5);
        }
        
        if (strlen(diffInfo.compressType)>0){
#ifdef  _CompressPlugin_zlib
            if (zlibDecompressPlugin.is_can_open(&zlibDecompressPlugin,diffInfo.compressType))
                decompressPlugin=&zlibDecompressPlugin;
#endif
#ifdef  _CompressPlugin_bz2
            if (bz2DecompressPlugin.is_can_open(&bz2DecompressPlugin,diffInfo.compressType))
                decompressPlugin=&bz2DecompressPlugin;
#endif
#ifdef  _CompressPlugin_lzma
            if (lzmaDecompressPlugin.is_can_open(&lzmaDecompressPlugin,diffInfo.compressType))
                decompressPlugin=&lzmaDecompressPlugin;
#endif
        }
        if (decompressPlugin==0){
            if (diffInfo.compressedCount>0){
                printf("\nerror! can no decompress \"%s\" data\n",diffInfo.compressType);
                _clear_return("",6);
            }else{
                if (strlen(diffInfo.compressType)>0)
                    printf("diffFile added useless compress tag \"%s\"\n",diffInfo.compressType);
                if (diffInfo.pluginInfoSize>0)
                    printf("diffFile added useless pluginInfo data(%dbyte)\n",diffInfo.pluginInfoSize);
                decompressPlugin=hpatch_kNodecompressPlugin;
            }
        }else{
            printf("HPatchZ used decompress tag \"%s\"\n",diffInfo.compressType);
        }
        
        if (!TFileStreamOutput_open(&newData, outNewFileName,diffInfo.newDataSize))
            _clear_return("\nopen out newFile error!\n",7);
    }
    
    time1=clock();
    if (!patch_decompress(&newData.base,&oldData.base,&diffData.base,decompressPlugin)){
        check_error(oldData.fileError,0,"\noldFile read error!\n");
        check_error(diffData.fileError,0,"\ndiffFile read error!\n");
        check_error(newData.fileError,0,"\nout newFile write error!\n");
        _clear_return("\npatch_decompress() run error!\n",8);
    }
    if (newData.out_length!=newData.base.streamSize){
        printf("\nerror! out newFile dataSize %lld != saved newDataSize %lld\n",
               newData.out_length,newData.base.streamSize);
        _clear_return("",9);
    }
    time2=clock();
    printf("  patch ok!\n");
    printf("oldDataSize : %lld\ndiffDataSize: %lld\nnewDataSize : %lld\n",
           oldData.base.streamSize,diffData.base.streamSize,newData.base.streamSize);
    printf("\nHPatchZ time: %.0f ms\n",(time2-time1)*(1000.0/CLOCKS_PER_SEC));
    
clear:
    check_error(!TFileStreamInput_close(&oldData),&result,"\noldFile close error!\n");
    check_error(!TFileStreamInput_close(&diffData),&result,"\ndiffFile close error!\n");
    check_error(!TFileStreamOutput_close(&newData),&result,"\nout newFile close error!\n");
    time3=clock();
    printf("all run time: %.0f ms\n",(time3-time0)*(1000.0/CLOCKS_PER_SEC));
    return result;
}

