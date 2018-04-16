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
#include "_atosize.h"

#ifndef _IS_NEED_MAIN
#   define  _IS_NEED_MAIN 1
#endif
#ifndef _IS_NEED_ORIGINAL
#   define  _IS_NEED_ORIGINAL 1
#endif
#ifndef _IS_NEED_ALL_CompressPlugin
#   define _IS_NEED_ALL_CompressPlugin 1
#endif
#if (_IS_NEED_ALL_CompressPlugin)
//===== select needs decompress plugins or change to your plugin=====
#   define _CompressPlugin_zlib
#   define _CompressPlugin_bz2
#   define _CompressPlugin_lzma
#   define _CompressPlugin_lz4 // & lz4hc
#   define _CompressPlugin_zstd
#endif

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

#if (_IS_NEED_ORIGINAL)
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
#endif

#define kPatchCacheSize_min      (hpatch_kStreamCacheSize*8)
#define kPatchCacheSize_bestmin  ((size_t)1<<21)
#define kPatchCacheSize_default  ((size_t)1<<27)
#define kPatchCacheSize_bestmax  ((size_t)1<<30)

int hpatch(const char* oldFileName,const char* diffFileName,const char* outNewFileName,
           hpatch_BOOL isOriginal,hpatch_BOOL isLoadOldAll,size_t patchCacheSize);

static void printUsage(){
    printf("usage: hpatch [-m|-s[-nbytes]] "
#if (_IS_NEED_ORIGINAL)
           "[-o] "
#endif
           "oldFile diffFile outNewFile\n"
           "memory options:\n"
           "  -m  oldFile all load into Memory;\n"
           "      fast, memory requires O(oldFileSize + 4 * decompress stream)\n"
           "  -s-nbytes \n"
           "      oldFile load as Stream, DEFAULT, with nbytes(cache memory size);\n"
           "      memory requires O(nbytes + 4 * decompress stream),\n"
           "      nbytes can like 524288 or 512k or 128m(DEFAULT) or 1g etc...\n"
#if (_IS_NEED_ORIGINAL)
           "special options:\n"
           "  -o  Original patch, DEPRECATED; compatible with \"patch_demo.c\",\n"
           "      diffFile must created by \"diff_demo.cpp\" or \"hdiff -o ...\"\n"
#endif
           );
}

#define _options_check(value){ if (!(value)) { printUsage(); return 1; } }

int hpatch_cmd_line(int argc, const char * argv[]){
    hpatch_BOOL isOriginal=hpatch_FALSE;
    hpatch_BOOL isLoadOldAll=hpatch_FALSE;
    size_t      patchCacheSize=0;
    _options_check(argc>=4);
    for (int i=1; i<argc-3; ++i) {
        const char* op=argv[i];
        _options_check((op!=0)&&(op[0]=='-'));
        switch (op[1]) {
#if (_IS_NEED_ORIGINAL)
            case 'o':{
                _options_check((!isOriginal)&&(op[2]=='\0'));
                isOriginal=hpatch_TRUE;
            } break;
#endif
            case 'm':{
                _options_check((!isLoadOldAll)&&(patchCacheSize==0)&&(op[2]=='\0'));
                isLoadOldAll=hpatch_TRUE;
                } break;
            case 's':{
                _options_check((!isLoadOldAll)&&(patchCacheSize==0));
                isLoadOldAll=hpatch_FALSE;
                if (op[2]=='-'){
                    const char* pnum=op+3;
                    _options_check(kmg_to_size(pnum,strlen(pnum),&patchCacheSize));
                    if (patchCacheSize<kPatchCacheSize_min)
                        patchCacheSize=kPatchCacheSize_min;
                }else{
                    _options_check(op[2]=='\0');
                }
            } break;
            default: {
                _options_check(hpatch_FALSE);
            } break;
        }//swich
    }
    if ((!isLoadOldAll)&&(patchCacheSize==0)){
        patchCacheSize=kPatchCacheSize_default;
    }
    
    {
        const char* oldFileName=argv[argc-3];
        const char* diffFileName=argv[argc-2];
        const char* outNewFileName=argv[argc-1];
        return hpatch(oldFileName,diffFileName,outNewFileName,isOriginal,isLoadOldAll,patchCacheSize);
    }
}

#if (_IS_NEED_MAIN)
int main(int argc, const char * argv[]){
    return hpatch_cmd_line(argc,argv);
}
#endif


int hpatch(const char* oldFileName,const char* diffFileName,const char* outNewFileName,
           hpatch_BOOL isOriginal,hpatch_BOOL isLoadOldAll,size_t patchCacheSize){
    int     exitCode=0;
    double  time0=clock_s();
    double  time1;
    hpatch_TDecompress* decompressPlugin=0;
    TFileStreamOutput   newData;
    TFileStreamInput    diffData;
    TFileStreamInput     oldData;
    hpatch_TStreamInput* poldData=&oldData.base;
    TByte*               temp_cache=0;
    size_t               temp_cache_size;
    hpatch_StreamPos_t   savedNewSize=0;
    hpatch_BOOL          patch_result;
    TFileStreamInput_init(&oldData);
    TFileStreamInput_init(&diffData);
    TFileStreamOutput_init(&newData);
    {//open
        printf("hpatch:\n");
        printf("  old: \"%s\"\n diff: \"%s\"\n  out: \"%s\"\n",oldFileName,diffFileName,outNewFileName);
        if (!TFileStreamInput_open(&oldData,oldFileName))
            _error_return("open oldFile for read ERROR!");
        if (!TFileStreamInput_open(&diffData,diffFileName))
            _error_return("open diffFile for read ERROR!");
    }

#if (_IS_NEED_ORIGINAL)
    if (isOriginal){
        int kNewDataSizeSavedSize=9;
        TByte buf[9];
        if (kNewDataSizeSavedSize>diffData.base.streamSize)
            kNewDataSizeSavedSize=(int)diffData.base.streamSize;
        if (kNewDataSizeSavedSize!=diffData.base.read(diffData.base.streamHandle,0,
                                                      buf,buf+kNewDataSizeSavedSize))
            _error_return("read savedNewSize error!");
        kNewDataSizeSavedSize=readSavedSize(buf,kNewDataSizeSavedSize,&savedNewSize);
        if (kNewDataSizeSavedSize<=0) _error_return("read savedNewSize error!");
        TFileStreamInput_setOffset(&diffData,kNewDataSizeSavedSize);
    }else
#endif
    {
        hpatch_compressedDiffInfo diffInfo;
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
        
        savedNewSize=diffInfo.newDataSize;
    }
    if (!TFileStreamOutput_open(&newData, outNewFileName,savedNewSize))
        _error_return("open out newFile for write ERROR!");
    printf("oldDataSize : %" PRId64 "\ndiffDataSize: %" PRId64 "\nnewDataSize : %" PRId64 "\n",
           poldData->streamSize,diffData.base.streamSize,newData.base.streamSize);
    
    if (isLoadOldAll){
        assert(patchCacheSize==0);
        temp_cache_size=(size_t)(oldData.base.streamSize+kPatchCacheSize_bestmin);
        if (temp_cache_size!=oldData.base.streamSize+kPatchCacheSize_bestmin)
            temp_cache_size=kPatchCacheSize_bestmax;//can not load all,load part
    }else{
        temp_cache_size=patchCacheSize;
        if (temp_cache_size>oldData.base.streamSize+kPatchCacheSize_bestmin)
            temp_cache_size=(size_t)(oldData.base.streamSize+kPatchCacheSize_bestmin);
    }
    while (!temp_cache) {
        temp_cache=(TByte*)malloc(temp_cache_size);
        if ((!temp_cache)&&(temp_cache_size>=kPatchCacheSize_min*2)) temp_cache_size>>=1;
    }
    if (!temp_cache) _error_return("alloc cache memory ERROR!");

#if (_IS_NEED_ORIGINAL)
    if (isOriginal)
        patch_result=patch_stream_with_cache(&newData.base,poldData,&diffData.base,
                                             temp_cache,temp_cache+temp_cache_size);
    else
#endif
        patch_result=patch_decompress_with_cache(&newData.base,poldData,&diffData.base,decompressPlugin,
                                                 temp_cache,temp_cache+temp_cache_size);
    if (!patch_result){
        const char* kRunErrInfo="patch run ERROR!";
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
    printf("  patch ok!\n");
    
clear:
    _check_error(!TFileStreamOutput_close(&newData),"out newFile close ERROR!");
    _check_error(!TFileStreamInput_close(&diffData),"diffFile close ERROR!");
    _check_error(!TFileStreamInput_close(&oldData),"oldFile close ERROR!");
    _free_mem(temp_cache);
    time1=clock_s();
    printf("\nhpatch time: %.3f s\n",(time1-time0));
    return exitCode;
}

