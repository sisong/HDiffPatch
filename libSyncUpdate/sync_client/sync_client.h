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

typedef struct TNeedSyncInfo{
    uint32_t           syncBlockCount;
    hpatch_StreamPos_t syncDataSize;
} TNeedSyncInfo;

typedef struct ISyncPatchListener:public ISyncInfoListener{
    //needSyncMsg can null
    void (*needSyncMsg)    (ISyncPatchListener* listener,const TNeedSyncInfo* needSyncInfo);
    //needSyncDataMsg can null; called befor all readSyncData called;
    void (*needSyncDataMsg)(ISyncPatchListener* listener,hpatch_StreamPos_t posInNewSyncData,
                            uint32_t syncDataSize);
    //download data
    bool (*readSyncData)   (ISyncPatchListener* listener,hpatch_StreamPos_t posInNewSyncData,
                            uint32_t syncDataSize,unsigned char* out_syncDataBuf);
} ISyncPatchListener;

int sync_patch(ISyncPatchListener* listener,const hpatch_TStreamOutput* out_newStream,
               const hpatch_TStreamInput* oldStream,const TNewDataSyncInfo* newSyncInfo,int threadNum=0);

int sync_patch_file2file(ISyncPatchListener* listener,const char* outNewFile,const char* oldFile,
                         const char* newSyncInfoFile,int threadNum=0);

#endif // sync_client_h
