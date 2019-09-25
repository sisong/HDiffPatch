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
    kSyncClient_newSyncInfoTypeError,
    kSyncClient_noStrongChecksumPluginError,
    kSyncClient_strongChecksumByteSizeError,
    kSyncClient_noDecompressPluginError,
    kSyncClient_newSyncInfoDataError,
    kSyncClient_newSyncInfoChecksumError,
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
    kSyncClient_strongChecksumOpenError,
    kSyncClient_checksumSyncDataError,
} TNewDataSyncInfo_resultType;
    

    typedef struct ISyncPatchListener{
        void*             import;
         // isChecksumNewSyncInfo can nil, meen true
        bool               (*isChecksumNewSyncInfo)(ISyncPatchListener* listener);
        hpatch_TDecompress* (*findDecompressPlugin)(ISyncPatchListener* listener,const char* compressType);
        hpatch_TChecksum*     (*findChecksumPlugin)(ISyncPatchListener* listener,const char* strongChecksumType);
        void   (*needSyncMsg)(ISyncPatchListener* listener,uint32_t needSyncCount,  // needSyncMsg can nil
                              hpatch_StreamPos_t posInNewSyncData,uint32_t syncDataSize);
        bool  (*readSyncData)(ISyncPatchListener* listener,unsigned char* out_syncDataBuf,
                              hpatch_StreamPos_t posInNewSyncData,uint32_t syncDataSize);
    } ISyncPatchListener;

int  TNewDataSyncInfo_open_by_file(TNewDataSyncInfo* self,
                                   const char* newSyncInfoPath,ISyncPatchListener *listener);
int  TNewDataSyncInfo_open(TNewDataSyncInfo* self,
                           const hpatch_TStreamInput* newSyncInfo,ISyncPatchListener *listener);
void TNewDataSyncInfo_close(TNewDataSyncInfo* self);


int sync_patch_by_file(const char* out_newPath,const char* oldPath,
                       const char* newSyncInfoPath,ISyncPatchListener* listener);

int sync_patch(const hpatch_TStreamOutput* out_newStream,const hpatch_TStreamInput* oldStream,
               const TNewDataSyncInfo* newSyncInfo,ISyncPatchListener* listener);

#ifdef __cplusplus
}
#endif
        
#endif // sync_client_h
