//  sync_client_main.cpp
//  sync_client:  test patch sync files
//  Created by housisong on 2019-09-18.
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
#include <vector>
#include "sync_client.h"
#include "download_emulation.h"
#include "../../_clock_for_demo.h"
#include "../../_atosize.h"
#include "../../libParallel/parallel_import.h"
#include "../../file_for_patch.h"
#include "../../_dir_ignore.h"


#ifndef _IS_NEED_DEFAULT_CompressPlugin
#   define _IS_NEED_DEFAULT_CompressPlugin 1
#endif
#if (_IS_NEED_DEFAULT_CompressPlugin)
//===== select needs decompress plugins or change to your plugin=====
#   define _CompressPlugin_zlib
#   define _CompressPlugin_lzma
#endif

#include "../../decompress_plugin_demo.h"

#ifndef _IS_NEED_DEFAULT_ChecksumPlugin
#   define _IS_NEED_DEFAULT_ChecksumPlugin 1
#endif
#if (_IS_NEED_DEFAULT_ChecksumPlugin)
//===== select needs checksum plugins or change to your plugin=====
#   define _ChecksumPlugin_md5
#endif

#include "../../checksum_plugin_demo.h"

static void printUsage(){
    printf("test sync_patch: [options] oldPath newSyncInfoFile test_newSyncDataFile outNewPath\n"
#if (_IS_NEED_DIR_DIFF_PATCH)
           " ( oldPath can be file or directory(folder); )\n"
#endif
           "options:\n"
#if (_IS_USED_MULTITHREAD)
           "  -p-parallelThreadNumber\n"
           "    if parallelThreadNumber>1 then open multi-thread Parallel mode;\n"
           "    DEFAULT -p-4; requires more and more memory!\n"
#endif
           "  -C-checksumSets\n"
           "      set Checksum data for patch, DEFAULT -C-sync;\n"
           "      checksumSets support:\n"
           "        -C-info         checksum newSyncInfoFile;\n"
           "        -C-sync         checksum outNewPath's data sync from newSyncDataFile;\n"
           "        -C-no           no checksum;\n"
           "        -C-all          same as: -C-info-sync;\n"
#if (_IS_NEED_DIR_DIFF_PATCH)
           "  -n-maxOpenFileNumber\n"
           "      limit Number of open files at same time when oldPath is directory;\n"
           "      maxOpenFileNumber>=8, DEFAULT -n-24, the best limit value by different\n"
           "        operating system.\n"
           "  -g#ignorePath[#ignorePath#...]\n"
           "      set iGnore path list in newDataPath directory; ignore path list such as:\n"
           "        #.DS_Store#desktop.ini#*thumbs*.db#.git*#.svn/#cache_*/00*11/*.tmp\n"
           "      # means separator between names; (if char # in name, need write #: )\n"
           "      * means can match any chars in name; (if char * in name, need write *: );\n"
           "      / at the end of name means must match directory;\n"
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
           "  -h or -?\n"
           "      output Help info (this usage).\n"
           "  -v  output Version info.\n\n"
           );
}


//ISyncPatchListener::findDecompressPlugin
static hpatch_TDecompress* findDecompressPlugin(ISyncPatchListener* listener,const char* compressType){
    if (compressType==0) return 0; //ok
    hpatch_TDecompress* decompressPlugin=0;
#ifdef  _CompressPlugin_zlib
    if ((!decompressPlugin)&&zlibDecompressPlugin.is_can_open(compressType))
        decompressPlugin=&zlibDecompressPlugin;
#endif
#ifdef  _CompressPlugin_lzma
    if ((!decompressPlugin)&&lzmaDecompressPlugin.is_can_open(compressType))
        decompressPlugin=&lzmaDecompressPlugin;
#endif
    if (decompressPlugin==0){
        printf("sync_patch can't decompress type: \"%s\"\n",compressType);
        return 0; //unsupport error
    }else{
        printf("sync_patch run with decompress plugin: \"%s\"\n",compressType);
        return decompressPlugin; //ok
    }
}
//ISyncPatchListener::findChecksumPlugin
static hpatch_TChecksum* findChecksumPlugin(ISyncPatchListener* listener,const char* strongChecksumType){
    if (strongChecksumType==0) return 0; //ok
    hpatch_TChecksum* strongChecksumPlugin=0;
#ifdef  _ChecksumPlugin_md5
    if ((!strongChecksumPlugin)&&(0==strcmp(strongChecksumType,md5ChecksumPlugin.checksumType())))
        strongChecksumPlugin=&md5ChecksumPlugin;
#endif
    if (strongChecksumPlugin==0){
        printf("sync_patch can't found checksum type: \"%s\"\n",strongChecksumType);
        return 0; //unsupport error
    }else{
        printf("sync_patch run with strongChecksum plugin: \"%s\"\n",strongChecksumType);
        return strongChecksumPlugin; //ok
    }
}

static hpatch_BOOL _toChecksumSet(const char* psets,bool* isChecksumNewSyncInfo,bool* isChecksumNewSyncData){
    while (hpatch_TRUE) {
        const char* pend=findUntilEnd(psets,'-');
        size_t len=(size_t)(pend-psets);
        if (len==0) return hpatch_FALSE; //error no set
        if        ((len==4)&&(0==memcmp(psets,"info",len))){
            *isChecksumNewSyncInfo=true;
        }else  if ((len==4)&&(0==memcmp(psets,"sync",len))){
            *isChecksumNewSyncData=true;
        }else  if ((len==3)&&(0==memcmp(psets,"all",len))){
            *isChecksumNewSyncInfo=true;
            *isChecksumNewSyncData=true;
        }else  if ((len==2)&&(0==memcmp(psets,"no",len))){
            *isChecksumNewSyncInfo=false;
            *isChecksumNewSyncData=false;
        }else{
            return hpatch_FALSE;//error unknow set
        }
        if (*pend=='\0')
            return hpatch_TRUE; //ok
        else
            psets=pend+1;
    }
}

#define _options_check(value,errorInfo){ \
    if (!(value)) { fprintf(stderr,"options " errorInfo " ERROR!\n\n"); \
                    printUsage(); return kSyncClient_optionsError; } }

#define _kNULL_VALUE    (-1)
#define _kNULL_SIZE     (~(size_t)0)

#define _THREAD_NUMBER_NULL     0
#define _THREAD_NUMBER_MIN      1
#define _THREAD_NUMBER_DEFUALT  4
#define _THREAD_NUMBER_MAX      (1<<8)

int main(int argc, const char * argv[]) {
    size_t      threadNum = _THREAD_NUMBER_NULL;
    hpatch_BOOL isForceOverwrite=_kNULL_VALUE;
    hpatch_BOOL isOutputHelp=_kNULL_VALUE;
    hpatch_BOOL isOutputVersion=_kNULL_VALUE;
    bool isChecksumNewSyncInfo=false;
    bool isChecksumNewSyncData=true; //checksumSet DEFAULT
#if (_IS_NEED_DIR_DIFF_PATCH)
    size_t                      kMaxOpenFileNumber=_kNULL_SIZE; //only used in oldPath is dir
    std::vector<std::string>    ignoreOldPathList;
#endif
    std::vector<const char *> arg_values;
    for (int i=1; i<argc; ++i) {
        const char* op=argv[i];
        _options_check(op!=0,"?");
        if (op[0]!='-'){
            arg_values.push_back(op); //file path
            continue;
        }
        switch (op[1]) {
#if (_IS_USED_MULTITHREAD)
            case 'p':{
                _options_check((threadNum==_THREAD_NUMBER_NULL)&&((op[2]=='-')),"-p-?");
                const char* pnum=op+3;
                _options_check(a_to_size(pnum,strlen(pnum),&threadNum),"-p-?");
                _options_check(threadNum>=_THREAD_NUMBER_MIN,"-p-?");
            } break;
#endif
            case 'C':{
                const char* psets=op+3;
                _options_check((op[2]=='-'),"-C-?");
                isChecksumNewSyncInfo=false;
                isChecksumNewSyncData=false;//all set false
                _options_check(_toChecksumSet(psets,&isChecksumNewSyncInfo,&isChecksumNewSyncData),"-C-?");
            } break;
#if (_IS_NEED_DIR_DIFF_PATCH)
            case 'n':{
                const char* pnum=op+3;
                _options_check((kMaxOpenFileNumber==_kNULL_SIZE)&&(op[2]=='-'),"-n-?");
                _options_check(kmg_to_size(pnum,strlen(pnum),&kMaxOpenFileNumber),"-n-?");
            } break;
            case 'g':{
                if (op[2]=='#'){ //-g#
                    const char* plist=op+3;
                    _options_check(_getIgnorePathSetList(ignoreOldPathList,plist),"-g#?");
                }else{
                    _options_check(hpatch_FALSE,"-g?");
                }
            } break;
#endif
            case 'f':{
                _options_check((isForceOverwrite==_kNULL_VALUE)&&(op[2]=='\0'),"-f");
                isForceOverwrite=hpatch_TRUE;
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
                _options_check(hpatch_FALSE,"?");
            } break;
        }//swich
    }
    
    if (threadNum==_THREAD_NUMBER_NULL)
        threadNum=_THREAD_NUMBER_DEFUALT;
    else if (threadNum>_THREAD_NUMBER_MAX)
        threadNum=_THREAD_NUMBER_MAX;
#if (_IS_USED_MULTITHREAD)
#else
    threadNum=1;
#endif
    _options_check(arg_values.size()==4,"input count");
    const char* oldPath             =arg_values[0];
    const char* newSyncInfoFile     =arg_values[1];
    const char* test_newSyncDataFile=arg_values[2];
    const char* outNewPath          =arg_values[3];

    double time0=clock_s();

    if (threadNum>1)
        printf("muti-thread parallel: opened, threadNum: %d\n",(uint32_t)threadNum);
    else
        printf("muti-thread parallel: closed\n");
    
    ISyncPatchListener emulation; memset(&emulation,0,sizeof(emulation));
    bool isUseCacheDownload=true;
    char downloadCacheTempFile[hpatch_kPathMaxSize+1];
    if (isUseCacheDownload){
        if (!hpatch_getTempPathName(outNewPath,downloadCacheTempFile,
                                    downloadCacheTempFile+sizeof(downloadCacheTempFile)))
            return kSyncClient_tempCacheFileError;
        if (!cacheDownloadEmulation_open_by_file(&emulation,test_newSyncDataFile,downloadCacheTempFile))
            return kSyncClient_readSyncDataError;
    }else{
        if (!downloadEmulation_open_by_file(&emulation,test_newSyncDataFile))
            return kSyncClient_readSyncDataError;
    }
    emulation.isChecksumNewSyncInfo=isChecksumNewSyncInfo;
    emulation.isChecksumNewSyncData=isChecksumNewSyncData;
    emulation.findChecksumPlugin=findChecksumPlugin;
    emulation.findDecompressPlugin=findDecompressPlugin;
    
    int result=sync_patch_by_file(outNewPath,oldPath,newSyncInfoFile,&emulation,(int)threadNum);
    if (isUseCacheDownload){
        cacheDownloadEmulation_close(&emulation);
        hpatch_removeFile(downloadCacheTempFile);
    }else{
        downloadEmulation_close(&emulation);
    }
    double time1=clock_s();
    printf("test sync_patch time: %.3f s\n\n",(time1-time0));
    return result;
}
