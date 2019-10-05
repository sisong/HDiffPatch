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
#include "../sync_client/mt_by_queue.h"

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

struct TChecksumInputStream:public hpatch_TStreamInput {
    inline TChecksumInputStream()
        :_base(0),_checksumPlugin(0),_checksum(0),_curReadPos(0),_checksumByteSize(0),_checksumBuf(0) {
        memset((hpatch_TStreamInput*)this,0,sizeof(hpatch_TStreamInput));
    }
    inline ~TChecksumInputStream(){
        if (_checksum) _checksumPlugin->close(_checksumPlugin,_checksum);
        if (_checksumBuf) free(_checksumBuf);
    }
    bool open(const hpatch_TStreamInput* base,hpatch_TChecksum* checksumPlugin,
              hpatch_StreamPos_t removeStreamSize){
        assert(_checksum==0);
        _base=base;
        _checksumPlugin=checksumPlugin;
        this->streamImport=this;
        this->streamSize=base->streamSize-removeStreamSize;
        this->read=_read;
        
        _checksumByteSize=_checksumPlugin->checksumByteSize();
        _checksum=_checksumPlugin->open(_checksumPlugin);
        _checksumBuf=(TByte*)malloc(_checksumByteSize);
        if (_checksum)
            _checksumPlugin->begin(_checksum);
        return _checksum!=0;
    }
    TByte* end(){
        _checksumPlugin->end(_checksum,_checksumBuf,_checksumBuf+_checksumByteSize);
        return _checksumBuf;
    }
    inline size_t checksumByteSize()const{ return _checksumByteSize; }
private:
    hpatch_BOOL _read_(hpatch_StreamPos_t readFromPos,unsigned char* out_data,unsigned char* out_data_end){
        hpatch_BOOL result=_base->read(_base,readFromPos,out_data,out_data_end);
        if (result){
            size_t readSize=(size_t)(out_data_end-out_data);
            if (readFromPos>_curReadPos) { assert(false); return hpatch_FALSE; }
            //    _curReadPos|
            //      [    readSize    ]
            if (readFromPos+readSize>_curReadPos){
                _checksumPlugin->append(_checksum,out_data+(_curReadPos-readFromPos),out_data_end);
                _curReadPos=readFromPos+readSize;
            }
        }
        return result;
    }
    static hpatch_BOOL _read(const struct hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                      unsigned char* out_data,unsigned char* out_data_end){
        TChecksumInputStream* self=(TChecksumInputStream*)stream->streamImport;
        return self->_read_(readFromPos,out_data,out_data_end);
    }
    const hpatch_TStreamInput*  _base;
    hpatch_TChecksum*           _checksumPlugin;
    hpatch_checksumHandle       _checksum;
    hpatch_StreamPos_t          _curReadPos;
    size_t                      _checksumByteSize;
    TByte*                      _checksumBuf;
};

template<class TUInt>
hpatch_inline static
hpatch_BOOL _clip_readUIntTo(TUInt* result,TStreamCacheClip* sclip){
    hpatch_StreamPos_t v;
    hpatch_BOOL rt=_TStreamCacheClip_readUInt(sclip,&v,sizeof(TUInt));
    if (rt) *result=(TUInt)v;
    return rt;
}
#define rollHashSize(self) (self->is32Bit_rollHash?sizeof(uint32_t):sizeof(uint64_t))

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
    char  compressType[hpatch_kMaxPluginTypeLength+1];
    hpatch_StreamPos_t headEndPos=0;
    hpatch_StreamPos_t uncompressDataSize=0;
    hpatch_StreamPos_t compressDataSize=0;
    TByte*             decompressBuf=0;
    _TDecompressInputSteram decompresser;
    TChecksumInputStream  checksumInputStream;
    const bool isChecksumNewSyncInfo=listener->isChecksumNewSyncInfo;
    const size_t kFileIOBufBetterSize=hpatch_kFileIOBufBetterSize;
    TByte* temp_cache=(TByte*)malloc(kFileIOBufBetterSize);
    check(temp_cache!=0,kSyncClient_memError);
    memset(&decompresser,0,sizeof(decompresser));
    {//head
        hpatch_StreamPos_t reservedDataSize;
        char  tempType[hpatch_kMaxPluginTypeLength+1];
        const size_t kHeadCacheSize =1024*2;
        assert(kFileIOBufBetterSize>=kHeadCacheSize);
        _TStreamCacheClip_init(&clip,newSyncInfo,0,newSyncInfo->streamSize,
                               temp_cache,isChecksumNewSyncInfo?kHeadCacheSize:kFileIOBufBetterSize);
        const char* kTypeVersion="SyncUpdate19";
        {//type
            check(_TStreamCacheClip_readType_end(&clip,'&',tempType),kSyncClient_newSyncInfoTypeError);
            check(0==strcmp(tempType,kTypeVersion),kSyncClient_newSyncInfoTypeError);
        }
        {//read compressType
            check(_TStreamCacheClip_readType_end(&clip,'&',compressType),kSyncClient_noDecompressPluginError);
            if (strlen(compressType)>0){
                decompressPlugin=listener->findDecompressPlugin(listener,compressType);
                check(decompressPlugin!=0,kSyncClient_noDecompressPluginError);
                self->_decompressPlugin=decompressPlugin;
            }
        }
        {//read strongChecksumType
            check(_TStreamCacheClip_readType_end(&clip,'\0',tempType),kSyncClient_noStrongChecksumPluginError);
            strongChecksumPlugin=listener->findChecksumPlugin(listener,tempType);
            check(strongChecksumPlugin!=0,kSyncClient_noStrongChecksumPluginError);
            checksumType=strongChecksumPlugin->checksumType();
            check(0==strcmp(tempType,checksumType),kSyncClient_noStrongChecksumPluginError);
            self->_strongChecksumPlugin=strongChecksumPlugin;
        }
        
        check(_clip_unpackUInt32To(&self->kStrongChecksumByteSize,&clip),kSyncClient_newSyncInfoDataError);
        check(strongChecksumPlugin->checksumByteSize()==self->kStrongChecksumByteSize,
              kSyncClient_strongChecksumByteSizeError);
        check(_clip_unpackUInt32To(&self->kMatchBlockSize,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackUInt32To(&self->samePairCount,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_readUIntTo(&self->is32Bit_rollHash,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackUIntTo(&self->newDataSize,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackUIntTo(&self->newSyncDataSize,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackUIntTo(&reservedDataSize,&clip),kSyncClient_newSyncInfoDataError);
        
        check(_clip_unpackUIntTo(&uncompressDataSize,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackUIntTo(&compressDataSize,&clip),kSyncClient_newSyncInfoDataError);
        check(compressDataSize<=uncompressDataSize, kSyncClient_newSyncInfoDataError);
        if (compressDataSize>0) check(decompressPlugin!=0, kSyncClient_newSyncInfoDataError);
        check(_clip_readUIntTo(&self->newSyncInfoSize,&clip), kSyncClient_newSyncInfoDataError);
        check(newSyncInfo->streamSize==self->newSyncInfoSize,kSyncClient_newSyncInfoDataError);
        
        if (reservedDataSize>0){
            check(_TStreamCacheClip_skipData(&clip,reservedDataSize), //now not used
                  kSyncClient_newSyncInfoDataError);
        }
        check(toUInt32(&kBlockCount,TNewDataSyncInfo_blockCount(self)), kSyncClient_newSyncInfoDataError);
        memSize=strlen(compressType)+1+strlen(checksumType)+1+kPartStrongChecksumByteSize;
        headEndPos=_TStreamCacheClip_readPosOfSrcStream(&clip);
    }
    {//mem
        TByte* curMem=0;
        hpatch_StreamPos_t dataSize=(rollHashSize(self)+kPartStrongChecksumByteSize)
                                    *(hpatch_StreamPos_t)kBlockCount;
        dataSize+=self->samePairCount*sizeof(TSameNewDataPair);
        if (decompressPlugin)
            dataSize+=sizeof(uint32_t)*(hpatch_StreamPos_t)kBlockCount;
        dataSize+=memSize;
        check(dataSize==(size_t)dataSize,kSyncClient_memError);
        memSize=(size_t)dataSize;
        curMem=(TByte*)malloc(memSize);
        check(curMem!=0,kSyncClient_memError);
        
        self->_import=curMem;
        self->samePairList=(TSameNewDataPair*)curMem;
        curMem+=self->samePairCount*sizeof(TSameNewDataPair);
        self->rollHashs=curMem;
        curMem+=rollHashSize(self)*(size_t)kBlockCount;
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
            memcpy((TByte*)self->compressType,compressType,strlen(compressType)+1);
        }
        curMem+=strlen(compressType)+1;
        assert(curMem==(TByte*)self->_import + memSize);
    }
    if (isChecksumNewSyncInfo){
        //reload clip for infoPartChecksum
        check(checksumInputStream.open(newSyncInfo,strongChecksumPlugin,kPartStrongChecksumByteSize),
              kSyncClient_strongChecksumOpenError);
        _TStreamCacheClip_init(&clip,&checksumInputStream,0,checksumInputStream.streamSize,
                               temp_cache,kFileIOBufBetterSize);
        check(_TStreamCacheClip_skipData(&clip,headEndPos), //checksum head data
              kSyncClient_newSyncInfoDataError);
    }
    {// compressed? buf
        #define _clear_decompresser(_decompresser) \
                    if (_decompresser.decompressHandle){   \
                        check(decompressPlugin->close(decompressPlugin, \
                            _decompresser.decompressHandle),kSyncClient_decompressError); \
                        _decompresser.decompressHandle=0; }
        TStreamCacheClip _cmCodeClip;
        TStreamCacheClip* codeClip=0;
        if (compressDataSize>0){
        }
        if (compressDataSize>0){
            codeClip=&_cmCodeClip;
            decompressBuf=(TByte*)malloc(kFileIOBufBetterSize);
            check(decompressBuf!=0,kSyncClient_memError);
            hpatch_StreamPos_t curPos=_TStreamCacheClip_readPosOfSrcStream(&clip);
            const hpatch_TStreamInput* inStream=isChecksumNewSyncInfo?&checksumInputStream:newSyncInfo;
            check(getStreamClip(codeClip,&decompresser,uncompressDataSize,compressDataSize,
                                inStream,&curPos,decompressPlugin,
                                decompressBuf,kFileIOBufBetterSize),kSyncClient_decompressError);
        }else{
            codeClip=&clip;
        }
        
        {//samePairList
            uint32_t pre=0;
            for (size_t i=0;i<self->samePairCount;++i){
                TSameNewDataPair& sp=self->samePairList[i];
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
                    &&(i==self->samePairList[curPair].curIndex)){
                    self->savedSizes[i]=self->savedSizes[self->samePairList[curPair].sameIndex];
                    ++curPair;
                }else{
                    check(_clip_unpackUInt32To(&self->savedSizes[i],codeClip),
                          kSyncClient_newSyncInfoDataError);
                    if (self->savedSizes[i]==0)
                        self->savedSizes[i]=TNewDataSyncInfo_newDataBlockSize(self,i);
                }
                sumSavedSize+=self->savedSizes[i];
            }
            assert(curPair==self->samePairCount);
            check(sumSavedSize==self->newSyncDataSize,kSyncClient_newSyncInfoDataError);
        }

        if (compressDataSize>0){
            _clear_decompresser(decompresser);
            hpatch_StreamPos_t curPos=_TStreamCacheClip_readPosOfSrcStream(&clip);
            const hpatch_TStreamInput* inStream=isChecksumNewSyncInfo?&checksumInputStream:newSyncInfo;
            _TStreamCacheClip_init(&clip,inStream,curPos+compressDataSize,inStream->streamSize,
                                   temp_cache,kFileIOBufBetterSize);
        }
    }
    {//rollHashs
        uint32_t curPair=0;
        bool is32Bit_rollHash=(0!=self->is32Bit_rollHash);
        uint32_t* rhashs32=(uint32_t*)self->rollHashs;
        uint64_t* rhashs64=(uint64_t*)self->rollHashs;
        for (size_t i=0; i<kBlockCount; ++i){
            if ((curPair<self->samePairCount)
                &&(i==self->samePairList[curPair].curIndex)){
                uint32_t sameIndex=self->samePairList[curPair].sameIndex;
                if (is32Bit_rollHash)
                    rhashs32[i]=rhashs32[sameIndex];
                else
                    rhashs64[i]=rhashs64[sameIndex];
                ++curPair;
            }else{
                if (is32Bit_rollHash){
                    check(_clip_readUIntTo(&rhashs32[i],&clip), kSyncClient_newSyncInfoDataError);
                }else{
                    check(_clip_readUIntTo(&rhashs64[i],&clip), kSyncClient_newSyncInfoDataError);
                }
            }
        }
        assert(curPair==self->samePairCount);
    }
    {//partStrongChecksums
        uint32_t curPair=0;
        for (size_t i=0; i<kBlockCount; ++i){
            TByte* curPartChecksum=self->partChecksums+i*(size_t)kPartStrongChecksumByteSize;
            if ((curPair<self->samePairCount)
                &&(i==self->samePairList[curPair].curIndex)){
                size_t sameIndex=self->samePairList[curPair].sameIndex;
                memcpy(curPartChecksum,self->partChecksums+sameIndex*(size_t)kPartStrongChecksumByteSize,
                       kPartStrongChecksumByteSize);
                ++curPair;
            }else{
                check(_TStreamCacheClip_readDataTo(&clip,curPartChecksum,
                                                   curPartChecksum+kPartStrongChecksumByteSize),
                      kSyncClient_newSyncInfoDataError);
            }
        }
        assert(curPair==self->samePairCount);
    }
    if (isChecksumNewSyncInfo){ //infoPartChecksum
        const hpatch_StreamPos_t infoChecksumPos=_TStreamCacheClip_readPosOfSrcStream(&clip);
        check(newSyncInfo->read(newSyncInfo,infoChecksumPos,self->infoPartChecksum,
                                self->infoPartChecksum+kPartStrongChecksumByteSize),
              kSyncClient_newSyncInfoDataError);
        
        TByte* strongChecksumInfo=checksumInputStream.end();
        toPartChecksum(strongChecksumInfo,strongChecksumInfo,checksumInputStream.checksumByteSize());
        check(0==memcmp(strongChecksumInfo,self->infoPartChecksum,kPartStrongChecksumByteSize),
              kSyncClient_newSyncInfoChecksumError);
    }
    check(_TStreamCacheClip_readPosOfSrcStream(&clip)+kPartStrongChecksumByteSize
            ==newSyncInfo->streamSize, kSyncClient_newSyncInfoDataError);
clear:
    _inClear=1;
    _clear_decompresser(decompresser);
    if (decompressBuf) free(decompressBuf);
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
                                  const char* newSyncInfoFile,ISyncPatchListener *listener){
    hpatch_TFileStreamInput  newSyncInfo;
    hpatch_TFileStreamInput_init(&newSyncInfo);
    int rt;
    int result=kSyncClient_ok;
    int _inClear=0;
    check(hpatch_TFileStreamInput_open(&newSyncInfo,newSyncInfoFile), kSyncClient_newSyncInfoOpenError);

    rt=TNewDataSyncInfo_open(self,&newSyncInfo.base,listener);
    check(rt==kSyncClient_ok,rt);
clear:
    _inClear=1;
    check(hpatch_TFileStreamInput_close(&newSyncInfo), kSyncClient_newSyncInfoCloseError);
    return result;
}

struct _TWriteDatas {
    const hpatch_TStreamOutput* out_newStream;
    const TNewDataSyncInfo*     newSyncInfo;
    const hpatch_StreamPos_t*   newDataPoss;
    const hpatch_TStreamInput*  oldStream;
    hpatch_TDecompress*         decompressPlugin;
    hpatch_TChecksum*           strongChecksumPlugin;
    ISyncPatchListener*         listener;
};

static int mt_writeToNew(_TWriteDatas& wd,void* _mt=0,int threadIndex=0) {
    const TNewDataSyncInfo* newSyncInfo=wd.newSyncInfo;
    ISyncPatchListener*     listener=wd.listener;
    hpatch_TChecksum*       strongChecksumPlugin=wd.strongChecksumPlugin;
    int result=kSyncClient_ok;
    int _inClear=0;
    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    const uint32_t kMatchBlockSize=newSyncInfo->kMatchBlockSize;
    const bool     isChecksumNewSyncData=listener->isChecksumNewSyncData;
    TByte*             dataBuf=0;
    TByte*             checksumSync_buf=0;
    hpatch_checksumHandle checksumSync=0;
    hpatch_StreamPos_t posInNewSyncData=0;
    hpatch_StreamPos_t outNewDataPos=0;
    const hpatch_StreamPos_t oldDataSize=wd.oldStream->streamSize;
    
    size_t _memSize=kMatchBlockSize*(wd.decompressPlugin?2:1)
                    +(isChecksumNewSyncData ? newSyncInfo->kStrongChecksumByteSize:0);
    dataBuf=(TByte*)malloc(_memSize);
    check(dataBuf!=0,kSyncClient_memError);
    if (isChecksumNewSyncData){
        checksumSync_buf=dataBuf+_memSize-newSyncInfo->kStrongChecksumByteSize;
        checksumSync=strongChecksumPlugin->open(strongChecksumPlugin);
        check(checksumSync!=0,kSyncClient_strongChecksumOpenError);
    }
    for (uint32_t syncSize,newDataSize,i=0; i<kBlockCount; ++i,
                            outNewDataPos+=newDataSize,posInNewSyncData+=syncSize) {
        syncSize=TNewDataSyncInfo_syncBlockSize(newSyncInfo,i);
        newDataSize=TNewDataSyncInfo_newDataBlockSize(newSyncInfo,i);
#if (_IS_USED_MULTITHREAD)
        if (_mt) { if (!((TMt_by_queue*)_mt)->getWork(threadIndex,i)) continue; } //next work;
#endif
        const hpatch_StreamPos_t curSyncPos=wd.newDataPoss[i];
        if (curSyncPos>=oldDataSize){ //needSync
            TByte* buf=(syncSize<newDataSize)?(dataBuf+kMatchBlockSize):dataBuf;
            if ((wd.out_newStream)||(listener)){
                {//read data
                    TSyncDataType cacheIndex=(curSyncPos==kBlockType_needSync)?kSyncDataType_needSync
                                                              :(curSyncPos-oldDataSize);
#if (_IS_USED_MULTITHREAD)
                    TMt_by_queue::TAutoInputLocker _autoLocker((TMt_by_queue*)_mt);
#endif
                    check(listener->readSyncData(listener,posInNewSyncData,syncSize,cacheIndex,buf),
                          kSyncClient_readSyncDataError);
                }
                if (syncSize<newDataSize){
                    check(hpatch_deccompress_mem(wd.decompressPlugin,buf,buf+syncSize,
                                                 dataBuf,dataBuf+newDataSize),kSyncClient_decompressError);
                }
                if (isChecksumNewSyncData){ //checksum
                    if (newDataSize<kMatchBlockSize)//for backZeroLen
                        memset(dataBuf+newDataSize,0,kMatchBlockSize-newDataSize);
                    strongChecksumPlugin->begin(checksumSync);
                    strongChecksumPlugin->append(checksumSync,dataBuf,dataBuf+kMatchBlockSize);
                    strongChecksumPlugin->end(checksumSync,checksumSync_buf,
                                              checksumSync_buf+newSyncInfo->kStrongChecksumByteSize);
                    toPartChecksum(checksumSync_buf,checksumSync_buf,newSyncInfo->kStrongChecksumByteSize);
                    check(0==memcmp(checksumSync_buf,
                                    newSyncInfo->partChecksums+i*(size_t)kPartStrongChecksumByteSize,
                                    kPartStrongChecksumByteSize),kSyncClient_checksumSyncDataError);
                }
            }
        }else{//copy from old
#if (_IS_USED_MULTITHREAD)
            TMt_by_queue::TAutoInputLocker _autoLocker((TMt_by_queue*)_mt); //can use other locker
#endif
            check(wd.oldStream->read(wd.oldStream,curSyncPos,dataBuf,dataBuf+newDataSize),
                  kSyncClient_readOldDataError);
        }
        if (wd.out_newStream){//write
#if (_IS_USED_MULTITHREAD)
            TMt_by_queue::TAutoOutputLocker _autoLocker((TMt_by_queue*)_mt,threadIndex,i);
#endif
            check(wd.out_newStream->write(wd.out_newStream,outNewDataPos,dataBuf,
                                          dataBuf+newDataSize), kSyncClient_writeNewDataError);
        }
    }
    assert(outNewDataPos==newSyncInfo->newDataSize);
    assert(posInNewSyncData==newSyncInfo->newSyncDataSize);
clear:
    _inClear=1;
    if (checksumSync) strongChecksumPlugin->close(strongChecksumPlugin,checksumSync);
    if (dataBuf) free(dataBuf);
    return result;
}


#if (_IS_USED_MULTITHREAD)
struct TMt_threadDatas{
    _TWriteDatas*       writeDatas;
    TMt_by_queue*       shareDatas;
    int                 result;
};

static void _mt_threadRunCallBackProc(int threadIndex,void* workData){
    TMt_threadDatas* tdatas=(TMt_threadDatas*)workData;
    int result=mt_writeToNew(*tdatas->writeDatas,tdatas->shareDatas,threadIndex);
    {//set result
        TMt_by_queue::TAutoLocker _auto_locker(tdatas->shareDatas);
        if (tdatas->result==kSyncClient_ok) tdatas->result=result;
    }
    tdatas->shareDatas->finish();
    bool isMainThread=(threadIndex==tdatas->shareDatas->threadNum-1);
    if (isMainThread) tdatas->shareDatas->waitAllFinish();
}
#endif

static int writeToNew(_TWriteDatas& writeDatas,int threadNum) {

#if (_IS_USED_MULTITHREAD)
    if (threadNum>1){
        const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(writeDatas.newSyncInfo);
        TMt_by_queue   shareDatas((int)threadNum,kBlockCount,true);
        TMt_threadDatas  tdatas;
        tdatas.shareDatas=&shareDatas;
        tdatas.writeDatas=&writeDatas;
        tdatas.result=kSyncClient_ok;
        thread_parallel((int)threadNum,_mt_threadRunCallBackProc,&tdatas,1);
        return tdatas.result;
    }else
#endif
    {
        return mt_writeToNew(writeDatas);
    }
}


struct TMatchedSyncInfo:public TNeedSyncInfo{
    inline explicit TMatchedSyncInfo(bool _isUseCache)
    :isUseCache(_isUseCache) { needSyncCount=0; needCacheSyncCount=0;
        needSyncSize=0; needCacheSyncSize=0; }
    const bool isUseCache;
};

static void setSameOldPos(hpatch_StreamPos_t* out_newDataPoss,TMatchedSyncInfo& msi,
                          const TNewDataSyncInfo* newSyncInfo,hpatch_StreamPos_t oldDataSize){
    static const hpatch_StreamPos_t kBlockType_repeat =kBlockType_needSync-1;
    
    uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    msi.needSyncCount=kBlockCount;
    msi.needSyncSize=newSyncInfo->newSyncDataSize;
    uint32_t curPair=0;
    for (uint32_t syncSize,i=0; i<kBlockCount; ++i){
        syncSize=TNewDataSyncInfo_syncBlockSize(newSyncInfo,i);
        if (out_newDataPoss[i]!=kBlockType_needSync){
            --msi.needSyncCount;
            msi.needSyncSize-=syncSize;
        }
        if ((curPair<newSyncInfo->samePairCount)
            &&(i==newSyncInfo->samePairList[curPair].curIndex)){
            assert(out_newDataPoss[i]==kBlockType_needSync);
            {
                uint32_t sameIndex=newSyncInfo->samePairList[curPair].sameIndex;
                hpatch_StreamPos_t syncInfo=out_newDataPoss[sameIndex];
                if (syncInfo<oldDataSize){
                    out_newDataPoss[i]=syncInfo; //ok from old
                    --msi.needSyncCount;
                    msi.needSyncSize-=syncSize;
                }else{
                    if (msi.isUseCache){
                        out_newDataPoss[i]=kBlockType_repeat;
                        out_newDataPoss[sameIndex]=kBlockType_repeat;
                        --msi.needSyncCount;
                        msi.needSyncSize-=syncSize;
                    } //else download,not cache
                }
            }
            ++curPair;
        }
    }
    assert(curPair==newSyncInfo->samePairCount);
    
    if (!msi.isUseCache) return;
    
    hpatch_StreamPos_t cacheIndex=0;
    curPair=0;
    for (uint32_t i=0; i<kBlockCount; ++i){
        if (out_newDataPoss[i]==kBlockType_repeat){
            out_newDataPoss[i]=oldDataSize+cacheIndex;
        }
        if ((curPair<newSyncInfo->samePairCount)
            &&(i==newSyncInfo->samePairList[curPair].curIndex)){
            {
                uint32_t sameIndex=newSyncInfo->samePairList[curPair].sameIndex;
                out_newDataPoss[i]=out_newDataPoss[sameIndex];
            }
            ++curPair;
        }
        if (out_newDataPoss[i]==oldDataSize+cacheIndex){
            ++msi.needCacheSyncCount;
            uint32_t syncSize=TNewDataSyncInfo_syncBlockSize(newSyncInfo,i);
            msi.needCacheSyncSize+=syncSize;
            ++cacheIndex;
        }
    }
    assert(curPair==newSyncInfo->samePairCount);
}

static void sendSyncMsg(ISyncPatchListener* listener,const hpatch_StreamPos_t* newDataPoss,
                        const TNewDataSyncInfo* newSyncInfo,const TMatchedSyncInfo& msi,hpatch_StreamPos_t oldDataSize){
    if (listener==0) return;
    if (listener->needSyncMsg)
        listener->needSyncMsg(listener,&msi);
    if (listener->needSyncDataMsg==0) return;
    
    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    hpatch_StreamPos_t posInNewSyncData=0;
    for (uint32_t syncSize,i=0; i<kBlockCount; ++i,posInNewSyncData+=syncSize){
        syncSize=TNewDataSyncInfo_syncBlockSize(newSyncInfo,i);
        hpatch_StreamPos_t curSyncInfo=newDataPoss[i];
        if (curSyncInfo<oldDataSize) continue;
        TSyncDataType syncType=(curSyncInfo==kBlockType_needSync)?kSyncDataType_needSync
                                                                 :(curSyncInfo-oldDataSize);
        listener->needSyncDataMsg(listener,posInNewSyncData,syncSize,syncType);
    }
    assert(posInNewSyncData==newSyncInfo->newSyncDataSize);
}

static void printMatchResult(const TNewDataSyncInfo* newSyncInfo,const TMatchedSyncInfo& msi) {
    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    printf("syncCount: %d (/%d=%.3f) (cacheCount: %d)  syncSize: %" PRIu64 "\n",
           msi.needSyncCount,kBlockCount,(double)msi.needSyncCount/kBlockCount,
           msi.needCacheSyncCount,msi.needSyncSize);
    hpatch_StreamPos_t downloadSize=newSyncInfo->newSyncInfoSize+msi.needSyncSize;
    printf("downloadSize: %" PRIu64 "+%" PRIu64 "= %" PRIu64 " (/%" PRIu64 "=%.3f)",
           newSyncInfo->newSyncInfoSize,msi.needSyncSize,downloadSize,
           newSyncInfo->newSyncDataSize,(double)downloadSize/newSyncInfo->newSyncDataSize);
    printf(" (/%" PRIu64 "=%.3f)\n",
           newSyncInfo->newDataSize,(double)downloadSize/newSyncInfo->newDataSize);
}

int sync_patch(const hpatch_TStreamOutput* out_newStream,const hpatch_TStreamInput*  oldStream,
               const TNewDataSyncInfo* newSyncInfo,ISyncPatchListener* listener,int threadNum){
    assert(listener!=0);
    hpatch_TDecompress* decompressPlugin=0;
    hpatch_TChecksum*   strongChecksumPlugin=0;
    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    TMatchedSyncInfo matchedSyncInfo(listener->isCanCacheRepeatSyncData);
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
    newDataPoss=(hpatch_StreamPos_t*)malloc(kBlockCount*(size_t)sizeof(hpatch_StreamPos_t));
    check(newDataPoss!=0,kSyncClient_memError);
    try{
        matchNewDataInOld(newDataPoss,newSyncInfo,oldStream,strongChecksumPlugin,threadNum);
    }catch(...){
        result=kSyncClient_matchNewDataInOldError;
    }
    check(result==kSyncClient_ok,result);
    setSameOldPos(newDataPoss,matchedSyncInfo,newSyncInfo,oldStream->streamSize);
    printMatchResult(newSyncInfo,matchedSyncInfo);
    
    //send msg: all need sync block
    sendSyncMsg(listener,newDataPoss,newSyncInfo,matchedSyncInfo,oldStream->streamSize);
    
    _TWriteDatas writeDatas;
    writeDatas.out_newStream=out_newStream;
    writeDatas.newSyncInfo=newSyncInfo;
    writeDatas.newDataPoss=newDataPoss;
    writeDatas.oldStream=oldStream;
    writeDatas.decompressPlugin=decompressPlugin;
    writeDatas.strongChecksumPlugin=strongChecksumPlugin;
    writeDatas.listener=listener;
    result=writeToNew(writeDatas,threadNum);
    check(result==kSyncClient_ok,result);
clear:
    _inClear=1;
    if (newDataPoss) free(newDataPoss);
    return result;
}

int sync_patch_by_file(const char* outNewPath,const char* oldPath,
                       const char* newSyncInfoFile,ISyncPatchListener* listener,int threadNum){
    int result=kSyncClient_ok;
    int _inClear=0;
    TNewDataSyncInfo         newSyncInfo;
    hpatch_TFileStreamInput  oldData;
    hpatch_TFileStreamOutput out_newData;
    
    TNewDataSyncInfo_init(&newSyncInfo);
    hpatch_TFileStreamInput_init(&oldData);
    hpatch_TFileStreamOutput_init(&out_newData);
    result=TNewDataSyncInfo_open_by_file(&newSyncInfo,newSyncInfoFile,listener);
    check(result==kSyncClient_ok,result);
    check(hpatch_TFileStreamInput_open(&oldData,oldPath),kSyncClient_oldFileOpenError);
    check(hpatch_TFileStreamOutput_open(&out_newData,outNewPath,(hpatch_StreamPos_t)(-1)),
          kSyncClient_newFileCreateError);
    
    result=sync_patch(&out_newData.base,&oldData.base,&newSyncInfo,listener,threadNum);
clear:
    _inClear=1;
    check(hpatch_TFileStreamOutput_close(&out_newData),kSyncClient_newFileCloseError);
    check(hpatch_TFileStreamInput_close(&oldData),kSyncClient_oldFileCloseError);
    TNewDataSyncInfo_close(&newSyncInfo);
    return result;
}
