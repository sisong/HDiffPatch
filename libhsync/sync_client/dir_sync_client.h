//  dir_sync_client.h
//  sync_client
//  Created by housisong on 2019-10-05.
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
#ifndef dir_sync_client_h
#define dir_sync_client_h
#include "sync_client.h"
#include "../../dirDiffPatch/dir_patch/dir_patch_types.h"
#if (_IS_NEED_DIR_DIFF_PATCH)
#include "../../dirDiffPatch/dir_diff/dir_manifest.h"
#include "../../dirDiffPatch/dir_patch/new_dir_output.h"

//sync patch(oldManifest+syncDataListener) to outNewFile
//  can use get_manifest(oldDir) to got oldManifest
TSyncClient_resultType sync_patch_2file(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                                        const TManifest& oldManifest,const char* newSyncInfoFile,
                                        const char* outNewFile,hpatch_BOOL isOutNewContinue,
                                        const char* cacheDiffInfoFile,size_t kMaxOpenFileNumber,int threadNum);

//sync_patch can split to two steps: sync_local_diff + sync_local_patch

//download diff data from syncDataListener to outDiffFile
//  if (isOutDiffContinue) then continue download
TSyncClient_resultType sync_local_diff_2file(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                                             const TManifest& oldManifest,const char* newSyncInfoFile,
                                             const char* outDiffFile,TSyncDiffType diffType,hpatch_BOOL isOutDiffContinue,
                                             size_t kMaxOpenFileNumber,int threadNum);

//patch(oldManifest+inDiffFile) to outNewFile
TSyncClient_resultType sync_local_patch_2file(ISyncInfoListener* listener,const char* inDiffFile,
                                              const TManifest& oldManifest,const char* newSyncInfoFile,const char* outNewFile,
                                              size_t kMaxOpenFileNumber,int threadNum);

struct IDirSyncPatchListener:public ISyncInfoListener{
    void*       patchImport;
    hpatch_BOOL (*patchFinish)(struct IDirSyncPatchListener* listener,hpatch_BOOL isPatchSuccess,
                               const TNewDataSyncInfo* newSyncInfo,TNewDirOutput* newDirOutput);
};

//sync patch(oldManifest+syncDataListener) to outNewDir
TSyncClient_resultType sync_patch_2dir(IDirPatchListener* patchListener,IDirSyncPatchListener* syncListener,
                                       IReadSyncDataListener* syncDataListener,
                                       const TManifest& oldManifest,const char* newSyncInfoFile,const char* outNewDir,
                                       const char* cacheDiffInfoFile,size_t kMaxOpenFileNumber,int threadNum);

//download diff data from syncDataListener to outDiffFile
//  if (isOutDiffContinue) then continue download
static hpatch_inline
TSyncClient_resultType sync_local_diff_2dir(IDirPatchListener*,IDirSyncPatchListener* syncListener,
                                            IReadSyncDataListener* syncDataListener,
                                            const TManifest& oldManifest,const char* newSyncInfoFile,
                                            const char* outDiffFile,TSyncDiffType diffType,hpatch_BOOL isOutDiffContinue,
                                            size_t kMaxOpenFileNumber,int threadNum){
            return sync_local_diff_2file(syncListener,syncDataListener,oldManifest,newSyncInfoFile,
                                         outDiffFile,diffType,isOutDiffContinue,kMaxOpenFileNumber,threadNum); }

//patch(oldManifest+inDiffFile) to outNewDir
TSyncClient_resultType sync_local_patch_2dir(IDirPatchListener* patchListener,IDirSyncPatchListener* syncListener,
                                             const char* inDiffFile,
                                             const TManifest& oldManifest,const char* newSyncInfoFile,const char* outNewDir,
                                             size_t kMaxOpenFileNumber,int threadNum);

#endif
#endif // dir_sync_client_h
