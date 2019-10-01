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

//ISyncPatchListener::isChecksumNewSyncInfo
static bool isChecksumNewSyncInfo(ISyncPatchListener* listener){
    return true;
}
//ISyncPatchListener::isChecksumNewSyncData
static bool isChecksumNewSyncData(ISyncPatchListener* listener){
    return true;
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

const char* kCmdInfo="test sync_patch: oldPath newSyncInfoFile test_newSyncDataPath out_newPath"
#if (_IS_USED_MULTITHREAD)
                     " [-p-threadNum]"
#endif
;
#define _options_check(value) do{ \
    if (!(value)) { printf("%s\n",kCmdInfo);  return -1; } }while(0)

#define _THREAD_NUMBER_NULL     0
#define _THREAD_NUMBER_MIN      1
#define _THREAD_NUMBER_DEFUALT  4
#define _THREAD_NUMBER_MAX      (1<<8)

int main(int argc, const char * argv[]) {
    size_t      threadNum = _THREAD_NUMBER_NULL;
    std::vector<const char *> arg_values;
    for (int i=1; i<argc; ++i) {
        const char* op=argv[i];
        _options_check(op!=0);
        if (op[0]!='-'){
            arg_values.push_back(op); //file path
            continue;
        }
        switch (op[1]) {
#if (_IS_USED_MULTITHREAD)
            case 'p':{
                _options_check((threadNum==_THREAD_NUMBER_NULL)&&((op[2]=='-')));
                const char* pnum=op+3;
                _options_check(a_to_size(pnum,strlen(pnum),&threadNum));
                _options_check(threadNum>=_THREAD_NUMBER_MIN);
            } break;
#endif
            default: {
                _options_check(hpatch_FALSE);
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
    _options_check(arg_values.size()==4);
    const char* oldPath             =arg_values[0];
    const char* newSyncInfoFile     =arg_values[1];
    const char* test_newSyncDataPath=arg_values[2];
    const char* out_newPath         =arg_values[3];

    double time0=clock_s();

    if (threadNum>1)
        printf("muti-thread parallel: opened, threadNum: %d\n",(uint32_t)threadNum);
    else
        printf("muti-thread parallel: closed\n");
    
    ISyncPatchListener emulation; memset(&emulation,0,sizeof(emulation));
    if (!downloadEmulation_open_by_file(&emulation,test_newSyncDataPath))
        return kSyncClient_readSyncDataError;
    emulation.isChecksumNewSyncInfo=isChecksumNewSyncInfo;
    emulation.isChecksumNewSyncData=isChecksumNewSyncData;
    emulation.findChecksumPlugin=findChecksumPlugin;
    emulation.findDecompressPlugin=findDecompressPlugin;
    
    int result=sync_patch_by_file(out_newPath,oldPath,newSyncInfoFile,&emulation,(int)threadNum);
    downloadEmulation_close(&emulation);
    double time1=clock_s();
    printf("test sync_patch time: %.3f s\n\n",(time1-time0));
    return result;
}
