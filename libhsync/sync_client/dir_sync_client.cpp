//  dir_sync_client.cpp
//  sync_client
//  Created by housisong on 2019-10-05.
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
#include "dir_sync_client.h"
#include "sync_client_type_private.h"
#include "sync_diff_data.h"
#if (_IS_NEED_DIR_DIFF_PATCH)
#include <stdexcept>
#include "../../dirDiffPatch/dir_diff/dir_diff_tools.h"
#include "../../dirDiffPatch/dir_patch/new_dir_output.h"
#include "../../dirDiffPatch/dir_patch/dir_patch_tools.h"
#include "sync_client_private.h"
using namespace hdiff_private;

namespace sync_private{

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
    explicit CFilesStream():resLimit(0){}
    ~CFilesStream(){ if (resLimit) delete resLimit; }
    bool open(const std::vector<std::string>& pathList,size_t kMaxOpenFileNumber,size_t kAlignSize){
        assert(resLimit==0);
        try{
            resLimit=new CFileResHandleLimit(kMaxOpenFileNumber,getFileCount(pathList));
            for (size_t i=0;i<pathList.size();++i){
                const std::string& fileName=pathList[i];
                if (!isDirName(fileName)){
                    hpatch_StreamPos_t fileSize=getFileSize(fileName);
                    resLimit->addRes(fileName,fileSize);
                }
            }
            resLimit->open();
            refStream.open(resLimit->limit.streamList,resLimit->resList.size(),kAlignSize);
        } catch (const std::exception& e){
            fprintf(stderr,"CFilesStream::open error: %s",e.what());
            return false;
        }
        return true;
    }
    bool closeFileHandles(){ return resLimit->closeFileHandles(); }
    CFileResHandleLimit* resLimit;
    CRefStream refStream;
};


static int _sync_patch_2file(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,TSyncDiffData* diffData,
                             const TManifest& oldManifest,const char* newSyncInfoFile,
                             const char* outNewFile,hpatch_BOOL isOutNewContinue,
                             const hpatch_TStreamOutput* out_diffStream,const hpatch_TStreamInput* diffContinue,
                             size_t kMaxOpenFileNumber,int threadNum){
    assert(listener!=0);
    assert(kMaxOpenFileNumber>=kMaxOpenFileNumber_limit_min);
    kMaxOpenFileNumber-=2; // for newSyncInfoFile & outNewFile

    int result=kSyncClient_ok;
    int _inClear=0;
    TNewDataSyncInfo            newSyncInfo;
    hpatch_TFileStreamOutput    out_newData;
    const hpatch_TStreamInput*  newDataContinue=0;
    hpatch_TStreamInput         _newDataContinue;
    CFilesStream                oldFilesStream;

    TNewDataSyncInfo_init(&newSyncInfo);
    hpatch_TFileStreamOutput_init(&out_newData);
    result=TNewDataSyncInfo_open_by_file(&newSyncInfo,newSyncInfoFile,listener);
    check_r(result==kSyncClient_ok,result);
    check_r(oldFilesStream.open(oldManifest.pathList,kMaxOpenFileNumber,
                                newSyncInfo.kSyncBlockSize), kSyncClient_oldDirFilesOpenError);
    if (outNewFile){
        check_r(_open_continue_out(isOutNewContinue,outNewFile,&out_newData,&_newDataContinue,newSyncInfo.newDataSize),
               isOutNewContinue?kSyncClient_newFileReopenWriteError:kSyncClient_newFileCreateError);
        if (isOutNewContinue) newDataContinue=&_newDataContinue;
    }
    result=_sync_patch(listener,syncDataListener,diffData,oldFilesStream.refStream.stream,&newSyncInfo,
                       outNewFile?&out_newData.base:0,newDataContinue,out_diffStream,diffContinue,threadNum);
    check_r(oldFilesStream.closeFileHandles(),kSyncClient_oldDirFilesCloseError);
clear:
    _inClear=1;
    check_r(hpatch_TFileStreamOutput_close(&out_newData),kSyncClient_newFileCloseError);
    TNewDataSyncInfo_close(&newSyncInfo);
    return result;
}


struct CNewDirOut{
    inline CNewDirOut(){ TNewDirOutput_init(&_newDir); }
    bool openDir(TNewDataSyncInfo* newSyncInfo,const char* outNewDir,IDirPatchListener* listener,
                 const hpatch_TStreamOutput** out_newDirStream,size_t kAlignSize){
        assert(_newDir._newRootDir==0);
        assert(newSyncInfo->dir_newNameList_isCString);
        _newDir.newUtf8PathList=(const char* const *)newSyncInfo->dir_utf8NewNameList;
        _newDir.newExecuteList=newSyncInfo->dir_newExecuteIndexList;
        _newDir.newRefSizeList=newSyncInfo->dir_newSizeList;
        _newDir.newDataSize=newSyncInfo->newDataSize;
        _newDir.newPathCount=newSyncInfo->dir_newPathCount;
        _newDir.newRefFileCount=newSyncInfo->dir_newPathCount;
        _newDir.newExecuteCount=newSyncInfo->dir_newExecuteCount;
        _newDir._newRootDir=_tempbuf;
        _newDir._newRootDir_bufEnd=_newDir._newRootDir+sizeof(_tempbuf);
        _newDir._newRootDir_end=setDirPath(_newDir._newRootDir,_newDir._newRootDir_bufEnd,outNewDir);
        return TNewDirOutput_openDir(&_newDir,listener,kAlignSize,out_newDirStream)!=0;
    }
    bool closeFileHandles(){ return TNewDirOutput_closeNewDirHandles(&_newDir)!=0; }
    bool closeDir(){ return TNewDirOutput_close(&_newDir)!=0; }
    char   _tempbuf[hpatch_kPathMaxSize];
    TNewDirOutput _newDir;
};

static int _sync_patch_2dir(IDirPatchListener* patchListener,IDirSyncPatchListener* syncListener,
                            IReadSyncDataListener* syncDataListener,TSyncDiffData* diffData,
                            const TManifest& oldManifest,const char* newSyncInfoFile,
                            const char* outNewDir,const hpatch_TStreamOutput* out_diffStream,
                            size_t kMaxOpenFileNumber,int threadNum){
    assert((patchListener!=0)&&(syncListener!=0));
    assert(kMaxOpenFileNumber>=kMaxOpenFileNumber_limit_min);
    kMaxOpenFileNumber-=2; // for newSyncInfoFile & outNewFile

    int result=kSyncClient_ok;
    int _inClear=0;
    TNewDataSyncInfo            newSyncInfo;
    const hpatch_TStreamOutput* out_newData=0;
    CNewDirOut                  newDirOut;
    CFilesStream                oldFilesStream;
    size_t                      kAlignSize=1;

    TNewDataSyncInfo_init(&newSyncInfo);
    result=TNewDataSyncInfo_open_by_file(&newSyncInfo,newSyncInfoFile,syncListener);
    check_r(result==kSyncClient_ok,result);
    check_r(newSyncInfo.isDirSyncInfo,kSyncClient_newSyncInfoTypeError);
    kAlignSize=newSyncInfo.kSyncBlockSize;
    if (outNewDir)
        check_r(newDirOut.openDir(&newSyncInfo,outNewDir,patchListener,&out_newData,kAlignSize),
                kSyncClient_newDirOpenError);
    if (syncListener->patchBegin)
        check_r(syncListener->patchBegin(syncListener,&newSyncInfo,&newDirOut._newDir),
                kSyncClient_newDirPatchBeginError);
    check_r(oldFilesStream.open(oldManifest.pathList,kMaxOpenFileNumber,kAlignSize),
            kSyncClient_oldDirFilesOpenError);

    result=_sync_patch(syncListener,syncDataListener,diffData,
                       oldFilesStream.refStream.stream,&newSyncInfo,out_newData,0,0,0,threadNum);
    check_r(newDirOut.closeFileHandles(),kSyncClient_newDirCloseError);
    check_r(oldFilesStream.closeFileHandles(),kSyncClient_oldDirFilesCloseError);
    if (syncListener->patchFinish)
        check_r(syncListener->patchFinish(syncListener,result==kSyncClient_ok,&newSyncInfo,
                                          &newDirOut._newDir), kSyncClient_newDirPatchFinishError);
clear:
    _inClear=1;
    check_r(newDirOut.closeDir(),kSyncClient_newDirCloseError);
    TNewDataSyncInfo_close(&newSyncInfo);
    return result;
}

}
using namespace sync_private;

int sync_patch_2file(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                     const TManifest& oldManifest,const char* newSyncInfoFile,
                     const char* outNewFile,hpatch_BOOL isOutNewContinue,
                     size_t kMaxOpenFileNumber,int threadNum){
    return _sync_patch_2file(listener,syncDataListener,0,oldManifest,newSyncInfoFile,
                             outNewFile,isOutNewContinue,0,0,kMaxOpenFileNumber,threadNum);
}

int sync_local_diff_2file(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                          const TManifest& oldManifest,const char* newSyncInfoFile,const char* outDiffFile,
                          hpatch_BOOL isOutDiffContinue,size_t kMaxOpenFileNumber,int threadNum){
    int result=kSyncClient_ok;
    int _inClear=0;
    hpatch_TFileStreamOutput out_diffData;
    hpatch_TFileStreamOutput_init(&out_diffData);
    const hpatch_TStreamInput* diffContinue=0;
    hpatch_TStreamInput _diffContinue;
    
    check_r(_open_continue_out(isOutDiffContinue,outDiffFile,&out_diffData,&_diffContinue,(hpatch_StreamPos_t)(-1)),
           isOutDiffContinue?kSyncClient_diffFileReopenWriteError:kSyncClient_diffFileCreateError);
    if (isOutDiffContinue) diffContinue=&_diffContinue;
    
    result=_sync_patch_2file(listener,syncDataListener,0,oldManifest,newSyncInfoFile,0,hpatch_FALSE,
                             &out_diffData.base,diffContinue,kMaxOpenFileNumber,threadNum);
    /*if ((result==kSyncClient_ok)&&isOutDiffContinue
        &&(out_diffData.out_length>0)&&(out_diffData.out_length<diffContinue->streamSize)){
        check_r(hpatch_TFileStreamOutput_truncate(&out_diffData,out_diffData.out_length),
                kSyncClient_diffFileReopenWriteError); // retained data error
    }*/
clear:
    _inClear=1;
    check_r(hpatch_TFileStreamOutput_close(&out_diffData),kSyncClient_diffFileCloseError);
    return result;
}

int sync_local_patch_2file(ISyncInfoListener* listener,const char* inDiffFile,
                           const TManifest& oldManifest,const char* newSyncInfoFile,const char* outNewFile,
                           size_t kMaxOpenFileNumber,int threadNum){
    int result=kSyncClient_ok;
    int _inClear=0;
    TSyncDiffData diffData;
    hpatch_TFileStreamInput in_diffData;
    hpatch_TFileStreamInput_init(&in_diffData);
    check_r(hpatch_TFileStreamInput_open(&in_diffData,inDiffFile),
            kSyncClient_diffFileOpenError);
    check_r(_TSyncDiffData_load(&diffData,&in_diffData.base),kSyncClient_loadDiffError);
    return _sync_patch_2file(listener,0,&diffData,oldManifest,newSyncInfoFile,outNewFile,hpatch_FALSE,
                             0,0,kMaxOpenFileNumber,threadNum);
clear:
    _inClear=1;
    check_r(hpatch_TFileStreamInput_close(&in_diffData),kSyncClient_diffFileCloseError);
    return result;
}


int sync_patch_2dir(IDirPatchListener* patchListener,IDirSyncPatchListener* syncListener,
                    IReadSyncDataListener* syncDataListener,
                    const TManifest& oldManifest,const char* newSyncInfoFile,const char* outNewDir,
                    size_t kMaxOpenFileNumber,int threadNum){
    return _sync_patch_2dir(patchListener,syncListener,syncDataListener,0,oldManifest,newSyncInfoFile,outNewDir,0,
                            kMaxOpenFileNumber,threadNum);
}

int sync_local_patch_2dir(IDirPatchListener* patchListener,IDirSyncPatchListener* syncListener,
                          const char* inDiffFile,
                          const TManifest& oldManifest,const char* newSyncInfoFile,const char* outNewDir,
                          size_t kMaxOpenFileNumber,int threadNum){
    int result=kSyncClient_ok;
    int _inClear=0;
    TSyncDiffData diffData;
    hpatch_TFileStreamInput in_diffData;
    hpatch_TFileStreamInput_init(&in_diffData);
    check_r(hpatch_TFileStreamInput_open(&in_diffData,inDiffFile),
            kSyncClient_diffFileOpenError);
    check_r(_TSyncDiffData_load(&diffData,&in_diffData.base),kSyncClient_loadDiffError);
    return _sync_patch_2dir(patchListener,syncListener,0,&diffData,oldManifest,newSyncInfoFile,outNewDir,0,
                            kMaxOpenFileNumber,threadNum);
clear:
    _inClear=1;
    check_r(hpatch_TFileStreamInput_close(&in_diffData),kSyncClient_diffFileCloseError);
    return result;
}

#endif //_IS_NEED_DIR_DIFF_PATCH
