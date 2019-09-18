//  main.cpp
//  sync_server
//  Created by housisong on 2019-09-17.
//  Copyright © 2019 sisong. All rights reserved.

#include <iostream>
#include "sync_server.h"
#include "../../ApkDiffPatch/HDiffPatch/_clock_for_demo.h"

#define  IS_NOTICE_compress_canceled 0
#define  _CompressPlugin_zlib
#include "zlib.h"
#define  _IsNeedIncludeDefaultCompressHead 0
#define  _CompressPlugin_lzma
#include "../../ApkDiffPatch/lzma/C/LzmaEnc.h"
#include "../../ApkDiffPatch/HDiffPatch/compress_plugin_demo.h"

#define  _IsNeedIncludeDefaultChecksumHead 0
#define  _ChecksumPlugin_md5
#include "../../libmd5/md5.h"
#include "../../ApkDiffPatch/HDiffPatch/checksum_plugin_demo.h"
#include "../../ApkDiffPatch/HDiffPatch/_atosize.h"

int main(int argc, const char * argv[]) {
    double time0=clock_s();
    if (argc!=1+4) return -1;
    hpatch_TChecksum*      strongChecksumPlugin=&md5ChecksumPlugin;
    //const hdiff_TCompress* compressPlugin=0;
    const hdiff_TCompress* compressPlugin=&zlibCompressPlugin.base;
    //const hdiff_TCompress* compressPlugin=&lzmaCompressPlugin.base;
    
    size_t kMatchBlockSize=-1;
    if (!kmg_to_size(argv[4],strlen(argv[4]),&kMatchBlockSize)
        &&(kMatchBlockSize!=(uint32_t)kMatchBlockSize)) {
        printf("kMatchBlockSize input error: %s",argv[4]);
        return -1;
    }
    create_sync_data(argv[1],argv[2],argv[3],
                     strongChecksumPlugin,compressPlugin,(uint32_t)kMatchBlockSize);
    double time1=clock_s();
    printf("create_sync_data time: %.3f s\n\n",(time1-time0));
    return 0;
}
