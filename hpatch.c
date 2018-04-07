//hpatch.c
// patch tool
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
#include "file_for_patch.h"
#include "_clock_for_demo.h"

//===== select needs decompress plugins or change to your plugin=====
#define _CompressPlugin_zlib
#define _CompressPlugin_bz2
#define _CompressPlugin_lzma
#define _CompressPlugin_lz4 // & lz4hc
#define _CompressPlugin_zstd

#include "decompress_plugin_demo.h"

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


int hpatch(const char* oldFileName,const char* diffFileName,const char* outNewFileName,
           hpatch_BOOL isOriginal,hpatch_BOOL isLoadOldAll,size_t patchCacheSize);

#if !(_IS_NOT_NEED_MAIN)
int main(int argc, const char * argv[]){
    if (argc<4) {
        printf("usage: hpatch oldFileName diffFileName outNewFileName\n");
        return 1;
    }
    const char* oldFileName=argv[1];
    const char* diffFileName=argv[2];
    const char* outNewFileName=argv[3];
    return hpatch(oldFileName,diffFileName,outNewFileName,hpatch_FALSE,hpatch_FALSE,(1<<26));
}
#endif // !_IS_NOT_NEED_MAIN


int hpatch(const char* oldFileName,const char* diffFileName,const char* outNewFileName,
           hpatch_BOOL isOriginal,hpatch_BOOL isLoadOldAll,size_t patchCacheSize){
    int     exitCode=0;
    double  time0=clock_s();
    double  time1,time2,time3;
    hpatch_TDecompress* decompressPlugin=0;
    TFileStreamOutput   newData;
    TFileStreamInput    diffData;
    TFileStreamInput     oldData;
    hpatch_TStreamInput* poldData=&oldData.base;
    TByte*               temp_cache=0;
    size_t               temp_cache_size;
    TFileStreamInput_init(&oldData);
    TFileStreamInput_init(&diffData);
    TFileStreamOutput_init(&newData);
    {//open file
        hpatch_compressedDiffInfo diffInfo;
        printf("hpatch:\n");
        printf("  old: \"%s\"\n diff: \"%s\"\n  out: \"%s\"\n",oldFileName,diffFileName,outNewFileName);
        if (!TFileStreamInput_open(&oldData,oldFileName))
            _error_return("open oldFile for read ERROR!");
        if (!TFileStreamInput_open(&diffData,diffFileName))
            _error_return("open diffFile for read ERROR!");
        if (!getCompressedDiffInfo(&diffInfo,&diffData.base)){
            _check_error(diffData.fileError,"diffFile read ERROR!");
            _error_return("getCompressedDiffInfo() run ERROR! is hdiff file?");
        }
        if (poldData->streamSize!=diffInfo.oldDataSize){
            printf("\n  ERROR! oldFile dataSize %" PRId64 " != saved oldDataSize %" PRId64 "\n",
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
#ifdef  _CompressPlugin_lz4
            if ((!decompressPlugin)&&lz4DecompressPlugin.is_can_open(&lz4DecompressPlugin,&diffInfo))
                decompressPlugin=&lz4DecompressPlugin;
#endif
#ifdef  _CompressPlugin_zstd
            if ((!decompressPlugin)&&zstdDecompressPlugin.is_can_open(&zstdDecompressPlugin,&diffInfo))
                decompressPlugin=&zstdDecompressPlugin;
#endif
        }
        if (!decompressPlugin){
            if (diffInfo.compressedCount>0){
                printf("\n  ERROR! can no decompress \"%s\" data\n",diffInfo.compressType);
                _error_return("");
            }else{
                if (strlen(diffInfo.compressType)>0)
                    printf("  diffFile added useless compress tag \"%s\"\n",diffInfo.compressType);
                decompressPlugin=0;
            }
        }else{
            printf("  hpatch used decompress tag \"%s\" (need decompress %d)\n",
                   diffInfo.compressType,diffInfo.compressedCount);
        }
        
        if (!TFileStreamOutput_open(&newData, outNewFileName,diffInfo.newDataSize))
            _error_return("open out newFile for write ERROR!");
    }
    printf("oldDataSize : %" PRId64 "\ndiffDataSize: %" PRId64 "\nnewDataSize : %" PRId64 "\n",
           poldData->streamSize,diffData.base.streamSize,newData.base.streamSize);
    
    time1=clock_s();
    if (isLoadOldAll){
        assert(patchCacheSize==0);
        temp_cache_size=(size_t)(oldData.base.streamSize+(1<<21));
        if (temp_cache_size!=oldData.base.streamSize+(1<<21))
            temp_cache_size=(1<<30);//fail load all,load part
    }else{
        temp_cache_size=patchCacheSize;
        if (temp_cache_size>oldData.base.streamSize+(1<<21))
            temp_cache_size=(size_t)(oldData.base.streamSize+(1<<21));
    }
    temp_cache=(TByte*)malloc(temp_cache_size);
    if (!temp_cache) _error_return("alloc cache memory ERROR!");
    if (!patch_decompress_with_cache(&newData.base,poldData,&diffData.base,decompressPlugin,
                                     temp_cache,temp_cache+temp_cache_size)){
        const char* kRunErrInfo="patch_decompress_with_cache() run ERROR!";
        _check_error(oldData.fileError,"oldFile read ERROR!");
        _check_error(diffData.fileError,"diffFile read ERROR!");
        _check_error(newData.fileError,"out newFile write ERROR!");
        _error_return(kRunErrInfo);
    }
    if (newData.out_length!=newData.base.streamSize){
        printf("\n  ERROR! out newFile dataSize %" PRId64 " != saved newDataSize %" PRId64 "\n",
               newData.out_length,newData.base.streamSize);
        _error_return("");
    }
    time2=clock_s();
    printf("  patch ok!\n");
    printf("\nhpatch time: %.3f s\n",(time2-time1));
    
clear:
    _check_error(!TFileStreamOutput_close(&newData),"out newFile close ERROR!");
    _check_error(!TFileStreamInput_close(&diffData),"diffFile close ERROR!");
    _check_error(!TFileStreamInput_close(&oldData),"oldFile close ERROR!");
    _free_mem(temp_cache);
    time3=clock_s();
    printf("all run time: %.3f s\n",(time3-time0));
    return exitCode;
}

