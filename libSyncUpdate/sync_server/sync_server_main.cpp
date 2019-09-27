//  sync_server_main.cpp
//  sync_server
//  Created by housisong on 2019-09-17.
/*
 The MIT License (MIT)
 Copyright (c) 2019-2019 HouSisong
 
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
#include <stdexcept>
#include <vector>
#include <string>
#include "../../file_for_patch.h"
#include "sync_server.h"
#include "../../_clock_for_demo.h"
#include "../../_atosize.h"
#include "../../libHDiffPatch/HDiff/private_diff/mem_buf.h"

#ifndef _IS_NEED_MAIN
#   define  _IS_NEED_MAIN 1
#endif

#ifndef _IS_NEED_DEFAULT_CompressPlugin
#   define _IS_NEED_DEFAULT_CompressPlugin 1
#endif
#if (_IS_NEED_DEFAULT_CompressPlugin)
//===== select needs decompress plugins or change to your plugin=====
#   define _CompressPlugin_zlib
#   define _CompressPlugin_lzma
#endif

#define  IS_NOTICE_compress_canceled 0
#include "../../compress_plugin_demo.h"


#ifndef _IS_NEED_DEFAULT_ChecksumPlugin
#   define _IS_NEED_DEFAULT_ChecksumPlugin 1
#endif
#if (_IS_NEED_DEFAULT_ChecksumPlugin)
//===== select needs checksum plugins or change to your plugin=====
#   define _ChecksumPlugin_md5
#endif

#include "../../checksum_plugin_demo.h"


static void printUsage(){
    printf("sync_serever: [options] newDataPath out_newSyncInfoPath [out_newSyncDataPath]\n"
           "options:\n"
           "  -s-matchBlockSize\n"
           "      matchBlockSize can like 4096 or 4k or 128k or 1m etc..., DEFAULT 2048\n"
           "  -c-compressType[-compressLevel]\n"
           "      set out_newSyncDataPath Compress type & level, DEFAULT uncompress;\n"
           "      support compress type & level:\n"
           "       (re. https://github.com/sisong/lzbench/blob/master/lzbench171_sorted.md )\n"
#ifdef _CompressPlugin_zlib
           "        -c-zlib[-{1..9}]                DEFAULT level 9\n"
#endif
#ifdef _CompressPlugin_lzma
           "        -c-lzma[-{0..9}[-dictSize]]     DEFAULT level 7\n"
           "            dictSize can like 4096 or 4k or 4m or 64m etc..., DEFAULT 8m\n"
#endif
           "  -f  Force overwrite, ignore write path already exists;\n"
           "      DEFAULT (no -f) not overwrite and then return error;\n"
           "      if used -f and write path is exist directory, will always return error.\n"
           "  -h or -?\n"
           "      output Help info (this usage).\n"
           "\n"
           );
}

typedef enum TSyncServerResult {
    SYNC_SERVER_SUCCESS=0,
    SYNC_SERVER_OPTIONS_ERROR,
    SYNC_SERVER_BLOCKSIZE_ERROR,
    SYNC_SERVER_NEWFILE_ERROR,
    SYNC_SERVER_OUTFILE_ERROR,
    SYNC_SERVER_CANNOT_OVERWRITE_ERROR,
    SYNC_SERVER_CREATE_SYNC_DATA_ERROR,
} TSyncServerResult;

int sync_server_cmd_line(int argc, const char * argv[]);


#if (_IS_NEED_MAIN)
#   if (_IS_USED_WIN32_UTF8_WAPI)
int wmain(int argc,wchar_t* argv_w[]){
    hdiff_private::TAutoMem  _mem(hpatch_kPathMaxSize*4);
    char** argv_utf8=(char**)_mem.data();
    if (!_wFileNames_to_utf8((const wchar_t**)argv_w,argc,argv_utf8,_mem.size()))
        return SYNC_SERVER_OPTIONS_ERROR;
    SetDefaultStringLocale();
    return sync_server_cmd_line(argc,(const char**)argv_utf8);
}
#   else
int main(int argc,char* argv[]){
    return  sync_server_cmd_line(argc,(const char**)argv);
}
#   endif
#endif


hpatch_inline static const char* findEnd(const char* str,char c){
    const char* result=strchr(str,c);
    return (result!=0)?result:(str+strlen(str));
}

static bool _tryGetCompressSet(const char** isMatchedType,const char* ptype,const char* ptypeEnd,
                               const char* cmpType,const char* cmpType2=0,
                               size_t* compressLevel=0,size_t levelMin=0,size_t levelMax=0,size_t levelDefault=0,
                               size_t* dictSize=0,size_t dictSizeMin=0,size_t dictSizeMax=0,size_t dictSizeDefault=0){
    if (*isMatchedType) return true; //ok
    const size_t ctypeLen=strlen(cmpType);
    const size_t ctype2Len=(cmpType2!=0)?strlen(cmpType2):0;
    if ( ((ctypeLen==(size_t)(ptypeEnd-ptype))&&(0==strncmp(ptype,cmpType,ctypeLen)))
        || ((cmpType2!=0)&&(ctype2Len==(size_t)(ptypeEnd-ptype))&&(0==strncmp(ptype,cmpType2,ctype2Len))) )
        *isMatchedType=cmpType; //ok
    else
        return true;//type mismatch
    
    if ((compressLevel)&&(ptypeEnd[0]=='-')){
        const char* plevel=ptypeEnd+1;
        const char* plevelEnd=findEnd(plevel,'-');
        if (!a_to_size(plevel,plevelEnd-plevel,compressLevel)) return false; //error
        if (*compressLevel<levelMin) *compressLevel=levelMin;
        else if (*compressLevel>levelMax) *compressLevel=levelMax;
        if ((dictSize)&&(plevelEnd[0]=='-')){
            const char* pdictSize=plevelEnd+1;
            const char* pdictSizeEnd=findEnd(pdictSize,'-');
            if (!kmg_to_size(pdictSize,pdictSizeEnd-pdictSize,dictSize)) return false; //error
            if (*dictSize<dictSizeMin) *dictSize=dictSizeMin;
            else if (*dictSize>dictSizeMax) *dictSize=dictSizeMax;
        }else{
            if (plevelEnd[0]!='\0') return false; //error
            if (dictSize) *dictSize=dictSizeDefault;
        }
    }else{
        if (ptypeEnd[0]!='\0') return false; //error
        if (compressLevel) *compressLevel=levelDefault;
        if (dictSize) *dictSize=dictSizeDefault;
    }
    return true;
}

#define _options_check(value,errorInfo) do{ \
    if (!(value)) { fprintf(stderr,"options " errorInfo " ERROR!\n\n"); \
                    printUsage(); return SYNC_SERVER_OPTIONS_ERROR; } }while(0)

#define _return_check(value,exitCode,fmt,errorInfo) do{ \
    if (!(value)) { fprintf(stderr,fmt,errorInfo); return exitCode; } }while(0)

static int _checkSetCompress(hdiff_TCompress** out_compressPlugin,
                             const char* ptype,const char* ptypeEnd){
    const char* isMatchedType=0;
    size_t      compressLevel=0;
#if (defined _CompressPlugin_lzma)||(defined _CompressPlugin_lzma2)
    size_t      dictSize=0;
#endif
#ifdef _CompressPlugin_zlib
    _options_check(_tryGetCompressSet(&isMatchedType,
                                      ptype,ptypeEnd,"zlib",0,&compressLevel,1,9,9),"-c-zlib-?");
    if ((isMatchedType)&&(0==strcmp(isMatchedType,"zlib"))){
        static TCompressPlugin_zlib _zlibCompressPlugin=zlibCompressPlugin;
        _zlibCompressPlugin.compress_level=(int)compressLevel;
        *out_compressPlugin=&_zlibCompressPlugin.base; }
#endif
#ifdef _CompressPlugin_bz2
    _options_check(_tryGetCompressSet(&isMatchedType,
                                      ptype,ptypeEnd,"bzip2","bz2",&compressLevel,1,9,9),"-c-bzip2-?");
    if ((isMatchedType)&&(0==strcmp(isMatchedType,"bzip2"))){
        static TCompressPlugin_bz2 _bz2CompressPlugin=bz2CompressPlugin;
        _bz2CompressPlugin.compress_level=(int)compressLevel;
        *out_compressPlugin=&_bz2CompressPlugin.base; }
#endif
#ifdef _CompressPlugin_lzma
    _options_check(_tryGetCompressSet(&isMatchedType,
                                      ptype,ptypeEnd,"lzma",0,&compressLevel,0,9,7, &dictSize,1<<12,
                                      (sizeof(size_t)<=4)?(1<<27):((size_t)3<<29),1<<23),"-c-lzma-?");
    if ((isMatchedType)&&(0==strcmp(isMatchedType,"lzma"))){
        static TCompressPlugin_lzma _lzmaCompressPlugin=lzmaCompressPlugin;
        _lzmaCompressPlugin.compress_level=(int)compressLevel;
        _lzmaCompressPlugin.dict_size=(int)dictSize;
        *out_compressPlugin=&_lzmaCompressPlugin.base; }
#endif
    _options_check((*out_compressPlugin!=0),"-c-?");
    return SYNC_SERVER_SUCCESS;
}

static bool getFileSize(const char *path_utf8,hpatch_StreamPos_t* out_fileSize){
    hpatch_TPathType out_type;
    if (!hpatch_getPathStat(path_utf8,&out_type,out_fileSize)) return false;
    return out_type==kPathType_file;
}

static bool printFileInfo(const char *path_utf8,const char *tag,hpatch_StreamPos_t* out_fileSize=0){
    hpatch_StreamPos_t fileSize=0;
    if (!getFileSize(path_utf8,&fileSize)) return false;
    printf("%s: %" PRIu64 "   \"%s\"\n",tag,fileSize,path_utf8);
    if (out_fileSize) * out_fileSize=fileSize;
    return true;
}


#define _kNULL_VALUE    ((hpatch_BOOL)(-1))
#define _kNULL_SIZE     (~(size_t)0)

int sync_server_cmd_line(int argc, const char * argv[]){
    hpatch_BOOL isForceOverwrite=_kNULL_VALUE;
    hpatch_BOOL isOutputHelp=_kNULL_VALUE;
    size_t      kMatchBlockSize=_kNULL_SIZE;
    hdiff_TCompress* compressPlugin=0;
    std::vector<const char *> arg_values;
    for (int i=1; i<argc; ++i) {
        const char* op=argv[i];
        _options_check(op!=0,"?");
        if (op[0]!='-'){
            arg_values.push_back(op); //file path
            continue;
        }
        switch (op[1]) {
            case '?':
            case 'h':{
                _options_check((isOutputHelp==_kNULL_VALUE)&&(op[2]=='\0'),"-h");
                isOutputHelp=hpatch_TRUE;
            } break;
            case 'f':{
                _options_check((isForceOverwrite==_kNULL_VALUE)&&(op[2]=='\0'),"-f");
                isForceOverwrite=hpatch_TRUE;
            } break;
            case 's':{
                _options_check((kMatchBlockSize==_kNULL_SIZE)&&(op[2]=='-'),"-s");
                const char* pnum=op+3;
                _options_check(kmg_to_size(pnum,strlen(pnum),&kMatchBlockSize),"-s-?");
                _options_check(kMatchBlockSize==(uint32_t)kMatchBlockSize,"-s-?");
                _options_check(kMatchBlockSize>=kMatchBlockSize_min,"-s-?");
            } break;
            case 'c':{
                _options_check((compressPlugin==0)&&(op[2]=='-'),"-c");
                const char* ptype=op+3;
                const char* ptypeEnd=findEnd(ptype,'-');
                int result=_checkSetCompress(&compressPlugin,ptype,ptypeEnd);
                if (SYNC_SERVER_SUCCESS!=result)
                    return result;
            } break;
            default: {
                _options_check(hpatch_FALSE,"-?");
            } break;
        }//swich
    }
    
    if (isOutputHelp==_kNULL_VALUE)
        isOutputHelp=hpatch_FALSE;
    if (isForceOverwrite==_kNULL_VALUE)
        isForceOverwrite=hpatch_FALSE;
    if (kMatchBlockSize==_kNULL_SIZE)
        kMatchBlockSize=kMatchBlockSize_default;
    
    if (isOutputHelp){
        printUsage();
        if (arg_values.empty())
            return SYNC_SERVER_SUCCESS; //ok
    }
    
    _options_check((arg_values.size()==2)||(arg_values.size()==3),"input count");
    const char* newDataPath        =arg_values[0];
    const char* out_newSyncInfoPath=arg_values[1];
    const char* out_newSyncDataPath=0;
    if (arg_values.size()>=3)
        out_newSyncDataPath=arg_values[2];
    if (compressPlugin)
        _options_check(out_newSyncDataPath!=0,"used compress need out_newSyncDataPath");

    if (!isForceOverwrite){
        hpatch_TPathType   outFileType;
        _return_check(hpatch_getPathStat(out_newSyncInfoPath,&outFileType,0),
                      SYNC_SERVER_CANNOT_OVERWRITE_ERROR,"get %s type","out_newSyncInfoPath");
        _return_check(outFileType==kPathType_notExist,
                      SYNC_SERVER_CANNOT_OVERWRITE_ERROR,"%s already exists, not overwrite","out_newSyncInfoPath");
        if (out_newSyncDataPath){
            _return_check(hpatch_getPathStat(out_newSyncDataPath,&outFileType,0),
                          SYNC_SERVER_CANNOT_OVERWRITE_ERROR,"get %s type","out_newSyncDataPath");
            _return_check(outFileType==kPathType_notExist,
                          SYNC_SERVER_CANNOT_OVERWRITE_ERROR,"%s already exists, not overwrite","out_newSyncDataPath");
        }
    }
    
    hpatch_TChecksum* strongChecksumPlugin=&md5ChecksumPlugin;
    printf("create_sync_data run with strongChecksum plugin: \"%s\"\n",strongChecksumPlugin->checksumType());
    if (compressPlugin)
        printf("create_sync_data run with compress plugin: \"%s\"\n",compressPlugin->compressType());
    hpatch_StreamPos_t newDataSize=0;
    _return_check(printFileInfo(newDataPath,"newFileSize",&newDataSize),
                  SYNC_SERVER_NEWFILE_ERROR,"printFileInfo(%s,) run error!\n",newDataPath);
    printf("block size : %d\n",(uint32_t)kMatchBlockSize);
    hpatch_StreamPos_t blockCount=getBlockCount(newDataSize,(uint32_t)kMatchBlockSize);
    printf("block count: %" PRIu64 "\n",blockCount);
    int hashClashBit=estimateHashClashBit(newDataSize,(uint32_t)kMatchBlockSize);
    _return_check(hashClashBit<=kAllowMaxHashClashBit,
                  SYNC_SERVER_BLOCKSIZE_ERROR,"hash clash warning! must increase matchBlockSize(%d) !\n",(uint32_t)kMatchBlockSize);
    double patchMemSize=estimatePatchMemSize(newDataSize,(uint32_t)kMatchBlockSize,(compressPlugin!=0));
    if (patchMemSize>=(1<<20))
        printf("sync_patch memory size: ~ %.3f MB\n",patchMemSize/(1<<20));
    else
        printf("sync_patch memory size: ~ %.3f KB\n",patchMemSize/(1<<10));

    double time0=clock_s();
    try {
        create_sync_data(newDataPath,out_newSyncInfoPath,out_newSyncDataPath,
                         compressPlugin,strongChecksumPlugin,(uint32_t)kMatchBlockSize);
    } catch (const std::exception& e){
        _return_check(false,SYNC_SERVER_CREATE_SYNC_DATA_ERROR,
                      "create_sync_data run error: %s\n",e.what());
    }
    double time1=clock_s();
    _return_check(printFileInfo(out_newSyncInfoPath,"outFileSize"),
                  SYNC_SERVER_OUTFILE_ERROR,"printFileInfo(%s,) run error!\n",out_newSyncInfoPath);
    if (out_newSyncDataPath){
        _return_check(printFileInfo(out_newSyncDataPath,"outFileSize"),
                      SYNC_SERVER_OUTFILE_ERROR,"printFileInfo(%s,) run error!\n",out_newSyncDataPath);
    }
    
    printf("\ncreate_sync_data time: %.3f s\n\n",(time1-time0));
    return 0;
}
