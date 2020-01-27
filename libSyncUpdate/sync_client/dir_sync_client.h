//  dir_sync_client.h
//  sync_client
//  Created by housisong on 2019-10-05.
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
#ifndef dir_sync_client_h
#define dir_sync_client_h
#include "sync_client.h"
#include "../../dirDiffPatch/dir_patch/dir_patch_types.h"
#if (_IS_NEED_DIR_DIFF_PATCH)
#include "../../dirDiffPatch/dir_diff/dir_manifest.h"
#include "../../dirDiffPatch/dir_patch/new_dir_output.h"

//sync patch oldDir to newFile
//  use get_manifest(dir) to get TManifest
int sync_patch_dir2file(ISyncPatchListener* listener,const char* outNewFile,const TManifest& oldManifest,
                        const char* newSyncInfoFile,size_t kMaxOpenFileNumber,int threadNum=1);

struct IDirSyncPatchListener:public ISyncPatchListener{
    hpatch_BOOL (*patchBegin) (struct IDirSyncPatchListener* listener,
                               const TNewDataSyncInfo* newSyncInfo,TNewDirOutput* newDirOutput);
    hpatch_BOOL (*patchFinish)(struct IDirSyncPatchListener* listener,hpatch_BOOL isPatchSuccess,
                               const TNewDataSyncInfo* newSyncInfo,TNewDirOutput* newDirOutput);
};

//sync patch oldPatch to newDir
int sync_patch_fileOrDir2dir(IDirPatchListener* patchListener,IDirSyncPatchListener* syncListener,
                             const char* outNewDir,const TManifest& oldManifest,
                             const char* newSyncInfoFile,size_t kMaxOpenFileNumber,int threadNum=1);

#endif
#endif // dir_sync_client_h
