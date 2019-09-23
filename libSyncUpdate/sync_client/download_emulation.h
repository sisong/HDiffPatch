//  download_emulation.h
//  sync_client
//
//  Created by housisong on 2019-09-23.
//  Copyright © 2019 sisong. All rights reserved.
#ifndef download_emulation_h
#define download_emulation_h
#include "sync_client.h"
#ifdef __cplusplus
extern "C" {
#endif

//downloadEmulation for patch test:
//  when need to download part of newSyncData, emulation read it from local data;
bool downloadEmulation_open_by_file(ISyncPatchListener* out_emulation,const char* newSyncDataPath);
bool downloadEmulation_open(ISyncPatchListener* out_emulation,const hpatch_TStreamInput* newSyncData);
bool downloadEmulation_close(ISyncPatchListener* emulation);

#ifdef __cplusplus
}
#endif
        
#endif // download_emulation_h
