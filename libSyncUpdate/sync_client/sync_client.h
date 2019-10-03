//  sync_client.h
//  sync_client
//  Created by housisong on 2019-09-18.
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
#ifndef sync_client_h
#define sync_client_h
#include "sync_client_type.h"
#ifdef __cplusplus
extern "C" {
#endif
    
typedef enum TSyncClient_resultType{
    kSyncClient_ok,
    kSyncClient_optionsError, //cmdline error
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
        bool              isChecksumNewSyncInfo;
        bool              isChecksumNewSyncData;
        hpatch_TDecompress* (*findDecompressPlugin)(ISyncPatchListener* listener,const char* compressType);
        hpatch_TChecksum*     (*findChecksumPlugin)(ISyncPatchListener* listener,const char* strongChecksumType);
        void   (*needSyncMsg)(ISyncPatchListener* listener,uint32_t needSyncCount,  // needSyncMsg can nil
                              hpatch_StreamPos_t posInNewSyncData,uint32_t syncDataSize);
        bool  (*readSyncData)(ISyncPatchListener* listener,unsigned char* out_syncDataBuf,
                              hpatch_StreamPos_t posInNewSyncData,uint32_t syncDataSize);
    } ISyncPatchListener;

int  TNewDataSyncInfo_open_by_file(TNewDataSyncInfo* self,
                                   const char* newSyncInfoFile,ISyncPatchListener *listener);
int  TNewDataSyncInfo_open(TNewDataSyncInfo* self,
                           const hpatch_TStreamInput* newSyncInfo,ISyncPatchListener *listener);
void TNewDataSyncInfo_close(TNewDataSyncInfo* self);


int sync_patch_by_file(const char* outNewPath,const char* oldPath,
                       const char* newSyncInfoFile,ISyncPatchListener* listener,int threadNum=0);

int sync_patch(const hpatch_TStreamOutput* out_newStream,const hpatch_TStreamInput* oldStream,
               const TNewDataSyncInfo* newSyncInfo,ISyncPatchListener* listener,int threadNum=0);

#ifdef __cplusplus
}
#endif
        
#endif // sync_client_h
