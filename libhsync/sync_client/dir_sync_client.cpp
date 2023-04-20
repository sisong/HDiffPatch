//  dir_sync_client.cpp
//  sync_client
//  Created by housisong on 2019-10-05.
/*
 The MIT License (MIT)
 Copyright (c) 2019-2023 HouSisong

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
    bool open(const std::vector<std::string>& pathList,const char* rootPath,
              size_t kMaxOpenFileNumber,size_t kAlignSize){
        assert(resLimit==0);
        std::vector<hpatch_StreamPos_t> sizeList(pathList.size(),0);
        size_t fileCount=0;
        try{
            for (size_t i=0;i<pathList.size();++i){
                const std::string& fileName=pathList[i];
                if (!isDirName(fileName)){
                    hpatch_StreamPos_t fileSize=getFileSize(fileName);
                    if (fileSize>0){
                        sizeList[i]=fileSize;
                        ++fileCount;
                    }
                }
            }
        } catch (const std::exception& e){
            fprintf(stderr,"CFilesStream::getFileSize error: %s",e.what());
            return false;
        }
        try{
            TNewDataSyncInfo_dir dirInfo={0};
            dirInfo.dir_newPathCount=pathList.size();
            dirInfo.dir_newNameList_isCString=hpatch_FALSE;
            dirInfo.dir_utf8NewNameList=pathList.data();
            dirInfo.dir_utf8NewRootPath=rootPath;
            dirInfo.dir_newSizeList=sizeList.data();
            TNewDataSyncInfo_dir_saveTo(&dirInfo,dirInfoSavedData);
        } catch (const std::exception& e){
            fprintf(stderr,"CFilesStream::TNewDataSyncInfo_dir_saveTo error: %s",e.what());
            return false;
        }
        try{
            resLimit=new CFileResHandleLimit(kMaxOpenFileNumber,fileCount+1);
            resLimit->addBufRes(dirInfoSavedData.data(),dirInfoSavedData.size());
            for (size_t i=0;i<pathList.size();++i){
                hpatch_StreamPos_t fileSize=sizeList[i];
                if (fileSize>0)
                    resLimit->addRes(pathList[i],fileSize);
            }
            resLimit->open();
            refStream.open(resLimit->limit.streamList,resLimit->limit.streamCount,kAlignSize);
        } catch (const std::exception& e){
            fprintf(stderr,"CFilesStream::open error: %s",e.what());
            return false;
        }
        return true;
    }
    bool closeFileHandles(){ return resLimit->closeFileHandles(); }
    CFileResHandleLimit*        resLimit;
    CRefStream                  refStream;
    std::vector<hpatch_byte>    dirInfoSavedData;
};


static TSyncClient_resultType
           _sync_patch_2file(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,TSyncDiffData* diffData,
                             const TManifest& oldManifest,const char* newSyncInfoFile,
                             const char* outNewFile,hpatch_BOOL isOutNewContinue,
                             const hpatch_TStreamOutput* out_diffStream,TSyncDiffType diffType,const hpatch_TStreamInput* diffContinue,
                             size_t kMaxOpenFileNumber,int threadNum){
    assert(listener!=0);
    assert(kMaxOpenFileNumber>=kMaxOpenFileNumber_limit_min);
    kMaxOpenFileNumber-=2; // for newSyncInfoFile & outNewFile

    TSyncClient_resultType result=kSyncClient_ok;
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
    check_r(oldFilesStream.open(oldManifest.pathList,oldManifest.rootPath.c_str(),kMaxOpenFileNumber,
                                newSyncInfo.kSyncBlockSize), kSyncClient_oldDirFilesOpenError);
    if (outNewFile){
        check_r(_open_continue_out(isOutNewContinue,outNewFile,&out_newData,&_newDataContinue,newSyncInfo.newDataSize),
               isOutNewContinue?kSyncClient_newFileReopenWriteError:kSyncClient_newFileCreateError);
        if (isOutNewContinue) newDataContinue=&_newDataContinue;
    }
    result=_sync_patch(listener,syncDataListener,diffData,oldFilesStream.refStream.stream,&newSyncInfo,
                       outNewFile?&out_newData.base:0,newDataContinue,
                       out_diffStream,diffType,diffContinue,threadNum);
    check_r(oldFilesStream.closeFileHandles(),kSyncClient_oldDirFilesCloseError);
clear:
    _inClear=1;
    check_r(hpatch_TFileStreamOutput_close(&out_newData),kSyncClient_newFileCloseError);
    TNewDataSyncInfo_close(&newSyncInfo);
    return result;
}

struct _TNewDirOut_openInfo{
    TNewDataSyncInfo*   newSyncInfo;
    IDirPatchListener*  listener;
    size_t              kAlignSize;
};

struct CNewDirOut{
    inline CNewDirOut(){
        TNewDirOutput_init(&_newDir); 
        memset(&_openInfo,0,sizeof(_openInfo));
        memset(&_stream,0,sizeof(_stream));
        
        errResult=kSyncClient_ok;
        _stream_dir=0;
        _dirInfoBuf=0;
        _dirInfoAlignSize=0;
    }
    inline ~CNewDirOut(){ _clear_dirInfo(); }

    bool openInfo(TNewDataSyncInfo* newSyncInfo,const char* outNewDir,IDirPatchListener* listener,
                  size_t kAlignSize,const hpatch_TStreamOutput** out_newDirStream){
        assert(_dirInfoBuf==0);
        assert(_openInfo.newSyncInfo==0);
        assert(_newDir._newRootDir==0);
        assert(newSyncInfo->dirInfo.dir_newNameList_isCString);
        assert(newSyncInfo->dirInfoSavedSize>0);
        assert(_newDir._newRootDir==0);
        _dirInfoAlignSize=(size_t)toAlignRangeSize(newSyncInfo->dirInfoSavedSize,kAlignSize);
        assert(_dirInfoAlignSize<=newSyncInfo->newDataSize);
        _newDir._newRootDir=_tempbuf;
        _newDir._newRootDir_bufEnd=_newDir._newRootDir+sizeof(_tempbuf);
        _newDir._newRootDir_end=setDirPath(_newDir._newRootDir,_newDir._newRootDir_bufEnd,outNewDir);
        if (_newDir._newRootDir_end==0) return false;

        _dirInfoBuf=(hpatch_byte*)malloc(_dirInfoAlignSize);
        if (_dirInfoBuf==0) return false;
        _openInfo.newSyncInfo=newSyncInfo;
        _openInfo.listener=listener;
        _openInfo.kAlignSize=kAlignSize;

        _stream.streamImport=this;
        _stream.streamSize=newSyncInfo->newDataSize;
        _stream.write=CNewDirOut::_write;
        *out_newDirStream=&_stream;
        return true;
    }
    bool _openDir(const hpatch_TStreamOutput** out_newDirStream){
        const TNewDataSyncInfo_dir* dirInfo=&_openInfo.newSyncInfo->dirInfo;
        _newDir.newUtf8PathList=(const char* const *)dirInfo->dir_utf8NewNameList;
        _newDir.newExecuteList=dirInfo->dir_newExecuteIndexList;
        _newDir.newRefSizeList=dirInfo->dir_newSizeList;
        _newDir.newDataSize=_openInfo.newSyncInfo->newDataSize-_dirInfoAlignSize;
        _newDir.newPathCount=dirInfo->dir_newPathCount;
        _newDir.newRefFileCount=dirInfo->dir_newPathCount;
        _newDir.newExecuteCount=dirInfo->dir_newExecuteCount;
        return TNewDirOutput_openDir(&_newDir,_openInfo.listener,_openInfo.kAlignSize,out_newDirStream)!=0;
    }
    bool closeFileHandles(){ return TNewDirOutput_closeNewDirHandles(&_newDir)!=0; }
    bool closeDir(){ return TNewDirOutput_close(&_newDir)!=0; }
    char   _tempbuf[hpatch_kPathMaxSize];
    _TNewDirOut_openInfo  _openInfo;
    TNewDirOutput         _newDir;
    hpatch_TStreamOutput  _stream;
    hpatch_TStreamOutput* _stream_dir;
    hpatch_byte*          _dirInfoBuf;
    size_t                _dirInfoAlignSize;
    TSyncClient_resultType  errResult;
    inline void _clear_dirInfo(){
        if (_dirInfoBuf){
            free(_dirInfoBuf);
            _dirInfoBuf=0;
        }
    }
    bool _initDir(){
        assert(_stream_dir==0);
        assert(_dirInfoBuf!=0);
        errResult=TNewDataSyncInfo_dir_load(&_openInfo.newSyncInfo->dirInfo,_dirInfoBuf,
                                             _openInfo.newSyncInfo->dirInfoSavedSize);
        _clear_dirInfo();
        if (errResult!=kSyncClient_ok)
            return false;
        if (!_openDir((const hpatch_TStreamOutput**)&_stream_dir)){
            errResult=kSyncClient_newDirOpenError;
            return false;
        }
        return true;
    }

    static hpatch_BOOL _write(const struct hpatch_TStreamOutput* stream,hpatch_StreamPos_t writeToPos,
                              const unsigned char* data,const unsigned char* data_end){
        CNewDirOut* self=(CNewDirOut*)stream->streamImport;
        const size_t dirInfoAlignSize=self->_dirInfoAlignSize;
        if (writeToPos<dirInfoAlignSize){
            assert(self->_dirInfoBuf!=0);
            const size_t wpos=(size_t)writeToPos;
            size_t wlen=data_end-data;
            if (wpos+wlen>dirInfoAlignSize)
                wlen=dirInfoAlignSize-wpos;
            memcpy(self->_dirInfoBuf+wpos,data,wlen);
            data+=wlen;
            writeToPos+=wlen;
            if (writeToPos==dirInfoAlignSize){
                if (!self->_initDir())
                    return hpatch_FALSE;
            }
            if (data==data_end) return hpatch_TRUE;
        }
        assert(self->_stream_dir!=0);
        return self->_stream_dir->write(self->_stream_dir,writeToPos-dirInfoAlignSize,data,data_end);
    }
};

static TSyncClient_resultType
           _sync_patch_2dir(IDirPatchListener* patchListener,IDirSyncPatchListener* syncListener,
                            IReadSyncDataListener* syncDataListener,TSyncDiffData* diffData,
                            const TManifest& oldManifest,const char* newSyncInfoFile,const char* outNewDir,
                            const hpatch_TStreamOutput* out_diffStream,TSyncDiffType diffType,const hpatch_TStreamInput* diffContinue,
                            size_t kMaxOpenFileNumber,int threadNum){
    assert((patchListener!=0)&&(syncListener!=0));
    assert(kMaxOpenFileNumber>=kMaxOpenFileNumber_limit_min);
    kMaxOpenFileNumber-=2; // for newSyncInfoFile & outNewFile

    TSyncClient_resultType result=kSyncClient_ok;
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
        check_r(newDirOut.openInfo(&newSyncInfo,outNewDir,patchListener,kAlignSize,&out_newData),
                kSyncClient_newDirOpenInfoError);
    check_r(oldFilesStream.open(oldManifest.pathList,oldManifest.rootPath.c_str(),
                                kMaxOpenFileNumber,kAlignSize),
            kSyncClient_oldDirFilesOpenError);

    result=_sync_patch(syncListener,syncDataListener,diffData,
                       oldFilesStream.refStream.stream,&newSyncInfo,out_newData,0,
                       out_diffStream,diffType,diffContinue,threadNum);
    if (newDirOut.errResult!=kSyncClient_ok)
        result=newDirOut.errResult;
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

TSyncClient_resultType
    sync_patch_2file(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                     const TManifest& oldManifest,const char* newSyncInfoFile,
                     const char* outNewFile,hpatch_BOOL isOutNewContinue,
                     const char* cacheDiffInfoFile,size_t kMaxOpenFileNumber,int threadNum){
    TSyncClient_resultType result=kSyncClient_ok;
    int _inClear=0;
    hpatch_TFileStreamOutput out_diffInfo;
    hpatch_TFileStreamOutput_init(&out_diffInfo);
    const hpatch_TStreamInput* diffContinue=0;
    hpatch_TStreamInput _diffContinue;
    if (cacheDiffInfoFile){
        hpatch_BOOL isOutDiffContinue=hpatch_TRUE;
        check_r(_open_continue_out(isOutDiffContinue,cacheDiffInfoFile,&out_diffInfo,&_diffContinue),
                isOutDiffContinue?kSyncClient_diffFileReopenWriteError:kSyncClient_diffFileCreateError);
        if (isOutDiffContinue) diffContinue=&_diffContinue;
    }
    
    return _sync_patch_2file(listener,syncDataListener,0,oldManifest,newSyncInfoFile,
                             outNewFile,isOutNewContinue,
                             cacheDiffInfoFile?&out_diffInfo.base:0,kSyncDiff_info,diffContinue,
                             kMaxOpenFileNumber,threadNum);
clear:
    _inClear=1;
    check_r(hpatch_TFileStreamOutput_close(&out_diffInfo),kSyncClient_diffFileCloseError);
    return result;
}

TSyncClient_resultType
    sync_local_diff_2file(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                          const TManifest& oldManifest,const char* newSyncInfoFile,
                          const char* outDiffFile,TSyncDiffType diffType,hpatch_BOOL isOutDiffContinue,
                          size_t kMaxOpenFileNumber,int threadNum){
    TSyncClient_resultType result=kSyncClient_ok;
    int _inClear=0;
    hpatch_TFileStreamOutput out_diff;
    hpatch_TFileStreamOutput_init(&out_diff);
    const hpatch_TStreamInput* diffContinue=0;
    hpatch_TStreamInput _diffContinue;
    
    check_r(_open_continue_out(isOutDiffContinue,outDiffFile,&out_diff,&_diffContinue),
           isOutDiffContinue?kSyncClient_diffFileReopenWriteError:kSyncClient_diffFileCreateError);
    if (isOutDiffContinue) diffContinue=&_diffContinue;
    
    result=_sync_patch_2file(listener,syncDataListener,0,oldManifest,newSyncInfoFile,0,hpatch_FALSE,
                             &out_diff.base,diffType,diffContinue,kMaxOpenFileNumber,threadNum);
clear:
    _inClear=1;
    check_r(hpatch_TFileStreamOutput_close(&out_diff),kSyncClient_diffFileCloseError);
    return result;
}

TSyncClient_resultType
    sync_local_patch_2file(ISyncInfoListener* listener,const char* inDiffFile,
                           const TManifest& oldManifest,const char* newSyncInfoFile,const char* outNewFile,
                           size_t kMaxOpenFileNumber,int threadNum){
    TSyncClient_resultType result=kSyncClient_ok;
    int _inClear=0;
    TSyncDiffData diffData;
    hpatch_TFileStreamInput in_diffData;
    hpatch_TFileStreamInput_init(&in_diffData);
    check_r(hpatch_TFileStreamInput_open(&in_diffData,inDiffFile),
            kSyncClient_diffFileOpenError);
    check_r(_TSyncDiffData_load(&diffData,&in_diffData.base),kSyncClient_loadDiffError);
    return _sync_patch_2file(listener,0,&diffData,oldManifest,newSyncInfoFile,outNewFile,hpatch_FALSE,
                             0,kSyncDiff_default,0,kMaxOpenFileNumber,threadNum);
clear:
    _inClear=1;
    check_r(hpatch_TFileStreamInput_close(&in_diffData),kSyncClient_diffFileCloseError);
    return result;
}


TSyncClient_resultType
    sync_patch_2dir(IDirPatchListener* patchListener,IDirSyncPatchListener* syncListener,
                    IReadSyncDataListener* syncDataListener,
                    const TManifest& oldManifest,const char* newSyncInfoFile,const char* outNewDir,
                    const char* cacheDiffInfoFile,size_t kMaxOpenFileNumber,int threadNum){
    TSyncClient_resultType result=kSyncClient_ok;
    int _inClear=0;
    hpatch_TFileStreamOutput out_diffInfo;
    hpatch_TFileStreamOutput_init(&out_diffInfo);
    const hpatch_TStreamInput* diffContinue=0;
    hpatch_TStreamInput _diffContinue;
    if (cacheDiffInfoFile){
        hpatch_BOOL isOutDiffContinue=hpatch_TRUE;
        check_r(_open_continue_out(isOutDiffContinue,cacheDiffInfoFile,&out_diffInfo,&_diffContinue),
                isOutDiffContinue?kSyncClient_diffFileReopenWriteError:kSyncClient_diffFileCreateError);
        if (isOutDiffContinue) diffContinue=&_diffContinue;
    }

    return _sync_patch_2dir(patchListener,syncListener,syncDataListener,0,oldManifest,newSyncInfoFile,outNewDir,
                            cacheDiffInfoFile?&out_diffInfo.base:0,kSyncDiff_info,diffContinue,
                            kMaxOpenFileNumber,threadNum);
clear:
    _inClear=1;
    check_r(hpatch_TFileStreamOutput_close(&out_diffInfo),kSyncClient_diffFileCloseError);
    return result;
}

TSyncClient_resultType
    sync_local_patch_2dir(IDirPatchListener* patchListener,IDirSyncPatchListener* syncListener,
                          const char* inDiffFile,
                          const TManifest& oldManifest,const char* newSyncInfoFile,const char* outNewDir,
                          size_t kMaxOpenFileNumber,int threadNum){
    TSyncClient_resultType result=kSyncClient_ok;
    int _inClear=0;
    TSyncDiffData diffData;
    hpatch_TFileStreamInput in_diffData;
    hpatch_TFileStreamInput_init(&in_diffData);
    check_r(hpatch_TFileStreamInput_open(&in_diffData,inDiffFile),
            kSyncClient_diffFileOpenError);
    check_r(_TSyncDiffData_load(&diffData,&in_diffData.base),kSyncClient_loadDiffError);
    return _sync_patch_2dir(patchListener,syncListener,0,&diffData,oldManifest,newSyncInfoFile,outNewDir,
                            0,kSyncDiff_default,0,kMaxOpenFileNumber,threadNum);
clear:
    _inClear=1;
    check_r(hpatch_TFileStreamInput_close(&in_diffData),kSyncClient_diffFileCloseError);
    return result;
}

#endif //_IS_NEED_DIR_DIFF_PATCH
