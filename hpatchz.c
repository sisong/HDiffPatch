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

#include <string.h>
#include <stdlib.h>
#include "libHDiffPatch/HPatch/patch.h"

//#define _IS_USE_PATCH_CACHE      //ON: faster, add some memory for patch cache
//#define _IS_USE_OLD_FILE_STREAM  //ON: slower, memroy requires less,because not need load oldFile
//#define _IS_USE_PATCH_REPEAT_OUT //ON: slower, decompress memroy requires to be halved!

#ifdef _IS_USE_PATCH_REPEAT_OUT
#   undef _IS_USE_PATCH_CACHE
#endif

#ifdef _IS_USE_PATCH_CACHE
#   define  k_patch_cache_size  (1<<22)
#endif

//===== select needs decompress plugins or your plugin=====
#define _CompressPlugin_zlib
#define _CompressPlugin_bz2
//#define _CompressPlugin_lzma

#include "decompress_plugin_demo.h"

#include "file_for_patch.h"

//  #include <time.h>
//  static double clock_s(){ return clock()*(1.0/CLOCKS_PER_SEC); }
#ifdef _WIN32
    #include <windows.h>
    static double clock_s(){ return GetTickCount()/1000.0; }
#else
    //Unix-like system
    #include <sys/time.h>
    static double clock_s(){
        struct timeval t={0,0};
        int ret=gettimeofday(&t,0);
        assert(ret==0);
        return t.tv_sec + t.tv_usec/1000000.0;
    }
#endif

#ifndef PRId64
#   ifdef _MSC_VER
#       define PRId64 "I64d"
#   else
#       define PRId64 "lld"
#   endif
#endif

#define _free_mem(p) { if (p) { free(p); p=0; } }

#define _error_return(info){ \
    if (strlen(info)>0)      \
        printf("\n  %s\n",(info)); \
    exitCode=1; \
    goto clear; \
}

#define _check_error(is_error,errorInfo){ \
    if (is_error){  \
        exitCode=1; \
        printf("\n  %s\n",(errorInfo)); \
    } \
}

//diffFile need create by HDiffZ
int main(int argc, const char * argv[]){
    int     exitCode=0;
    double  time0=clock_s();
    double  time1,time2,time3;
    hpatch_TDecompress* decompressPlugin=0;
    TFileStreamOutput   newData;
    TFileStreamInput    diffData;
#ifdef _IS_USE_OLD_FILE_STREAM
    TFileStreamInput     oldData;
    hpatch_TStreamInput* poldData=&oldData.base;
#else
    TByte*               poldData_mem=0;
    size_t               oldDataSize=0;
    hpatch_TStreamInput  oldData;
    hpatch_TStreamInput* poldData=&oldData;
#endif
#ifdef _IS_USE_PATCH_CACHE
    TByte*            temp_cache=0;
#endif
#ifdef _IS_USE_PATCH_REPEAT_OUT
    TFileStreamInput  readNewData;
#endif
    if (argc!=4) {
        printf("HPatchZ command parameter:\n oldFileName diffFileName outNewFileName\n");
        return 1;
    }
#ifdef _IS_USE_OLD_FILE_STREAM
    TFileStreamInput_init(&oldData);
#endif
    TFileStreamInput_init(&diffData);
    TFileStreamOutput_init(&newData);
#ifdef _IS_USE_PATCH_REPEAT_OUT
    TFileStreamInput_init(&readNewData);
#endif
    {//open file
        hpatch_compressedDiffInfo diffInfo;
        const char* oldFileName=argv[1];
        const char* diffFileName=argv[2];
        const char* outNewFileName=argv[3];
        printf("old :\"%s\"\ndiff:\"%s\"\nout :\"%s\"\n",oldFileName,diffFileName,outNewFileName);
#ifdef _IS_USE_OLD_FILE_STREAM
        if (!TFileStreamInput_open(&oldData,oldFileName))
            _error_return("open oldFile for read error!");
#else
        if (!readFileAll(&poldData_mem,&oldDataSize,oldFileName))
            _error_return("open read oldFile error!");
        mem_as_hStreamInput(&oldData,poldData_mem,poldData_mem+oldDataSize);
#endif
        if (!TFileStreamInput_open(&diffData,diffFileName))
            _error_return("open diffFile for read error!");
        if (!getCompressedDiffInfo(&diffInfo,&diffData.base)){
            _check_error(diffData.fileError,"diffFile read error!");
            _error_return("getCompressedDiffInfo() run error! in HDiffZ file?");
        }
        if (poldData->streamSize!=diffInfo.oldDataSize){
            printf("\nerror! oldFile dataSize %" PRId64 " != saved oldDataSize %" PRId64 "\n",
                   poldData->streamSize,diffInfo.oldDataSize);
            _error_return("");
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
                _error_return("");
            }else{
                if (strlen(diffInfo.compressType)>0)
                    printf("  diffFile added useless compress tag \"%s\"\n",diffInfo.compressType);
                decompressPlugin=0;
            }
        }else{
            printf("  HPatchZ used decompress tag \"%s\" (need decompress %d)\n",
                   diffInfo.compressType,diffInfo.compressedCount);
        }
        
        if (!TFileStreamOutput_open(&newData, outNewFileName,diffInfo.newDataSize))
            _error_return("open out newFile for write error!");
#ifdef _IS_USE_PATCH_REPEAT_OUT
        TFileStreamOutput_setRepeatOut(&newData,hpatch_TRUE);
        if (!TFileStreamInput_open(&readNewData,outNewFileName))
            _error_return("open newFile for read error!");
#endif
    }
    printf("oldDataSize : %" PRId64 "\ndiffDataSize: %" PRId64 "\nnewDataSize : %" PRId64 "\n",
           poldData->streamSize,diffData.base.streamSize,newData.base.streamSize);
    
    time1=clock_s();
#ifdef _IS_USE_PATCH_REPEAT_OUT
    if (!patch_decompress_repeat_out(&newData.base,&readNewData.base,
                                     poldData,&diffData.base,decompressPlugin)){
        const char* kRunErrInfo="\npatch_decompress_repeat_out() run error!\n";
#elif defined(_IS_USE_PATCH_CACHE)
    temp_cache=(TByte*)malloc(k_patch_cache_size);
    if (!temp_cache) _error_return("alloc cache memory error!");
    if (!patch_decompress_with_cache(&newData.base,poldData,&diffData.base,decompressPlugin,
                                     temp_cache,temp_cache+k_patch_cache_size)){
        const char* kRunErrInfo="patch_decompress_with_cache() run error!";
#else
    if (!patch_decompress(&newData.base,poldData,&diffData.base,decompressPlugin)){
        const char* kRunErrInfo="patch_decompress() run error!";
#endif
#ifdef _IS_USE_OLD_FILE_STREAM
        _check_error(oldData.fileError,"oldFile read error!");
#endif
        _check_error(diffData.fileError,"diffFile read error!");
        _check_error(newData.fileError,"out newFile write error!");
#ifdef _IS_USE_PATCH_REPEAT_OUT
        _check_error(readNewData.fileError,"newFile read error!");
#endif
        _error_return(kRunErrInfo);
    }
    if (newData.out_length!=newData.base.streamSize){
        printf("\nerror! out newFile dataSize %" PRId64 " != saved newDataSize %" PRId64 "\n",
               newData.out_length,newData.base.streamSize);
        _error_return("");
    }
    time2=clock_s();
    printf("  patch ok!\n");
    printf("\nHPatchZ time: %.3f s\n",(time2-time1));
    
clear:
    _check_error(!TFileStreamOutput_close(&newData),"out newFile close error!");
    _check_error(!TFileStreamInput_close(&diffData),"diffFile close error!");
#ifdef _IS_USE_OLD_FILE_STREAM
    _check_error(!TFileStreamInput_close(&oldData),"oldFile close error!");
#else
    _free_mem(poldData_mem);
#endif
#ifdef _IS_USE_PATCH_CACHE
    _free_mem(temp_cache);
#endif
#ifdef _IS_USE_PATCH_REPEAT_OUT
    _check_error(!TFileStreamInput_close(&readNewData),"read newFile close error!");
#endif
    time3=clock_s();
    printf("all run time: %.3f s\n",(time3-time0));
    return exitCode;
}

