//  dir_sync_server.h
//  sync_server
//  Created by housisong on 2019-10-01.
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
#ifndef dir_sync_server_h
#define dir_sync_server_h
#include "sync_server.h"
#include "../../dirDiffPatch/dir_patch/dir_patch_types.h"
#if (_IS_NEED_DIR_DIFF_PATCH)
#include "../../dirDiffPatch/dir_patch/dir_patch.h"
#include "../../dirDiffPatch/dir_diff/dir_diff.h"

struct IDirSyncListener{
    virtual ~IDirSyncListener(){}
    virtual bool isExecuteFile(const std::string& fileName) { return false; }
    virtual void syncRefInfo(size_t pathCount,hpatch_StreamPos_t refFileSize,
                             uint32_t kMatchBlockSize,bool isMatchBlockSizeWarning){}
    virtual void externData(std::vector<unsigned char>& out_externData){}
    virtual void externDataPosInSyncInfoStream(hpatch_StreamPos_t externDataPos,size_t externDataSize){}
};

void create_dir_sync_data(IDirSyncListener*         listener,
                          const TManifest&          newManifest,
                          const char*               outNewSyncInfoFile, // .hsyni
                          const char*               outNewSyncDataFile, // .hsynd
                          const hdiff_TCompress*    compressPlugin,
                          hpatch_TChecksum*         strongChecksumPlugin,
                          size_t                    kMaxOpenFileNumber,
                          uint32_t kMatchBlockSize=kMatchBlockSize_default,size_t threadNum=1);

#endif
#endif // dir_sync_server_h
