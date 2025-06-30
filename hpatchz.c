//hpatchz.c
// patch tool
//
/*
 This is the HDiffPatch copyright.

 Copyright (c) 2012-2021 HouSisong All Rights Reserved.

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
#include "dirDiffPatch/dir_patch/dir_patch.h"
#if (_IS_NEED_DIR_DIFF_PATCH)
#include "hpatch_dir_listener.h"
#endif

#ifndef _IS_NEED_PRINT_LOG
#   define _IS_NEED_PRINT_LOG   1
#endif
#if (_IS_NEED_PRINT_LOG)
#   define  _log_info_utf8  hpatch_printPath_utf8
#else
#   define  printf(...)
#   define  _log_info_utf8(...) do{}while(0)
#endif

#ifndef _IS_NEED_MAIN
#   define  _IS_NEED_MAIN 1
#endif
#ifndef _IS_NEED_CMDLINE
#   define  _IS_NEED_CMDLINE 1
#endif
#ifndef _IS_NEED_SINGLE_STREAM_DIFF
#   define _IS_NEED_SINGLE_STREAM_DIFF 1
#endif
#ifndef _IS_NEED_BSDIFF
#   define _IS_NEED_BSDIFF 1
#endif
#ifndef _IS_NEED_VCDIFF
#   define _IS_NEED_VCDIFF 1
#endif
#ifndef _IS_NEED_SFX
#   define _IS_NEED_SFX 1
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
#   ifndef _IS_NEED_decompressor_ldef_replace_zlib
#       define _IS_NEED_decompressor_ldef_replace_zlib  0 
#   endif
#   if (_IS_NEED_decompressor_ldef_replace_zlib)
#       define _CompressPlugin_ldef  //optimized zlib&ldef's deflate code decompress speed by libdeflate
#   else
#       define _CompressPlugin_zlib
#   endif
#   define _CompressPlugin_bz2
#   define _CompressPlugin_lzma
#   define _CompressPlugin_lzma2
#   define _CompressPlugin_zstd
#if (_IS_NEED_VCDIFF)
#   define _CompressPlugin_7zXZ
#endif
#endif
#if (_IS_NEED_ALL_CompressPlugin)
//===== select needs decompress plugins or change to your plugin=====
#   define _CompressPlugin_lz4 // || _CompressPlugin_lz4hc
#   define _CompressPlugin_brotli
#   define _CompressPlugin_lzham
#   define _CompressPlugin_tuz
#endif
#ifdef _CompressPlugin_ldef
#   ifndef _CompressPlugin_ldef_is_use_zlib
#       define _CompressPlugin_ldef_is_use_zlib         1 //now ldef need zlib decompressor for any all of deflate code
#   endif
#   if (_IS_NEED_decompressor_ldef_replace_zlib&&(defined(_CompressPlugin_zlib)))
#       undef _CompressPlugin_zlib
#   endif
#endif

#if (_IS_NEED_BSDIFF)
#   include "bsdiff_wrapper/bspatch_wrapper.h"
#   ifndef _CompressPlugin_bz2
#       define _CompressPlugin_bz2  //bsdiff4 need bz2
#   endif
#endif
#if (_IS_NEED_VCDIFF)
#   include "vcdiff_wrapper/vcpatch_wrapper.h"
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
#   define _ChecksumPlugin_crc32    //    32 bit effective  //need zlib
#   define _ChecksumPlugin_fadler64 // ?  63 bit effective
#endif
#if (_IS_NEED_ALL_ChecksumPlugin)
//===== select needs checksum plugins or change to your plugin=====
#   define _ChecksumPlugin_adler32  // ?  29 bit effective
#   define _ChecksumPlugin_adler64  // ?  30 bit effective
#   define _ChecksumPlugin_fadler32 // ~  32 bit effective
#   define _ChecksumPlugin_fadler128// ?  81 bit effective
#   define _ChecksumPlugin_md5      //   128 bit
#   define _ChecksumPlugin_blake3   //   256 bit
#   define _ChecksumPlugin_xxh3     //    64 bit fast
#   define _ChecksumPlugin_xxh128   //   128 bit fast
#endif

#include "checksum_plugin_demo.h"
#endif

#if (_IS_NEED_CMDLINE)
static void printVersion(){
    printf("HDiffPatch::hpatchz v" HDIFFPATCH_VERSION_STRING "\n");
}

static void printHelpInfo(){
    printf("  -h  (or -?)\n"
           "      print usage info.\n");
}

int hpatch_printFilesInfos(int fileCount,const char* fileNames[]);

#if (_IS_NEED_SFX)
#   ifdef _WIN32
        #define kSFX_curDefaultPath ".\\"
#   else
        #define kSFX_curDefaultPath "./"
#   endif
#endif

static void printUsage(){
    printVersion();
    printf("\n");
    printf("patch usage: hpatchz [options] oldPath diffFile outNewPath\n"
           "print  info: hpatchz -info diffFile\n"
#if (_IS_NEED_SFX)
           "create  SFX: hpatchz [-X-exe#selfExecuteFile] diffFile -X#outSelfExtractArchive\n"
           "run     SFX: selfExtractArchive [[options] oldPath -X outNewPath]\n"
           "extract SFX: selfExtractArchive  (same as: $selfExtractArchive -f {\"\"|\"" kSFX_curDefaultPath "\"} -X "
           "\"" kSFX_curDefaultPath "\")\n"
#endif
           "  if oldPath is empty input parameter \"\"\n"
           "options:\n"
           "  -s[-cacheSize] \n"
           "      DEFAULT -s-4m; oldPath loaded as Stream;\n"
           "      cacheSize can like 262144 or 256k or 512m or 2g etc....\n"
           "      requires (cacheSize + 4*decompress buffer size)+O(1) bytes of memory.\n"
           "      if diffFile is single compressed diffData(created by hdiffz -SD-stepSize), then requires\n"
           "        (cacheSize+ stepSize + 1*decompress buffer size)+O(1) bytes of memory;\n"
#if (_IS_NEED_BSDIFF||_IS_NEED_VCDIFF)
           "      if diffFile is created by"
#if (_IS_NEED_BSDIFF)
           " hdiffz -BSD,bsdiff4,"
#endif
#if (_IS_NEED_VCDIFF)
           " hdiffz -VCD,"
#endif
           " then requires\n"
           "        (cacheSize + 3*decompress buffer size)+O(1) bytes of memory;\n"
#endif
#if (_IS_NEED_VCDIFF)
           "      if diffFile is created by xdelta3,open-vcdiff, then requires\n"
           "        (sourceWindowSize+targetWindowSize + 3*decompress buffer size)+O(1) bytes of memory.\n"
#endif
           "  -m  oldPath all loaded into Memory;\n"
           "      requires (oldFileSize + 4*decompress buffer size)+O(1) bytes of memory.\n"
           "      if diffFile is single compressed diffData(created by hdiffz -SD-stepSize), then requires\n"
           "        (oldFileSize+ stepSize + 1*decompress buffer size)+O(1) bytes of memory.\n"
#if (_IS_NEED_BSDIFF)
           "      if diffFile is created by hdiffz -BSD,bsdiff4, then requires\n"
           "        (oldFileSize + 3*decompress buffer size)+O(1) bytes of memory.\n"
#endif
#if (_IS_NEED_VCDIFF)
           "      if diffFile is VCDIFF(created by hdiffz -VCD,xdelta3,open-vcdiff), then requires\n"
           "        (sourceWindowSize+targetWindowSize + 3*decompress buffer size)+O(1) bytes of memory.\n"
#endif
#if (_IS_NEED_DIR_DIFF_PATCH)
           "  -C-checksumSets\n"
           "      set Checksum data for directory patch, DEFAULT -C-new-copy;\n"
           "      checksumSets support (can choose multiple):\n"
           "        -C-diff         checksum diffFile;\n"
           "        -C-old          checksum old reference files;\n"
           "        -C-new          checksum new files edited from old reference files;\n"
           "        -C-copy         checksum new files copy from old same files;\n"
           "        -C-no           no checksum;\n"
           "        -C-all          same as: -C-diff-old-new-copy;\n"
#endif
#if (_IS_NEED_VCDIFF)
           "  -C-no or -C-new\n"
           "      if diffFile is VCDIFF, then to close or open checksum, DEFAULT -C-new.\n"
#endif
#if (_IS_NEED_DIR_DIFF_PATCH)
           "  -n-maxOpenFileNumber\n"
           "      limit Number of open files at same time when stream directory patch;\n"
           "      maxOpenFileNumber>=8, DEFAULT -n-24, the best limit value by different\n"
           "        operating system.\n"
#endif
           "  -f  Force overwrite, ignore write path already exists;\n"
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
           "  -info\n"
           "      print infos of diffFile.\n"
           "  -v  print Version info.\n"
           );
    printHelpInfo();
    printf("\n");
}
#endif// _IS_NEED_CMDLINE

typedef enum THPatchResult {
    HPATCH_SUCCESS=0,
    HPATCH_OPTIONS_ERROR=1,
    HPATCH_OPENREAD_ERROR,
    HPATCH_OPENWRITE_ERROR,
    HPATCH_FILEREAD_ERROR,
    HPATCH_FILEWRITE_ERROR, // 5 //see 24
    HPATCH_FILEDATA_ERROR,
    HPATCH_FILECLOSE_ERROR,
    HPATCH_MEM_ERROR, //see 22
    HPATCH_HDIFFINFO_ERROR,
    HPATCH_COMPRESSTYPE_ERROR, // 10
    HPATCH_HPATCH_ERROR,
    HPATCH_PATHTYPE_ERROR, //adding begin v3.0
    HPATCH_TEMPPATH_ERROR,
    HPATCH_DELETEPATH_ERROR,
    HPATCH_RENAMEPATH_ERROR, // 15
    HPATCH_SPATCH_ERROR,
    HPATCH_BSPATCH_ERROR,
    HPATCH_VCPATCH_ERROR,

    HPATCH_DECOMPRESSER_OPEN_ERROR=20,
    HPATCH_DECOMPRESSER_CLOSE_ERROR,
    HPATCH_DECOMPRESSER_MEM_ERROR,
    HPATCH_DECOMPRESSER_DECOMPRESS_ERROR,
    HPATCH_FILEWRITE_NO_SPACE_ERROR,

#if (_IS_NEED_DIR_DIFF_PATCH)
    DIRPATCH_DIRDIFFINFO_ERROR=101,
    DIRPATCH_CHECKSUMTYPE_ERROR,
    DIRPATCH_CHECKSUMSET_ERROR,
    DIRPATCH_CHECKSUM_DIFFDATA_ERROR,
    DIRPATCH_CHECKSUM_OLDDATA_ERROR, // 105
    DIRPATCH_CHECKSUM_NEWDATA_ERROR,
    DIRPATCH_CHECKSUM_COPYDATA_ERROR,
    DIRPATCH_PATCH_ERROR,
    DIRPATCH_LAOD_DIRDIFFDATA_ERROR,
    DIRPATCH_OPEN_OLDPATH_ERROR, // 110
    DIRPATCH_OPEN_NEWPATH_ERROR,
    DIRPATCH_CLOSE_OLDPATH_ERROR,
    DIRPATCH_CLOSE_NEWPATH_ERROR,
    DIRPATCH_PATCHBEGIN_ERROR,
    DIRPATCH_PATCHFINISH_ERROR, // 115
    DIRPATCH_PATCH_FILE_ERROR,
#endif
#if (_IS_NEED_SFX)
    HPATCH_CREATE_SFX_DIFFFILETYPE_ERROR=201,
    HPATCH_CREATE_SFX_SFXTYPE_ERROR,
    HPATCH_CREATE_SFX_EXECUTETAG_ERROR,
    HPATCH_RUN_SFX_NOTSFX_ERROR,
    HPATCH_RUN_SFX_DIFFOFFSERT_ERROR, // 205
#endif
} THPatchResult;

int hpatch(const char* oldFileName,const char* diffFileName,const char* outNewFileName,
           hpatch_BOOL isLoadOldAll,size_t patchCacheSize,hpatch_StreamPos_t diffDataOffert,
           hpatch_StreamPos_t diffDataSize,hpatch_BOOL vcpatch_isChecksum,hpatch_BOOL vcpatch_isInMem);
#if (_IS_NEED_DIR_DIFF_PATCH)
int hpatch_dir(const char* oldPath,const char* diffFileName,const char* outNewPath,
               hpatch_BOOL isLoadOldAll,size_t patchCacheSize,size_t kMaxOpenFileNumber,
               TDirPatchChecksumSet* checksumSet,IHPatchDirListener* hlistener,
               hpatch_StreamPos_t diffDataOffert,hpatch_StreamPos_t diffDataSize);
#endif
#if (_IS_NEED_SFX)
int createSfx(const char* selfExecuteFileName,const char* diffFileName,const char* out_sfxFileName);
hpatch_BOOL getDiffDataOffertInSfx(hpatch_StreamPos_t* out_diffDataOffert,hpatch_StreamPos_t* out_diffDataSize);
#endif

#if (_IS_NEED_MAIN)
int hpatch_cmd_line(int argc, const char * argv[]);
#   if (_IS_USED_WIN32_UTF8_WAPI)
int wmain(int argc,wchar_t* argv_w[]){
    char* argv_utf8[hpatch_kPathMaxSize*3/sizeof(char*)];
    if (!_wFileNames_to_utf8((const wchar_t**)argv_w,argc,argv_utf8,sizeof(argv_utf8)))
        return HPATCH_OPTIONS_ERROR;
    SetDefaultStringLocale();
    return hpatch_cmd_line(argc,(const char**)argv_utf8);
}
#   else
int main(int argc, const char * argv[]){
    return hpatch_cmd_line(argc,argv);
}
#   endif
#endif

#if ((_IS_NEED_DIR_DIFF_PATCH)||(_IS_NEED_VCDIFF))
static hpatch_BOOL _toChecksumSet(const char* psets,TDirPatchChecksumSet* checksumSet){
    while (hpatch_TRUE) {
        const char* pend=findUntilEnd(psets,'-');
        size_t len=(size_t)(pend-psets);
        if (len==0) return hpatch_FALSE; //error no set
        if ((len==3)&&(0==memcmp(psets,"new",len))){
            checksumSet->isCheck_newRefData=hpatch_TRUE;
        }else if ((len==3)&&(0==memcmp(psets,"all",len))){
            checksumSet->isCheck_dirDiffData=hpatch_TRUE;
            checksumSet->isCheck_oldRefData=hpatch_TRUE;
            checksumSet->isCheck_newRefData=hpatch_TRUE;
            checksumSet->isCheck_copyFileData=hpatch_TRUE;
        }else if ((len==2)&&(0==memcmp(psets,"no",len))){
            checksumSet->isCheck_dirDiffData=hpatch_FALSE;
            checksumSet->isCheck_oldRefData=hpatch_FALSE;
            checksumSet->isCheck_newRefData=hpatch_FALSE;
            checksumSet->isCheck_copyFileData=hpatch_FALSE;
        }else
#if (_IS_NEED_DIR_DIFF_PATCH)
        if       ((len==4)&&(0==memcmp(psets,"diff",len))){
            checksumSet->isCheck_dirDiffData=hpatch_TRUE;
        }else if ((len==3)&&(0==memcmp(psets,"old",len))){
            checksumSet->isCheck_oldRefData=hpatch_TRUE;
        }else if ((len==4)&&(0==memcmp(psets,"copy",len))){
            checksumSet->isCheck_copyFileData=hpatch_TRUE;
        }else
#endif
        {
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
    if (!(value)) { LOG_ERR(errorInfo " ERROR!\n"); return exitCode; } }

#define _options_check(value,errorInfo){ \
    if (!(value)) { LOG_ERR("options " errorInfo " ERROR!\n\n"); \
        printHelpInfo(); return HPATCH_OPTIONS_ERROR; } }

#define kPatchCacheSize_min      (hpatch_kStreamCacheSize*8)
#define kPatchCacheSize_bestmin  ((size_t)1<<21)
#define kPatchCacheSize_default  ((size_t)1<<22)

#define _kNULL_VALUE    (-1)
#define _kNULL_SIZE     (~(size_t)0)

#if (_IS_NEED_CMDLINE)
#define _isSwapToPatchTag(tag) (0==strcmp("--patch",tag))

int isSwapToPatchMode(int argc,const char* argv[]){
    int i;
    for (i=1;i<argc;++i){
        if (_isSwapToPatchTag(argv[i]))
            return hpatch_TRUE;
    }
#if (_IS_NEED_SFX)
    if (getDiffDataOffertInSfx(0,0))
        return hpatch_TRUE;
#endif
    return hpatch_FALSE;
}

int hpatch_cmd_line(int argc, const char * argv[]){
    hpatch_BOOL isPrintFileInfo=_kNULL_VALUE;
    hpatch_BOOL isLoadOldAll=_kNULL_VALUE;
    hpatch_BOOL isForceOverwrite=_kNULL_VALUE;
    hpatch_BOOL isOutputHelp=_kNULL_VALUE;
    hpatch_BOOL isOutputVersion=_kNULL_VALUE;
    hpatch_BOOL isOldPathInputEmpty=_kNULL_VALUE;
    hpatch_BOOL isRunSFX=_kNULL_VALUE;
#if (_IS_NEED_SFX)
    const char* out_SFX=0;
    const char* selfExecuteFile=0;
#endif
    size_t      patchCacheSize=0;
#if (_IS_NEED_DIR_DIFF_PATCH)
    size_t      kMaxOpenFileNumber=_kNULL_SIZE; //only used in stream dir patch
#endif
#if ((_IS_NEED_DIR_DIFF_PATCH)||(_IS_NEED_VCDIFF))
    TDirPatchChecksumSet checksumSet={0,hpatch_FALSE,hpatch_TRUE,hpatch_TRUE,hpatch_FALSE}; //DEFAULT
#endif
    hpatch_BOOL vcpatch_isChecksum=hpatch_TRUE;
    #define kMax_arg_values_size 3
    const char* arg_values[kMax_arg_values_size]={0};
    int         arg_values_size=0;
    int         i;
    for (i=1; i<argc; ++i) {
        const char* op=argv[i];
        _options_check(op!=0,"?");
        if (_isSwapToPatchTag(op))
            continue;
        if (op[0]!='-'){
            hpatch_BOOL isEmpty=(strlen(op)==0);
            _options_check(arg_values_size<kMax_arg_values_size,"input count");
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
#if (_IS_NEED_SFX)
            case 'X':{
                if (op[2]=='#'){
                    const char* pnum=op+3;
                    _options_check(out_SFX==0,"-X#?");
                    out_SFX=pnum;
                    _options_check(strlen(out_SFX)>0,"-X#?");
                }else if (op[2]=='-'){
                    _options_check(selfExecuteFile==0,"-X-?");
                    if ((op[3]=='e')&&(op[4]=='x')&&(op[5]=='e')&&(op[6]=='#')){
                        selfExecuteFile=op+7;
                        _options_check(strlen(selfExecuteFile)>0,"-X-exe#?");
                    }else{
                        _options_check(hpatch_FALSE,"-X-?");
                    }
                }else{
                    _options_check((isRunSFX==_kNULL_VALUE)&&(op[2]=='\0'),"-X");
                    isRunSFX=hpatch_TRUE;
                }
            } break;
#endif
            case 'f':{
                _options_check((isForceOverwrite==_kNULL_VALUE)&&(op[2]=='\0'),"-f");
                isForceOverwrite=hpatch_TRUE;
            } break;
#if ((_IS_NEED_DIR_DIFF_PATCH)||(_IS_NEED_VCDIFF))
            case 'C':{
                const char* psets=op+3;
                _options_check((op[2]=='-'),"-C-?");
                memset(&checksumSet,0,sizeof(checksumSet));//all set false
                _options_check(_toChecksumSet(psets,&checksumSet),"-C-?");
            } break;
#endif
#if (_IS_NEED_DIR_DIFF_PATCH)
            case 'n':{
                const char* pnum=op+3;
                _options_check((kMaxOpenFileNumber==_kNULL_SIZE)&&(op[2]=='-'),"-n");
                _options_check(kmg_to_size(pnum,strlen(pnum),&kMaxOpenFileNumber),"-n-?");
                _options_check((kMaxOpenFileNumber!=_kNULL_SIZE),"-n-?");
            } break;
#endif
            case 'i':{
                _options_check((isPrintFileInfo==_kNULL_VALUE)&&(op[2]=='n')&&(op[3]=='f')
                               &&(op[4]=='o')&&(op[5]=='\0'),"-info");
                isPrintFileInfo=hpatch_TRUE;
            } break;
            case '?':
            case 'h':{
                _options_check((isOutputHelp==_kNULL_VALUE)&&(op[2]=='\0'),"-h");
                isOutputHelp=hpatch_TRUE;
            } break;
            case 'v':{
                _options_check((isOutputVersion==_kNULL_VALUE)&&(op[2]=='\0'),"-v");
                isOutputVersion=hpatch_TRUE;
            } break;
            default: {
                _options_check(hpatch_FALSE,"-?");
            } break;
        }//switch
    }
    
    if (isOutputHelp==_kNULL_VALUE)
        isOutputHelp=hpatch_FALSE;
    if (isOutputVersion==_kNULL_VALUE)
        isOutputVersion=hpatch_FALSE;
    if (isForceOverwrite==_kNULL_VALUE)
        isForceOverwrite=hpatch_FALSE;
    if (isPrintFileInfo==_kNULL_VALUE)
        isPrintFileInfo=hpatch_FALSE;
    if (isRunSFX==_kNULL_VALUE)
        isRunSFX=hpatch_FALSE;
#if (_IS_NEED_SFX)
    if ((argc<=1)&&(!isRunSFX)){
        hpatch_StreamPos_t _diffDataOffert=0;
        hpatch_StreamPos_t _diffDataSize=0;
        if (getDiffDataOffertInSfx(&_diffDataOffert,&_diffDataSize)){//autoExtractSFX
            isForceOverwrite=hpatch_TRUE;
            isRunSFX=hpatch_TRUE;
        }
    }
#endif
    if ((argc<=1)&&(!isRunSFX)){
        printUsage();
        return HPATCH_OPTIONS_ERROR;
    }
    if (isOutputHelp||isOutputVersion){
        if (isOutputHelp)
            printUsage();//with version
        else
            printVersion();
#if (_IS_NEED_SFX)
        if ((arg_values_size==0)&&(!isRunSFX)&&(out_SFX==0)&&(selfExecuteFile==0))
#else
        if (arg_values_size==0)
#endif
            return 0; //ok
    }
    if (isPrintFileInfo){
        return hpatch_printFilesInfos(arg_values_size,arg_values);
    }
#if (_IS_NEED_DIR_DIFF_PATCH)
    if (kMaxOpenFileNumber==_kNULL_SIZE)
        kMaxOpenFileNumber=kMaxOpenFileNumber_default_patch;
    if (kMaxOpenFileNumber<kMaxOpenFileNumber_default_min)
        kMaxOpenFileNumber=kMaxOpenFileNumber_default_min;
#endif
#if (_IS_NEED_VCDIFF)
    vcpatch_isChecksum=checksumSet.isCheck_newRefData;
#endif
    
    if (isLoadOldAll==_kNULL_VALUE){
        isLoadOldAll=hpatch_FALSE;
        patchCacheSize=kPatchCacheSize_default;
    }
    if (isOldPathInputEmpty==_kNULL_VALUE)
        isOldPathInputEmpty=hpatch_FALSE;
    
#if (_IS_NEED_SFX)
    if ((out_SFX!=0)||(selfExecuteFile!=0)){ //create SFX
        _options_check(out_SFX!=0,"-X#?");
        _options_check(!isRunSFX, "-X");
        _options_check(arg_values_size==1,"-X# input count");
        {
            const char* diffFileName =arg_values[0];
            if (selfExecuteFile==0) selfExecuteFile=argv[0];
            if (!isForceOverwrite){
                hpatch_TPathType   outPathType;
                _return_check(hpatch_getPathStat(out_SFX,&outPathType,0),
                              HPATCH_PATHTYPE_ERROR,"get outSelfExtractArchive type");
                _return_check(outPathType==kPathType_notExist,
                              HPATCH_PATHTYPE_ERROR,"outSelfExtractArchive already exists, overwrite");
            }
            return createSfx(selfExecuteFile,diffFileName,out_SFX);
        }
    }else if (isRunSFX){//patch run as SFX mode
        _options_check((arg_values_size==2)||(arg_values_size==0),"-X input count");
    }else
#endif
    {//patch default mode
        _options_check(arg_values_size==3,"input count");
    }
    {//patch
        const char* oldPath     =0;
        const char* diffFileName=0;
        const char* outNewPath  =0;
#if (_IS_NEED_SFX)
        hpatch_BOOL isSfxDefaultPath=hpatch_FALSE;
        const char* kSFX_emptyPath="";
#endif
#if (_IS_NEED_DIR_DIFF_PATCH)
        TDirDiffInfo dirDiffInfo;
        hpatch_BOOL  isOutDir;
#endif
        hpatch_BOOL  isSamePath;
        hpatch_StreamPos_t diffDataOffert=0;
        hpatch_StreamPos_t diffDataSize=0;
#if (_IS_NEED_SFX)
        if (isRunSFX){
            printf("run as SFX mode!\n");
            _return_check(getDiffDataOffertInSfx(&diffDataOffert,&diffDataSize),
                          HPATCH_RUN_SFX_NOTSFX_ERROR,"not found diff data in selfExecuteFile");
            diffFileName=argv[0];//selfExecuteFileName
            if (arg_values_size==0){//autoExtractSFX
                oldPath     =kSFX_emptyPath;
                outNewPath  =kSFX_curDefaultPath;
                isSfxDefaultPath=hpatch_TRUE;
            }else{
                oldPath     =arg_values[0];
                outNewPath  =arg_values[1];
            }
            //continue
        }else
#endif
        {
            oldPath     =arg_values[0];
            diffFileName=arg_values[1];
            outNewPath  =arg_values[2];
        }
        
        _return_check(!hpatch_getIsSamePath(oldPath,diffFileName),
                      HPATCH_PATHTYPE_ERROR,"oldPath diffFile same path");
        _return_check(!hpatch_getIsSamePath(outNewPath,diffFileName),
                      HPATCH_PATHTYPE_ERROR,"outNewPath diffFile same path");
        if (!isForceOverwrite){
            hpatch_TPathType   outNewPathType;
            _return_check(hpatch_getPathStat(outNewPath,&outNewPathType,0),
                          HPATCH_PATHTYPE_ERROR,"get outNewPath type");
            _return_check(outNewPathType==kPathType_notExist,
                          HPATCH_PATHTYPE_ERROR,"outNewPath already exists, overwrite");
        }
#if (_IS_NEED_DIR_DIFF_PATCH)
        _return_check(getDirDiffInfoByFile(&dirDiffInfo,diffFileName,diffDataOffert,diffDataSize),
                      HPATCH_OPENREAD_ERROR,"input diffFile open read");
        isOutDir=(dirDiffInfo.isDirDiff)&&(dirDiffInfo.newPathIsDir);
        #if (_IS_NEED_SFX)
            if (isSfxDefaultPath && (dirDiffInfo.isDirDiff&&dirDiffInfo.oldPathIsDir))
                oldPath=kSFX_curDefaultPath;
        #endif
#endif
        isSamePath=hpatch_getIsSamePath(oldPath,outNewPath);
        if (isSamePath)
            _return_check(isForceOverwrite,HPATCH_PATHTYPE_ERROR,"oldPath outNewPath same path, overwrite");
        if (!isSamePath){ // out new file or new dir
#if (_IS_NEED_DIR_DIFF_PATCH)
            if (dirDiffInfo.isDirDiff){
                return hpatch_dir(oldPath,diffFileName,outNewPath,isLoadOldAll,patchCacheSize,kMaxOpenFileNumber,
                                  &checksumSet,&defaultPatchDirlistener,diffDataOffert,diffDataSize);
            }else
#endif
            {
                return hpatch(oldPath,diffFileName,outNewPath,isLoadOldAll,
                              patchCacheSize,diffDataOffert,diffDataSize,vcpatch_isChecksum,hpatch_TRUE);
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
                                  kMaxOpenFileNumber,&checksumSet,&defaultPatchDirlistener,
                                  diffDataOffert,diffDataSize);
            }else
#endif
            {
                result=hpatch(oldPath,diffFileName,newTempName,isLoadOldAll,
                              patchCacheSize,diffDataOffert,diffDataSize,vcpatch_isChecksum,hpatch_TRUE);
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
                    _log_info_utf8(newTempName); printf("\"\n");
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
            result=hpatch_dir(oldPath,diffFileName,newTempDir,isLoadOldAll,patchCacheSize,kMaxOpenFileNumber,
                              &checksumSet,&tempDirPatchListener,diffDataOffert,diffDataSize);
            if (result==HPATCH_SUCCESS){
                printf("all in outNewPath temp directory moved to oldDirectory!\n");
            }else if(!hpatch_isPathNotExist(newTempDir)){
                printf("WARNING: not remove temp directory \"");
                _log_info_utf8(newTempDir); printf("\"\n");
            }
            return result;
        }
#endif
    }
}
#endif //_IS_NEED_CMDLINE

#define  check_on_error(errorType) { \
    if (result==HPATCH_SUCCESS) result=errorType; if (!_isInClear){ goto clear; } }
#define  check(value,errorType,errorInfo) { \
    if (!(value)){ LOG_ERR(errorInfo " ERROR!\n"); check_on_error(errorType); } }

#if (_HPATCH_IS_USED_errno)
#   define check_ferr(fileError,errorType,errorInfo){ \
        if (fileError){ \
            result=HPATCH_SUCCESS; \
            if (ENOSPC==(fileError)){ \
                check(hpatch_FALSE,HPATCH_FILEWRITE_NO_SPACE_ERROR,errorInfo); \
            }else{ \
                check(hpatch_FALSE,errorType,errorInfo); } } }
#else
#   define check_ferr(fileError,errorType,errorInfo) { \
        if (fileError) { result=HPATCH_SUCCESS; check(hpatch_FALSE,errorType,errorInfo); } }
#endif

#define check_dec(decError) { \
        switch (decError){    \
            case hpatch_dec_ok:          break; \
            case hpatch_dec_mem_error:   check(0,HPATCH_DECOMPRESSER_MEM_ERROR,"decompressPlugin alloc memory"); \
            case hpatch_dec_open_error:  check(0,HPATCH_DECOMPRESSER_OPEN_ERROR,"decompressPlugin open"); \
            case hpatch_dec_close_error: check(0,HPATCH_DECOMPRESSER_CLOSE_ERROR,"decompressPlugin close"); \
            default: { assert(decError==hpatch_dec_error); \
                       check(0,HPATCH_DECOMPRESSER_DECOMPRESS_ERROR,"decompressPlugin decompress"); } break; \
        } }

#define _try_rt_dec(dec) { if (dec.is_can_open(compressType)) return &dec; }

static const hpatch_TDecompress* __find_decompressPlugin(const char* compressType){
#if ((defined(_CompressPlugin_ldef))&&_IS_NEED_decompressor_ldef_replace_zlib)
    _try_rt_dec(ldefDecompressPlugin);
#else
#  ifdef  _CompressPlugin_zlib
    _try_rt_dec(zlibDecompressPlugin);
#  endif
#endif
#ifdef  _CompressPlugin_bz2
    _try_rt_dec(bz2DecompressPlugin);
#endif
#ifdef  _CompressPlugin_lzma
    _try_rt_dec(lzmaDecompressPlugin);
#endif
#ifdef  _CompressPlugin_lzma2
    _try_rt_dec(lzma2DecompressPlugin);
#endif
#if (defined(_CompressPlugin_lz4) || (defined(_CompressPlugin_lz4hc)))
    _try_rt_dec(lz4DecompressPlugin);
#endif
#ifdef  _CompressPlugin_zstd
    _try_rt_dec(zstdDecompressPlugin);
#endif
#ifdef  _CompressPlugin_brotli
    _try_rt_dec(brotliDecompressPlugin);
#endif
#ifdef  _CompressPlugin_lzham
    _try_rt_dec(lzhamDecompressPlugin);
#endif
#ifdef  _CompressPlugin_tuz
    _try_rt_dec(tuzDecompressPlugin);
#endif
    return 0;
}


#if (_IS_NEED_VCDIFF)
static hpatch_BOOL getVcDiffDecompressPlugin(hpatch_TDecompress* out_decompressPlugin,
                                             const hpatch_VcDiffInfo* vcdInfo,char* out_compressType){
    const hpatch_TDecompress* decompressPlugin=0;

    out_compressType[0]='\0';
    memset(out_decompressPlugin,0,sizeof(*out_decompressPlugin));
    switch (vcdInfo->compressorID){
        case 0: return hpatch_TRUE;
#ifdef _CompressPlugin_7zXZ
        case kVcDiff_compressorID_7zXZ:{
            static const char* _7zXZCompressType="7zXZ";
            _init_CompressPlugin_7zXZ();
            if (vcdInfo->isHDiffzAppHead_a)
                decompressPlugin=&_7zXZDecompressPlugin_a;
            else
                decompressPlugin=&_7zXZDecompressPlugin;
            memcpy(out_compressType,_7zXZCompressType,strlen(_7zXZCompressType)+1);
        } break;
#endif
        default: return hpatch_FALSE; //now unsupport
    }
    if (decompressPlugin){
        *out_decompressPlugin=*decompressPlugin;
        out_decompressPlugin->decError=hpatch_dec_ok;
    }
    return hpatch_TRUE;
}
#endif

static hpatch_BOOL getDecompressPlugin(const hpatch_compressedDiffInfo* diffInfo,
                                       hpatch_TDecompress* out_decompressPlugin){
    const hpatch_TDecompress* decompressPlugin=0;
    memset(out_decompressPlugin,0,sizeof(*out_decompressPlugin));
    if (diffInfo->compressedCount>0){
        decompressPlugin=__find_decompressPlugin(diffInfo->compressType);
        if ((0==decompressPlugin)||(decompressPlugin->open==0)) return hpatch_FALSE; //error
    }
    if (decompressPlugin){
        *out_decompressPlugin=*decompressPlugin;
        out_decompressPlugin->decError=hpatch_dec_ok;
    }
    return hpatch_TRUE;
}

#if (_IS_NEED_DIR_DIFF_PATCH)
static hpatch_inline 
hpatch_BOOL _trySetChecksum(hpatch_TChecksum** out_checksumPlugin,const char* checksumType,
                            hpatch_TChecksum* testChecksumPlugin){
    assert(0==*out_checksumPlugin);
    if (0!=strcmp(checksumType,testChecksumPlugin->checksumType())) return hpatch_FALSE;
    *out_checksumPlugin=testChecksumPlugin;
    return hpatch_TRUE;
}
#define __setChecksum(_checksumPlugin) \
    if (_trySetChecksum(out_checksumPlugin,checksumType,_checksumPlugin)) return hpatch_TRUE;

static hpatch_BOOL findChecksum(hpatch_TChecksum** out_checksumPlugin,const char* checksumType){
    *out_checksumPlugin=0;
    if (strlen(checksumType)==0) return hpatch_TRUE;
#ifdef _ChecksumPlugin_fadler64
    __setChecksum(&fadler64ChecksumPlugin);
#endif
#ifdef _ChecksumPlugin_xxh128
    __setChecksum(&xxh128ChecksumPlugin);
#endif
#ifdef _ChecksumPlugin_xxh3
    __setChecksum(&xxh3ChecksumPlugin);
#endif
#ifdef _ChecksumPlugin_md5
    __setChecksum(&md5ChecksumPlugin);
#endif
#ifdef _ChecksumPlugin_crc32
    __setChecksum(&crc32ChecksumPlugin);
#endif
#ifdef _ChecksumPlugin_adler32
    __setChecksum(&adler32ChecksumPlugin);
#endif
#ifdef _ChecksumPlugin_adler64
    __setChecksum(&adler64ChecksumPlugin);
#endif
#ifdef _ChecksumPlugin_fadler32
    __setChecksum(&fadler32ChecksumPlugin);
#endif
#ifdef _ChecksumPlugin_fadler128
    __setChecksum(&fadler128ChecksumPlugin);
#endif
#ifdef _ChecksumPlugin_blake3
    __setChecksum(&blake3ChecksumPlugin);
#endif
    return hpatch_FALSE;
}
#endif


typedef struct _THDiffInfos{
    hpatch_BOOL                 isSingleCompressedDiff;
    hpatch_BOOL                 isBsDiff;
    hpatch_BOOL                 isVcDiff;
    hpatch_compressedDiffInfo   diffInfo;
#if (_IS_NEED_SINGLE_STREAM_DIFF)
    hpatch_singleCompressedDiffInfo sdiffInfo;
#endif
#if (_IS_NEED_BSDIFF)
    hpatch_BsDiffInfo           bsdiffInfo;
#endif
#if (_IS_NEED_VCDIFF)
    hpatch_VcDiffInfo           vcdiffInfo;
#endif
    hpatch_TDecompress          _decompressPlugin;
} _THDiffInfos;

#define _kUnavailableSize   hpatch_kNullStreamPos

static int _getHDiffInfos(_THDiffInfos* out_diffInfos,const hpatch_TFileStreamInput* diffData){
    int     result=HPATCH_SUCCESS;
    int     _isInClear=hpatch_FALSE;
    hpatch_TDecompress* decompressPlugin=&out_diffInfos->_decompressPlugin;
    hpatch_compressedDiffInfo* diffInfo=&out_diffInfos->diffInfo;
    if (getCompressedDiffInfo(diffInfo,&diffData->base)){
        check(diffInfo->oldDataSize!=_kUnavailableSize,HPATCH_HDIFFINFO_ERROR,"saved oldDataSize");
    }else{
#if (_IS_NEED_SINGLE_STREAM_DIFF)
        if (getSingleCompressedDiffInfo(&out_diffInfos->sdiffInfo,&diffData->base,0)){
            out_diffInfos->isSingleCompressedDiff=hpatch_TRUE;
            _singleDiffInfoToHDiffInfo(diffInfo,&out_diffInfos->sdiffInfo);
            check(diffInfo->oldDataSize!=_kUnavailableSize,HPATCH_HDIFFINFO_ERROR,"saved oldDataSize");
        }else
#endif
#if (_IS_NEED_BSDIFF)
        if (getBsDiffInfo(&out_diffInfos->bsdiffInfo,&diffData->base)){
            const char* bsCompressType="bz2";
            *decompressPlugin=_bz2DecompressPlugin_unsz;
            decompressPlugin->decError=hpatch_dec_ok;
            memcpy(diffInfo->compressType,bsCompressType,strlen(bsCompressType)+1);
            diffInfo->compressedCount=out_diffInfos->bsdiffInfo.isEndsleyBsdiff?1:3;
            diffInfo->newDataSize=out_diffInfos->bsdiffInfo.newDataSize;
            diffInfo->oldDataSize=_kUnavailableSize; //not saved oldDataSize
            out_diffInfos->isBsDiff=hpatch_TRUE;
        }else
#endif
#if (_IS_NEED_VCDIFF)
        if (getVcDiffInfo(&out_diffInfos->vcdiffInfo,&diffData->base,hpatch_TRUE)){
            diffInfo->newDataSize=out_diffInfos->vcdiffInfo.sumTargetWindowsSize;
            diffInfo->oldDataSize=_kUnavailableSize; //not saved oldDataSize
            out_diffInfos->isVcDiff=hpatch_TRUE;
            if (getVcDiffDecompressPlugin(decompressPlugin,&out_diffInfos->vcdiffInfo,out_diffInfos->diffInfo.compressType)){
                if (decompressPlugin->open) out_diffInfos->diffInfo.compressedCount=3;
                else diffInfo->compressedCount=0;
            }else{
                LOG_ERR("can not decompress VCDIFF compressID \"%d\" data ERROR!\n",out_diffInfos->vcdiffInfo.compressorID);
                check_on_error(HPATCH_COMPRESSTYPE_ERROR);
            }
        }else
#endif
        check(hpatch_FALSE,HPATCH_HDIFFINFO_ERROR,"is hdiff file? get diffInfo");
    }
    if (decompressPlugin->open==0){
        if (getDecompressPlugin(diffInfo,decompressPlugin)){
        }else{
            LOG_ERR("can not decompress \"%s\" data ERROR!\n",out_diffInfos->diffInfo.compressType);
            check_on_error(HPATCH_COMPRESSTYPE_ERROR);
        }
    }
clear:
    _isInClear=hpatch_TRUE;
    check(!diffData->fileError,HPATCH_FILEREAD_ERROR,"read file");
    return result;
}

static void _printHDiffInfos(const _THDiffInfos* diffInfos,hpatch_BOOL isInDirDiff){
    const   hpatch_compressedDiffInfo* diffInfo=&diffInfos->diffInfo;
    {
        const char* typeTag="HDiff";
#if (_IS_NEED_SINGLE_STREAM_DIFF)
        if (diffInfos->isSingleCompressedDiff) typeTag="SHDiff";
#endif
#if (_IS_NEED_BSDIFF)
        if (diffInfos->isBsDiff){
            if (diffInfos->bsdiffInfo.isEndsleyBsdiff)
                typeTag="BSDiff (endsley/bsdiff)";
            else
                typeTag="BSDiff";
        }
#endif
#if (_IS_NEED_VCDIFF)
        if (diffInfos->isVcDiff){
            if (diffInfos->vcdiffInfo.isHDiffzAppHead_a)
                typeTag="VCDiff (with hdiffz tag)";
            else if (diffInfos->vcdiffInfo.isGoogleVersion)
                typeTag="VCDiff (with google tag)";
            else
                typeTag="VCDiff";
        } 
#endif
        printf("       diffDataType: %s %s\n",typeTag,isInDirDiff?"(in DirHDiff)":"");
    }
    if (diffInfo->oldDataSize==_kUnavailableSize)
        printf("  saved oldDataSize: unavailable\n");
    else
        printf("  saved oldDataSize: %" PRIu64 "\n",diffInfo->oldDataSize);
        printf("  saved newDataSize: %" PRIu64 "\n",diffInfo->newDataSize);
        printf("       compressType: \"%s\" (need decompress %d)\n",diffInfo->compressType,diffInfo->compressedCount);
#if (_IS_NEED_SINGLE_STREAM_DIFF)
    if (diffInfos->isSingleCompressedDiff)
        printf("        stepMemSize: %" PRIu64 "\n",diffInfos->sdiffInfo.stepMemSize);
#endif
#if (_IS_NEED_VCDIFF)
    if (diffInfos->isVcDiff){
        printf("      maxSrcWindows: %" PRIu64 "\n",diffInfos->vcdiffInfo.maxSrcWindowsSize);
        printf("   maxTargetWindows: %" PRIu64 "\n",diffInfos->vcdiffInfo.maxTargetWindowsSize);
    }
#endif
}


#if (_IS_NEED_CMDLINE)
#if (_IS_NEED_DIR_DIFF_PATCH)
#include "dirDiffPatch/dir_patch/dir_patch_private.h"

static void _printDirDiffInfos(const TDirDiffInfo* dirDiffInfo,const _TDirDiffHead* head){
    if (!dirDiffInfo->isDirDiff) return;
    printf("         diffDataType: DirHDiff\n");
    printf("         checksumType: \"%s\"\n",dirDiffInfo->checksumType);
    printf("         oldPathIsDir: %s\n",dirDiffInfo->oldPathIsDir?"true":"false");
    printf("         newPathIsDir: %s\n",dirDiffInfo->newPathIsDir?"true":"false");
    printf("       new path count: %" PRIu64 " (fileCount:%" PRIu64 ")\n",
            (hpatch_StreamPos_t)head->newPathCount,(hpatch_StreamPos_t)(head->sameFilePairCount+head->newRefFileCount));
    printf("  copy from old count: %" PRIu64 " (copySize: %" PRIu64 ")\n",
            (hpatch_StreamPos_t)head->sameFilePairCount,head->sameFileSize);
    printf("   ref old file count: %" PRIu64 " (oldRefSize: %" PRIu64 ")\n",
            (hpatch_StreamPos_t)head->oldRefFileCount,dirDiffInfo->hdiffInfo.oldDataSize);
    printf("   ref new file count: %" PRIu64 " (newRefSize: %" PRIu64 ")\n",
            (hpatch_StreamPos_t)head->newRefFileCount,dirDiffInfo->hdiffInfo.newDataSize);
    printf("           oldSumSize: %" PRIu64 "\n",dirDiffInfo->hdiffInfo.oldDataSize+head->sameFileSize);
    printf("           newSumSize: %" PRIu64 "\n",dirDiffInfo->hdiffInfo.newDataSize+head->sameFileSize);
    
    {//hdiffInfo
        _THDiffInfos diffInfos={0};
        diffInfos.diffInfo=dirDiffInfo->hdiffInfo;
    #if (_IS_NEED_SINGLE_STREAM_DIFF)
        if (dirDiffInfo->isSingleCompressedDiff){
            diffInfos.isSingleCompressedDiff=dirDiffInfo->isSingleCompressedDiff;
            diffInfos.sdiffInfo=dirDiffInfo->sdiffInfo;
        }
    #endif
        _printHDiffInfos(&diffInfos,hpatch_TRUE);
    }
}

#endif

static int _printFileInfos(const char* fileName,const char* fileTag){
    int     result=HPATCH_SUCCESS;
    int     _isInClear=hpatch_FALSE;
    hpatch_BOOL  isDirDiff=hpatch_FALSE;
    hpatch_TFileStreamInput     diffData;
    hpatch_TFileStreamInput_init(&diffData);
    {//open
        printf("%sName: \"",fileTag); _log_info_utf8(fileName); printf("\"\n");
        check(hpatch_TFileStreamInput_open(&diffData,fileName),
              HPATCH_OPENREAD_ERROR,"open file for read");
        printf("%sSize: %" PRIu64 "\n",fileTag,diffData.base.streamSize);
    }
#if (_IS_NEED_DIR_DIFF_PATCH)
    {
        TDirDiffInfo    dirDiffInfo={0};
        _TDirDiffHead   dirHead={0};
        if (read_dirdiff_head(&dirDiffInfo,&dirHead,&diffData.base))
            isDirDiff=dirDiffInfo.isDirDiff;
        check(!diffData.fileError,HPATCH_FILEREAD_ERROR,"read file");
        if (isDirDiff)
            _printDirDiffInfos(&dirDiffInfo,&dirHead);
    }
#endif
    if (!isDirDiff){
        _THDiffInfos diffInfos={0};
        result=_getHDiffInfos(&diffInfos,&diffData);
        if ((result!=HPATCH_SUCCESS)&&(result!=HPATCH_COMPRESSTYPE_ERROR))
            check_on_error(result);
        _printHDiffInfos(&diffInfos,isDirDiff);
    }

clear:
    _isInClear=hpatch_TRUE;
    check(hpatch_TFileStreamInput_close(&diffData),HPATCH_FILECLOSE_ERROR,"file close");
    return result;
}

int hpatch_printFilesInfos(int fileCount,const char* fileNames[]){
    int i;
    int result=0;
    for (i=0;i<fileCount;++i){
        if (i>0) printf("\n");
        int ret=_printFileInfos(fileNames[i],"file");
        if (result==0) result=ret;
    }
    if (fileCount==0)
        printf("please input diffFile for print it's infos\n");
    return result;
}
#endif //_IS_NEED_CMDLINE


#define _free_mem(p) { if (p) { free(p); p=0; } }

static TByte* getPatchMemCache(hpatch_BOOL isLoadOldAll,size_t patchCacheSize,size_t mustAppendMemSize,
                               hpatch_StreamPos_t oldDataSize,size_t* out_memCacheSize){
    TByte* temp_cache=0;
    size_t temp_cache_size;
    {
        hpatch_StreamPos_t limitCacheSize;
        const hpatch_StreamPos_t bestMaxCacheSize=oldDataSize+kPatchCacheSize_bestmin;
        if (isLoadOldAll){
            limitCacheSize=bestMaxCacheSize;
        }else{
            limitCacheSize=(patchCacheSize<kPatchCacheSize_min)?kPatchCacheSize_min:patchCacheSize;
            limitCacheSize=(limitCacheSize<bestMaxCacheSize)?limitCacheSize:bestMaxCacheSize;
        }
        if (limitCacheSize>(size_t)(_kNULL_SIZE-mustAppendMemSize))//too large
            limitCacheSize=(size_t)(_kNULL_SIZE-mustAppendMemSize);
        temp_cache_size=(size_t)limitCacheSize;
    }
    while (temp_cache_size>=kPatchCacheSize_min){
        temp_cache=(TByte*)malloc(mustAppendMemSize+temp_cache_size);
        if (temp_cache) break;
        temp_cache_size>>=1;
    }
    *out_memCacheSize=(temp_cache)?(mustAppendMemSize+temp_cache_size):0;
    return temp_cache;
}

int hpatch(const char* oldFileName,const char* diffFileName,const char* outNewFileName,
           hpatch_BOOL isLoadOldAll,size_t patchCacheSize,hpatch_StreamPos_t diffDataOffert,
           hpatch_StreamPos_t diffDataSize,hpatch_BOOL vcpatch_isChecksum,hpatch_BOOL vcpatch_isInMem){
    int     result=HPATCH_SUCCESS;
    int     _isInClear=hpatch_FALSE;
    double  time0=clock_s();
    _THDiffInfos diffInfos={0};
    hpatch_TDecompress*  decompressPlugin=&diffInfos._decompressPlugin;
    hpatch_TFileStreamOutput    newData;
    hpatch_TFileStreamInput     diffData;
    hpatch_TFileStreamInput     oldData;
    hpatch_TStreamInput* poldData=&oldData.base;
    TByte*               temp_cache=0;
    size_t               temp_cache_size;
    int                  patch_result=HPATCH_SUCCESS;
    hpatch_TFileStreamInput_init(&oldData);
    hpatch_TFileStreamInput_init(&diffData);
    hpatch_TFileStreamOutput_init(&newData);
    {//open
        printf(    "old : \""); if (oldFileName) _log_info_utf8(oldFileName);
        printf("\"\ndiff: \""); _log_info_utf8(diffFileName);
        printf("\"\nout : \""); _log_info_utf8(outNewFileName);
        printf("\"\n");
        if ((0==oldFileName)||(0==strlen(oldFileName))){
            mem_as_hStreamInput(&oldData.base,0,0);
        }else{
            check(hpatch_TFileStreamInput_open(&oldData,oldFileName),
                  HPATCH_OPENREAD_ERROR,"open oldFile for read");
        }
        check(hpatch_TFileStreamInput_open(&diffData,diffFileName),
              HPATCH_OPENREAD_ERROR,"open diffFile for read");
#if (_IS_NEED_SFX)
        if (diffDataOffert>0){ //run sfx
            check(hpatch_TFileStreamInput_setOffset(&diffData,diffDataOffert),
                  HPATCH_RUN_SFX_DIFFOFFSERT_ERROR,"readed sfx diffFile offset");
            check(diffData.base.streamSize>=diffDataSize,
                  HPATCH_RUN_SFX_DIFFOFFSERT_ERROR,"readed sfx diffFile size");
            diffData.base.streamSize=diffDataSize;
        }
#endif
    }
    printf("  input oldDataSize: %" PRIu64 "\n",poldData->streamSize);
    printf("       diffDataSize: %" PRIu64 "\n",diffData.base.streamSize);

    {//info
        int ret=_getHDiffInfos(&diffInfos,&diffData);
#if (_IS_NEED_PRINT_LOG)
        if ((ret!=HPATCH_SUCCESS)&&(ret!=HPATCH_COMPRESSTYPE_ERROR))
            check_on_error(ret);
        _printHDiffInfos(&diffInfos,hpatch_FALSE);
        printf("\n");
#endif
        if (ret!=HPATCH_SUCCESS)
            check_on_error(ret);
    }
    if (decompressPlugin->open==0) decompressPlugin=0;
    if ((poldData->streamSize!=diffInfos.diffInfo.oldDataSize)&&(diffInfos.diffInfo.oldDataSize!=_kUnavailableSize)){
        LOG_ERR("input oldFile oldDataSize %" PRIu64 " != diffFile saved oldDataSize %" PRIu64 " ERROR!\n",
                poldData->streamSize,diffInfos.diffInfo.oldDataSize);
        check_on_error(HPATCH_FILEDATA_ERROR);
    }

    check(hpatch_TFileStreamOutput_open(&newData, outNewFileName,diffInfos.diffInfo.newDataSize),
          HPATCH_OPENWRITE_ERROR,"open out newFile for write");
#if (_IS_NEED_VCDIFF)
    if (diffInfos.isVcDiff) hpatch_TFileStreamOutput_setRandomOut(&newData,hpatch_TRUE);
#endif
    {
        hpatch_StreamPos_t maxWindowSize=poldData->streamSize;
        hpatch_size_t      mustAppendMemSize=0;
#if (_IS_NEED_VCDIFF)
        if (diffInfos.isVcDiff){
            hpatch_StreamPos_t maxSrcTargetSize=diffInfos.vcdiffInfo.maxSrcWindowsSize+diffInfos.vcdiffInfo.maxTargetWindowsSize;
            maxWindowSize=((!diffInfos.vcdiffInfo.isHDiffzAppHead_a)&&(maxSrcTargetSize<=maxWindowSize+64*(1<<20)))?
                    maxSrcTargetSize:diffInfos.vcdiffInfo.maxSrcWindowsSize;
            if ((vcpatch_isInMem)&&(!diffInfos.vcdiffInfo.isHDiffzAppHead_a))
                isLoadOldAll=hpatch_TRUE;
        }
#endif
#if (_IS_NEED_SINGLE_STREAM_DIFF)
        if (diffInfos.isSingleCompressedDiff){
            check(diffInfos.sdiffInfo.stepMemSize==(size_t)diffInfos.sdiffInfo.stepMemSize,HPATCH_MEM_ERROR,"stepMemSize too large");
            mustAppendMemSize=(size_t)diffInfos.sdiffInfo.stepMemSize;
        }
#endif
        temp_cache=getPatchMemCache(isLoadOldAll,patchCacheSize,mustAppendMemSize,maxWindowSize, &temp_cache_size);
    }
    check(temp_cache,HPATCH_MEM_ERROR,"alloc cache memory");
#if (_IS_NEED_SINGLE_STREAM_DIFF)
    if (diffInfos.isSingleCompressedDiff){
        check(temp_cache_size>=diffInfos.sdiffInfo.stepMemSize+hpatch_kStreamCacheSize*3,HPATCH_MEM_ERROR,"alloc cache memory");
        if (!patch_single_compressed_diff(&newData.base,poldData,&diffData.base,diffInfos.sdiffInfo.diffDataPos,
                                          diffInfos.sdiffInfo.uncompressedSize,diffInfos.sdiffInfo.compressedSize,decompressPlugin,
                                          diffInfos.sdiffInfo.coverCount,(size_t)diffInfos.sdiffInfo.stepMemSize,
                                          temp_cache,temp_cache+temp_cache_size,0))
            patch_result=HPATCH_SPATCH_ERROR;
    }else
#endif
#if (_IS_NEED_BSDIFF)
    if (diffInfos.isBsDiff){
        if (!bspatch_with_cache(&newData.base,poldData,&diffData.base,decompressPlugin,
                                temp_cache,temp_cache+temp_cache_size))
            patch_result=HPATCH_BSPATCH_ERROR;
    }else
#endif
#if (_IS_NEED_VCDIFF)
    if (diffInfos.isVcDiff){
        if (!vcpatch_with_cache(&newData.base,poldData,&diffData.base,decompressPlugin,
                                vcpatch_isChecksum,temp_cache,temp_cache+temp_cache_size))
            patch_result=HPATCH_VCPATCH_ERROR;
    }else
#endif
    {
        if (!patch_decompress_with_cache(&newData.base,poldData,&diffData.base,decompressPlugin,
                                         temp_cache,temp_cache+temp_cache_size))
            patch_result=HPATCH_HPATCH_ERROR;
    }
    if (patch_result!=HPATCH_SUCCESS){
        check(!oldData.fileError,HPATCH_FILEREAD_ERROR,"oldFile read");
        check(!diffData.fileError,HPATCH_FILEREAD_ERROR,"diffFile read");
        check_ferr(newData.fileError,HPATCH_FILEWRITE_ERROR,"out newFile write");
        if (decompressPlugin) check_dec(decompressPlugin->decError);
        check(hpatch_FALSE,patch_result,"patch run");
    }
    if (newData.out_length!=newData.base.streamSize){
        LOG_ERR("out newFile dataSize %" PRIu64 " != diffFile saved newDataSize %" PRIu64 " ERROR!\n",
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
               TDirPatchChecksumSet* checksumSet,IHPatchDirListener* hlistener,
               hpatch_StreamPos_t diffDataOffert,hpatch_StreamPos_t diffDataSize){
    int     result=HPATCH_SUCCESS;
    int     _isInClear=hpatch_FALSE;
    double  time0=clock_s();
    hpatch_TFileStreamInput     diffData;
    TDirPatcher          dirPatcher;
    const TDirDiffInfo*  dirDiffInfo=0;
    TByte*               p_temp_mem=0;
    TByte*               temp_cache=0;
    size_t               temp_cache_size;
    size_t               min_temp_cache_size=kPatchCacheSize_min;
    hpatch_TDecompress   _decompressPlugin={0};
    const hpatch_TStreamInput*  oldStream=0;
    const hpatch_TStreamOutput* newStream=0;
    hpatch_TFileStreamInput_init(&diffData);
    TDirPatcher_init(&dirPatcher);
    if (oldPath) assert(0!=strcmp(oldPath,outNewPath));
    {//dir diff info
        hpatch_BOOL  rt;
        hpatch_TPathType    oldType;
        if ((0==oldPath)||(0==strlen(oldPath))){
            oldType=kPathType_file; //as empty file
        }else{
            check(hpatch_getPathStat(oldPath,&oldType,0),HPATCH_PATHTYPE_ERROR,"get oldPath type");
            check((oldType!=kPathType_notExist),HPATCH_PATHTYPE_ERROR,"oldPath not exist");
        }
        check(hpatch_TFileStreamInput_open(&diffData,diffFileName),HPATCH_OPENREAD_ERROR,"open diffFile for read");
#if (_IS_NEED_SFX)
        if (diffDataOffert>0){ //run sfx
            check(hpatch_TFileStreamInput_setOffset(&diffData,diffDataOffert),
                  HPATCH_RUN_SFX_DIFFOFFSERT_ERROR,"readed sfx diffFile offset");
            check(diffData.base.streamSize>=diffDataSize,
                  HPATCH_RUN_SFX_DIFFOFFSERT_ERROR,"readed sfx diffFile size");
            diffData.base.streamSize=diffDataSize;
        }
#endif
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
        if (oldPath) _log_info_utf8(oldPath);
        if ((oldPath)&&dirDiffInfo->oldPathIsDir&&(!hpatch_getIsDirName(oldPath))) 
            printf("%c",kPatch_dirSeparator);
        printf(                                             "\"\ndiffFile: \"");
        _log_info_utf8(diffFileName);
        printf(dirDiffInfo->newPathIsDir?"\"\nout  dir: \"":"\"\nout file: \"");
        _log_info_utf8(outNewPath);
        if (dirDiffInfo->newPathIsDir&&(!hpatch_getIsDirName(outNewPath))) printf("%c",kPatch_dirSeparator);
        printf("\"\n");
        printf("         diffDataSize: %" PRIu64 "\n",diffData.base.streamSize);
#if (_IS_NEED_PRINT_LOG)
        _printDirDiffInfos(dirDiffInfo,&dirPatcher.dirDiffHead);
        printf("\n");
#endif
    }
    {//decompressPlugin
        hpatch_TDecompress* decompressPlugin=&_decompressPlugin;
        hpatch_compressedDiffInfo hdiffInfo;
        hdiffInfo=dirDiffInfo->hdiffInfo;
        hdiffInfo.compressedCount+=(dirDiffInfo->dirDataIsCompressed)?1:0;
        if(!getDecompressPlugin(&hdiffInfo,decompressPlugin)){
            LOG_ERR("can not decompress \"%s\" data ERROR!\n",hdiffInfo.compressType);
            check_on_error(HPATCH_COMPRESSTYPE_ERROR);
        }
        if (decompressPlugin->open==0) decompressPlugin=0;
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
                printf("  WARNING: no checksum saved in diffFile,can not do checksum\n");
            }else{
                if (!findChecksum(&checksumSet->checksumPlugin,dirDiffInfo->checksumType)){
                    LOG_ERR("not found checksumType \"%s\" ERROR!\n",dirDiffInfo->checksumType);
                    check_on_error(DIRPATCH_CHECKSUMTYPE_ERROR);
                }
                printf("hpatchz run with checksum plugin: \"%s\" (checksumSets:%s%s%s%s)\n",dirDiffInfo->checksumType,
                       checksumSet->isCheck_dirDiffData?" diff":"",checksumSet->isCheck_oldRefData?" old":"",
                       checksumSet->isCheck_newRefData?" new":"",checksumSet->isCheck_copyFileData?" copy":"");
                if (!TDirPatcher_checksum(&dirPatcher,checksumSet)){
                    check(!TDirPatcher_isDiffDataChecksumError(&dirPatcher),
                          DIRPATCH_CHECKSUM_DIFFDATA_ERROR,"diffFile checksum");
                    check(hpatch_FALSE,DIRPATCH_CHECKSUMSET_ERROR,"diffFile set checksum");
                }
            }
        }
    }

    {//mem cache
        size_t mustAppendMemSize=0;
#if (_IS_NEED_SINGLE_STREAM_DIFF)
        if (dirDiffInfo->isSingleCompressedDiff){
            mustAppendMemSize+=(size_t)dirDiffInfo->sdiffInfo.stepMemSize;
            min_temp_cache_size+=(size_t)dirDiffInfo->sdiffInfo.stepMemSize;
        }
#endif
        p_temp_mem=getPatchMemCache(isLoadOldAll,patchCacheSize,mustAppendMemSize,
                                    dirDiffInfo->hdiffInfo.oldDataSize, &temp_cache_size);
        check(p_temp_mem,HPATCH_MEM_ERROR,"alloc cache memory");
        temp_cache=p_temp_mem;
    }
    {//old data
        if (temp_cache_size>=dirDiffInfo->hdiffInfo.oldDataSize+min_temp_cache_size){
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
        check_dec(_decompressPlugin.decError);
        check(!TDirPatcher_isOldRefDataChecksumError(&dirPatcher),
              DIRPATCH_CHECKSUM_OLDDATA_ERROR,"oldFile checksum");
        check(!TDirPatcher_isCopyDataChecksumError(&dirPatcher),
              DIRPATCH_CHECKSUM_COPYDATA_ERROR,"copyOldFile checksum");
        check(!TDirPatcher_isNewRefDataChecksumError(&dirPatcher),
              DIRPATCH_CHECKSUM_NEWDATA_ERROR,"newFile checksum");
        check(hpatch_FALSE,DIRPATCH_PATCH_ERROR,"dir patch run");
    }
clear:
    _isInClear=hpatch_TRUE;
    check(hlistener->patchFinish(hlistener,result==HPATCH_SUCCESS),DIRPATCH_PATCHFINISH_ERROR,"dir patch finish");
    check(TDirPatcher_closeNewDirStream(&dirPatcher),DIRPATCH_CLOSE_NEWPATH_ERROR,"newPath close");
    check(TDirPatcher_closeOldRefStream(&dirPatcher),DIRPATCH_CLOSE_OLDPATH_ERROR,"oldPath close");
    TDirPatcher_close(&dirPatcher);
    check_ferr(dirPatcher.fileError,DIRPATCH_PATCH_FILE_ERROR,"dir patch file");
    check_ferr(hlistener->fileError,DIRPATCH_PATCH_FILE_ERROR,"dir patch file");
    check(hpatch_TFileStreamInput_close(&diffData),HPATCH_FILECLOSE_ERROR,"diffFile close");
    _free_mem(p_temp_mem);
    printf("\nhpatchz dir patch time: %.3f s\n",(clock_s()-time0));
    return result;
}
#endif

#if (_IS_NEED_SFX)
#define _sfx_guid_size      16
#define _sfx_guid_node_size (_sfx_guid_size+sizeof(hpatch_uint32_t)+sizeof(hpatch_uint64_t))
//_sfx_guid_node:{fd6c3a9d-1498-4a5d-a9d5-7303a82fd08e-ffffffff-ffffffffffffffff}
TByte _sfx_guid_node[_sfx_guid_node_size]={
    0xfd,0x6c,0x3a,0x9d,
    0x14,0x98,
    0x4a,0x5d,
    0xa9,0xd5,
    0x73,0x03,0xa8,0x2f,0xd0,0x8e,
    0xff,0xff,0xff,0xff,  // saved diffDataOffert , selfExecute fileSize < 4GB
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}; // diffDataSize

static TByte* _search(TByte* src,TByte* src_end,const TByte* sub,const TByte* sub_end){
    size_t sub_len=(size_t)(sub_end-sub);
    TByte* search_last=src_end-sub_len;
    TByte  c0;
    if (sub_len==0) return src;
    if ((size_t)(src_end-src)<sub_len)  return src_end;
    c0=*sub;
    for (;src<=search_last;++src) {
        if (c0!=*src){
            continue;
        }else{
            hpatch_BOOL is_eq=hpatch_TRUE;
            size_t i;
            for (i=1; i<sub_len;++i) {
                if (sub[i]!=src[i]){
                    is_eq=hpatch_FALSE;
                    break;
                }
            }
            if (!is_eq)
                continue;
            else
                return src;
        }
    }
    return src_end;
}

static int createSfx_notCheckDiffFile_byStream(const hpatch_TStreamInput*  selfExecute,
                                               const hpatch_TStreamInput*  diffData,
                                               const hpatch_TStreamOutput* out_sfxData){
    int     result=HPATCH_SUCCESS;
    int     _isInClear=hpatch_FALSE;
    TByte*  buf=0;
    TByte*  pmem=0;
    hpatch_StreamPos_t  writePos=0;
    hpatch_BOOL         isFoundGuid=hpatch_FALSE;
    const hpatch_StreamPos_t  diffDataOffert=selfExecute->streamSize;
    const hpatch_StreamPos_t  diffDataSize=diffData->streamSize;
    {//mem
        pmem=(TByte*)malloc(_sfx_guid_node_size+hpatch_kFileIOBufBetterSize);
        check(pmem!=0,HPATCH_MEM_ERROR,"createSfx() malloc");
        memset(pmem,0,_sfx_guid_node_size);
        buf=pmem+_sfx_guid_node_size; // [ _sfx_guid_node_size | hpatch_kFileIOBufBetterSize ]
    }
    {//copy exe ,find guid pos and write diffDataOffset
        const hpatch_uint32_t dataSize=(hpatch_uint32_t)(selfExecute->streamSize);
        hpatch_StreamPos_t readPos=0;
        check(dataSize==selfExecute->streamSize,
              HPATCH_CREATE_SFX_SFXTYPE_ERROR,"createSfx() selfExecute fileSize");
        while (readPos < dataSize) {
            TByte* wbuf=0;
            const TByte* wbuf_end=0;
            TByte* rbuf_end=0;
            size_t readLen=hpatch_kFileIOBufBetterSize;
            if (readLen+readPos > dataSize)
                readLen=(size_t)(dataSize - readPos);
            rbuf_end=buf+readLen;
            check(selfExecute->read(selfExecute,readPos,buf,rbuf_end),
                  HPATCH_FILEREAD_ERROR,"createSfx() read selfExecute");
            wbuf=(readPos!=0)?pmem:buf;
            if (!isFoundGuid){ //find in [wbuf..rbuf_end)
                TByte*  pos=_search(wbuf,rbuf_end,_sfx_guid_node,_sfx_guid_node+_sfx_guid_node_size);
                if (pos!=rbuf_end){
                    size_t i;
                    isFoundGuid=hpatch_TRUE;
                    for (i=0; i<sizeof(hpatch_uint32_t); ++i){
                        size_t di=_sfx_guid_size+i;
                        assert(pos[di]==0xff);
                        pos[di]=(TByte)(diffDataOffert>>(8*i));
                    }
                    for (i=0; i<sizeof(hpatch_uint64_t); ++i){
                        size_t di=_sfx_guid_size+sizeof(hpatch_uint32_t)+i;
                        assert(pos[di]==0xff);
                        pos[di]=(TByte)(diffDataSize>>(8*i));
                    }
                }
            }
            wbuf_end=(readPos+readLen<dataSize)?(rbuf_end-_sfx_guid_node_size):rbuf_end;
            check(out_sfxData->write(out_sfxData,writePos,wbuf,wbuf_end),
                  HPATCH_FILEWRITE_ERROR,"createSfx() write sfxData");
            memcpy(pmem,rbuf_end-_sfx_guid_node_size,_sfx_guid_node_size);
            readPos+=readLen;
            writePos+=(size_t)(wbuf_end-wbuf);
        }
    }
    assert(writePos==diffDataOffert);
    check(isFoundGuid,HPATCH_CREATE_SFX_SFXTYPE_ERROR,"createSfx() selfExecute type");
    {//copy diffData
        const hpatch_StreamPos_t dataSize=diffData->streamSize;
        hpatch_StreamPos_t readPos=0;
        while (readPos < dataSize) {
            size_t readLen=hpatch_kFileIOBufBetterSize;
            if (readLen+readPos > dataSize)
                readLen=(size_t)(dataSize - readPos);
            check(diffData->read(diffData,readPos,buf,buf+readLen),
                  HPATCH_FILEREAD_ERROR,"createSfx() read diffData");
            check(out_sfxData->write(out_sfxData,writePos,buf,buf+readLen),
                  HPATCH_FILEWRITE_ERROR,"createSfx() write sfxData");
            readPos+=readLen;
            writePos+=readLen;
        }
    }
clear:
    _isInClear=hpatch_TRUE;
    _free_mem(pmem);
    return result;
}

int createSfx_notCheckDiffFile(const char* selfExecuteFileName,const char* diffFileName,
                               const char* out_sfxFileName){
    int     result=HPATCH_SUCCESS;
    int     _isInClear=hpatch_FALSE;
    hpatch_TFileStreamInput  exeData;
    hpatch_TFileStreamInput  diffData;
    hpatch_TFileStreamOutput out_sfxData;
    hpatch_TFileStreamInput_init(&exeData);
    hpatch_TFileStreamInput_init(&diffData);
    hpatch_TFileStreamOutput_init(&out_sfxData);
    printf(    "selfExecute: \""); _log_info_utf8(selfExecuteFileName);
    printf("\"\ndiffFile   : \""); _log_info_utf8(diffFileName);
    printf("\"\nout sfxFile: \""); _log_info_utf8(out_sfxFileName);
    printf("\"\n\n");
    {//open
        hpatch_StreamPos_t sfxFileLength=0;
        check(hpatch_TFileStreamInput_open(&exeData,selfExecuteFileName),
              HPATCH_OPENREAD_ERROR,"createSfx() open selfExecuteFile read");
        sfxFileLength+=exeData.base.streamSize;
        check(hpatch_TFileStreamInput_open(&diffData,diffFileName),
              HPATCH_OPENREAD_ERROR,"createSfx() open diffFileName read");
        sfxFileLength+=diffData.base.streamSize;
        check(hpatch_TFileStreamOutput_open(&out_sfxData,out_sfxFileName,sfxFileLength),
              HPATCH_OPENWRITE_ERROR,"createSfx() open sfxFile write");
    }
    result=createSfx_notCheckDiffFile_byStream(&exeData.base,&diffData.base,&out_sfxData.base);
    if (result!=HPATCH_SUCCESS) check_on_error(result);
    assert(out_sfxData.base.streamSize==out_sfxData.out_length);
    check(hpatch_TFileStreamOutput_close(&out_sfxData),HPATCH_FILECLOSE_ERROR,"createSfx() out sfxFile close");
    printf("sfxFileSize: %" PRIu64 "\n",out_sfxData.base.streamSize);
    if (hpatch_getIsExecuteFile(selfExecuteFileName)){
        check(hpatch_setIsExecuteFile(out_sfxFileName),
              HPATCH_CREATE_SFX_EXECUTETAG_ERROR,"createSfx() set sfxFile Execute tag");
    }
    printf("out sfxFile ok!\n");
clear:
    _isInClear=hpatch_TRUE;
    check(hpatch_TFileStreamOutput_close(&out_sfxData),HPATCH_FILECLOSE_ERROR,"createSfx() out sfxFile close");
    check(hpatch_TFileStreamInput_close(&diffData),HPATCH_FILECLOSE_ERROR,"createSfx() diffFile close");
    check(hpatch_TFileStreamInput_close(&exeData),HPATCH_FILECLOSE_ERROR,"createSfx() selfExecuteFile close");
    return result;
}

static hpatch_BOOL _getIsCompressedDiffFile(const char* diffFileName){
    hpatch_TFileStreamInput diffData;
    hpatch_compressedDiffInfo diffInfo;
    hpatch_BOOL result=hpatch_TRUE;
    hpatch_TFileStreamInput_init(&diffData);
    if (!hpatch_TFileStreamInput_open(&diffData,diffFileName)) return hpatch_FALSE;
    result=getCompressedDiffInfo(&diffInfo,&diffData.base);
    if (!hpatch_TFileStreamInput_close(&diffData)) return hpatch_FALSE;
    return result;
}

int createSfx(const char* selfExecuteFileName,const char* diffFileName,const char* out_sfxFileName){
    int     result=HPATCH_SUCCESS;
    int     _isInClear=hpatch_FALSE;
    {//check diffFile type
        hpatch_BOOL isDiffFile=hpatch_FALSE;
#if (_IS_NEED_DIR_DIFF_PATCH)
        if (!isDiffFile)
            isDiffFile=getIsDirDiffFile(diffFileName);
#endif
        if (!isDiffFile)
            isDiffFile=_getIsCompressedDiffFile(diffFileName);
        check(isDiffFile,HPATCH_CREATE_SFX_DIFFFILETYPE_ERROR,
              "createSfx() input diffFile is unsupported type");
    }
    result=createSfx_notCheckDiffFile(selfExecuteFileName,diffFileName,out_sfxFileName);
clear:
    _isInClear=hpatch_TRUE;
    return result;
}

hpatch_BOOL getDiffDataOffertInSfx(hpatch_StreamPos_t* out_diffDataOffert,hpatch_StreamPos_t* out_diffDataSize){
    hpatch_uint32_t diffOff=0;
    hpatch_uint64_t diffSize=0;
    size_t i;
    for (i=0; i<sizeof(hpatch_uint32_t); ++i){
        size_t si=_sfx_guid_size+i;
        diffOff|=((hpatch_uint32_t)_sfx_guid_node[si])<<(8*i);
    }
    if (diffOff==(~(hpatch_uint32_t)0)) return hpatch_FALSE;
    assert(diffOff>_sfx_guid_node_size);
    
    for (i=0; i<sizeof(hpatch_uint64_t); ++i){
        size_t si=_sfx_guid_size+sizeof(hpatch_uint32_t)+i;
        diffSize|=((hpatch_uint64_t)_sfx_guid_node[si])<<(8*i);
    }
    if (diffSize==(~(hpatch_uint64_t)0)) return hpatch_FALSE;
    
    if (out_diffDataOffert) *out_diffDataOffert=diffOff;
    if (out_diffDataSize) *out_diffDataSize=diffSize;
    return hpatch_TRUE;
}

#endif // _IS_NEED_SFX
