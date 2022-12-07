//  sync_client.h
//  sync_client
//  Created by housisong on 2019-09-18.
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
#ifndef sync_client_h
#define sync_client_h
#include "sync_client_type.h"
#include "sync_info_client.h"

//sync_patch(oldStream+syncDataListener) to out_newStream
int sync_patch(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
               const hpatch_TStreamInput* oldStream,const TNewDataSyncInfo* newSyncInfo,
               const hpatch_TStreamOutput* out_newStream,const hpatch_TStreamInput* newDataContinue,int threadNum=1);

//sync patch(oldFile+syncDataListener) to outNewFile
int sync_patch_file2file(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                         const char* oldFile,const char* newSyncInfoFile,
                         const char* outNewFile,hpatch_BOOL isOutNewContinue,int threadNum=1);


//sync_patch can split to two steps: sync_local_diff + sync_local_patch


//download diff data from syncDataListener to out_diffStream
//  if (diffContinue) then continue download
int sync_local_diff(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                    const hpatch_TStreamInput* oldStream,const TNewDataSyncInfo* newSyncInfo,
                    const hpatch_TStreamOutput* out_diffStream,const hpatch_TStreamInput* diffContinue,int threadNum=1);

//patch(oldStream+in_diffStream) to out_newStream
int sync_local_patch(ISyncInfoListener* listener,const hpatch_TStreamInput* in_diffStream,
                     const hpatch_TStreamInput* oldStream,const TNewDataSyncInfo* newSyncInfo,
                     const hpatch_TStreamOutput* out_newStream,const hpatch_TStreamInput* newDataContinue,int threadNum=1);


//download diff data from syncDataListener to outDiffFile
//  if (isOutDiffContinue) then continue download
int sync_local_diff_file2file(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                              const char* oldFile,const char* newSyncInfoFile,
                              const char* outDiffFile,hpatch_BOOL isOutDiffContinue,int threadNum=1);

//patch(oldFile+inDiffFile) to outNewFile
int sync_local_patch_file2file(ISyncInfoListener* listener,const char* inDiffFile,
                               const char* oldFile,const char* newSyncInfoFile,
                               const char* outNewFile,hpatch_BOOL isOutNewContinue,int threadNum=1);

#endif // sync_client_h
