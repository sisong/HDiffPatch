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
#if (_IS_NEED_DIR_DIFF_PATCH)
#define check(v,errorCode) \
    do{ if (!(v)) { if (result==kSyncClient_ok) result=errorCode; \
                    if (!_inClear) goto clear; } }while(0)


void get_oldManifest(IDirPathIgnore* filter,const char* oldPath,TManifest& out_oldManifest){
    out_oldManifest.rootPath=oldPath;
    out_oldManifest.pathList.clear();
    if (hpatch_getIsDirName(oldPath))
        getDirAllPathList(out_oldManifest.rootPath,out_oldManifest.pathList,filter,true);
}

int sync_patch_2file(ISyncPatchListener* listener,const char* outNewFile,const TManifest& oldManifest,
                     const char* newSyncInfoFile,size_t kMaxOpenFileNumber,int threadNum){
    int result=kSyncClient_ok;
    int _inClear=0;
    TNewDataSyncInfo         newSyncInfo;
    hpatch_TFileStreamInput  oldData;
    hpatch_TFileStreamOutput out_newData;
    const hpatch_TStreamInput* oldStream=0;
    
    TNewDataSyncInfo_init(&newSyncInfo);
    hpatch_TFileStreamInput_init(&oldData);
    hpatch_TFileStreamOutput_init(&out_newData);
    result=TNewDataSyncInfo_open_by_file(&newSyncInfo,newSyncInfoFile,listener);
    check(result==kSyncClient_ok,result);
    
    //todo:
    
    check(hpatch_TFileStreamOutput_open(&out_newData,outNewFile,(hpatch_StreamPos_t)(-1)),
          kSyncClient_newFileCreateError);
    
    result=sync_patch(listener,&out_newData.base,oldStream,&newSyncInfo,threadNum);
clear:
    _inClear=1;
    check(hpatch_TFileStreamOutput_close(&out_newData),kSyncClient_newFileCloseError);
    check(hpatch_TFileStreamInput_close(&oldData),kSyncClient_oldFileCloseError);
    TNewDataSyncInfo_close(&newSyncInfo);
    return result;
}

#endif //_IS_NEED_DIR_DIFF_PATCH
