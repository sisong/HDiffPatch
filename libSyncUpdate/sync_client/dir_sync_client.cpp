//  dir_sync_client.cpp
//  sync_client
//  Created by housisong on 2019-10-05.
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
#include "dir_sync_client.h"
#include <stdexcept>
#if (_IS_NEED_DIR_DIFF_PATCH)
#include "../../dirDiffPatch/dir_diff/dir_diff_tools.h"
#include "../../dirDiffPatch/dir_patch/new_dir_output.h"
using namespace sync_private;
using namespace hdiff_private;


#define check_r(v,errorCode) \
    do{ if (!(v)) { if (result==kSyncClient_ok) result=errorCode; \
                    if (!_inClear) goto clear; } }while(0)

static size_t getFileCount(const std::vector<std::string>& pathList){
    size_t result=0;
    for (size_t i=0; i<pathList.size(); ++i){
        const std::string& fileName=pathList[i];
        if (!isDirName(fileName))
            ++result;
    }
    return result;
}

struct CFilesStream{
    explicit CFilesStream(const std::vector<std::string>& pathList,
                          size_t kMaxOpenFileNumber,size_t kAlignSize)
    :resLimit(kMaxOpenFileNumber,getFileCount(pathList)){
        for (size_t i=0;i<pathList.size();++i){
            const std::string& fileName=pathList[i];
            if (!isDirName(fileName)){
                hpatch_StreamPos_t fileSize=getFileSize(fileName);
                resLimit.addRes(fileName,fileSize);
            }
        }
        resLimit.open();
        refStream.open(resLimit.limit.streamList,resLimit.resList.size(),kAlignSize);
    }
    CFileResHandleLimit resLimit;
    CRefStream refStream;
};


int sync_patch_dir2file(ISyncPatchListener* listener,const char* outNewFile,const TManifest& oldManifest,
                        const char* newSyncInfoFile,size_t kMaxOpenFileNumber,int threadNum){
    assert(listener!=0);
    assert(kMaxOpenFileNumber>=kMaxOpenFileNumber_limit_min);
    kMaxOpenFileNumber-=2; // for newSyncInfoFile & outNewFile
    
    int result=kSyncClient_ok;
    int _inClear=0;
    TNewDataSyncInfo         newSyncInfo;
    hpatch_TFileStreamOutput out_newData;
    
    TNewDataSyncInfo_init(&newSyncInfo);
    hpatch_TFileStreamOutput_init(&out_newData);
    result=TNewDataSyncInfo_open_by_file(&newSyncInfo,newSyncInfoFile,listener);
    check_r(result==kSyncClient_ok,result);
    check_r(hpatch_TFileStreamOutput_open(&out_newData,outNewFile,(hpatch_StreamPos_t)(-1)),
                                          kSyncClient_newFileCreateError);
    try {
        CFilesStream oldFilesStream(oldManifest.pathList,kMaxOpenFileNumber,newSyncInfo.kMatchBlockSize);
        const hpatch_TStreamInput* oldStream=oldFilesStream.refStream.stream;
        result=sync_patch(listener,&out_newData.base,oldStream,&newSyncInfo,threadNum);
    } catch (const std::exception& e){
        result=kSyncClient_oldDirFilesError;
    }
clear:
    _inClear=1;
    check_r(hpatch_TFileStreamOutput_close(&out_newData),kSyncClient_newFileCloseError);
    TNewDataSyncInfo_close(&newSyncInfo);
    return result;
}



int sync_patch_fileOrDir2dir(IDirPatchListener* patchListener,ISyncPatchListener* syncListener,
                             const char* outNewDir,const TManifest& oldManifest,
                             const char* newSyncInfoFile,size_t kMaxOpenFileNumber,int threadNum){
    assert((patchListener!=0)&&(syncListener!=0));
    assert(kMaxOpenFileNumber>=kMaxOpenFileNumber_limit_min);
    kMaxOpenFileNumber-=2; // for newSyncInfoFile & outNewFile
    
    int result=kSyncClient_ok;
    int _inClear=0;
    TNewDataSyncInfo         newSyncInfo;
    hpatch_TFileStreamOutput out_newData;
    
    TNewDataSyncInfo_init(&newSyncInfo);
    hpatch_TFileStreamOutput_init(&out_newData);
    result=TNewDataSyncInfo_open_by_file(&newSyncInfo,newSyncInfoFile,syncListener);
    check_r(result==kSyncClient_ok,result);
    check_r(newSyncInfo.isDirSyncInfo,kSyncClient_newSyncInfoTypeError);
    
    
    check_r(hpatch_TFileStreamOutput_open(&out_newData,outNewDir,(hpatch_StreamPos_t)(-1)),
            kSyncClient_newFileCreateError);
    try {
        CFilesStream oldFilesStream(oldManifest.pathList,kMaxOpenFileNumber,newSyncInfo.kMatchBlockSize);
        const hpatch_TStreamInput* oldStream=oldFilesStream.refStream.stream;
        result=sync_patch(syncListener,&out_newData.base,oldStream,&newSyncInfo,threadNum);
    } catch (const std::exception& e){
        result=kSyncClient_oldDirFilesError;
    }
clear:
    _inClear=1;
    check_r(hpatch_TFileStreamOutput_close(&out_newData),kSyncClient_newFileCloseError);
    TNewDataSyncInfo_close(&newSyncInfo);
    return result;
}


#endif //_IS_NEED_DIR_DIFF_PATCH
