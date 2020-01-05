//  sync_info_client.cpp
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
#include "sync_info_client.h"
#include "../../file_for_patch.h"
#include "../../libHDiffPatch/HPatch/patch_private.h"

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

int TNewDataSyncInfo_open(TNewDataSyncInfo* self,const hpatch_TStreamInput* newSyncInfo,
                          ISyncInfoListener *listener){
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
    const bool isChecksumNewSyncInfo=listener->checksumSet.isChecksumNewSyncInfo;
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
        dataSize+=self->samePairCount*sizeof(TSameNewBlockPair);
        if (decompressPlugin)
            dataSize+=sizeof(uint32_t)*(hpatch_StreamPos_t)kBlockCount;
        dataSize+=memSize;
        check(dataSize==(size_t)dataSize,kSyncClient_memError);
        memSize=(size_t)dataSize;
        curMem=(TByte*)malloc(memSize);
        check(curMem!=0,kSyncClient_memError);
        
        self->_import=curMem;
        self->samePairList=(TSameNewBlockPair*)curMem;
        curMem+=self->samePairCount*sizeof(TSameNewBlockPair);
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
                TSameNewBlockPair& sp=self->samePairList[i];
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
                uint32_t savedSize=0;
                if ((curPair<self->samePairCount)
                    &&(i==self->samePairList[curPair].curIndex)){
                    savedSize=self->savedSizes[self->samePairList[curPair].sameIndex];
                    ++curPair;
                }else{
                    check(_clip_unpackUInt32To(&savedSize,codeClip),
                          kSyncClient_newSyncInfoDataError);
                    if (savedSize==0)
                        savedSize=TNewDataSyncInfo_newDataBlockSize(self,i);
                }
                self->savedSizes[i]=savedSize;
                sumSavedSize+=savedSize;
            }
            check(curPair==self->samePairCount,kSyncClient_newSyncInfoDataError);
            check(sumSavedSize==self->newSyncDataSize,kSyncClient_newSyncInfoDataError);
        }else{
            assert(self->savedSizes==0);
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
        check(curPair==self->samePairCount,kSyncClient_newSyncInfoDataError);
    }
    if (isChecksumNewSyncInfo){ //infoPartChecksum
        const hpatch_StreamPos_t infoChecksumPos=_TStreamCacheClip_readPosOfSrcStream(&clip);
        check(newSyncInfo->read(newSyncInfo,infoChecksumPos,self->infoPartChecksum,
                                self->infoPartChecksum+kPartStrongChecksumByteSize),
              kSyncClient_newSyncInfoDataError);
        
        TByte* strongChecksumInfo=checksumInputStream.end();
        toSyncPartChecksum(strongChecksumInfo,strongChecksumInfo,checksumInputStream.checksumByteSize());
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

int TNewDataSyncInfo_open_by_file(TNewDataSyncInfo* self,const char* newSyncInfoFile,
                                  ISyncInfoListener *listener){
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

