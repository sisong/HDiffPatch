//hpatchz.c
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
#include <assert.h>
#include <string.h> //strlen
#include <time.h>   //clock
#include "libHDiffPatch/HPatch/patch.h"

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
        unsigned long readLen,readed;
        TFileStreamInput* self=(TFileStreamInput*)streamHandle;
        assert(out_data<=out_data_end);
        readLen=(unsigned long)(out_data_end-out_data);
        if ((readFromPos+readLen<readFromPos)
            ||(readFromPos+readLen>self->base.streamSize)) _fileError_return;
        if (self->m_fpos!=readFromPos){
            if (!fileSeek64(self->m_file,readFromPos,SEEK_SET)) _fileError_return;
        }
        readed=(unsigned long)fread(out_data,1,readLen,self->m_file);
        if (readed!=readLen) _fileError_return;
        self->m_fpos=readFromPos+readed;
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


//===== select decompress plugin =====
#define _CompressPlugin_zlib
#define _CompressPlugin_bz2
//#define _CompressPlugin_lzma

#include "decompress_plugin_demo.h"

#define _clear_return(info){ \
    if (strlen(info)>0)      \
        printf("%s",(info)); \
    exitCode=1; \
    goto clear; \
}

#define _check_error(is_error,errorInfo){ \
    if (is_error){  \
        exitCode=1; \
        printf("%s",(errorInfo)); \
    } \
}


//diffFile need create by HDiffZ
int main(int argc, const char * argv[]){
    int     exitCode=0;
    clock_t time0=clock();
    clock_t time1;
    clock_t time2;
    clock_t time3;
    hpatch_TDecompress* decompressPlugin=0;
    TFileStreamInput  oldData;
    TFileStreamInput  diffData;
    TFileStreamOutput newData;
    if (argc!=4) {
        printf("HPatchZ command parameter:\n oldFileName diffFileName outNewFileName\n");
        return 1;
    }
    TFileStreamInput_init(&oldData);
    TFileStreamInput_init(&diffData);
    TFileStreamOutput_init(&newData);
    {//open file
        hpatch_compressedDiffInfo diffInfo;
        const char* oldFileName=argv[1];
        const char* diffFileName=argv[2];
        const char* outNewFileName=argv[3];
        printf("old :\"%s\"\ndiff:\"%s\"\nout :\"%s\"\n",oldFileName,diffFileName,outNewFileName);
        if (!TFileStreamInput_open(&oldData,oldFileName))
            _clear_return("\nopen oldFile error!\n");
        if (!TFileStreamInput_open(&diffData,diffFileName))
            _clear_return("\nopen diffFile error!\n");
        if (!getCompressedDiffInfo(&diffInfo,&diffData.base)){
            _check_error(diffData.fileError,"\ndiffFile read error!\n");
            _clear_return("\ngetCompressedDiffInfo() run error!\n");
        }
        if (oldData.base.streamSize!=diffInfo.oldDataSize){
            printf("\nerror! oldFile dataSize %lld != saved oldDataSize %lld\n",
                   oldData.base.streamSize,diffInfo.oldDataSize);
            _clear_return("");
        }
        
        if (strlen(diffInfo.compressType)>0){
#ifdef  _CompressPlugin_zlib
            if ((!decompressPlugin)&&zlibDecompressPlugin.is_can_open(&zlibDecompressPlugin,&diffInfo))
                decompressPlugin=&zlibDecompressPlugin;
#endif
#ifdef  _CompressPlugin_bz2
            if ((!decompressPlugin)&&bz2DecompressPlugin.is_can_open(&bz2DecompressPlugin,&diffInfo))
                decompressPlugin=&bz2DecompressPlugin;
#endif
#ifdef  _CompressPlugin_lzma
            if ((!decompressPlugin)&&lzmaDecompressPlugin.is_can_open(&lzmaDecompressPlugin,&diffInfo))
                decompressPlugin=&lzmaDecompressPlugin;
#endif
        }
        if (!decompressPlugin){
            if (diffInfo.compressedCount>0){
                printf("\nerror! can no decompress \"%s\" data\n",diffInfo.compressType);
                _clear_return("");
            }else{
                if (strlen(diffInfo.compressType)>0)
                    printf("  diffFile added useless compress tag \"%s\"\n",diffInfo.compressType);
                decompressPlugin=hpatch_kNodecompressPlugin;
            }
        }else{
            printf("  HPatchZ used decompress tag \"%s\" (need decompress %d)\n",
                   diffInfo.compressType,diffInfo.compressedCount);
        }
        
        if (!TFileStreamOutput_open(&newData, outNewFileName,diffInfo.newDataSize))
            _clear_return("\nopen out newFile error!\n");
    }
    printf("oldDataSize : %lld\ndiffDataSize: %lld\nnewDataSize : %lld\n",
           oldData.base.streamSize,diffData.base.streamSize,newData.base.streamSize);
    
    time1=clock();
    if (!patch_decompress(&newData.base,&oldData.base,&diffData.base,decompressPlugin)){
        _check_error(oldData.fileError,"\noldFile read error!\n");
        _check_error(diffData.fileError,"\ndiffFile read error!\n");
        _check_error(newData.fileError,"\nout newFile write error!\n");
        _clear_return("\npatch_decompress() run error!\n");
    }
    if (newData.out_length!=newData.base.streamSize){
        printf("\nerror! out newFile dataSize %lld != saved newDataSize %lld\n",
               newData.out_length,newData.base.streamSize);
        _clear_return("");
    }
    time2=clock();
    printf("  patch ok!\n");
    printf("\nHPatchZ time: %.0f ms\n",(time2-time1)*(1000.0/CLOCKS_PER_SEC));
    
clear:
    _check_error(!TFileStreamInput_close(&oldData),"\noldFile close error!\n");
    _check_error(!TFileStreamInput_close(&diffData),"\ndiffFile close error!\n");
    _check_error(!TFileStreamOutput_close(&newData),"\nout newFile close error!\n");
    time3=clock();
    printf("all run time: %.0f ms\n",(time3-time0)*(1000.0/CLOCKS_PER_SEC));
    return exitCode;
}

