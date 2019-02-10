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
#include <stdio.h>  //fprintf
#include "libHDiffPatch/HPatch/patch.h"
#include "_clock_for_demo.h"
#include "_atosize.h"
#include "file_for_patch.h"

#if (_IS_NEED_DIR_DIFF_PATCH)
#include "dirDiffPatch/dir_patch/dir_patch.h"
#include "hpatch_dir_listener.h"
#endif

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
#   define _CompressPlugin_lzma2
#endif
#if (_IS_NEED_ALL_CompressPlugin)
//===== select needs decompress plugins or change to your plugin=====
#   define _CompressPlugin_lz4 // || _CompressPlugin_lz4hc
#   define _CompressPlugin_zstd
#endif

#include "decompress_plugin_demo.h"

#if (_IS_NEED_DIR_DIFF_PATCH)

#ifndef _IS_NEED_DEFAULT_ChecksumPlugin
#   define _IS_NEED_DEFAULT_ChecksumPlugin 1
#endif
#if (_IS_NEED_ALL_ChecksumPlugin)
#   undef  _IS_NEED_DEFAULT_ChecksumPlugin
#   define _IS_NEED_DEFAULT_ChecksumPlugin 1
#endif
#if (_IS_NEED_DEFAULT_ChecksumPlugin)
//===== select needs checksum plugins or change to your plugin=====
#   define _ChecksumPlugin_crc32    // =  32 bit effective  //need zlib
#   define _ChecksumPlugin_fadler64 // ?  63 bit effective
#endif
#if (_IS_NEED_ALL_ChecksumPlugin)
//===== select needs checksum plugins or change to your plugin=====
#   define _ChecksumPlugin_adler32  // ~  29 bit effective
#   define _ChecksumPlugin_adler64  // ?  36 bit effective
#   define _ChecksumPlugin_fadler32 // ~  32 bit effective
#   define _ChecksumPlugin_fadler128// ? 126 bit effective
#   define _ChecksumPlugin_md5      // ? 128 bit effective
#endif

#include "checksum_plugin_demo.h"
#endif

static void printUsage(){
    printf("usage: hpatchz [options] oldPath diffFile outNewPath\n"
           "  ( if oldPath is empty input parameter \"\" )\n"
           "memory options:\n"
           "  -m  oldPath all loaded into Memory; fast;\n"
           "      requires (oldFileSize+ 4*decompress stream size)+O(1) bytes of memory.\n"
           "  -s[-cacheSize] \n"
           "      DEFAULT; oldPath loaded as Stream;\n"
           "      requires (cacheSize+ 4*decompress stream size)+O(1) bytes of memory;\n"
           "      cacheSize can like 262144 or 256k or 512m or 2g etc..., DEFAULT 64m.\n"
           "special options:\n"
#if (_IS_NEED_DIR_DIFF_PATCH)
           "  -C-checksumSets\n"
           "      set Checksum data for directory patch, DEFAULT -new-copy;\n"
           "      checksumSets support (can choose multiple):\n"
           "        -diff      checksum diffFile;\n"
           "        -old       checksum old reference files;\n"
           "        -new       checksum new reference files edited from old reference files;\n"
           "        -copy      checksum new files copy from old same files;\n"
           "        -no        no checksum\n"
           "        -all       same as: -diff-old-new-copy\n"
           "  -n-maxOpenFileNumber\n"
           "      limit Number of open files at same time when stream directory patch;\n"
           "      maxOpenFileNumber>=8, DEFAULT 24, the best limit value by different\n"
           "        operating system.\n"
#endif
           "  -f  Force overwrite, ignore outNewPath already exists;\n"
           "      DEFAULT (no -f) not overwrite and then return error;\n"
           "      support oldPath outNewPath same path!(patch to tempPath and overwrite old)\n"
           "      if used -f and outNewPath is exist file:\n"
#if (_IS_NEED_DIR_DIFF_PATCH)
           "        if patch output file, will overwrite;\n"
#else
           "        will overwrite;\n"
#endif
#if (_IS_NEED_DIR_DIFF_PATCH)
           "        if patch output directory, will always return error;\n"
#endif
           "      if used -f and outNewPath is exist directory:\n"
#if (_IS_NEED_DIR_DIFF_PATCH)
           "        if patch output file, will always return error;\n"
#else
           "        will always return error;\n"
#endif
#if (_IS_NEED_DIR_DIFF_PATCH)
           "        if patch output directory, will overwrite, but not delete\n"
           "          needless existing files in directory.\n"
#endif
#if (_IS_NEED_ORIGINAL)
           "  -o  DEPRECATED; Original patch; compatible with \"patch_demo.c\",\n"
           "      diffFile must created by \"diff_demo.cpp\" or \"hdiffz -o ...\"\n"
#endif
           "  -h or -?\n"
           "      output Help info (this usage).\n"
           "  -v  output Version info.\n\n"
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
    
    HPATCH_PATHTYPE_ERROR, //adding begin v3.0
    HPATCH_TEMPPATH_ERROR,
    HPATCH_DELETEPATH_ERROR,
    HPATCH_RENAMEPATH_ERROR,
    
    DIRPATCH_DIRDIFFINFO_ERROR=101,
    DIRPATCH_CHECKSUMTYPE_ERROR,
    DIRPATCH_CHECKSUMSET_ERROR,
    DIRPATCH_CHECKSUM_DIFFDATA_ERROR,
    DIRPATCH_CHECKSUM_OLDDATA_ERROR,
    DIRPATCH_CHECKSUM_NEWDATA_ERROR,
    DIRPATCH_CHECKSUM_COPYDATA_ERROR,
    DIRPATCH_PATCH_ERROR,
    DIRPATCH_LAOD_DIRDIFFDATA_ERROR,
    DIRPATCH_OPEN_OLDPATH_ERROR,
    DIRPATCH_OPEN_NEWPATH_ERROR,
    DIRPATCH_CLOSE_OLDPATH_ERROR,
    DIRPATCH_CLOSE_NEWPATH_ERROR,
    DIRPATCH_PATCHBEGIN_ERROR,
    DIRPATCH_PATCHFINISH_ERROR,
} THPatchResult;

int hpatch_cmd_line(int argc, const char * argv[]);

int hpatch(const char* oldFileName,const char* diffFileName,const char* outNewFileName,
           hpatch_BOOL isOriginal,hpatch_BOOL isLoadOldAll,size_t patchCacheSize);
#if (_IS_NEED_DIR_DIFF_PATCH)
int hpatch_dir(const char* oldPath,const char* diffFileName,const char* outNewPath,
               hpatch_BOOL isLoadOldAll,size_t patchCacheSize,size_t kMaxOpenFileNumber,
               TDirPatchChecksumSet* checksumSet,IHPatchDirListener* hlistener);
#endif

#if (_IS_NEED_MAIN)
#   if (_IS_USE_WIN32_UTF8_WAPI)
int wmain(int argc,wchar_t* argv_w[]){
    char* argv_utf8[hpatch_kPathMaxSize*3/sizeof(char*)];
    if (!_wFileNames_to_utf8((const wchar_t**)argv_w,argc,argv_utf8,sizeof(argv_utf8)))
        return HPATCH_OPTIONS_ERROR;
    SetDefaultStringLocale();
    return hpatch_cmd_line(argc,argv_utf8);
}
#   else
int main(int argc, const char * argv[]){
    return hpatch_cmd_line(argc,argv);
}
#   endif
#endif

#if (_IS_NEED_DIR_DIFF_PATCH)
hpatch_inline static const char* findEnd(const char* str,char c){
    const char* result=strchr(str,c);
    return (result!=0)?result:(str+strlen(str));
}

static hpatch_BOOL _toChecksumSet(const char* psets,TDirPatchChecksumSet* checksumSet){
    while (hpatch_TRUE) {
        const char* pend=findEnd(psets,'-');
        size_t len=(size_t)(pend-psets);
        if (len==0) return hpatch_FALSE; //error no set
        if        ((len==4)&&(0==memcmp(psets,"diff",len))){
            checksumSet->isCheck_dirDiffData=hpatch_TRUE;
        }else  if ((len==3)&&(0==memcmp(psets,"old",len))){
            checksumSet->isCheck_oldRefData=hpatch_TRUE;
        }else  if ((len==3)&&(0==memcmp(psets,"new",len))){
            checksumSet->isCheck_newRefData=hpatch_TRUE;
        }else  if ((len==4)&&(0==memcmp(psets,"copy",len))){
            checksumSet->isCheck_copyFileData=hpatch_TRUE;
        }else  if ((len==3)&&(0==memcmp(psets,"all",len))){
            checksumSet->isCheck_dirDiffData=hpatch_TRUE;
            checksumSet->isCheck_oldRefData=hpatch_TRUE;
            checksumSet->isCheck_newRefData=hpatch_TRUE;
            checksumSet->isCheck_copyFileData=hpatch_TRUE;
        }else  if ((len==2)&&(0==memcmp(psets,"no",len))){
            checksumSet->isCheck_dirDiffData=hpatch_FALSE;
            checksumSet->isCheck_oldRefData=hpatch_FALSE;
            checksumSet->isCheck_newRefData=hpatch_FALSE;
            checksumSet->isCheck_copyFileData=hpatch_FALSE;
        }else{
            return hpatch_FALSE;//error unknow set
        }
        if (*pend=='\0')
            return hpatch_TRUE; //ok
        else
            psets=pend+1;
    }
}
#endif

#define _return_check(value,exitCode,errorInfo){ \
    if (!(value)) { fprintf(stderr,errorInfo " ERROR!\n"); return exitCode; } }

#define _options_check(value,errorInfo){ \
    if (!(value)) { fprintf(stderr,"options " errorInfo " ERROR!\n\n"); \
        printUsage(); return HPATCH_OPTIONS_ERROR; } }

#define kPatchCacheSize_min      (hpatch_kStreamCacheSize*8)
#define kPatchCacheSize_bestmin  ((size_t)1<<21)
#define kPatchCacheSize_default  ((size_t)1<<26)
#define kPatchCacheSize_bestmax  ((size_t)1<<30)

#define _kNULL_VALUE    (-1)
#define _kNULL_SIZE     ((size_t)(-1))

int hpatch_cmd_line(int argc, const char * argv[]){
    hpatch_BOOL isOriginal=_kNULL_VALUE;
    hpatch_BOOL isLoadOldAll=_kNULL_VALUE;
    hpatch_BOOL isForceOverwrite=_kNULL_VALUE;
    hpatch_BOOL isOutputHelp=_kNULL_VALUE;
    hpatch_BOOL isOutputVersion=_kNULL_VALUE;
    hpatch_BOOL isOldPathInputEmpty=_kNULL_VALUE;
    size_t      patchCacheSize=0;
#if (_IS_NEED_DIR_DIFF_PATCH)
    size_t      kMaxOpenFileNumber=_kNULL_SIZE; //only used in stream dir patch
    TDirPatchChecksumSet checksumSet={0,hpatch_FALSE,hpatch_TRUE,hpatch_TRUE,hpatch_FALSE}; //DEFAULT
#endif
    #define kMax_arg_values_size 3
    const char* arg_values[kMax_arg_values_size]={0};
    int         arg_values_size=0;
    int         i;
    for (i=1; i<argc; ++i) {
        const char* op=argv[i];
        _options_check(op!=0,"?");
        if (op[0]!='-'){
            hpatch_BOOL isEmpty=(strlen(op)==0);
            _options_check(arg_values_size<kMax_arg_values_size,"count");
            if (isEmpty){
                if (isOldPathInputEmpty==_kNULL_VALUE)
                    isOldPathInputEmpty=hpatch_TRUE;
                else
                    _options_check(!isEmpty,"?"); //error return
            }else{
                if (isOldPathInputEmpty==_kNULL_VALUE)
                    isOldPathInputEmpty=hpatch_FALSE;
            }
            arg_values[arg_values_size]=op; //path: file or directory
            ++arg_values_size;
            continue;
        }
        _options_check((op!=0)&&(op[0]=='-'),"?");
        switch (op[1]) {
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
                }else{
                    patchCacheSize=kPatchCacheSize_default;
                }
            } break;
            case 'f':{
                _options_check((isForceOverwrite==_kNULL_VALUE)&&(op[2]=='\0'),"-f");
                isForceOverwrite=hpatch_TRUE;
            } break;
#if (_IS_NEED_DIR_DIFF_PATCH)
            case 'C':{
                const char* psets=op+3;
                _options_check((op[2]=='-'),"-C-?");
                memset(&checksumSet,0,sizeof(checksumSet));//all set false
                _options_check(_toChecksumSet(psets,&checksumSet),"-C-?");
            } break;
            case 'n':{
                const char* pnum=op+3;
                _options_check((kMaxOpenFileNumber==_kNULL_SIZE)&&(op[2]=='-'),"-n-?");
                _options_check(kmg_to_size(pnum,strlen(pnum),&kMaxOpenFileNumber),"-n-?");
            } break;
#endif
            case '?':
            case 'h':{
                _options_check((isOutputHelp==_kNULL_VALUE)&&(op[2]=='\0'),"-h");
                isOutputHelp=hpatch_TRUE;
            } break;
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
            default: {
                _options_check(hpatch_FALSE,"-?");
            } break;
        }//swich
    }
    
    if (isOutputHelp==_kNULL_VALUE)
        isOutputHelp=hpatch_FALSE;
    if (isOutputVersion==_kNULL_VALUE)
        isOutputVersion=hpatch_FALSE;
    if (isForceOverwrite==_kNULL_VALUE)
        isForceOverwrite=hpatch_FALSE;
    if (isOutputHelp||isOutputVersion){
        printf("HDiffPatch::hpatchz v" HDIFFPATCH_VERSION_STRING "\n\n");
        if (isOutputHelp)
            printUsage();
        if (arg_values_size==0)
            return 0; //ok
    }
#if (_IS_NEED_DIR_DIFF_PATCH)
    if (kMaxOpenFileNumber==_kNULL_SIZE)
        kMaxOpenFileNumber=kMaxOpenFileNumber_default_patch;
    if (kMaxOpenFileNumber<kMaxOpenFileNumber_default_min)
        kMaxOpenFileNumber=kMaxOpenFileNumber_default_min;
#endif

    _options_check(arg_values_size==kMax_arg_values_size,"count");
    if (isOriginal==_kNULL_VALUE)
        isOriginal=hpatch_FALSE;
    if (isLoadOldAll==_kNULL_VALUE){
        isLoadOldAll=hpatch_FALSE;
        patchCacheSize=kPatchCacheSize_default;
    }
    if (isOldPathInputEmpty==_kNULL_VALUE)
        isOldPathInputEmpty=hpatch_FALSE;
    
    {
        const char* oldPath     =arg_values[0];
        const char* diffFileName=arg_values[1];
        const char* outNewPath  =arg_values[2];
#if (_IS_NEED_DIR_DIFF_PATCH)
        TDirDiffInfo dirDiffInfo;
        hpatch_BOOL  isOutDir;
#endif
        hpatch_BOOL  isSamePath=hpatch_getIsSamePath(oldPath,outNewPath);
        _return_check(!hpatch_getIsSamePath(oldPath,diffFileName),
                      HPATCH_PATHTYPE_ERROR,"oldPath diffFile same path");
        _return_check(!hpatch_getIsSamePath(outNewPath,diffFileName),
                      HPATCH_PATHTYPE_ERROR,"outNewPath diffFile same path");
        if (!isForceOverwrite){
            hpatch_TPathType   outNewPathType;
            _return_check(hpatch_getPathStat(outNewPath,&outNewPathType,0),
                          HPATCH_PATHTYPE_ERROR,"get outNewPath type");
            _return_check(outNewPathType==kPathType_notExist,
                          HPATCH_PATHTYPE_ERROR,"outNewPath already exists, not overwrite");
        }
        if (isSamePath)
            _return_check(isForceOverwrite,HPATCH_PATHTYPE_ERROR,"oldPath outNewPath same path");
#if (_IS_NEED_DIR_DIFF_PATCH)
        _return_check(getDirDiffInfoByFile(diffFileName,&dirDiffInfo),
                      HPATCH_OPENREAD_ERROR,"input diffFile open read");
        if (dirDiffInfo.isDirDiff)
            _options_check(!isOriginal,"-o unsupport dir patch");
        isOutDir=(dirDiffInfo.isDirDiff)&&(dirDiffInfo.newPathIsDir);
#endif
        if (!isSamePath){ // out new file or new dir
#if (_IS_NEED_DIR_DIFF_PATCH)
            if (dirDiffInfo.isDirDiff){
                return hpatch_dir(oldPath,diffFileName,outNewPath,isLoadOldAll,patchCacheSize,
                                  kMaxOpenFileNumber,&checksumSet,&defaultPatchDirlistener);
            }else
#endif
            {
                return hpatch(oldPath,diffFileName,outNewPath,isOriginal,isLoadOldAll,patchCacheSize);
            }
        }else
#if (_IS_NEED_DIR_DIFF_PATCH)
            if (!isOutDir)
#endif
        { // isSamePath==true and out to file
            int result;
            char newTempName[hpatch_kPathMaxSize];
#if (_IS_NEED_DIR_DIFF_PATCH)
            if (dirDiffInfo.isDirDiff)
                _return_check(!dirDiffInfo.oldPathIsDir,
                              HPATCH_PATHTYPE_ERROR,"can not use file overwrite oldDirectory");
#endif
            // 1. patch to newTempName
            // 2. if patch ok    then  { delelte oldPath; rename newTempName to oldPath; }
            //    if patch error then  { delelte newTempName; }
            _return_check(hpatch_getTempPathName(outNewPath,newTempName,newTempName+hpatch_kPathMaxSize),
                          HPATCH_TEMPPATH_ERROR,"getTempPathName(outNewPath)");
            printf("NOTE: outNewPath temp file will be rename to oldPath name after patch!\n");
#if (_IS_NEED_DIR_DIFF_PATCH)
            if (dirDiffInfo.isDirDiff){
                result=hpatch_dir(oldPath,diffFileName,newTempName,isLoadOldAll,patchCacheSize,
                                  kMaxOpenFileNumber,&checksumSet,&defaultPatchDirlistener);
            }else
#endif
            {
                result=hpatch(oldPath,diffFileName,newTempName,isOriginal,isLoadOldAll,patchCacheSize);
            }
            if (result==HPATCH_SUCCESS){
                _return_check(hpatch_removeFile(oldPath),
                              HPATCH_DELETEPATH_ERROR,"removeFile(oldPath)");
                _return_check(hpatch_renamePath(newTempName,oldPath),
                              HPATCH_RENAMEPATH_ERROR,"renamePath(temp,oldPath)");
                printf("outNewPath temp file renamed to oldPath name!\n");
            }else{//patch error
                if (!hpatch_removeFile(newTempName)){
                    printf("WARNING: can't remove temp file \"");
                    hpatch_printPath_utf8(newTempName); printf("\"\n");
                }
            }
            return result;
        }
#if (_IS_NEED_DIR_DIFF_PATCH)
        else{ // isDirDiff==true isSamePath==true and out to dir
            int result;
            char newTempDir[hpatch_kPathMaxSize];
            assert(dirDiffInfo.isDirDiff);
            _return_check(dirDiffInfo.oldPathIsDir,
                          HPATCH_PATHTYPE_ERROR,"can not use directory overwrite oldFile");
            _return_check(hpatch_getTempPathName(outNewPath,newTempDir,newTempDir+hpatch_kPathMaxSize),
                          HPATCH_TEMPPATH_ERROR,"getTempPathName(outNewPath)");
            printf("NOTE: all in outNewPath temp directory will be move to oldDirectory after patch!\n");
            result=hpatch_dir(oldPath,diffFileName,newTempDir,isLoadOldAll,patchCacheSize,
                              kMaxOpenFileNumber,&checksumSet,&tempDirPatchListener);
            if (result==HPATCH_SUCCESS){
                printf("all in outNewPath temp directory moved to oldDirectory!\n");
            }else if(!_isPathNotExist(newTempDir)){
                printf("WARNING: not remove temp directory \"");
                hpatch_printPath_utf8(newTempDir); printf("\"\n");
            }
            return result;
        }
#endif
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
    if (!(value)){ fprintf(stderr,errorInfo " ERROR!\n"); check_on_error(errorType); } }


static hpatch_BOOL getDecompressPlugin(const hpatch_compressedDiffInfo* diffInfo,
                                       hpatch_TDecompress** out_decompressPlugin){
    hpatch_TDecompress*  decompressPlugin=0;
    assert(0==*out_decompressPlugin);
    if (strlen(diffInfo->compressType)>0){
#ifdef  _CompressPlugin_zlib
        if ((!decompressPlugin)&&zlibDecompressPlugin.is_can_open(diffInfo->compressType))
            decompressPlugin=&zlibDecompressPlugin;
#endif
#ifdef  _CompressPlugin_bz2
        if ((!decompressPlugin)&&bz2DecompressPlugin.is_can_open(diffInfo->compressType))
            decompressPlugin=&bz2DecompressPlugin;
#endif
#ifdef  _CompressPlugin_lzma
        if ((!decompressPlugin)&&lzmaDecompressPlugin.is_can_open(diffInfo->compressType))
            decompressPlugin=&lzmaDecompressPlugin;
#endif
#ifdef  _CompressPlugin_lzma2
        if ((!decompressPlugin)&&lzma2DecompressPlugin.is_can_open(diffInfo->compressType))
            decompressPlugin=&lzma2DecompressPlugin;
#endif
#if (defined(_CompressPlugin_lz4) || (defined(_CompressPlugin_lz4hc)))
        if ((!decompressPlugin)&&lz4DecompressPlugin.is_can_open(diffInfo->compressType))
            decompressPlugin=&lz4DecompressPlugin;
#endif
#ifdef  _CompressPlugin_zstd
        if ((!decompressPlugin)&&zstdDecompressPlugin.is_can_open(diffInfo->compressType))
            decompressPlugin=&zstdDecompressPlugin;
#endif
    }
    if (!decompressPlugin){
        if (diffInfo->compressedCount>0){
            return hpatch_FALSE; //error
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
    return hpatch_TRUE;
}

#if (_IS_NEED_DIR_DIFF_PATCH)
static void _trySetChecksum(hpatch_TChecksum** out_checksumPlugin,const char* checksumType,
                            hpatch_TChecksum* testChecksumPlugin){
    if ((*out_checksumPlugin)!=0) return;
    if (0==strcmp(checksumType,testChecksumPlugin->checksumType()))
        *out_checksumPlugin=testChecksumPlugin;
}
static hpatch_BOOL _findChecksum(hpatch_TChecksum** out_checksumPlugin,const char* checksumType){
    assert(0==*out_checksumPlugin);
#ifdef _ChecksumPlugin_crc32
    _trySetChecksum(out_checksumPlugin,checksumType,&crc32ChecksumPlugin);
#endif
#ifdef _ChecksumPlugin_adler32
    _trySetChecksum(out_checksumPlugin,checksumType,&adler32ChecksumPlugin);
#endif
#ifdef _ChecksumPlugin_adler64
    _trySetChecksum(out_checksumPlugin,checksumType,&adler64ChecksumPlugin);
#endif
#ifdef _ChecksumPlugin_fadler32
    _trySetChecksum(out_checksumPlugin,checksumType,&fadler32ChecksumPlugin);
#endif
#ifdef _ChecksumPlugin_fadler64
    _trySetChecksum(out_checksumPlugin,checksumType,&fadler64ChecksumPlugin);
#endif
#ifdef _ChecksumPlugin_fadler128
    _trySetChecksum(out_checksumPlugin,checksumType,&fadler128ChecksumPlugin);
#endif
#ifdef _ChecksumPlugin_md5
    _trySetChecksum(out_checksumPlugin,checksumType,&md5ChecksumPlugin);
#endif
    return (0!=*out_checksumPlugin);
}
#endif

#define _free_mem(p) { if (p) { free(p); p=0; } }

static void* getPatchMemCache(hpatch_BOOL isLoadOldAll,size_t patchCacheSize,
                              hpatch_StreamPos_t oldDataSize,size_t* out_memCacheSize){
    void*  temp_cache=0;
    size_t temp_cache_size;
    if (isLoadOldAll){
        size_t addSize=kPatchCacheSize_bestmin;
        if (addSize>oldDataSize+kPatchCacheSize_min)
            addSize=(size_t)(oldDataSize+kPatchCacheSize_min);
        assert(patchCacheSize==0);
        temp_cache_size=(size_t)(oldDataSize+addSize);
        if (temp_cache_size!=oldDataSize+addSize)
            temp_cache_size=kPatchCacheSize_bestmax;//can not load all,load part
    }else{
        if (patchCacheSize<kPatchCacheSize_min)
            patchCacheSize=kPatchCacheSize_min;
        temp_cache_size=patchCacheSize;
        if (temp_cache_size>oldDataSize+kPatchCacheSize_bestmin)
            temp_cache_size=(size_t)(oldDataSize+kPatchCacheSize_bestmin);
    }
    while (!temp_cache) {
        temp_cache=(TByte*)malloc(temp_cache_size);
        if ((!temp_cache)&&(temp_cache_size>=kPatchCacheSize_min*2))
            temp_cache_size>>=1;
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
    hpatch_TFileStreamOutput    newData;
    hpatch_TFileStreamInput     diffData;
    hpatch_TFileStreamInput     oldData;
    hpatch_TStreamInput* poldData=&oldData.base;
    TByte*               temp_cache=0;
    size_t               temp_cache_size;
    hpatch_StreamPos_t   savedNewSize=0;
    hpatch_BOOL          patch_result;
    hpatch_TFileStreamInput_init(&oldData);
    hpatch_TFileStreamInput_init(&diffData);
    hpatch_TFileStreamOutput_init(&newData);
    {//open
        printf(    "old : \""); hpatch_printPath_utf8(oldFileName);
        printf("\"\ndiff: \""); hpatch_printPath_utf8(diffFileName);
        printf("\"\nout : \""); hpatch_printPath_utf8(outNewFileName);
        printf("\"\n");
        if (0==strcmp(oldFileName,"")){ // isOldPathInputEmpty
            mem_as_hStreamInput(&oldData.base,0,0);
        }else{
            check(hpatch_TFileStreamInput_open(&oldData,oldFileName),
                  HPATCH_OPENREAD_ERROR,"open oldFile for read");
        }
        check(hpatch_TFileStreamInput_open(&diffData,diffFileName),
              HPATCH_OPENREAD_ERROR,"open diffFile for read");
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
        hpatch_TFileStreamInput_setOffset(&diffData,kNewDataSizeSavedSize);
    }else
#endif
    {
        hpatch_compressedDiffInfo diffInfo;
        assert(!isOriginal);
        if (!getCompressedDiffInfo(&diffInfo,&diffData.base)){
            check(!diffData.fileError,HPATCH_FILEREAD_ERROR,"read diffFile");
            check(hpatch_FALSE,HPATCH_HDIFFINFO_ERROR,"is hdiff file? getCompressedDiffInfo()");
        }
        if (poldData->streamSize!=diffInfo.oldDataSize){
            fprintf(stderr,"oldFile dataSize %" PRIu64 " != diffFile saved oldDataSize %" PRIu64 " ERROR!\n",
                    poldData->streamSize,diffInfo.oldDataSize);
            check_on_error(HPATCH_FILEDATA_ERROR);
        }
        if(!getDecompressPlugin(&diffInfo,&decompressPlugin)){
            fprintf(stderr,"can not decompress \"%s\" data ERROR!\n",diffInfo.compressType);
            check_on_error(HPATCH_COMPRESSTYPE_ERROR);
        }
        savedNewSize=diffInfo.newDataSize;
    }
    check(hpatch_TFileStreamOutput_open(&newData, outNewFileName,savedNewSize),
          HPATCH_OPENWRITE_ERROR,"open out newFile for write");
    printf("oldDataSize : %" PRIu64 "\ndiffDataSize: %" PRIu64 "\nnewDataSize : %" PRIu64 "\n",
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
        fprintf(stderr,"out newFile dataSize %" PRIu64 " != diffFile saved newDataSize %" PRIu64 " ERROR!\n",
               newData.out_length,newData.base.streamSize);
        check_on_error(HPATCH_FILEDATA_ERROR);
    }
    printf("  patch ok!\n");
    
clear:
    _isInClear=hpatch_TRUE;
    check(hpatch_TFileStreamOutput_close(&newData),HPATCH_FILECLOSE_ERROR,"out newFile close");
    check(hpatch_TFileStreamInput_close(&diffData),HPATCH_FILECLOSE_ERROR,"diffFile close");
    check(hpatch_TFileStreamInput_close(&oldData),HPATCH_FILECLOSE_ERROR,"oldFile close");
    _free_mem(temp_cache);
    printf("\nhpatchz time: %.3f s\n",(clock_s()-time0));
    return result;
}

#if (_IS_NEED_DIR_DIFF_PATCH)
int hpatch_dir(const char* oldPath,const char* diffFileName,const char* outNewPath,
               hpatch_BOOL isLoadOldAll,size_t patchCacheSize,size_t kMaxOpenFileNumber,
               TDirPatchChecksumSet* checksumSet,IHPatchDirListener* hlistener){
    int     result=HPATCH_SUCCESS;
    int     _isInClear=hpatch_FALSE;
    double  time0=clock_s();
    hpatch_TFileStreamInput     diffData;
    TDirPatcher          dirPatcher;
    const TDirDiffInfo*  dirDiffInfo=0;
    TByte*               p_temp_mem=0;
    TByte*               temp_cache=0;
    size_t               temp_cache_size;
    const hpatch_TStreamInput*  oldStream=0;
    const hpatch_TStreamOutput* newStream=0;
    hpatch_TFileStreamInput_init(&diffData);
    TDirPatcher_init(&dirPatcher);
    assert(0!=strcmp(oldPath,outNewPath));
    {//dir diff info
        hpatch_BOOL  rt;
        hpatch_TPathType    oldType;
        if (0==strcmp(oldPath,"")){ // isOldPathInputEmpty
            assert(!isLoadOldAll);
            oldType=kPathType_file; //as empty file
        }else{
            check(hpatch_getPathTypeByName(oldPath,&oldType,0),HPATCH_PATHTYPE_ERROR,"get oldPath type");
            check((oldType!=kPathType_notExist),HPATCH_PATHTYPE_ERROR,"oldPath not exist");
        }
        check(hpatch_TFileStreamInput_open(&diffData,diffFileName),HPATCH_OPENREAD_ERROR,"open diffFile for read");
        rt=TDirPatcher_open(&dirPatcher,&diffData.base,&dirDiffInfo);
        if((!rt)||(!dirDiffInfo->isDirDiff)){
            check(!diffData.fileError,HPATCH_FILEREAD_ERROR,"read diffFile");
            check(hpatch_FALSE,DIRPATCH_DIRDIFFINFO_ERROR,"is hdiff file? get dir diff info");
        }
        if (dirDiffInfo->oldPathIsDir){
            check(kPathType_dir==oldType,HPATCH_PATHTYPE_ERROR,"input old path need dir");
        }else{
            check(kPathType_file==oldType,HPATCH_PATHTYPE_ERROR,"input old path need file");
        }
        printf(        dirDiffInfo->oldPathIsDir?"old  dir: \"":"old file: \"");
        hpatch_printPath_utf8(oldPath);
        if (dirDiffInfo->oldPathIsDir&&(!hpatch_getIsDirName(oldPath))) printf("%c",kPatch_dirSeparator);
        printf(                                             "\"\ndiffFile: \"");
        hpatch_printPath_utf8(diffFileName);
        printf(dirDiffInfo->newPathIsDir?"\"\nout  dir: \"":"\"\nout file: \"");
        hpatch_printPath_utf8(outNewPath);
        if (dirDiffInfo->newPathIsDir&&(!hpatch_getIsDirName(outNewPath))) printf("%c",kPatch_dirSeparator);
        printf("\"\n");
    }
    {//decompressPlugin
        hpatch_TDecompress*  decompressPlugin=0;
        hpatch_compressedDiffInfo hdiffInfo;
        hdiffInfo=dirDiffInfo->hdiffInfo;
        hdiffInfo.compressedCount+=(dirDiffInfo->dirDataIsCompressed)?1:0;
        if(!getDecompressPlugin(&hdiffInfo,&decompressPlugin)){
            fprintf(stderr,"can not decompress \"%s\" data ERROR!\n",hdiffInfo.compressType);
            check_on_error(HPATCH_COMPRESSTYPE_ERROR);
        }
        //load dir data
        check(TDirPatcher_loadDirData(&dirPatcher,decompressPlugin,oldPath,outNewPath),
              DIRPATCH_LAOD_DIRDIFFDATA_ERROR,"load dir data in diffFile");
    }
    check(hlistener->patchBegin(hlistener,&dirPatcher),
          DIRPATCH_PATCHBEGIN_ERROR,"dir patch begin");
    {// checksumPlugin
        int wantChecksumCount = checksumSet->isCheck_dirDiffData+checksumSet->isCheck_oldRefData+
        checksumSet->isCheck_newRefData+checksumSet->isCheck_copyFileData;
        assert(checksumSet->checksumPlugin==0);
        if (wantChecksumCount>0){
            if (strlen(dirDiffInfo->checksumType)==0){
                memset(checksumSet,0,sizeof(*checksumSet));
                printf("  NOTE: no checksum saved in diffFile,can not do checksum\n");
            }else{
                if (!_findChecksum(&checksumSet->checksumPlugin,dirDiffInfo->checksumType)){
                    fprintf(stderr,"not found checksumType \"%s\" ERROR!\n",dirDiffInfo->checksumType);
                    check_on_error(DIRPATCH_CHECKSUMTYPE_ERROR);
                }
                printf("hpatchz run with checksum plugin: \"%s\" (need checksum %d)\n",
                       dirDiffInfo->checksumType,wantChecksumCount);
                if (!TDirPatcher_checksum(&dirPatcher,checksumSet)){
                    check(!dirPatcher.isDiffDataChecksumError,
                          DIRPATCH_CHECKSUM_DIFFDATA_ERROR,"diffFile checksum");
                    check(hpatch_FALSE,DIRPATCH_CHECKSUMSET_ERROR,"diffFile set checksum");
                }
            }
        }
    }
    {//info
        const _TDirDiffHead* head=&dirPatcher.dirDiffHead;
        printf("DirPatch new path count: %" PRIu64 " (fileCount:%" PRIu64 ")\n",(hpatch_StreamPos_t)head->newPathCount,
               (hpatch_StreamPos_t)(head->sameFilePairCount+head->newRefFileCount));
        printf("    copy from old count: %" PRIu64 " (dataSize: %" PRIu64 ")\n",
               (hpatch_StreamPos_t)head->sameFilePairCount,head->sameFileSize);
        printf("     ref old file count: %" PRIu64 "\n",(hpatch_StreamPos_t)head->oldRefFileCount);
        printf("     ref new file count: %" PRIu64 "\n",(hpatch_StreamPos_t)head->newRefFileCount);
        printf("oldRefSize  : %" PRIu64 "\ndiffDataSize: %" PRIu64 "\nnewRefSize  : %" PRIu64 " (all newSize: %" PRIu64 ")\n",
               dirDiffInfo->hdiffInfo.oldDataSize,diffData.base.streamSize,dirDiffInfo->hdiffInfo.newDataSize,
               dirDiffInfo->hdiffInfo.newDataSize+head->sameFileSize);
    }
    {//mem cache
        p_temp_mem=getPatchMemCache(isLoadOldAll,patchCacheSize,dirDiffInfo->hdiffInfo.oldDataSize, &temp_cache_size);
        check(p_temp_mem,HPATCH_MEM_ERROR,"alloc cache memory");
        temp_cache=p_temp_mem;
    }
    {//old data
        if (temp_cache_size>=dirDiffInfo->hdiffInfo.oldDataSize+kPatchCacheSize_min){
            // all old while auto cache by patch; not need open old files at same time
            kMaxOpenFileNumber=kMaxOpenFileNumber_limit_min;
        }
        check(TDirPatcher_openOldRefAsStream(&dirPatcher,kMaxOpenFileNumber,&oldStream),
              DIRPATCH_OPEN_OLDPATH_ERROR,"open oldFile");
    }
    {//new data
        check(TDirPatcher_openNewDirAsStream(&dirPatcher,&hlistener->base,&newStream),
              DIRPATCH_OPEN_NEWPATH_ERROR,"open newFile");
    }
    //patch
    if(!TDirPatcher_patch(&dirPatcher,newStream,oldStream,temp_cache,temp_cache+temp_cache_size)){
        check(!dirPatcher.isOldRefDataChecksumError,
              DIRPATCH_CHECKSUM_OLDDATA_ERROR,"oldFile checksum");
        check(!dirPatcher.isCopyDataChecksumError,
              DIRPATCH_CHECKSUM_COPYDATA_ERROR,"copyOldFile checksum");
        check(!dirPatcher.isNewRefDataChecksumError,
              DIRPATCH_CHECKSUM_NEWDATA_ERROR,"newFile checksum");
        check(hpatch_FALSE,DIRPATCH_PATCH_ERROR,"dir patch run");
    }
clear:
    _isInClear=hpatch_TRUE;
    check(hlistener->patchFinish(hlistener,result==HPATCH_SUCCESS),
          DIRPATCH_PATCHFINISH_ERROR,"dir patch finish");
    check(TDirPatcher_closeNewDirStream(&dirPatcher),DIRPATCH_CLOSE_NEWPATH_ERROR,"newPath close");
    check(TDirPatcher_closeOldRefStream(&dirPatcher),DIRPATCH_CLOSE_OLDPATH_ERROR,"oldPath close");
    TDirPatcher_close(&dirPatcher);
    check(hpatch_TFileStreamInput_close(&diffData),HPATCH_FILECLOSE_ERROR,"diffFile close");
    _free_mem(p_temp_mem);
    printf("\nhpatchz dir patch time: %.3f s\n",(clock_s()-time0));
    return result;
}
#endif
