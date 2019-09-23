//  sync_client.cpp
//  sync_client
//
//  Created by housisong on 2019-09-18.
//  Copyright © 2019 sisong. All rights reserved.

#include "download_emulation.h"
#include "../../file_for_patch.h"

struct TDownloadEmulation {
    const hpatch_TStreamInput* newSyncData;
    hpatch_TFileStreamInput    file;
};

static bool _readSyncData(ISyncPatchListener* listener,unsigned char* out_syncDataBuf,
                          hpatch_StreamPos_t posInNewSyncData,uint32_t syncDataSize){
    TDownloadEmulation* self=(TDownloadEmulation*)listener->import;
    return hpatch_FALSE!=self->newSyncData->read(self->newSyncData,posInNewSyncData,
                                                 out_syncDataBuf,out_syncDataBuf+syncDataSize);
}

static void downloadEmulation_open_by(TDownloadEmulation* self,ISyncPatchListener* out_emulation,
                                      const hpatch_TStreamInput* newSyncData){
    memset(out_emulation,0,sizeof(*out_emulation));
    self->newSyncData=newSyncData;
    out_emulation->import=self;
    out_emulation->readSyncData=_readSyncData;
}

bool downloadEmulation_open(ISyncPatchListener* out_emulation,const hpatch_TStreamInput* newSyncData){
    assert(out_emulation->import==0);
    TDownloadEmulation* self=(TDownloadEmulation*)malloc(sizeof(TDownloadEmulation));
    if (self==0) return false;
    memset(self,0,sizeof(*self));
    downloadEmulation_open_by(self,out_emulation,newSyncData);
    return true;
}

bool downloadEmulation_open_by_file(ISyncPatchListener* out_emulation,const char* newSyncDataPath){
    assert(out_emulation->import==0);
    TDownloadEmulation* self=(TDownloadEmulation*)malloc(sizeof(TDownloadEmulation));
    if (self==0) return false;
    memset(self,0,sizeof(*self));
    if (!hpatch_TFileStreamInput_open(&self->file,newSyncDataPath)){
        free(self);
        return false;
    }
    downloadEmulation_open_by(self,out_emulation,&self->file.base);
    return true;
}

bool downloadEmulation_close(ISyncPatchListener* emulation){
    if (emulation==0) return true;
    TDownloadEmulation* self=(TDownloadEmulation*)emulation->import;
    memset(emulation,0,sizeof(*emulation));
    if (self==0) return true;
    bool result=hpatch_TFileStreamInput_close(&self->file);
    free(self);
    return result;
}
