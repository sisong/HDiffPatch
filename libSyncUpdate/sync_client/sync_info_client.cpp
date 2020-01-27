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
#if (_IS_NEED_DIR_DIFF_PATCH)
#include "../../dirDiffPatch/dir_patch/dir_patch_tools.h"
#endif
namespace sync_private{

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
inline static bool _clip_unpackToUInt32(uint32_t* out_v,TStreamCacheClip* clip){
    hpatch_StreamPos_t v;
    if (_clip_unpackUIntTo(&v,clip))
        return toUInt32(out_v,v);
    return false;
}
#if (_IS_NEED_DIR_DIFF_PATCH)
inline static bool _clip_unpackToSize_t(size_t* out_v,TStreamCacheClip* clip){
    if (sizeof(size_t)==sizeof(hpatch_StreamPos_t))
        return _clip_unpackUIntTo((hpatch_StreamPos_t*)out_v,clip)!=0;
    else
        return _clip_unpackToUInt32((uint32_t*)out_v,clip);
}
#endif


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

} //namespace sync_private
using namespace sync_private;


int _checkNewSyncInfoType(TStreamCacheClip* newSyncInfo_clip,hpatch_BOOL* out_newIsDir){
    char  tempType[hpatch_kMaxPluginTypeLength+1];
    int result=kSyncClient_ok;
    int _inClear=0;
    check(_TStreamCacheClip_readType_end(newSyncInfo_clip,'&',tempType),
          kSyncClient_newSyncInfoTypeError);
    if (0==strcmp(tempType,"HSync20"))
        *out_newIsDir=hpatch_FALSE;
#if (_IS_NEED_DIR_DIFF_PATCH)
    else if (0==strcmp(tempType,"HDirSync20"))
        *out_newIsDir=hpatch_TRUE;
#endif
    else //unknow type
        check(hpatch_FALSE,kSyncClient_newSyncInfoTypeError);
clear:
    _inClear=1;
    return result;
}

int checkNewSyncInfoType(const hpatch_TStreamInput* newSyncInfo,hpatch_BOOL* out_newIsDir){
    TStreamCacheClip    clip;
    TByte temp_cache[hpatch_kMaxPluginTypeLength+1];
    _TStreamCacheClip_init(&clip,newSyncInfo,0,newSyncInfo->streamSize,temp_cache,sizeof(temp_cache));
    return _checkNewSyncInfoType(&clip,out_newIsDir);
}

int checkNewSyncInfoType_by_file(const char* newSyncInfoFile,hpatch_BOOL* out_newIsDir){
    hpatch_TFileStreamInput  newSyncInfo;
    hpatch_TFileStreamInput_init(&newSyncInfo);
    int result=kSyncClient_ok;
    int _inClear=0;
    check(hpatch_TFileStreamInput_open(&newSyncInfo,newSyncInfoFile), kSyncClient_newSyncInfoOpenError);
    result=checkNewSyncInfoType(&newSyncInfo.base,out_newIsDir);
clear:
    _inClear=1;
    check(hpatch_TFileStreamInput_close(&newSyncInfo), kSyncClient_newSyncInfoCloseError);
    return result;
}

static bool readSamePairListTo(TStreamCacheClip* codeClip,TSameNewBlockPair* samePairList,
                               size_t samePairCount,uint32_t kBlockCount){
    uint32_t pre=0;
    for (size_t i=0;i<samePairCount;++i){
        TSameNewBlockPair& sp=samePairList[i];
        uint32_t v;
        if (!_clip_unpackToUInt32(&v,codeClip)) return false;
        sp.curIndex=v+pre;
        if (sp.curIndex>=kBlockCount) return false;
        if (!_clip_unpackToUInt32(&v,codeClip)) return false;
        if (v>=sp.curIndex) return false;
        sp.sameIndex=sp.curIndex-v;
        pre=sp.curIndex;
    }
    return true;
}

static bool readSavedSizesTo(TStreamCacheClip* codeClip,TNewDataSyncInfo* self){
    uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(self);
    hpatch_StreamPos_t sumSavedSize=0;
    uint32_t curPair=0;
    for (uint32_t i=0; i<kBlockCount; ++i){
        uint32_t savedSize=0;
        if ((curPair<self->samePairCount)&&(i==self->samePairList[curPair].curIndex)){
            savedSize=self->savedSizes[self->samePairList[curPair].sameIndex];
            ++curPair;
        }else{
            if (!_clip_unpackToUInt32(&savedSize,codeClip)) return false;
            if (savedSize==0)
                savedSize=TNewDataSyncInfo_newDataBlockSize(self,i);
        }
        self->savedSizes[i]=savedSize;
        sumSavedSize+=savedSize;
    }
    if (curPair!=self->samePairCount) return false;
    if (sumSavedSize!=self->newSyncDataSize) return false;
    return true;
}

static bool readRollHashsTo(TStreamCacheClip* clip,void* rollHashs,uint8_t is32Bit_rollHash,
                            TSameNewBlockPair* samePairList,size_t samePairCount,uint32_t kBlockCount){
    uint32_t curPair=0;
    uint32_t* rhashs32=((uint32_t*)rollHashs);
    uint64_t* rhashs64=(uint64_t*)rollHashs;
    for (size_t i=0; i<kBlockCount; ++i){
        if ((curPair<samePairCount)&&(i==samePairList[curPair].curIndex)){
            uint32_t sameIndex=samePairList[curPair].sameIndex;
            if (is32Bit_rollHash)
                rhashs32[i]=rhashs32[sameIndex];
                else
                    rhashs64[i]=rhashs64[sameIndex];
                    ++curPair;
        }else{
            if (is32Bit_rollHash){
                if (!_clip_readUIntTo(&rhashs32[i],clip)) return false;
            }else{
                if (!_clip_readUIntTo(&rhashs64[i],clip)) return false;
            }
        }
    }
    if (curPair!=samePairCount) return false;
    return true;
}

static bool readPartStrongChecksumsTo(TStreamCacheClip* clip,TByte* partChecksums,
                                      TSameNewBlockPair* samePairList,size_t samePairCount,uint32_t kBlockCount){
    uint32_t curPair=0;
    TByte* curPartChecksum=partChecksums;
    for (size_t i=0; i<kBlockCount; ++i,curPartChecksum+=kPartStrongChecksumByteSize){
        if ((curPair<samePairCount)&&(i==samePairList[curPair].curIndex)){
            size_t sameIndex=samePairList[curPair].sameIndex;
            memcpy(curPartChecksum,partChecksums+sameIndex*kPartStrongChecksumByteSize,
                   kPartStrongChecksumByteSize);
            ++curPair;
        }else{
            if (!_TStreamCacheClip_readDataTo(clip,curPartChecksum,curPartChecksum+kPartStrongChecksumByteSize))
                return false;
        }
    }
    if (curPair!=samePairCount) return false;
    return true;
}

int TNewDataSyncInfo_open(TNewDataSyncInfo* self,const hpatch_TStreamInput* newSyncInfo,
                          ISyncInfoListener *listener){
    assert(self->_import==0);
    hpatch_TDecompress* decompressPlugin=0;
    hpatch_TChecksum*   strongChecksumPlugin=0;
    TStreamCacheClip    clip;
    int result=kSyncClient_ok;
    int _inClear=0;

    hpatch_BOOL newIsDir_byType=hpatch_FALSE;
#if (_IS_NEED_DIR_DIFF_PATCH)
    size_t      dir_newPathSumCharSize=0;
#endif
    uint32_t    kBlockCount=0;
    const char* checksumType=0;
    char  compressType[hpatch_kMaxPluginTypeLength+1];
    hpatch_StreamPos_t headEndPos=0;
    hpatch_StreamPos_t privateExternDataSize=0;
    hpatch_StreamPos_t externDataSize=0;
    hpatch_StreamPos_t uncompressDataSize=0;
    hpatch_StreamPos_t compressDataSize=0;
    TByte*             decompressBuf=0;
    _TDecompressInputSteram decompresser;
    TChecksumInputStream  checksumInputStream;
    const bool isChecksumNewSyncInfo=listener->checksumSet.isChecksumNewSyncInfo;
    uint8_t isSavedSizes=0;
    const size_t kFileIOBufBetterSize=hpatch_kFileIOBufBetterSize;
    TByte* temp_cache=(TByte*)malloc(kFileIOBufBetterSize);
    check(temp_cache!=0,kSyncClient_memError);
    memset(&decompresser,0,sizeof(decompresser));
    {//head
        char  tempType[hpatch_kMaxPluginTypeLength+1];
        const size_t kHeadCacheSize =1024*2;
        assert(kFileIOBufBetterSize>=kHeadCacheSize);
        _TStreamCacheClip_init(&clip,newSyncInfo,0,newSyncInfo->streamSize,
                               temp_cache,isChecksumNewSyncInfo?kHeadCacheSize:kFileIOBufBetterSize);
        {//type
            result=_checkNewSyncInfoType(&clip,&newIsDir_byType);
            check(result==kSyncClient_ok,result);
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
        
        check(_clip_unpackToUInt32(&self->kStrongChecksumByteSize,&clip),kSyncClient_newSyncInfoDataError);
        check(strongChecksumPlugin->checksumByteSize()==self->kStrongChecksumByteSize,
              kSyncClient_strongChecksumByteSizeError);
        check(_clip_unpackToUInt32(&self->kMatchBlockSize,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackToUInt32(&self->samePairCount,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_readUIntTo(&self->isDirSyncInfo,&clip),kSyncClient_newSyncInfoDataError);
        check(newIsDir_byType==self->isDirSyncInfo, kSyncClient_newSyncInfoTypeError);
        check(_clip_readUIntTo(&self->is32Bit_rollHash,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_readUIntTo(&isSavedSizes,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackUIntTo(&self->newDataSize,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackUIntTo(&self->newSyncDataSize,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackUIntTo(&privateExternDataSize,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackUIntTo(&externDataSize,&clip),kSyncClient_newSyncInfoDataError);
        
        check(_clip_unpackUIntTo(&uncompressDataSize,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackUIntTo(&compressDataSize,&clip),kSyncClient_newSyncInfoDataError);
        check(compressDataSize<uncompressDataSize, kSyncClient_newSyncInfoDataError);
        if (compressDataSize>0) check(decompressPlugin!=0, kSyncClient_newSyncInfoDataError);
#if (_IS_NEED_DIR_DIFF_PATCH)
        if (self->isDirSyncInfo){
            check(_clip_unpackToSize_t(&dir_newPathSumCharSize,&clip),kSyncClient_newSyncInfoDataError);
            check(_clip_unpackToSize_t(&self->dir_newPathCount,&clip),kSyncClient_newSyncInfoDataError);
            check(_clip_unpackToSize_t(&self->dir_newExecuteCount,&clip),kSyncClient_newSyncInfoDataError);
        }
#endif
        check(_clip_readUIntTo(&self->newSyncInfoSize,&clip), kSyncClient_newSyncInfoDataError);
        check(newSyncInfo->streamSize==self->newSyncInfoSize,kSyncClient_newSyncInfoDataError);
        check(toUInt32(&kBlockCount,TNewDataSyncInfo_blockCount(self)), kSyncClient_newSyncInfoDataError);
        headEndPos=_TStreamCacheClip_readPosOfSrcStream(&clip);
    }
    {//mem
        TByte* curMem=0;
        hpatch_StreamPos_t memSize=strlen(compressType)+1+strlen(checksumType)+1
                                   +kPartStrongChecksumByteSize + externDataSize;
        memSize+=(rollHashSize(self)+kPartStrongChecksumByteSize)*(hpatch_StreamPos_t)kBlockCount;
        memSize+=self->samePairCount*sizeof(TSameNewBlockPair);
        if (decompressPlugin)
            memSize+=sizeof(uint32_t)*(hpatch_StreamPos_t)kBlockCount;
#if (_IS_NEED_DIR_DIFF_PATCH)
        if (self->isDirSyncInfo){
            self->dir_newNameList_isCString=hpatch_TRUE;
            memSize+=dir_newPathSumCharSize + sizeof(const char*)*self->dir_newPathCount
                    +sizeof(hpatch_StreamPos_t)*self->dir_newPathCount
                    +sizeof(size_t)*self->dir_newExecuteCount;
        }
#endif
        check(memSize==(size_t)memSize,kSyncClient_memError);
        curMem=(TByte*)malloc((size_t)memSize);
        check(curMem!=0,kSyncClient_memError);
        self->_import=curMem;
        self->samePairList=(TSameNewBlockPair*)curMem;
        curMem+=self->samePairCount*sizeof(TSameNewBlockPair);
        self->partChecksums=curMem;
        curMem+=kPartStrongChecksumByteSize*(size_t)kBlockCount;
        self->infoPartChecksum=curMem;
        curMem+=kPartStrongChecksumByteSize;
#if (_IS_NEED_DIR_DIFF_PATCH)
        if (self->isDirSyncInfo){
            self->dir_newSizeList=(hpatch_StreamPos_t*)curMem;
            curMem+=sizeof(hpatch_StreamPos_t)*self->dir_newPathCount;
            self->dir_utf8NewNameList=(const char**)curMem;
            curMem+=sizeof(const char*)*self->dir_newPathCount;
            self->dir_newExecuteIndexList=(size_t*)curMem;
            curMem+=sizeof(size_t)*self->dir_newExecuteCount;
        }
#endif
        self->rollHashs=curMem;
        curMem+=rollHashSize(self)*(size_t)kBlockCount;
        if (isSavedSizes){
            self->savedSizes=(uint32_t*)curMem;
            curMem+=sizeof(uint32_t)*(size_t)kBlockCount;
        }
#if (_IS_NEED_DIR_DIFF_PATCH)
        if (self->isDirSyncInfo){
            if (self->dir_newPathCount>0){
                ((const char**)self->dir_utf8NewNameList)[0]=(char*)curMem;
                curMem+=dir_newPathSumCharSize;
            }
        }
#endif
        self->strongChecksumType=(const char*)curMem;
        curMem+=strlen(checksumType)+1;
        memcpy((TByte*)self->strongChecksumType,checksumType,curMem-(TByte*)self->strongChecksumType);
        if (decompressPlugin!=0){
            self->compressType=(const char*)curMem;
            memcpy((TByte*)self->compressType,compressType,strlen(compressType)+1);
        }
        curMem+=strlen(compressType)+1;
        self->externData_begin=curMem;
        self->externData_end=self->externData_begin+(size_t)externDataSize;
        curMem+=externDataSize;
        assert(curMem==(TByte*)self->_import + memSize);
    }
    if (isChecksumNewSyncInfo){
        //reload clip for infoPartChecksum
        check(checksumInputStream.open(newSyncInfo,strongChecksumPlugin,kPartStrongChecksumByteSize),
              kSyncClient_strongChecksumOpenError);
        _TStreamCacheClip_init(&clip,&checksumInputStream,0,checksumInputStream.streamSize,
                               temp_cache,kFileIOBufBetterSize);
        check(_TStreamCacheClip_skipData(&clip,headEndPos), //checksum head data[0--headEndPos)
              kSyncClient_newSyncInfoDataError);
    }
    if (privateExternDataSize>0){
        check(_TStreamCacheClip_skipData(&clip,privateExternDataSize), //reserved ,now unknow
              kSyncClient_newSyncInfoDataError);
    }
    if (externDataSize>0){
        check(_TStreamCacheClip_readDataTo(&clip,(TByte*)self->externData_begin,(TByte*)self->externData_end),
              kSyncClient_newSyncInfoDataError);
    }
    {// compressed? buf
        #define _clear_decompresser(_decompresser,_decompressBuf) \
                    if (_decompresser.decompressHandle){   \
                        check(decompressPlugin->close(decompressPlugin, \
                            _decompresser.decompressHandle),kSyncClient_decompressError); \
                        _decompresser.decompressHandle=0; } \
                    if (_decompressBuf) { free(_decompressBuf); _decompressBuf=0; }
        TStreamCacheClip _cmCodeClip;
        TStreamCacheClip* codeClip=0;
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
        
        //samePairList
        check(readSamePairListTo(codeClip,self->samePairList,self->samePairCount,kBlockCount),
              kSyncClient_newSyncInfoDataError);
        if (isSavedSizes) //savedSizes
            check(readSavedSizesTo(codeClip,self),kSyncClient_newSyncInfoDataError);
        else
            assert(self->savedSizes==0);
        
#if (_IS_NEED_DIR_DIFF_PATCH)
        if (self->isDirSyncInfo){
            //dir_utf8NewNameList
            if (self->dir_newPathCount>0){
                char* charsBuf=((char**)self->dir_utf8NewNameList)[0];
                check(_TStreamCacheClip_readDataTo(codeClip,(TByte*)charsBuf,
                                                   (TByte*)charsBuf+dir_newPathSumCharSize),
                      kSyncClient_newSyncInfoDataError);
                formatDirTagForLoad(charsBuf,charsBuf+dir_newPathSumCharSize);
                check(clipCStrsTo(charsBuf,charsBuf+dir_newPathSumCharSize,
                                  (const char**)self->dir_utf8NewNameList,self->dir_newPathCount),
                      kSyncClient_newSyncInfoDataError);
            }
            //dir_newSizeList
            check(readListTo(codeClip,self->dir_newSizeList,self->dir_newPathCount),
                  kSyncClient_newSyncInfoDataError);
            //dir_newExecuteIndexList
            check(readIncListTo(codeClip,self->dir_newExecuteIndexList,self->dir_newExecuteCount,
                                self->dir_newPathCount),kSyncClient_newSyncInfoDataError);
        }
#endif
        if (compressDataSize>0){
            _clear_decompresser(decompresser,decompressBuf);
            hpatch_StreamPos_t curPos=_TStreamCacheClip_readPosOfSrcStream(&clip);
            const hpatch_TStreamInput* inStream=isChecksumNewSyncInfo?&checksumInputStream:newSyncInfo;
            _TStreamCacheClip_init(&clip,inStream,curPos+compressDataSize,inStream->streamSize,
                                   temp_cache,kFileIOBufBetterSize);
        }
    }
    //rollHashs
    check(readRollHashsTo(&clip,self->rollHashs,self->is32Bit_rollHash, self->samePairList,
                          self->samePairCount,kBlockCount), kSyncClient_newSyncInfoDataError);
    //partStrongChecksums
    check(readPartStrongChecksumsTo(&clip,self->partChecksums,self->samePairList,self->samePairCount,
                                    kBlockCount), kSyncClient_newSyncInfoDataError);

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
    _clear_decompresser(decompresser,decompressBuf);
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
    int result=kSyncClient_ok;
    int _inClear=0;
    check(hpatch_TFileStreamInput_open(&newSyncInfo,newSyncInfoFile), kSyncClient_newSyncInfoOpenError);
    result=TNewDataSyncInfo_open(self,&newSyncInfo.base,listener);
clear:
    _inClear=1;
    check(hpatch_TFileStreamInput_close(&newSyncInfo), kSyncClient_newSyncInfoCloseError);
    return result;
}

