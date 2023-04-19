//  sync_client_private.h
//  sync_client
//  Created by housisong on 2020-02-09.
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
#ifndef sync_client_private_h
#define sync_client_private_h
#include "sync_client_type_private.h"
#include "sync_client.h"
struct hpatch_TFileStreamOutput;
namespace sync_private{
    struct TSyncDiffData;

    TSyncClient_resultType
        _sync_patch(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,TSyncDiffData* diffData,
                    const hpatch_TStreamInput* oldStream,const TNewDataSyncInfo* newSyncInfo,
                    const hpatch_TStreamOutput* out_newStream,const hpatch_TStreamInput* newDataContinue,
                    const hpatch_TStreamOutput* out_diffStream,TSyncDiffType diffType,
                    const hpatch_TStreamInput* diffContinue,int threadNum);
    
    bool _open_continue_out(hpatch_BOOL& isOutContinue,const char* outFile,hpatch_TFileStreamOutput* out_stream,
                            hpatch_TStreamInput* continue_stream,hpatch_StreamPos_t maxContinueLength=hpatch_kNullStreamPos);
}


#endif // sync_client_private_h
