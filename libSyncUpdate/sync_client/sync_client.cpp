//  sync_client.cpp
//  sync_client
//
//  Created by housisong on 2019-09-18.
//  Copyright © 2019 sisong. All rights reserved.

#include "sync_client.h"
#include "../../file_for_patch.h"
#include "match_in_old.h"

#define  _ChecksumPlugin_md5
#include "../../checksum_plugin_demo.h"

#define check(v,errorCode) \
    do{ if (!(v)) { if (result==kSyncClient_ok) result=errorCode; \
                    if (!_inClear) goto clear; } }while(0)

int TNewDataSyncInfo_open(TNewDataSyncInfo* self,const hpatch_TStreamInput* newSyncInfo){
    assert(false);
    return -1;
}

void TNewDataSyncInfo_close(TNewDataSyncInfo* self){
    //todo:
    assert(false);
    TNewDataSyncInfo_init(self);
}

int TNewDataSyncInfo_open_by_file(TNewDataSyncInfo* self,const char* newSyncInfoPath){
    hpatch_TFileStreamInput  newSyncInfo;
    hpatch_TFileStreamInput_init(&newSyncInfo);
    int rt;
    int result=kSyncClient_ok;
    int _inClear=0;
    check(hpatch_TFileStreamInput_open(&newSyncInfo,newSyncInfoPath), kSyncClient_newSyncInfoOpenError);

    rt=TNewDataSyncInfo_open(self,&newSyncInfo.base);
    check(rt==kSyncClient_ok,rt);
clear:
    _inClear=1;
    check(hpatch_TFileStreamInput_close(&newSyncInfo), kSyncClient_newSyncInfoCloseError);
    return result;
}

static int writeToNew(const hpatch_TStreamOutput* out_newStream,const TNewDataSyncInfo* newSyncInfo,
                      const hpatch_StreamPos_t* newDataPoss,const hpatch_TStreamInput* oldStream,
                      hpatch_TDecompress* decompressPlugin,hpatch_TChecksum* strongChecksumPlugin,
                      ISyncPatchListener *listener) {
    int result=kSyncClient_ok;
    int _inClear=0;
    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    const uint32_t kMatchBlockSize=newSyncInfo->kMatchBlockSize;
    TByte*             dataBuf=0;
    TByte*             checksumSync_buf=0;
    hpatch_checksumHandle checksumSync=0;
    hpatch_StreamPos_t posInNewSyncData=0;
    hpatch_StreamPos_t outNewDataPos=0;
    
    size_t _memSize=kMatchBlockSize*(decompressPlugin?2:1)+newSyncInfo->kStrongChecksumByteSize;
    dataBuf=(TByte*)malloc(_memSize);
    check(dataBuf!=0,kSyncClient_memError);
    checksumSync_buf=dataBuf+_memSize-newSyncInfo->kStrongChecksumByteSize;
    checksumSync=strongChecksumPlugin->open(strongChecksumPlugin);
    check(checksumSync!=0,kSyncClient_strongChecksumOpenError);
    for (uint32_t i=0; i<kBlockCount; ++i) {
        uint32_t syncSize=TNewDataSyncInfo_syncBlockSize(newSyncInfo,i);
        uint32_t newDataSize=TNewDataSyncInfo_newDataBlockSize(newSyncInfo,i);
        if (newDataPoss[i]==kBlockType_needSync){ //sync
            TByte* buf=decompressPlugin?(dataBuf+kMatchBlockSize):dataBuf;
            if ((out_newStream)||(listener)){
                check(listener->readSyncData(listener,dataBuf,posInNewSyncData,syncSize),
                      kSyncClient_readSyncDataError);
                if (decompressPlugin){
                    check(hpatch_deccompress_mem(decompressPlugin,buf,buf+syncSize,
                                                 dataBuf,dataBuf+newDataSize),kSyncClient_decompressError);
                }
                //checksum
                strongChecksumPlugin->begin(checksumSync);
                strongChecksumPlugin->append(checksumSync,dataBuf,dataBuf+newDataSize);
                strongChecksumPlugin->end(checksumSync,checksumSync_buf,
                                          checksumSync_buf+newSyncInfo->kStrongChecksumByteSize);
                toPartChecksum(checksumSync_buf,checksumSync_buf,newSyncInfo->kStrongChecksumByteSize);
                check(0==memcmp(checksumSync_buf,
                                newSyncInfo->partChecksums+i*(size_t)kPartStrongChecksumByteSize,
                                kPartStrongChecksumByteSize),kSyncClient_checksumSyncDataError);
            }
        }else{//copy from old
            check(oldStream->read(oldStream,newDataPoss[i],dataBuf,dataBuf+newDataSize),
                  kSyncClient_readOldDataError);
        }
        if (out_newStream){//write
            check(out_newStream->write(out_newStream,outNewDataPos,dataBuf,
                                       dataBuf+newDataSize), kSyncClient_writeNewDataError);
        }
        outNewDataPos+=newDataSize;
        posInNewSyncData+=syncSize;
    }
    assert(outNewDataPos==newSyncInfo->newDataSize);
    assert(posInNewSyncData==newSyncInfo->newSyncDataSize);
clear:
    _inClear=1;
    if (checksumSync) strongChecksumPlugin->close(strongChecksumPlugin,checksumSync);
    if (dataBuf) free(dataBuf);
    return result;
}

static void sendSyncMsg(ISyncPatchListener* listener,hpatch_StreamPos_t* newDataPoss,
                        const TNewDataSyncInfo* newSyncInfo,uint32_t needSyncCount) {
    if ((listener)&&(listener->needSyncMsg)){
        const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
        hpatch_StreamPos_t posInNewSyncData=0;
        for (uint32_t i=0; i<kBlockCount; ++i) {
            uint32_t syncSize=TNewDataSyncInfo_syncBlockSize(newSyncInfo,i);
            if (newDataPoss[i]==kBlockType_needSync)
                listener->needSyncMsg(listener,needSyncCount,posInNewSyncData,syncSize);
            posInNewSyncData+=syncSize;
        }
    }
}

static void printMatchResult(const TNewDataSyncInfo* newSyncInfo,
                             uint32_t needSyncCount,hpatch_StreamPos_t needSyncSize) {
    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    printf("syncCount: %d (/%d=%.3f)  syncSize: %" PRIu64 "\n",
           needSyncCount,kBlockCount,(double)needSyncCount/kBlockCount,needSyncSize);
    hpatch_StreamPos_t downloadSize=newSyncInfo->newSyncInfoSize+needSyncSize;
    printf("downloadSize: %" PRIu64 "+%" PRIu64 "= %" PRIu64 " (/%" PRIu64 "=%.3f)",
           newSyncInfo->newSyncInfoSize,needSyncSize,downloadSize,
           newSyncInfo->newSyncDataSize,(double)downloadSize/newSyncInfo->newSyncDataSize);
    printf(" (/%" PRIu64 "=%.3f)\n",
           newSyncInfo->newDataSize,(double)downloadSize/newSyncInfo->newDataSize);
}

int sync_patch(const hpatch_TStreamOutput* out_newStream,
               const TNewDataSyncInfo*     newSyncInfo,
               const hpatch_TStreamInput*  oldStream, ISyncPatchListener* listener){
    //todo: select checksum\decompressPlugin
    //assert(listener!=0);

    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    hpatch_TChecksum* strongChecksumPlugin=&md5ChecksumPlugin;
    hpatch_TDecompress* decompressPlugin=0;
    uint32_t needSyncCount=0;
    hpatch_StreamPos_t needSyncSize=0;
    int result=kSyncClient_ok;
    int _inClear=0;
    
    //match in oldData
    hpatch_StreamPos_t* newDataPoss=(hpatch_StreamPos_t*)malloc(kBlockCount*sizeof(hpatch_StreamPos_t));
    check(newDataPoss!=0,kSyncClient_memError);
    try{
        matchNewDataInOld(newDataPoss,&needSyncCount,&needSyncSize,
                          newSyncInfo,oldStream,strongChecksumPlugin);
        printMatchResult(newSyncInfo,needSyncCount, needSyncSize);
    }catch(...){
        result=kSyncClient_matchNewDataInOldError;
    }
    check(result==kSyncClient_ok,result);
    
    //send msg: all need sync block
    sendSyncMsg(listener,newDataPoss,newSyncInfo,needSyncCount);
    
    result=writeToNew(out_newStream,newSyncInfo,newDataPoss,oldStream,
                      decompressPlugin,strongChecksumPlugin,listener);
    check(result==kSyncClient_ok,result);
clear:
    _inClear=1;
    if (newDataPoss) free(newDataPoss);
    return result;
}

int sync_patch_by_file(const char* out_newPath,
                       const char* newSyncInfoPath,
                       const char* oldPath, ISyncPatchListener* listener){
    int result=kSyncClient_ok;
    int _inClear=0;
    TNewDataSyncInfo         newSyncInfo;
    hpatch_TFileStreamInput  oldData;
    hpatch_TFileStreamOutput out_newData;
    
    TNewDataSyncInfo_init(&newSyncInfo);
    hpatch_TFileStreamInput_init(&oldData);
    hpatch_TFileStreamOutput_init(&out_newData);
    result=TNewDataSyncInfo_open_by_file(&newSyncInfo,newSyncInfoPath);
    check(result==kSyncClient_ok,result);
    check(hpatch_TFileStreamInput_open(&oldData,oldPath),kSyncClient_oldFileOpenError);
    check(hpatch_TFileStreamOutput_open(&out_newData,out_newPath,(hpatch_StreamPos_t)(-1)),
          kSyncClient_newFileCreateError);
    
    result=sync_patch(&out_newData.base,&newSyncInfo,&oldData.base,listener);
clear:
    _inClear=1;
    check(hpatch_TFileStreamOutput_close(&out_newData),kSyncClient_newFileCloseError);
    check(hpatch_TFileStreamInput_close(&oldData),kSyncClient_oldFileCloseError);
    TNewDataSyncInfo_close(&newSyncInfo);
    return result;
}
