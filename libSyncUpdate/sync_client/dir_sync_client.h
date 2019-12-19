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
#include "../../dirDiffPatch/dir_patch/dir_patch.h"
#include "../../dirDiffPatch/dir_diff/dir_diff.h"

//sync patch oldDir to newFile
void get_oldManifest(IDirPathIgnore* filter,const char* oldPath,TManifest& out_oldManifest);
int sync_patch_2file(ISyncPatchListener* listener,const char* outNewFile,const TManifest& oldManifest,
                     const char* newSyncInfoFile,size_t kMaxOpenFileNumber,int threadNum=0);

int  get_isNewDirSyncInfo(const char* newDirSyncInfoFile,hpatch_BOOL* out_newIsDir);

typedef struct TNewDirSyncInfo{
    hpatch_BOOL             newPathIsDir;
    TNewDataSyncInfo        baseSyncInfo;
    hpatch_StreamPos_t      externDataOffset;
    hpatch_StreamPos_t      externDataSize;
    //todo:
} TNewDirSyncInfo;

struct IDirSyncPatchListener:public ISyncPatchListener,IDirPatchListener {
};

hpatch_inline static void
     TNewDirSyncInfo_init        (TNewDirSyncInfo* self) { memset(self,0,sizeof(*self)); }
int  TNewDirSyncInfo_open_by_file(TNewDirSyncInfo* self,const char* newDirSyncInfoFile,
                                  IDirSyncPatchListener* listener);
int  TNewDirSyncInfo_open        (TNewDirSyncInfo* self,const hpatch_TStreamInput* newDirSyncInfo,
                                  IDirSyncPatchListener* listener);
void TNewDirSyncInfo_close       (TNewDirSyncInfo* self);


//sync patch oldPatch(file or dir) to newDir
int  sync_patch_2dir(IDirSyncPatchListener* listener,const char* outNewDir,const TManifest& oldManifest,
                     TNewDirSyncInfo* newDirSyncInfo,int threadNum=0);

#endif
#endif // dir_sync_client_h
