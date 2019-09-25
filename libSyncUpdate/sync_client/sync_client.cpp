//  sync_client.cpp
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
#include "sync_client.h"
#include "match_in_old.h"
#include "../../file_for_patch.h"
#include "../../libHDiffPatch/HPatch/patch_private.h"
#include "../../libHDiffPatch/HPatch/patch_types.h"

#define check(v,errorCode) \
    do{ if (!(v)) { if (result==kSyncClient_ok) result=errorCode; \
                    if (!_inClear) goto clear; } }while(0)

#define _clip_unpackUIntTo(puint,sclip) \
    _TStreamCacheClip_unpackUIntWithTag(sclip,puint,0)

inline static bool toUInt32(uint32_t* out_v,hpatch_StreamPos_t v){
    uint32_t result=(uint32_t)v;
    if (v==result){
        *out_v=result;
        return true;
    }
    return false;
}
inline static bool _clip_unpackUInt32To(uint32_t* out_v,TStreamCacheClip* clip){
    hpatch_StreamPos_t v;
    if (_clip_unpackUIntTo(&v,clip))
        return toUInt32(out_v,v);
    return false;
}

static bool _checksumAppendData(const hpatch_TStreamInput* stream,hpatch_StreamPos_t pos,
                                hpatch_StreamPos_t posEnd,hpatch_TChecksum* strongChecksumPlugin,
                                hpatch_checksumHandle checksum,TByte* temp_cache,const size_t bufSize){
    while (pos<posEnd) {
        size_t readSize=bufSize;
        if (pos+readSize>posEnd) readSize=(size_t)(posEnd-pos);
        if (!stream->read(stream,pos,temp_cache,temp_cache+readSize))
            return false;
        strongChecksumPlugin->append(checksum,temp_cache,temp_cache+readSize);
        pos+=readSize;
    }
    return true;
}

static int _checksumInfo(const hpatch_TStreamInput* newSyncInfo,hpatch_StreamPos_t checksumPos,
                         hpatch_TChecksum* strongChecksumPlugin,const TByte* savedChecksum){
    hpatch_checksumHandle checksum=0;
    TByte*                checksum_buf=0;
    int result=kSyncClient_ok;
    int _inClear=0;
    const size_t bufSize=hpatch_kFileIOBufBetterSize;
    const size_t checksumByteSize=strongChecksumPlugin->checksumByteSize();
    TByte* temp_cache=(TByte*)malloc(bufSize+checksumByteSize);
    check(temp_cache!=0,kSyncClient_memError);
    checksum_buf=temp_cache+bufSize;
    checksum=strongChecksumPlugin->open(strongChecksumPlugin);
    check(checksum!=0, kSyncClient_strongChecksumOpenError);
    strongChecksumPlugin->begin(checksum);
    
    check(_checksumAppendData(newSyncInfo,0,checksumPos,strongChecksumPlugin,checksum,
                              temp_cache,bufSize),kSyncClient_newSyncInfoDataError);
    check(_checksumAppendData(newSyncInfo,checksumPos+kPartStrongChecksumByteSize,
                              newSyncInfo->streamSize,strongChecksumPlugin,checksum,
                              temp_cache,bufSize),kSyncClient_newSyncInfoDataError);
    
    strongChecksumPlugin->end(checksum,checksum_buf,checksum_buf+checksumByteSize);
    toPartChecksum(checksum_buf,checksum_buf,checksumByteSize);
    check(0==memcmp(checksum_buf,savedChecksum,kPartStrongChecksumByteSize),
          kSyncClient_newSyncInfoChecksumError);
clear:
    _inClear=1;
    if (checksum) strongChecksumPlugin->close(strongChecksumPlugin,checksum);
    if (temp_cache) free(temp_cache);
    return result;
}

int TNewDataSyncInfo_open(TNewDataSyncInfo* self,
                          const hpatch_TStreamInput* newSyncInfo,ISyncPatchListener *listener){
    assert(self->_import==0);
    hpatch_TDecompress* decompressPlugin=0;
    hpatch_TChecksum*   strongChecksumPlugin=0;
    TStreamCacheClip    clip;
    int result=kSyncClient_ok;
    int _inClear=0;

    uint32_t kBlockCount=0;
    size_t   memSize=0;
    const char* checksumType=0;
    char  tempType[hpatch_kMaxPluginTypeLength+1];
    hpatch_StreamPos_t uncompressDataSize=0;
    hpatch_StreamPos_t compressDataSize=0;
    TSameNewDataPair*  sameNewDataPairs=0;
    _TDecompressInputSteram decompresser;
    TByte* temp_cache=(TByte*)malloc(hpatch_kFileIOBufBetterSize);
    check(temp_cache!=0,kSyncClient_memError);
    memset(&decompresser,0,sizeof(decompresser));
    _TStreamCacheClip_init(&clip,newSyncInfo,0,newSyncInfo->streamSize,
                           temp_cache,hpatch_kFileIOBufBetterSize);
    {//head
        const char* kTypeVersion="SyncUpdate19";
        {//type
            check(_TStreamCacheClip_readType_end(&clip,'&',tempType),kSyncClient_newSyncInfoTypeError);
            check(0==strcmp(tempType,kTypeVersion),kSyncClient_newSyncInfoTypeError);
        }
        {//read strongChecksumType
            check(_TStreamCacheClip_readType_end(&clip,'&',tempType),kSyncClient_noStrongChecksumPluginError);
            strongChecksumPlugin=listener->findChecksumPlugin(listener,tempType);
            check(strongChecksumPlugin!=0,kSyncClient_noStrongChecksumPluginError);
            checksumType=strongChecksumPlugin->checksumType();
            check(0==strcmp(tempType,checksumType),kSyncClient_noStrongChecksumPluginError);
            self->_strongChecksumPlugin=strongChecksumPlugin;
        }
        {//read compressType
            check(_TStreamCacheClip_readType_end(&clip,'\0',tempType),kSyncClient_noDecompressPluginError);
            if (strlen(tempType)>0){
                decompressPlugin=listener->findDecompressPlugin(listener,tempType);
                check(decompressPlugin!=0,kSyncClient_noDecompressPluginError);
                self->_decompressPlugin=decompressPlugin;
            }
        }
        
        check(_clip_unpackUInt32To(&self->kStrongChecksumByteSize,&clip),kSyncClient_newSyncInfoDataError);
        check(strongChecksumPlugin->checksumByteSize()==self->kStrongChecksumByteSize,
              kSyncClient_strongChecksumByteSizeError);
        check(_clip_unpackUInt32To(&self->kMatchBlockSize,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackUInt32To(&self->samePairCount,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackUIntTo(&self->newDataSize,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackUIntTo(&self->newSyncDataSize,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackUIntTo(&uncompressDataSize,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackUIntTo(&compressDataSize,&clip),kSyncClient_newSyncInfoDataError);
        check(compressDataSize<=uncompressDataSize, kSyncClient_newSyncInfoDataError);
        if (compressDataSize>0) check(decompressPlugin!=0, kSyncClient_newSyncInfoDataError);
        check(_TStreamCacheClip_readUInt(&clip,&self->newSyncInfoSize,sizeof(hpatch_StreamPos_t)),
              kSyncClient_newSyncInfoDataError);
        check(newSyncInfo->streamSize==self->newSyncInfoSize,kSyncClient_newSyncInfoDataError);
        
        check(toUInt32(&kBlockCount,TNewDataSyncInfo_blockCount(self)), kSyncClient_newSyncInfoDataError);
        memSize=strlen(tempType)+1+strlen(checksumType)+1+kPartStrongChecksumByteSize;
    }
    {//mem
        TByte* curMem=0;
        hpatch_StreamPos_t dataSize=(sizeof(roll_uint_t)+kPartStrongChecksumByteSize)*(hpatch_StreamPos_t)kBlockCount;
        if (decompressPlugin)
            dataSize+=sizeof(uint32_t)*(hpatch_StreamPos_t)kBlockCount;
        dataSize+=memSize;
        check(dataSize==(size_t)dataSize,kSyncClient_memError);
        memSize=(size_t)dataSize;
        curMem=(TByte*)malloc(memSize);
        check(curMem!=0,kSyncClient_memError);
        
        self->_import=curMem;
        self->rollHashs=(roll_uint_t*)curMem;
        curMem+=sizeof(roll_uint_t)*(size_t)kBlockCount;
        if (decompressPlugin){
            self->savedSizes=(uint32_t*)curMem;
            curMem+=sizeof(uint32_t)*(size_t)kBlockCount;
        }
        self->partChecksums=curMem;
        curMem+=kPartStrongChecksumByteSize*(size_t)kBlockCount;
        self->infoPartChecksum=curMem;
        curMem+=kPartStrongChecksumByteSize;
        self->strongChecksumType=(const char*)curMem;
        curMem+=strlen(checksumType)+1;
        memcpy((TByte*)self->strongChecksumType,checksumType,curMem-(TByte*)self->strongChecksumType);
        if (decompressPlugin!=0){
            self->compressType=(const char*)curMem;
            memcpy((TByte*)self->compressType,tempType,strlen(tempType)+1);
        }
        curMem+=strlen(tempType)+1;
        assert(curMem==(TByte*)self->_import + memSize);
    }
    {//load infoPartChecksum
        const hpatch_StreamPos_t infoChecksumPos=_TStreamCacheClip_readPosOfSrcStream(&clip);
        check(_TStreamCacheClip_readDataTo(&clip,self->infoPartChecksum,
                                           self->infoPartChecksum+kPartStrongChecksumByteSize),
              kSyncClient_newSyncInfoDataError);
        if ((listener->isChecksumNewSyncInfo==0)||listener->isChecksumNewSyncInfo(listener)){
            result=_checksumInfo(newSyncInfo,infoChecksumPos,
                                 strongChecksumPlugin,self->infoPartChecksum);
            check(result==kSyncClient_ok,result);
        }
    }
    {// compressed? buf
        #define _clear_decompresser(_decompresser) \
                    if (_decompresser.decompressHandle){   \
                        check(decompressPlugin->close(decompressPlugin, \
                            _decompresser.decompressHandle),kSyncClient_decompressError); \
                        _decompresser.decompressHandle=0; }
        TStreamCacheClip _cmCodeClip;
        TStreamCacheClip* codeClip=0;
        size_t tMemSize=self->samePairCount*(size_t)sizeof(TSameNewDataPair);
        if (compressDataSize>0) tMemSize+=hpatch_kFileIOBufBetterSize;
        sameNewDataPairs=(TSameNewDataPair*)malloc(tMemSize);
        check(sameNewDataPairs!=0,kSyncClient_memError);
        if (compressDataSize>0){
            codeClip=&_cmCodeClip;
            TByte* cmbuf=(TByte*)(sameNewDataPairs+self->samePairCount);
            hpatch_StreamPos_t curPos=_TStreamCacheClip_readPosOfSrcStream(&clip);
            check(getStreamClip(codeClip,&decompresser,uncompressDataSize,compressDataSize,
                                newSyncInfo,&curPos,decompressPlugin,
                                cmbuf,hpatch_kFileIOBufBetterSize),kSyncClient_decompressError);
        }else{
            codeClip=&clip;
        }
        
        {//samePairList
            uint32_t pre=0;
            for (size_t i=0;i<self->samePairCount;++i){
                TSameNewDataPair& sp=sameNewDataPairs[i];
                uint32_t v;
                check(_clip_unpackUInt32To(&v,codeClip),kSyncClient_newSyncInfoDataError);
                sp.curIndex=v+pre;
                check(sp.curIndex<kBlockCount, kSyncClient_newSyncInfoDataError);
                check(_clip_unpackUInt32To(&v,codeClip),kSyncClient_newSyncInfoDataError);
                check(v<=sp.curIndex,kSyncClient_newSyncInfoDataError);
                sp.sameIndex=sp.curIndex-v;
                pre=sp.curIndex;
            }
        }
        if (decompressPlugin){ //savedSizes
            hpatch_StreamPos_t sumSavedSize=0;
            uint32_t curPair=0;
            for (uint32_t i=0; i<kBlockCount; ++i){
                if ((curPair<self->samePairCount)
                    &&(i==sameNewDataPairs[curPair].curIndex)){
                    self->savedSizes[i]=self->savedSizes[sameNewDataPairs[curPair].sameIndex];
                    ++curPair;
                }else{
                    check(_clip_unpackUInt32To(&self->savedSizes[i],codeClip),
                          kSyncClient_newSyncInfoDataError);
                    if (self->savedSizes[i]==0)
                        self->savedSizes[i]=TNewDataSyncInfo_newDataBlockSize(self,i);
                }
                sumSavedSize+=self->savedSizes[i];
            }
            check(sumSavedSize==self->newSyncDataSize,kSyncClient_newSyncInfoDataError);
        }

        if (compressDataSize>0){
            _clear_decompresser(decompresser);
            check(_TStreamCacheClip_skipData(&clip,compressDataSize),kSyncClient_newSyncInfoDataError);
        }
    }
    {//rollHashs
        uint32_t curPair=0;
        for (size_t i=0; i<kBlockCount; ++i){
            if ((curPair<self->samePairCount)
                &&(i==sameNewDataPairs[curPair].curIndex)){
                self->rollHashs[i]=self->rollHashs[sameNewDataPairs[curPair].sameIndex];
                ++curPair;
            }else{
                check(_TStreamCacheClip_readUInt(&clip,&self->rollHashs[i],sizeof(roll_uint_t)),
                      kSyncClient_newSyncInfoDataError);
            }
        }
    }
    {//partStrongChecksums
        uint32_t curPair=0;
        for (size_t i=0; i<kBlockCount; ++i){
            TByte* curPartChecksum=self->partChecksums+i*(size_t)kPartStrongChecksumByteSize;
            if ((curPair<self->samePairCount)
                &&(i==sameNewDataPairs[curPair].curIndex)){
                size_t sameIndex=sameNewDataPairs[curPair].sameIndex;
                memcpy(curPartChecksum,self->partChecksums+sameIndex*(size_t)kPartStrongChecksumByteSize,
                       kPartStrongChecksumByteSize);
                ++curPair;
            }else{
                check(_TStreamCacheClip_readDataTo(&clip,curPartChecksum,
                                                   curPartChecksum+kPartStrongChecksumByteSize),
                      kSyncClient_newSyncInfoDataError);
            }
        }
    }
    check(newSyncInfo->streamSize==_TStreamCacheClip_readPosOfSrcStream(&clip),
          kSyncClient_newSyncInfoDataError);
clear:
    _inClear=1;
    _clear_decompresser(decompresser);
    if (sameNewDataPairs) free(sameNewDataPairs);
    if (result!=kSyncClient_ok) TNewDataSyncInfo_close(self);
    if (temp_cache) free(temp_cache);
    return result;
}

void TNewDataSyncInfo_close(TNewDataSyncInfo* self){
    if (self==0) return;
    if (self->_import!=0) free(self->_import);
    TNewDataSyncInfo_init(self);
}

int TNewDataSyncInfo_open_by_file(TNewDataSyncInfo* self,
                                  const char* newSyncInfoPath,ISyncPatchListener *listener){
    hpatch_TFileStreamInput  newSyncInfo;
    hpatch_TFileStreamInput_init(&newSyncInfo);
    int rt;
    int result=kSyncClient_ok;
    int _inClear=0;
    check(hpatch_TFileStreamInput_open(&newSyncInfo,newSyncInfoPath), kSyncClient_newSyncInfoOpenError);

    rt=TNewDataSyncInfo_open(self,&newSyncInfo.base,listener);
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
            TByte* buf=(syncSize<newDataSize)?(dataBuf+kMatchBlockSize):dataBuf;
            if ((out_newStream)||(listener)){
                check(listener->readSyncData(listener,buf,posInNewSyncData,syncSize),
                      kSyncClient_readSyncDataError);
                if (syncSize<newDataSize){
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

int sync_patch(const hpatch_TStreamOutput* out_newStream,const hpatch_TStreamInput*  oldStream,
               const TNewDataSyncInfo* newSyncInfo,ISyncPatchListener* listener){
    assert(listener!=0);
    hpatch_TDecompress* decompressPlugin=0;
    hpatch_TChecksum*   strongChecksumPlugin=0;
    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    uint32_t needSyncCount=0;
    hpatch_StreamPos_t needSyncSize=0;
    hpatch_StreamPos_t* newDataPoss=0;
    int result=kSyncClient_ok;
    int _inClear=0;
    
    //decompressPlugin
    if (newSyncInfo->compressType){
        if ((newSyncInfo->_decompressPlugin!=0)
            &&(newSyncInfo->_decompressPlugin->is_can_open(newSyncInfo->compressType))){
            decompressPlugin=newSyncInfo->_decompressPlugin;
        }else{
            decompressPlugin=listener->findDecompressPlugin(listener,newSyncInfo->compressType);
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

    //match in oldData
    newDataPoss=(hpatch_StreamPos_t*)malloc(kBlockCount*sizeof(hpatch_StreamPos_t));
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

int sync_patch_by_file(const char* out_newPath,const char* oldPath,
                       const char* newSyncInfoPath,ISyncPatchListener* listener){
    int result=kSyncClient_ok;
    int _inClear=0;
    TNewDataSyncInfo         newSyncInfo;
    hpatch_TFileStreamInput  oldData;
    hpatch_TFileStreamOutput out_newData;
    
    TNewDataSyncInfo_init(&newSyncInfo);
    hpatch_TFileStreamInput_init(&oldData);
    hpatch_TFileStreamOutput_init(&out_newData);
    result=TNewDataSyncInfo_open_by_file(&newSyncInfo,newSyncInfoPath,listener);
    check(result==kSyncClient_ok,result);
    check(hpatch_TFileStreamInput_open(&oldData,oldPath),kSyncClient_oldFileOpenError);
    check(hpatch_TFileStreamOutput_open(&out_newData,out_newPath,(hpatch_StreamPos_t)(-1)),
          kSyncClient_newFileCreateError);
    
    result=sync_patch(&out_newData.base,&oldData.base,&newSyncInfo,listener);
clear:
    _inClear=1;
    check(hpatch_TFileStreamOutput_close(&out_newData),kSyncClient_newFileCloseError);
    check(hpatch_TFileStreamInput_close(&oldData),kSyncClient_oldFileCloseError);
    TNewDataSyncInfo_close(&newSyncInfo);
    return result;
}
