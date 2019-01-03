//hpatchz.c
// patch tool
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

#include <string.h>
#include <stdlib.h>
#include "libHDiffPatch/HPatch/patch.h"
#include "file_for_patch.h"
#include "_clock_for_demo.h"
#include "_atosize.h"
#include "dirDiffPatch/file_for_dirPatch.h"
#include "dirDiffPatch/dir_patch/dir_patch.h"

#ifndef _IS_NEED_MAIN
#   define  _IS_NEED_MAIN 1
#endif
#ifndef _IS_NEED_ORIGINAL
#   define  _IS_NEED_ORIGINAL 1
#endif

#ifndef _IS_NEED_DEFAULT_CompressPlugin
#   define _IS_NEED_DEFAULT_CompressPlugin 1
#endif
#if (_IS_NEED_ALL_CompressPlugin)
#   undef  _IS_NEED_DEFAULT_CompressPlugin
#   define _IS_NEED_DEFAULT_CompressPlugin 1
#endif
#if (_IS_NEED_DEFAULT_CompressPlugin)
//===== select needs decompress plugins or change to your plugin=====
#   define _CompressPlugin_zlib
#   define _CompressPlugin_bz2
#   define _CompressPlugin_lzma
#endif
#if (_IS_NEED_ALL_CompressPlugin)
//===== select needs decompress plugins or change to your plugin=====
#   define _CompressPlugin_lz4 // || _CompressPlugin_lz4hc
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


static void printUsage(){
    printf("usage: hpatchz [-m|-s[-s-cacheSize]] "
#if (_IS_NEED_ORIGINAL)
           "[-o] "
#endif
           "oldPath diffFile outNewPath\n"
           "memory options:\n"
           "  -m  oldPath all loaded into Memory; fast;\n"
           "      requires (oldFileSize + 4 * decompress stream size) + O(1) bytes of memory\n"
           "  -s-cacheSize \n"
           "      oldPath loaded as Stream, with cacheSize; DEFAULT;\n"
           "      requires (cacheSize + 4 * decompress stream size) + O(1) bytes of memory;\n"
           "      cacheSize can like 262144 or 256k or 512m or 2g etc..., DEFAULT 128m\n"
#if (_IS_NEED_ORIGINAL)
           "special options:\n"
           "  -v  output Version info. \n"
           "  -o  Original patch; DEPRECATED; compatible with \"patch_demo.c\",\n"
           "      diffFile must created by \"diff_demo.cpp\" or \"hdiffz -o ...\"\n"
#endif
           );
}

typedef enum THPatchResult {
    HPATCH_SUCCESS=0,
    HPATCH_OPTIONS_ERROR,
    HPATCH_OPENREAD_ERROR,
    HPATCH_OPENWRITE_ERROR,
    HPATCH_FILEREAD_ERROR,
    HPATCH_FILEWRITE_ERROR,
    HPATCH_FILEDATA_ERROR,
    HPATCH_FILECLOSE_ERROR,
    HPATCH_MEM_ERROR,
    HPATCH_HDIFFINFO_ERROR,
    HPATCH_COMPRESSTYPE_ERROR,
    HPATCH_PATCH_ERROR,
    
    HPATCH_PATHTYPE_ERROR,
    HPATCH_DIRDIFFINFO_ERROR,
    HPATCH_DIRPATCH_ERROR,
    HPATCH_DIRPATCH_LAODDATA_ERROR,
} THPatchResult;

int hpatch_cmd_line(int argc, const char * argv[]);

int hpatch(const char* oldFileName,const char* diffFileName,const char* outNewFileName,
           hpatch_BOOL isOriginal,hpatch_BOOL isLoadOldAll,size_t patchCacheSize);

int hpatch_dir(const char* oldPath,const char* diffFileName,const char* outNewPath,
               hpatch_BOOL isLoadOldAll,size_t patchCacheSize);

#if (_IS_NEED_MAIN)
int main(int argc, const char * argv[]){
    SetDefaultLocale();
    return hpatch_cmd_line(argc,argv);
}
#endif


#define _options_check(value,errorInfo){ \
    if (!(value)) { printf("options " errorInfo " ERROR!\n"); printUsage(); return HPATCH_OPTIONS_ERROR; } }

#define kPatchCacheSize_min      (hpatch_kStreamCacheSize*8)
#define kPatchCacheSize_bestmin  ((size_t)1<<21)
#define kPatchCacheSize_default  ((size_t)1<<27)
#define kPatchCacheSize_bestmax  ((size_t)1<<30)

#define _kNULL_VALUE    (-1)

int hpatch_cmd_line(int argc, const char * argv[]){
    hpatch_BOOL isOriginal=_kNULL_VALUE;
    hpatch_BOOL isLoadOldAll=_kNULL_VALUE;
    hpatch_BOOL isOutputVersion=_kNULL_VALUE;
    size_t      patchCacheSize=0;
    #define kMax_arg_values_size 3
    const char * arg_values[kMax_arg_values_size]={0};
    int          arg_values_size=0;
    int         i;
    for (i=1; i<argc; ++i) {
        const char* op=argv[i];
        _options_check((op!=0)&&(strlen(op)>0),"?");
        if (op[0]!='-'){
            _options_check(arg_values_size<kMax_arg_values_size,"count");
            arg_values[arg_values_size]=op; //path: file or dir
            ++arg_values_size;
            continue;
        }
        _options_check((op!=0)&&(op[0]=='-'),"?");
        switch (op[1]) {
            case 'v':{
                _options_check((isOutputVersion==_kNULL_VALUE)&&(op[2]=='\0'),"-v");
                isOutputVersion=hpatch_TRUE;
            } break;
#if (_IS_NEED_ORIGINAL)
            case 'o':{
                _options_check((isOriginal==_kNULL_VALUE)&&(op[2]=='\0'),"-o");
                isOriginal=hpatch_TRUE;
            } break;
#endif
            case 'm':{
                _options_check((isLoadOldAll==_kNULL_VALUE)&&(op[2]=='\0'),"-m");
                isLoadOldAll=hpatch_TRUE;
            } break;
            case 's':{
                _options_check((isLoadOldAll==_kNULL_VALUE)&&((op[2]=='-')||(op[2]=='\0')),"-s");
                isLoadOldAll=hpatch_FALSE; //stream
                if (op[2]=='-'){
                    const char* pnum=op+3;
                    _options_check(kmg_to_size(pnum,strlen(pnum),&patchCacheSize),"-s-?");
                    if (patchCacheSize<kPatchCacheSize_min)
                        patchCacheSize=kPatchCacheSize_min;
                }else{
                    patchCacheSize=kPatchCacheSize_default;
                }
            } break;
            default: {
                _options_check(hpatch_FALSE,"-?");
            } break;
        }//swich
    }
    
    if (isOutputVersion==_kNULL_VALUE)
        isOutputVersion=hpatch_FALSE;
    if (isOutputVersion){
        printf("HDiffPatch::hpatchz v" HDIFFPATCH_VERSION_STRING "\n\n");
        if (arg_values_size==0)
            return 0; //ok
    }
    
    _options_check(arg_values_size==kMax_arg_values_size,"count");
    if (isOriginal==_kNULL_VALUE)
        isOriginal=hpatch_FALSE;
    if (isLoadOldAll==_kNULL_VALUE){
        isLoadOldAll=hpatch_FALSE;
        patchCacheSize=kPatchCacheSize_default;
    }
    
    {
        const char* oldPath     =arg_values[0];
        const char* diffFileName=arg_values[1];
        const char* outNewPath  =arg_values[2];
        hpatch_BOOL isDirDiff;
        if (isSamePath(oldPath,outNewPath))
            _options_check(hpatch_FALSE,"now unsupport oldPath outNewPath same path");
        _options_check(getIsDirDiffFile(diffFileName,&isDirDiff),"input diffFile open read");
        if (isDirDiff){
            _options_check(!isOriginal,"-o unsupport dir patch");
            return hpatch_dir(oldPath,diffFileName,outNewPath,isLoadOldAll,patchCacheSize);
        }else{
            return hpatch(oldPath,diffFileName,outNewPath,isOriginal,isLoadOldAll,patchCacheSize);
        }
    }
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

#define  check_on_error(errorType) { \
    if (result==HPATCH_SUCCESS) result=errorType; if (!_isInClear){ goto clear; } }
#define  check(value,errorType,errorInfo) { \
    if (!(value)){ printf(errorInfo " ERROR!\n"); check_on_error(errorType); } }


static int getDecompressPlugin(const hpatch_compressedDiffInfo* diffInfo,
                               hpatch_TDecompress** out_decompressPlugin){
    int     result=HPATCH_SUCCESS;
    hpatch_TDecompress*  decompressPlugin=0;
    if (strlen(diffInfo->compressType)>0){
#ifdef  _CompressPlugin_zlib
        if ((!decompressPlugin)&&zlibDecompressPlugin.is_can_open(&zlibDecompressPlugin,diffInfo))
            decompressPlugin=&zlibDecompressPlugin;
#endif
#ifdef  _CompressPlugin_bz2
        if ((!decompressPlugin)&&bz2DecompressPlugin.is_can_open(&bz2DecompressPlugin,diffInfo))
            decompressPlugin=&bz2DecompressPlugin;
#endif
#ifdef  _CompressPlugin_lzma
        if ((!decompressPlugin)&&lzmaDecompressPlugin.is_can_open(&lzmaDecompressPlugin,diffInfo))
            decompressPlugin=&lzmaDecompressPlugin;
#endif
#if (defined(_CompressPlugin_lz4) || (defined(_CompressPlugin_lz4hc)))
        if ((!decompressPlugin)&&lz4DecompressPlugin.is_can_open(&lz4DecompressPlugin,diffInfo))
            decompressPlugin=&lz4DecompressPlugin;
#endif
#ifdef  _CompressPlugin_zstd
        if ((!decompressPlugin)&&zstdDecompressPlugin.is_can_open(&zstdDecompressPlugin,diffInfo))
            decompressPlugin=&zstdDecompressPlugin;
#endif
    }
    if (!decompressPlugin){
        if (diffInfo->compressedCount>0){
            printf("can no decompress \"%s\" data",diffInfo->compressType);
            result=HPATCH_COMPRESSTYPE_ERROR; //error
        }else{
            if (strlen(diffInfo->compressType)>0)
                printf("  diffFile added useless compress tag \"%s\"\n",diffInfo->compressType);
            decompressPlugin=0;
        }
    }else{
        printf("hpatchz run with decompress plugin: \"%s\" (need decompress %d)\n",
               diffInfo->compressType,diffInfo->compressedCount);
    }
    *out_decompressPlugin=decompressPlugin;
    return result;
}

static void* getPatchMemCache(hpatch_BOOL isLoadOldAll,size_t patchCacheSize,
                              hpatch_StreamPos_t oldDataSize,size_t* out_memCacheSize){
    void*  temp_cache=0;
    size_t temp_cache_size;
    if (isLoadOldAll){
        assert(patchCacheSize==0);
        temp_cache_size=(size_t)(oldDataSize+kPatchCacheSize_bestmin);
        if (temp_cache_size!=oldDataSize+kPatchCacheSize_bestmin)
            temp_cache_size=kPatchCacheSize_bestmax;//can not load all,load part
    }else{
        temp_cache_size=patchCacheSize;
        if (temp_cache_size>oldDataSize+kPatchCacheSize_bestmin)
            temp_cache_size=(size_t)(oldDataSize+kPatchCacheSize_bestmin);
    }
    while (!temp_cache) {
        temp_cache=(TByte*)malloc(temp_cache_size);
        if ((!temp_cache)&&(temp_cache_size>=kPatchCacheSize_min*2)) temp_cache_size>>=1;
    }
    *out_memCacheSize=(temp_cache)?temp_cache_size:0;
    return temp_cache;
}


int hpatch(const char* oldFileName,const char* diffFileName,const char* outNewFileName,
           hpatch_BOOL isOriginal,hpatch_BOOL isLoadOldAll,size_t patchCacheSize){
    int     result=HPATCH_SUCCESS;
    int     _isInClear=hpatch_FALSE;
    double  time0=clock_s();
    hpatch_TDecompress*  decompressPlugin=0;
    TFileStreamOutput    newData;
    TFileStreamInput     diffData;
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
        printf("old : \"%s\"\ndiff: \"%s\"\nout : \"%s\"\n",oldFileName,diffFileName,outNewFileName);
        check(TFileStreamInput_open(&oldData,oldFileName),HPATCH_OPENREAD_ERROR,"open oldFile for read");
        check(TFileStreamInput_open(&diffData,diffFileName),HPATCH_OPENREAD_ERROR,"open diffFile for read");
    }

#if (_IS_NEED_ORIGINAL)
    if (isOriginal){
        int kNewDataSizeSavedSize=9;
        TByte buf[9];
        if (kNewDataSizeSavedSize>diffData.base.streamSize)
            kNewDataSizeSavedSize=(int)diffData.base.streamSize;
        check(diffData.base.read(&diffData.base,0,buf,buf+kNewDataSizeSavedSize),
              HPATCH_FILEREAD_ERROR,"read diffFile");
        kNewDataSizeSavedSize=readSavedSize(buf,kNewDataSizeSavedSize,&savedNewSize);
        check(kNewDataSizeSavedSize>0,HPATCH_FILEDATA_ERROR,"read diffFile savedNewSize");
        TFileStreamInput_setOffset(&diffData,kNewDataSizeSavedSize);
    }else
#endif
    {
        assert(!isOriginal);
        hpatch_compressedDiffInfo diffInfo;
        if (!getCompressedDiffInfo(&diffInfo,&diffData.base)){
            check(!diffData.fileError,HPATCH_FILEREAD_ERROR,"read diffFile");
            check(hpatch_FALSE,HPATCH_HDIFFINFO_ERROR,"is hdiff file? getCompressedDiffInfo()");
        }
        if (poldData->streamSize!=diffInfo.oldDataSize){
            printf("oldFile dataSize %" PRId64 " != diffFile saved oldDataSize %" PRId64 "",
                   poldData->streamSize,diffInfo.oldDataSize);
            check_on_error(HPATCH_FILEDATA_ERROR);
        }
        result=getDecompressPlugin(&diffInfo,&decompressPlugin);
        if (result!=HPATCH_SUCCESS) check_on_error(result);
        savedNewSize=diffInfo.newDataSize;
    }
    check(TFileStreamOutput_open(&newData, outNewFileName,savedNewSize),
          HPATCH_OPENWRITE_ERROR,"open out newFile for write");
    printf("oldDataSize : %" PRId64 "\ndiffDataSize: %" PRId64 "\nnewDataSize : %" PRId64 "\n",
           poldData->streamSize,diffData.base.streamSize,newData.base.streamSize);
    
    temp_cache=getPatchMemCache(isLoadOldAll,patchCacheSize,poldData->streamSize, &temp_cache_size);
    check(temp_cache,HPATCH_MEM_ERROR,"alloc cache memory");

#if (_IS_NEED_ORIGINAL)
    if (isOriginal)
        patch_result=patch_stream_with_cache(&newData.base,poldData,&diffData.base,
                                             temp_cache,temp_cache+temp_cache_size);
    else
#endif
        patch_result=patch_decompress_with_cache(&newData.base,poldData,&diffData.base,decompressPlugin,
                                                 temp_cache,temp_cache+temp_cache_size);
    if (!patch_result){
        check(!oldData.fileError,HPATCH_FILEREAD_ERROR,"oldFile read");
        check(!diffData.fileError,HPATCH_FILEREAD_ERROR,"diffFile read");
        check(!newData.fileError,HPATCH_FILEWRITE_ERROR,"out newFile write");
        check(hpatch_FALSE,HPATCH_PATCH_ERROR,"patch run");
    }
    if (newData.out_length!=newData.base.streamSize){
        printf("out newFile dataSize %" PRId64 " != diffFile saved newDataSize %" PRId64 "",
               newData.out_length,newData.base.streamSize);
        check_on_error(HPATCH_FILEDATA_ERROR);
    }
    printf("  patch ok!\n");
    
clear:
    _isInClear=hpatch_TRUE;
    check(TFileStreamOutput_close(&newData),HPATCH_FILECLOSE_ERROR,"out newFile close");
    check(TFileStreamInput_close(&diffData),HPATCH_FILECLOSE_ERROR,"diffFile close");
    check(TFileStreamInput_close(&oldData),HPATCH_FILECLOSE_ERROR,"oldFile close");
    _free_mem(temp_cache);
    printf("\nhpatchz time: %.3f s\n",(clock_s()-time0));
    return result;
}

int hpatch_dir(const char* oldPath,const char* diffFileName,const char* outNewPath,
               hpatch_BOOL isLoadOldAll,size_t patchCacheSize){
    int     result=HPATCH_SUCCESS;
    int     _isInClear=hpatch_FALSE;
    double  time0=clock_s();
    TFileStreamOutput    newFile;
    TFileStreamInput     diffData;
    TFileStreamInput     oldFile;
    hpatch_TStreamInput  oldMemStream;
    const hpatch_TStreamInput*  oldStream=0;
    const hpatch_TStreamOutput* newStream=0;
    TByte*               p_temp_mem=0;
    TByte*               temp_cache=0;
    size_t               temp_cache_size;
    TDirPatcher          dirPatcher;
    TDirDiffInfo*        dirDiffInfo=0;
    TFileStreamOutput_init(&newFile);
    TFileStreamInput_init(&diffData);
    TFileStreamInput_init(&oldFile);
    memset(&oldMemStream,0,sizeof(oldMemStream));
    TDirPatcher_init(&dirPatcher);
    assert(0!=strcmp(oldPath,outNewPath));
    {//dir diff info
        hpatch_BOOL  rt;
        TPathType    oldType;
        check(getPathType(oldPath,&oldType),HPATCH_PATHTYPE_ERROR,"input old path must file or dir");
        check(TFileStreamInput_open(&diffData,diffFileName),HPATCH_OPENREAD_ERROR,"open diffFile for read");
        rt=TDirPatcher_open(&dirPatcher,&diffData.base);
        dirDiffInfo=&dirPatcher.dirDiffInfo;
        if((!rt)||(!dirDiffInfo->isDirDiff)){
            check(!diffData.fileError,HPATCH_FILEREAD_ERROR,"read diffFile");
            check(hpatch_FALSE,HPATCH_DIRDIFFINFO_ERROR,"is hdiff file? getDirDiffInfo()");
        }
        if (dirDiffInfo->oldPathIsDir){
            check(kPathType_dir==oldType,HPATCH_PATHTYPE_ERROR,"input old path need dir");
        }else{
            check(kPathType_dir!=oldType,HPATCH_PATHTYPE_ERROR,"input old path need file");
        }
        printf("%s: \"%s\"\ndiffFile: \"%s\"\n%s: \"%s\"\n",
               dirDiffInfo->oldPathIsDir?"old  dir":"old file", oldPath, diffFileName,
               dirDiffInfo->newPathIsDir?"out  dir":"out file", outNewPath);
    }
    {   //decompressPlugin
        hpatch_TDecompress*  decompressPlugin=0;
        hpatch_compressedDiffInfo hdiffInfo;
        hdiffInfo=dirDiffInfo->hdiffInfo;
        hdiffInfo.compressedCount+=(dirDiffInfo->dirDataIsCompressed)?1:0;
        result=getDecompressPlugin(&hdiffInfo,&decompressPlugin);
        if (result!=HPATCH_SUCCESS) check_on_error(result);
        //load dir data
        check(TDirPatcher_loadDirData(&dirPatcher,decompressPlugin),
              HPATCH_DIRPATCH_LAODDATA_ERROR,"load dir data in diffFile");
    }
    //cache
    p_temp_mem=getPatchMemCache(isLoadOldAll,patchCacheSize,dirDiffInfo->hdiffInfo.oldDataSize, &temp_cache_size);
    check(p_temp_mem,HPATCH_MEM_ERROR,"alloc cache memory");
    temp_cache=p_temp_mem;
    //old data
    if (!dirDiffInfo->oldPathIsDir){
        check(TFileStreamInput_open(&oldFile,oldPath),HPATCH_OPENREAD_ERROR,"open oldFile for read");
        if (oldFile.base.streamSize!=dirDiffInfo->hdiffInfo.oldDataSize){
            printf("oldFile dataSize %" PRId64 " != diffFile saved oldDataSize %" PRId64 "",
                   oldFile.base.streamSize,dirDiffInfo->hdiffInfo.oldDataSize);
            check_on_error(HPATCH_FILEDATA_ERROR);
        }
        oldStream=&oldFile.base;
    }else{
        hpatch_StreamPos_t oldDataSize=dirDiffInfo->hdiffInfo.oldDataSize;
        if (temp_cache_size>=oldDataSize+kPatchCacheSize_min){
            check(TDirPatcher_loadOldRefToMem(&dirPatcher,oldPath,temp_cache,temp_cache+oldDataSize),
                  HPATCH_OPENREAD_ERROR,"open oldFiles read");
            mem_as_hStreamInput(&oldMemStream,temp_cache,temp_cache+oldDataSize);
            temp_cache+=oldDataSize;
            oldStream=&oldMemStream;
        }else{
            oldStream=TDirPatcher_loadOldRefAsStream(&dirPatcher,oldPath);
        }
    }
    //new data
    if (!dirDiffInfo->newPathIsDir){
        check(TFileStreamOutput_open(&newFile,outNewPath,dirDiffInfo->hdiffInfo.newDataSize),
              HPATCH_OPENWRITE_ERROR,"open out newFile for write");
        newStream=&newFile.base;
    }else{
        //todo:
    }
    
    check(TDirPatcher_patch(&dirPatcher,newStream,oldStream, temp_cache,
                            temp_cache+temp_cache_size),HPATCH_DIRPATCH_ERROR,"dir patch run");
    
clear:
    _isInClear=hpatch_TRUE;
    check(TFileStreamOutput_close(&newFile),HPATCH_FILECLOSE_ERROR,"out newFile close");
    check(TDirPatcher_closeOldRefStream(&dirPatcher),HPATCH_FILECLOSE_ERROR,"oldFiles close");
    TDirPatcher_close(&dirPatcher);
    check(TFileStreamInput_close(&diffData),HPATCH_FILECLOSE_ERROR,"diffFile close");
    check(TFileStreamInput_close(&oldFile),HPATCH_FILECLOSE_ERROR,"oldFile close");
    _free_mem(p_temp_mem);
    printf("\nhpatchz time: %.3f s\n",(clock_s()-time0));
    return result;
}
