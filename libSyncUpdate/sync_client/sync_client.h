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
    kSyncClient_TODO_Error,
} TNewDataSyncInfo_resultType;

int  TNewDataSyncInfo_open(TNewDataSyncInfo* self,const hpatch_TStreamInput* newSyncInfo);
int  TNewDataSyncInfo_openByFile(TNewDataSyncInfo* self,const char* newSyncInfoPath);
void TNewDataSyncInfo_close(TNewDataSyncInfo* self);

typedef struct ISyncPatchListener{
    
} ISyncPatchListener;

int sync_patch(const hpatch_TStreamInput* oldStream,const hpatch_TStreamInput* newSyncInfoStream,
               const hpatch_TStreamOutput* out_newStream,ISyncPatchListener* listener);

int sync_patch_by_info(const hpatch_TStreamInput* oldStream,const TNewDataSyncInfo* newSyncInfo,
                       const hpatch_TStreamOutput* out_newStream,ISyncPatchListener* listener);
int sync_patch_by_file(const char* oldPath,const char* newSyncInfoPath,
                       const char* out_newPath,ISyncPatchListener* listener);

#ifdef __cplusplus
}
#endif
        
#endif // sync_client_h
