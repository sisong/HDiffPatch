//  sync_client.cpp
//  sync_client
//  Created by housisong on 2019-09-18.
/*
 The MIT License (MIT)
 Copyright (c) 2019-2022 HouSisong
 
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
#include "sync_client_private.h"
#include "../../file_for_patch.h"
#include "match_in_old.h"
#include "sync_diff_data.h"
#include <stdexcept>
namespace sync_private{
#define _kMinCacheBlockDictMemSize ((1<<10)*256)
#define _kMinCacheBlockDictSize_r  4    // dictSize/_kMinCacheBlockDictSize_r
#define check(v,errorCode) \
            do{ if (!(v)) { if (result==kSyncClient_ok) result=errorCode; \
                            if (!_inClear) goto clear; } }while(0)

static size_t _getBlockDict(size_t dictSize,uint32_t kSyncBlockSize){
    if (dictSize==0) return 0;
    size_t bestMem=_kMinCacheBlockDictMemSize;
    if (bestMem<(dictSize/_kMinCacheBlockDictSize_r)) bestMem=dictSize/_kMinCacheBlockDictSize_r;
    return (bestMem+kSyncBlockSize/2)/kSyncBlockSize;
}

bool _open_continue_out(hpatch_BOOL& isOutContinue,const char* outFile,hpatch_TFileStreamOutput* out_stream,
                        hpatch_TStreamInput* continue_stream,hpatch_StreamPos_t maxContinueLength){
    memset(continue_stream,0,sizeof(*continue_stream));
    if ((isOutContinue)&&(hpatch_isPathExist(outFile))){
        if (!hpatch_TFileStreamOutput_reopen(out_stream,outFile,(hpatch_StreamPos_t)(-1))) return false;
        hpatch_TFileStreamOutput_setRandomOut(out_stream,hpatch_TRUE); //can rewrite
        if (out_stream->out_length>maxContinueLength) return false;
        if (out_stream->out_length>0){
            assert(out_stream->base.read_writed!=0);
            *continue_stream=*(hpatch_TStreamInput*)(void*)(&out_stream->base);
            continue_stream->streamSize=out_stream->out_length;
            out_stream->out_length=0;
        }else{
            isOutContinue=hpatch_FALSE;
        }
    }else{
        isOutContinue=hpatch_FALSE;
        if (!hpatch_TFileStreamOutput_open(out_stream,outFile,(hpatch_StreamPos_t)(-1))) return false;
    }
    return true;
}
    
    
struct _TWriteDatas {
    const hpatch_TStreamOutput* out_newStream;
    const hpatch_TStreamInput*  newDataContinue;
    const hpatch_TStreamOutput* out_diffStream;
    hpatch_StreamPos_t          outDiffDataPos;
    const hpatch_TStreamInput*  oldStream;
    const TNewDataSyncInfo*     newSyncInfo;
    const hpatch_StreamPos_t*   newBlockDataInOldPoss;
    TSyncDiffData*              continueDiffData;
    uint32_t                    needSyncBlockCount;
    bool                        isLocalPatch;
    hpatch_TChecksum*           strongChecksumPlugin;
    hsync_TDictDecompress*      decompressPlugin;
    IReadSyncDataListener*      syncDataListener;
};
    
#define _checkSumNewDataBuf() { \
    if (newDataSize<kSyncBlockSize)/*for backZeroLen*/ \
        memset(dataBuf+newDataSize,0,kSyncBlockSize-newDataSize);  \
    strongChecksumPlugin->begin(checksumSync);  \
    strongChecksumPlugin->append(checksumSync,dataBuf,dataBuf+kSyncBlockSize); \
    strongChecksumPlugin->end(checksumSync,checksumSync_buf+newSyncInfo->savedStrongChecksumByteSize,    \
                              checksumSync_buf+newSyncInfo->savedStrongChecksumByteSize \
                                  +newSyncInfo->kStrongChecksumByteSize);\
    toPartChecksum(checksumSync_buf,newSyncInfo->savedStrongChecksumByteSize, \
                   checksumSync_buf+newSyncInfo->savedStrongChecksumByteSize, \
                   newSyncInfo->kStrongChecksumByteSize);   \
    check(0==memcmp(checksumSync_buf,   \
                    newSyncInfo->partChecksums+i*newSyncInfo->savedStrongChecksumByteSize,   \
                    newSyncInfo->savedStrongChecksumByteSize),kSyncClient_checksumSyncDataError);    \
}

static int writeToNewOrDiff(_TWriteDatas& wd) {
    const TNewDataSyncInfo* newSyncInfo=wd.newSyncInfo;
    IReadSyncDataListener*  syncDataListener=wd.syncDataListener;
    hpatch_TChecksum*       strongChecksumPlugin=wd.strongChecksumPlugin;
    int result=kSyncClient_ok;
    int _inClear=0;
    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    const uint32_t kSyncBlockSize=newSyncInfo->kSyncBlockSize;
    TByte*             _memBuf=0;
    TByte*             checksumSync_buf=0;
    hsync_dictDecompressHandle decompressHandle=0;
    hpatch_checksumHandle checksumSync=0;
    hpatch_StreamPos_t posInNewSyncData=0;
    hpatch_StreamPos_t posInNeedSyncData=0;
    hpatch_StreamPos_t outNewDataPos=0;
    const hpatch_StreamPos_t oldDataSize=wd.oldStream->streamSize;
    bool isNeedSync;
    bool isOnDiffContinue =(wd.continueDiffData!=0);
    bool isOnNewDataContinue =(wd.newDataContinue!=0);
    const size_t dictSize=wd.newSyncInfo->dictSize;
    const size_t blockDict=_getBlockDict(dictSize,kSyncBlockSize); //cache block count in dict
    const size_t _memSize=kSyncBlockSize*(wd.decompressPlugin?2:1)+dictSize
                        +kSyncBlockSize*blockDict
                        +newSyncInfo->kStrongChecksumByteSize
                        +checkChecksumBufByteSize(newSyncInfo->kStrongChecksumByteSize);
    _memBuf=(TByte*)malloc(_memSize);
    check(_memBuf!=0,kSyncClient_memError);
    TByte* const blockDictBuf=_memBuf;
    size_t       blockDictBuf_i=blockDict;
    if (dictSize>0)
        memset(blockDictBuf+kSyncBlockSize*(blockDict+1),0,dictSize);
    {//checksum newSyncData
        checksumSync_buf=_memBuf+_memSize-(newSyncInfo->kStrongChecksumByteSize
                                           +checkChecksumBufByteSize(newSyncInfo->kStrongChecksumByteSize));
        checksumSync=strongChecksumPlugin->open(strongChecksumPlugin);
        check(checksumSync!=0,kSyncClient_strongChecksumOpenError);
    }
    hpatch_BOOL dict_isReset=hpatch_TRUE;
    for (uint32_t syncSize=0,newDataSize=0,i=0; i<kBlockCount; ++i, outNewDataPos+=newDataSize,
            posInNewSyncData+=syncSize,posInNeedSyncData+=isNeedSync?syncSize:0){
        if (dictSize>0){
            if (blockDictBuf_i==blockDict){
                memmove(blockDictBuf,blockDictBuf+kSyncBlockSize*(blockDict+1),dictSize);
                blockDictBuf_i=0;
            }else{
                ++blockDictBuf_i;
            }
        }
        TByte* const dictBuf=blockDictBuf+kSyncBlockSize*blockDictBuf_i;
        TByte* const dataBuf=dictBuf+dictSize;
        syncSize=TNewDataSyncInfo_syncBlockSize(newSyncInfo,i);
        newDataSize=TNewDataSyncInfo_newDataBlockSize(newSyncInfo,i);
        check(syncSize<=newDataSize,kSyncClient_newSyncInfoDataError);
        const hpatch_StreamPos_t curSyncPos=wd.newBlockDataInOldPoss[i];
        isNeedSync=(curSyncPos==kBlockType_needSync);
        if (isOnNewDataContinue){ //copy from newDataContinue
            dict_isReset=hpatch_TRUE;
            check(wd.newDataContinue->read(wd.newDataContinue,outNewDataPos,dataBuf,dataBuf+newDataSize),
                  kSyncClient_newFileReopenReadError);
        }else if (isNeedSync){ //download or read from diff data
            TByte* buf=(syncSize<newDataSize)?(dataBuf+kSyncBlockSize):dataBuf;
            /*if ((wd.out_newStream)||(wd.out_diffStream))*/{//download data
                if (isOnDiffContinue){ //read from local data
                    if (!wd.continueDiffData->readSyncData(wd.continueDiffData,i,posInNewSyncData,
                                                           posInNeedSyncData,syncSize,buf))
                        isOnDiffContinue=false; // swap to download
                }
                if (!isOnDiffContinue){ //downloaded data or read from diff data
                    check(syncDataListener->readSyncData(syncDataListener,i,posInNewSyncData,
                                                         posInNeedSyncData,syncSize,buf),
                          kSyncClient_readSyncDataError);
                }
                if (wd.out_diffStream){ //out diff
                    if (!isOnDiffContinue){ //save downloaded data
                        check(wd.out_diffStream->write(wd.out_diffStream,wd.outDiffDataPos,
                                                       buf,buf+syncSize),kSyncClient_saveDiffError);
                    }
                    wd.outDiffDataPos+=syncSize;
                }
            }
            if (/*wd.out_newStream&&*/(syncSize<newDataSize)){// need deccompress
                if (decompressHandle==0){
                    decompressHandle=wd.decompressPlugin->dictDecompressOpen(wd.decompressPlugin);
                    check(decompressHandle,kSyncClient_decompressOpenError);
                }
                check(wd.decompressPlugin->dictDecompress(decompressHandle,buf,buf+syncSize,
                                                          dictBuf,dataBuf,dataBuf+newDataSize,
                                                          dict_isReset,i+1==kBlockCount),
                      kSyncClient_decompressError);
            }
            dict_isReset=!(syncSize<newDataSize);
        }else{//copy from old
            dict_isReset=hpatch_TRUE;
            assert(curSyncPos<oldDataSize);
            /*if (wd.out_newStream)*/{
                uint32_t readSize=newDataSize;
                if (curSyncPos+readSize>oldDataSize)
                    readSize=(uint32_t)(oldDataSize-curSyncPos);
                check(wd.oldStream->read(wd.oldStream,curSyncPos,dataBuf,dataBuf+readSize),
                      kSyncClient_readOldDataError);
                if (readSize<newDataSize)
                    memset(dataBuf+readSize,0,newDataSize-readSize);
            }
        }
        /*if (wd.out_newStream)*/{//write
            bool isNeedChecksumAppend=isNeedSync|wd.isLocalPatch|(wd.continueDiffData!=0);
            if (isNeedChecksumAppend|isOnNewDataContinue)
                _checkSumNewDataBuf();
            if (isNeedChecksumAppend)
                checkChecksumAppendData(newSyncInfo->savedNewDataCheckChecksum, i,
                                        checksumSync_buf+wd.newSyncInfo->savedStrongChecksumByteSize,
                                        newSyncInfo->kStrongChecksumByteSize);
            if ((!isOnNewDataContinue)&&wd.out_newStream){
                check(wd.out_newStream->write(wd.out_newStream,outNewDataPos,dataBuf,
                                              dataBuf+newDataSize), kSyncClient_writeNewDataError);
            }
        }
    }
    check(outNewDataPos==newSyncInfo->newDataSize,kSyncClient_newDataSizeError);
    assert(posInNewSyncData==newSyncInfo->newSyncDataSize);//checked in readSavedSizesTo()
clear:
    _inClear=1;
    if (decompressHandle) wd.decompressPlugin->dictDecompressClose(wd.decompressPlugin,decompressHandle);
    if (checksumSync) strongChecksumPlugin->close(strongChecksumPlugin,checksumSync);
    if (_memBuf) free(_memBuf);
    return result;
}


    struct TNeedSyncInfosImport:public TNeedSyncInfos{
        const hpatch_StreamPos_t* newBlockDataInOldPoss;
        const TNewDataSyncInfo*   newSyncInfo;  // opened .hsyni
    };
static void _getBlockInfoByIndex(const TNeedSyncInfos* needSyncInfos,uint32_t blockIndex,
                                 hpatch_BOOL* out_isNeedSync,uint32_t* out_syncSize){
    const TNeedSyncInfosImport* self=(const TNeedSyncInfosImport*)needSyncInfos->import;
    assert(blockIndex<self->blockCount);
    *out_isNeedSync=(self->newBlockDataInOldPoss[blockIndex]==kBlockType_needSync);
    *out_syncSize=TNewDataSyncInfo_syncBlockSize(self->newSyncInfo,blockIndex);
}
    
static void getNeedSyncInfo(const hpatch_StreamPos_t* newBlockDataInOldPoss,
                            const TNewDataSyncInfo* newSyncInfo,TNeedSyncInfosImport* out_nsi){
    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    out_nsi->newBlockDataInOldPoss=newBlockDataInOldPoss;
    out_nsi->newSyncInfo=newSyncInfo;
    out_nsi->import=out_nsi; //self
    
    out_nsi->newDataSize=newSyncInfo->newDataSize;
    out_nsi->newSyncDataSize=newSyncInfo->newSyncDataSize;
    out_nsi->newSyncInfoSize=newSyncInfo->newSyncInfoSize;
    out_nsi->kSyncBlockSize=newSyncInfo->kSyncBlockSize;
    out_nsi->blockCount=kBlockCount;
    out_nsi->blockCount=kBlockCount;
    out_nsi->getBlockInfoByIndex=_getBlockInfoByIndex;
    out_nsi->needSyncBlockCount=0;
    out_nsi->needSyncSumSize=0;
    for (uint32_t i=0; i<kBlockCount; ++i){
        if (newBlockDataInOldPoss[i]==kBlockType_needSync){
            ++out_nsi->needSyncBlockCount;
            out_nsi->needSyncSumSize+=TNewDataSyncInfo_syncBlockSize(newSyncInfo,i);
        }
    }
}
    
int _sync_patch(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,TSyncDiffData* diffData,
                const hpatch_TStreamInput* oldStream,const TNewDataSyncInfo* newSyncInfo,
                const hpatch_TStreamOutput* out_newStream,const hpatch_TStreamInput* newDataContinue,
                const hpatch_TStreamOutput* out_diffStream,const hpatch_TStreamInput* diffContinue,int threadNum){
    assert(listener!=0);
    hsync_TDictDecompress* decompressPlugin=0;
    hpatch_TChecksum*   strongChecksumPlugin=0;
    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    TNeedSyncInfosImport needSyncInfo; memset(&needSyncInfo,0,sizeof(needSyncInfo));
    hpatch_StreamPos_t* newBlockDataInOldPoss=0;
    hpatch_StreamPos_t  outDiffDataPos=0;
    int result=kSyncClient_ok;
    int _inClear=0;
    TSyncDiffData continueDiffData; memset(&continueDiffData,0,sizeof(continueDiffData));
    
    //decompressPlugin
    if (newSyncInfo->compressType){
        if ((newSyncInfo->_decompressPlugin!=0)
            &&(newSyncInfo->_decompressPlugin->is_can_open(newSyncInfo->compressType))){
            decompressPlugin=newSyncInfo->_decompressPlugin;
        }else{
            decompressPlugin=listener->findDecompressPlugin(listener,newSyncInfo->compressType,newSyncInfo->dictSize);
            check(decompressPlugin!=0,kSyncClient_noDecompressPluginError);
        }
    }
    //strongChecksumPlugin
    if ((newSyncInfo->_strongChecksumPlugin!=0)
        &&(newSyncInfo->kStrongChecksumByteSize==newSyncInfo->_strongChecksumPlugin->checksumByteSize())
        &&(0==strcmp(newSyncInfo->strongChecksumType,newSyncInfo->_strongChecksumPlugin->checksumType()))){
        strongChecksumPlugin=newSyncInfo->_strongChecksumPlugin;
    }else{
        strongChecksumPlugin=listener->findChecksumPlugin(listener,newSyncInfo->strongChecksumType);
        check(strongChecksumPlugin!=0,kSyncClient_noStrongChecksumPluginError);
        check(strongChecksumPlugin->checksumByteSize()==newSyncInfo->kStrongChecksumByteSize,
              kSyncClient_strongChecksumByteSizeError);
    }

    checkChecksumInit(newSyncInfo->savedNewDataCheckChecksum,newSyncInfo->kStrongChecksumByteSize);
    //matched in oldData
    newBlockDataInOldPoss=(hpatch_StreamPos_t*)malloc(kBlockCount*(size_t)sizeof(hpatch_StreamPos_t));
    check(newBlockDataInOldPoss!=0,kSyncClient_memError);
    if (diffData!=0){ //local patch
        assert(syncDataListener==0);
        assert(diffContinue==0);
        check(_TSyncDiffData_readOldPoss(diffData,newBlockDataInOldPoss,kBlockCount,newSyncInfo->kSyncBlockSize,
                                         oldStream->streamSize,newSyncInfo->savedNewDataCheckChecksum,
                                         newSyncInfo->kStrongChecksumByteSize), kSyncClient_loadDiffError);
        syncDataListener=diffData;
    }else{
        bool isLoadedOldPoss=false;
        assert(syncDataListener!=0);
        if (diffContinue!=0){ // try continue local diff
            assert(out_diffStream!=0);
            if (_TSyncDiffData_load(&continueDiffData,diffContinue)){
                if (_TSyncDiffData_readOldPoss(&continueDiffData,newBlockDataInOldPoss,kBlockCount,
                                               newSyncInfo->kSyncBlockSize,oldStream->streamSize,
                                               newSyncInfo->savedNewDataCheckChecksum,newSyncInfo->kStrongChecksumByteSize)){
                    isLoadedOldPoss=true;//ok diffContinue
                }else
                    diffContinue=0; // not use diffContinue
            }else
                diffContinue=0; // not use diffContinue
        }
        if (!isLoadedOldPoss){
            try{
                matchNewDataInOld(newBlockDataInOldPoss,newSyncInfo,oldStream,
                                  strongChecksumPlugin,threadNum);
            }catch(const std::exception& e){
                fprintf(stderr,"matchNewDataInOld() run an error: %s",e.what());
                result=kSyncClient_matchNewDataInOldError;
            }
        }
    }
    check(result==kSyncClient_ok,result);
    if ((out_diffStream!=0)){
        if (diffContinue==0){
            check(_saveSyncDiffData(newBlockDataInOldPoss,kBlockCount,newSyncInfo->kSyncBlockSize,oldStream->streamSize,
                                    newSyncInfo->savedNewDataCheckChecksum,newSyncInfo->kStrongChecksumByteSize,
                                    out_diffStream,&outDiffDataPos),kSyncClient_saveDiffError);
        }else{ // continue local diff;  same newBlockDataInOldPoss data
            outDiffDataPos=continueDiffData.diffDataPos0;
        }
    }
    getNeedSyncInfo(newBlockDataInOldPoss,newSyncInfo,&needSyncInfo);
    if (diffContinue)
        needSyncInfo.localDiffDataSize=(diffContinue->streamSize-continueDiffData.diffDataPos0);

    if (listener->needSyncInfo)
        listener->needSyncInfo(listener,&needSyncInfo);
    
    if (syncDataListener->readSyncDataBegin)
        check(syncDataListener->readSyncDataBegin(syncDataListener,&needSyncInfo),
              kSyncClient_readSyncDataBeginError);
    /*if (out_newStream||out_diffStream)*/{
        _TWriteDatas writeDatas;
        writeDatas.out_newStream=out_newStream;
        writeDatas.newDataContinue=newDataContinue;
        writeDatas.out_diffStream=out_diffStream;
        writeDatas.outDiffDataPos=outDiffDataPos;
        writeDatas.oldStream=oldStream;
        writeDatas.newSyncInfo=newSyncInfo;
        writeDatas.newBlockDataInOldPoss=newBlockDataInOldPoss;
        writeDatas.needSyncBlockCount=needSyncInfo.needSyncBlockCount;
        writeDatas.decompressPlugin=decompressPlugin;
        writeDatas.strongChecksumPlugin=strongChecksumPlugin;
        writeDatas.syncDataListener=syncDataListener;
        writeDatas.continueDiffData=diffContinue?&continueDiffData:0;
        writeDatas.isLocalPatch=(diffData!=0);
        result=writeToNewOrDiff(writeDatas);
        
        if (/*(*/result==kSyncClient_ok/*)&&out_newStream*/){
            checkChecksumEndTo(newSyncInfo->savedNewDataCheckChecksum+newSyncInfo->kStrongChecksumByteSize,
                               newSyncInfo->savedNewDataCheckChecksum,newSyncInfo->kStrongChecksumByteSize);
            check(0==memcmp(newSyncInfo->savedNewDataCheckChecksum+newSyncInfo->kStrongChecksumByteSize,
                            newSyncInfo->savedNewDataCheckChecksum,newSyncInfo->kStrongChecksumByteSize),
                  kSyncClient_newDataCheckChecksumError);
        }
    }
    
    if (syncDataListener->readSyncDataEnd)
        syncDataListener->readSyncDataEnd(syncDataListener);
clear:
    _inClear=1;
    if (newBlockDataInOldPoss) free(newBlockDataInOldPoss);
    return result;
}

static int _sync_patch_file2file(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                                 TSyncDiffData* diffData,const char* oldFile,const char* newSyncInfoFile,
                                 const char* outNewFile,hpatch_BOOL isOutNewContinue,const hpatch_TStreamOutput* out_diffStream,
                                 const hpatch_TStreamInput* diffContinue,int threadNum){
    int result=kSyncClient_ok;
    int _inClear=0;
    TNewDataSyncInfo            newSyncInfo;
    hpatch_TFileStreamInput     oldData;
    hpatch_TFileStreamOutput    out_newData;
    const hpatch_TStreamInput*  newDataContinue=0;
    hpatch_TStreamInput         _newDataContinue;
    const hpatch_TStreamInput*  oldStream=0;
    bool isOldPathInputEmpty=(oldFile==0)||(strlen(oldFile)==0);
    
    TNewDataSyncInfo_init(&newSyncInfo);
    hpatch_TFileStreamInput_init(&oldData);
    hpatch_TFileStreamOutput_init(&out_newData);
    result=TNewDataSyncInfo_open_by_file(&newSyncInfo,newSyncInfoFile,listener);
    check(result==kSyncClient_ok,result);
    
    if (!isOldPathInputEmpty)
        check(hpatch_TFileStreamInput_open(&oldData,oldFile),kSyncClient_oldFileOpenError);
    oldStream=&oldData.base;
    if (outNewFile){
        check(_open_continue_out(isOutNewContinue,outNewFile,&out_newData,&_newDataContinue,newSyncInfo.newDataSize),
              isOutNewContinue?kSyncClient_newFileReopenWriteError:kSyncClient_newFileCreateError);
        if (isOutNewContinue) newDataContinue=&_newDataContinue;
    }
    
    result=_sync_patch(listener,syncDataListener,diffData,oldStream,&newSyncInfo,
                       outNewFile?&out_newData.base:0,newDataContinue,out_diffStream,diffContinue,threadNum);
clear:
    _inClear=1;
    check(hpatch_TFileStreamOutput_close(&out_newData),kSyncClient_newFileCloseError);
    check(hpatch_TFileStreamInput_close(&oldData),kSyncClient_oldFileCloseError);
    TNewDataSyncInfo_close(&newSyncInfo);
    return result;
}

} //namespace sync_private
using namespace  sync_private;

int sync_patch(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
               const hpatch_TStreamInput* oldStream,const TNewDataSyncInfo* newSyncInfo,
               const hpatch_TStreamOutput* out_newStream,const hpatch_TStreamInput* newDataContinue,int threadNum){
    return _sync_patch(listener,syncDataListener,0,oldStream,newSyncInfo,out_newStream,newDataContinue,0,0,threadNum);
}

int sync_local_diff(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                    const hpatch_TStreamInput* oldStream,const TNewDataSyncInfo* newSyncInfo,
                    const hpatch_TStreamOutput* out_diffStream,const hpatch_TStreamInput* diffContinue,int threadNum){
    return _sync_patch(listener,syncDataListener,0,oldStream,newSyncInfo,0,0,out_diffStream,diffContinue,threadNum);
}


int sync_local_patch(ISyncInfoListener* listener,const hpatch_TStreamInput* in_diffStream,
                     const hpatch_TStreamInput* oldStream,const TNewDataSyncInfo* newSyncInfo,
                     const hpatch_TStreamOutput* out_newStream,const hpatch_TStreamInput* newDataContinue,int threadNum){
    TSyncDiffData diffData;
    if (!_TSyncDiffData_load(&diffData,in_diffStream)) return kSyncClient_loadDiffError;
    return _sync_patch(listener,0,&diffData,oldStream,newSyncInfo,out_newStream,newDataContinue,0,0,threadNum);
}


int sync_patch_file2file(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                         const char* oldFile,const char* newSyncInfoFile,const char* outNewFile,
                         hpatch_BOOL isOutNewContinue,int threadNum){
    return _sync_patch_file2file(listener,syncDataListener,0,oldFile,newSyncInfoFile,
                                 outNewFile,isOutNewContinue,0,0,threadNum);
}


int sync_local_diff_file2file(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                              const char* oldFile,const char* newSyncInfoFile,
                              const char* outDiffFile,hpatch_BOOL isOutDiffContinue,int threadNum){
    int result=kSyncClient_ok;
    int _inClear=0;
    hpatch_TFileStreamOutput out_diffData;
    hpatch_TFileStreamOutput_init(&out_diffData);
    const hpatch_TStreamInput* diffContinue=0;
    hpatch_TStreamInput _diffContinue;
    
    check(_open_continue_out(isOutDiffContinue,outDiffFile,&out_diffData,&_diffContinue,(hpatch_StreamPos_t)(-1)),
          isOutDiffContinue?kSyncClient_diffFileReopenWriteError:kSyncClient_diffFileCreateError);
    if (isOutDiffContinue) diffContinue=&_diffContinue;
    
    result=_sync_patch_file2file(listener,syncDataListener,0,oldFile,newSyncInfoFile,0,hpatch_FALSE,
                                 &out_diffData.base,diffContinue,threadNum);
    /*if ((result==kSyncClient_ok)&&isOutDiffContinue
        &&(out_diffData.out_length>0)&&(out_diffData.out_length<diffContinue->streamSize)){
        check(hpatch_TFileStreamOutput_truncate(&out_diffData,out_diffData.out_length),
              kSyncClient_diffFileReopenWriteError);  // retained data error
    }*/

clear:
    _inClear=1;
    check(hpatch_TFileStreamOutput_close(&out_diffData),kSyncClient_diffFileCloseError);
    return result;
}

int sync_local_patch_file2file(ISyncInfoListener* listener,const char* inDiffFile,
                               const char* oldFile,const char* newSyncInfoFile,
                               const char* outNewFile,hpatch_BOOL isOutNewContinue,int threadNum){
    int result=kSyncClient_ok;
    int _inClear=0;
    TSyncDiffData diffData;
    hpatch_TFileStreamInput in_diffData;
    hpatch_TFileStreamInput_init(&in_diffData);
    check(hpatch_TFileStreamInput_open(&in_diffData,inDiffFile),
          kSyncClient_diffFileOpenError);
    check(_TSyncDiffData_load(&diffData,&in_diffData.base),kSyncClient_loadDiffError);
    result=_sync_patch_file2file(listener,0,&diffData,oldFile,newSyncInfoFile,outNewFile,isOutNewContinue,0,0,threadNum);
clear:
    _inClear=1;
    check(hpatch_TFileStreamInput_close(&in_diffData),kSyncClient_diffFileCloseError);
    return result;
}
