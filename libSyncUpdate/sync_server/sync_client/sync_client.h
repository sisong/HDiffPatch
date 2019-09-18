//  sync_client.h
//  sync_client
//
//  Created by housisong on 2019-09-18.
//  Copyright © 2019 sisong. All rights reserved.
#ifndef sync_client_h
#define sync_client_h
#include "sync_client_type.h"
#ifdef __cplusplus
extern "C" {
#endif
    
typedef enum TSyncClient_resultType{
    kSyncClient_ok,
    kSyncClient_infoFileOpenError,
    kSyncClient_infoFileCloseError,
} TNewDataSyncInfo_resultType;

int  TNewDataSyncInfo_open(TNewDataSyncInfo* self,const hpatch_TStreamInput* newSyncInfo);
int  TNewDataSyncInfo_openByFile(TNewDataSyncInfo* self,const char* newSyncInfoPath);
void TNewDataSyncInfo_close(TNewDataSyncInfo* self);

typedef struct ISyncPatchListener{
    
} ISyncPatchListener;

int sync_patch(const hpatch_TStreamInput*  oldStream,
               const hpatch_TStreamOutput* out_newStream,
               const TNewDataSyncInfo* newInfo,ISyncPatchListener* listener);

#ifdef __cplusplus
}
#endif
        
#endif // sync_client_h
