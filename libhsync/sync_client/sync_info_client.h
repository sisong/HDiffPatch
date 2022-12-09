//  sync_info_client.h
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
#ifndef sync_info_client_h
#define sync_info_client_h
#include "sync_client_type.h"

typedef enum TSyncClient_resultType{
    kSyncClient_ok=0,
    kSyncClient_optionsError, //cmdline error
    kSyncClient_memError,
    kSyncClient_tempFileError,
    kSyncClient_pathTypeError,
    kSyncClient_overwritePathError,         // 5
    kSyncClient_deleteFileError,
    kSyncClient_renameFileError,
    kSyncClient_getSyncDownloadPluginError,
    kSyncClient_newSyncInfoCreateError,
    kSyncClient_newSyncInfoCloseError,      // 10
    kSyncClient_newSyncInfoDownloadError,
    kSyncClient_newSyncInfoOpenError,
    kSyncClient_newSyncInfoDataError,
    kSyncClient_newSyncInfoChecksumError,
    kSyncClient_newSyncInfoTypeError,       // 15
    kSyncClient_oldFileOpenError,
    kSyncClient_oldFileCloseError,
    kSyncClient_readOldDataError,
    kSyncClient_newFileCreateError,
    kSyncClient_newFileReopenWriteError,    // 20
    kSyncClient_newFileReopenReadError,
    kSyncClient_newFileCloseError,
    kSyncClient_writeNewDataError,
    kSyncClient_matchNewDataInOldError,
    kSyncClient_syncDataDownloadError,      // 25
    kSyncClient_syncDataCloseError,
    kSyncClient_readSyncDataBeginError,
    kSyncClient_readSyncDataError,
    kSyncClient_noStrongChecksumPluginError,
    kSyncClient_strongChecksumByteSizeError,// 30
    kSyncClient_strongChecksumOpenError,
    kSyncClient_checksumSyncDataError,
    kSyncClient_newDataCheckChecksumError,
    kSyncClient_noDecompressPluginError,
    kSyncClient_decompressOpenError,        // 35
    kSyncClient_decompressError,
    kSyncClient_newDataSizeError,
    kSyncClient_diffFileCreateError,
    kSyncClient_diffFileReopenWriteError,
    kSyncClient_diffFileCloseError,         // 40
    kSyncClient_diffFileOpenError,
    kSyncClient_saveDiffError,             
    kSyncClient_loadDiffError,
    
    //_IS_NEED_DIR_DIFF_PATCH
    kSyncClient_oldDirOpenError=51,
    kSyncClient_oldDirFilesOpenError,
    kSyncClient_oldDirFilesCloseError,
    kSyncClient_newDirOpenError,
    kSyncClient_newDirCloseError, // 55
    kSyncClient_newDirPatchBeginError,
    kSyncClient_newDirPatchFinishError,
} TNewDataSyncInfo_resultType;

typedef struct ISyncInfoListener{
    void*                    infoImport;
    hsync_TDictDecompress* (*findDecompressPlugin)(ISyncInfoListener* listener,const char* compressType,size_t dictSize);
    hpatch_TChecksum*      (*findChecksumPlugin)  (ISyncInfoListener* listener,const char* strongChecksumType);
    //loadedNewSyncInfo can null
    void                   (*loadedNewSyncInfo)   (ISyncInfoListener* listener,TNewDataSyncInfo* newSyncInfo);
    //needSyncInfo can null
    void                   (*needSyncInfo)        (ISyncInfoListener* listener,const TNeedSyncInfos* needSyncInfo);
} ISyncInfoListener;

int  TNewDataSyncInfo_open_by_file(TNewDataSyncInfo* self,const char* newSyncInfoFile,
                                   ISyncInfoListener* listener);
int  TNewDataSyncInfo_open        (TNewDataSyncInfo* self,const hpatch_TStreamInput* newSyncInfo,
                                   ISyncInfoListener* listener);
void TNewDataSyncInfo_close       (TNewDataSyncInfo* self);

int  checkNewSyncInfoType_by_file(const char* newSyncInfoFile,hpatch_BOOL* out_newIsDir);
int  checkNewSyncInfoType(const hpatch_TStreamInput* newSyncInfo,hpatch_BOOL* out_newIsDir);

#endif // sync_info_client_h
