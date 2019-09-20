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
    kSyncClient_memError,
    kSyncClient_newSyncInfoOpenError,
    kSyncClient_newSyncInfoCloseError,
    kSyncClient_oldFileOpenError,
    kSyncClient_oldFileCloseError,
    kSyncClient_newFileCreateError,
    kSyncClient_newFileCloseError,
    kSyncClient_matchNewDataInOldError,
    kSyncClient_readSyncDataError,
    kSyncClient_decompressError,
    kSyncClient_readOldDataError,
    kSyncClient_writeNewDataError,
    
} TNewDataSyncInfo_resultType;

int  TNewDataSyncInfo_open(TNewDataSyncInfo* self,const hpatch_TStreamInput* newSyncInfo);
int  TNewDataSyncInfo_open_by_file(TNewDataSyncInfo* self,const char* newSyncInfoPath);
void TNewDataSyncInfo_close(TNewDataSyncInfo* self);

typedef struct ISyncPatchListener{
    void      (*needSyncMsg)(ISyncPatchListener* listener,hpatch_StreamPos_t posInNewSyncData,uint32_t syncDataSize);
    void (*addedNeedSyncMsg)(ISyncPatchListener* listener,hpatch_StreamPos_t posInNewSyncData,uint32_t syncDataSize);
    bool     (*readSyncData)(ISyncPatchListener* listener,unsigned char* out_syncDataBuf,
                             hpatch_StreamPos_t posInNewSyncData,uint32_t syncDataSize);
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
