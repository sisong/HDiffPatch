//  dir_sync_make.h
//  sync_make
//  Created by housisong on 2019-10-01.
/*
 The MIT License (MIT)
 Copyright (c) 2019-2020 HouSisong
 
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
#ifndef dir_sync_make_h
#define dir_sync_make_h
#include "sync_make.h"
#include "../../dirDiffPatch/dir_patch/dir_patch_types.h"
#if (_IS_NEED_DIR_DIFF_PATCH)
#include "../../dirDiffPatch/dir_diff/dir_manifest.h"

struct IDirSyncListener{
    virtual ~IDirSyncListener(){}
    virtual bool isExecuteFile(const std::string& fileName) { return false; }
    virtual void syncRefInfo(const char* rootDirPath,size_t pathCount,hpatch_StreamPos_t refFileSize,
                             uint32_t kSyncBlockSize,bool isSafeHashClash){}
};

void create_dir_sync_data(IDirSyncListener*         listener,
                          const TManifest&          newManifest,
                          const char*               out_hsyni_file, // .hsyni
                          const char*               out_hsynz_file, // .hsynz
                          size_t                    kMaxOpenFileNumber,
                          hpatch_TChecksum*         strongChecksumPlugin,
                          hsync_TDictCompress*      compressPlugin,
                          hsync_THsynz*             hsynzPlugin=0,
                          uint32_t kSyncBlockSize=kSyncBlockSize_default,
                          size_t kSafeHashClashBit=kSafeHashClashBit_default,
                          size_t threadNum=1);

#endif
#endif // dir_sync_make_h
