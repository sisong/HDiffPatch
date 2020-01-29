//  client_download_test.cpp
//  sync_client
//  Created by housisong on 2019-09-23.
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
#include "client_download_test.h"
#include "../file_for_patch.h"

struct TDownloadEmulation {
    const hpatch_TStreamInput* emulation_newSyncData;
    hpatch_TFileStreamInput    newSyncFile;
};

static bool _readSyncData(ISyncPatchListener* listener,hpatch_StreamPos_t posInNewSyncData,
                          uint32_t syncDataSize,unsigned char* out_syncDataBuf){
//warning: Read newSyncData from emulation data;
//         In the actual project, these data need downloaded from server.
    TDownloadEmulation* self=(TDownloadEmulation*)listener->syncImport;
    if (!self->emulation_newSyncData->read(self->emulation_newSyncData,posInNewSyncData,out_syncDataBuf,
                                           out_syncDataBuf+syncDataSize)) return false;
    return true;
}

static void downloadEmulation_open_by(TDownloadEmulation* self,ISyncPatchListener* out_emulation,
                                      const hpatch_TStreamInput* newSyncData){
    assert(out_emulation->syncImport==0);
    self->emulation_newSyncData=newSyncData;
    out_emulation->syncImport=self;
    out_emulation->readSyncData=_readSyncData;
}

bool downloadEmulation_open(ISyncPatchListener* out_emulation,const hpatch_TStreamInput* newSyncData){
    assert(out_emulation->syncImport==0);
    TDownloadEmulation* self=(TDownloadEmulation*)malloc(sizeof(TDownloadEmulation));
    if (self==0) return false;
    memset(self,0,sizeof(*self));
    downloadEmulation_open_by(self,out_emulation,newSyncData);
    return true;
}

bool downloadEmulation_open_by_file(ISyncPatchListener* out_emulation,const char* newSyncDataPath){
    assert(out_emulation->syncImport==0);
    TDownloadEmulation* self=(TDownloadEmulation*)malloc(sizeof(TDownloadEmulation));
    if (self==0) return false;
    memset(self,0,sizeof(*self));
    if (!hpatch_TFileStreamInput_open(&self->newSyncFile,newSyncDataPath)){
        free(self);
        return false;
    }
    downloadEmulation_open_by(self,out_emulation,&self->newSyncFile.base);
    return true;
}

bool downloadEmulation_close(ISyncPatchListener* emulation){
    if (emulation==0) return true;
    TDownloadEmulation* self=(TDownloadEmulation*)emulation->syncImport;
    memset(emulation,0,sizeof(*emulation));
    if (self==0) return true;
    bool result=(0!=hpatch_TFileStreamInput_close(&self->newSyncFile));
    free(self);
    return result;
}

