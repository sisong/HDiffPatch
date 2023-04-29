//  sync_info_client.cpp
//  sync_client
//  Created by housisong on 2019-09-18.
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
#include "sync_info_client.h"
#include "sync_client_type_private.h"
#include "../../file_for_patch.h"
#include "../../libHDiffPatch/HPatch/patch_private.h"
#if (_IS_NEED_DIR_DIFF_PATCH)
#include "../../dirDiffPatch/dir_patch/dir_patch_tools.h"
#include "../../dirDiffPatch/dir_diff/dir_diff_tools.h"
#endif
#if (_IS_NEED_DIR_DIFF_PATCH)
void TNewDataSyncInfo_dir_saveTo(TNewDataSyncInfo_dir* self,std::vector<hpatch_byte>& out_buf){
    self->dir_newPathSumCharSize=0;
    {
        hdiff_private::packIncList(out_buf,self->dir_newExecuteIndexList,self->dir_newExecuteCount);
        hdiff_private::packList(out_buf,self->dir_newSizeList,self->dir_newPathCount);
        checkv(!self->dir_newNameList_isCString);
        self->dir_newPathSumCharSize=hdiff_private::pushNameList(out_buf,self->dir_utf8NewRootPath,
                                        (std::string*)self->dir_utf8NewNameList,self->dir_newPathCount);
    }
}
#endif

namespace sync_private{
#ifdef  check
# undef check
#endif
#define check(v,errorCode) \
            do{ if (!(v)) { if (result==kSyncClient_ok) result=errorCode; \
                            if (!_inClear) goto clear; } }while(0)

#define _clip_unpackUIntTo(puint,sclip) \
    _TStreamCacheClip_unpackUIntWithTag(sclip,puint,0)

template<class Type_t>
static hpatch_BOOL __clip_unpackToType_t(Type_t* out_v,TStreamCacheClip* clip,hpatch_uint kTagBit=0){
    if (sizeof(Type_t)==sizeof(hpatch_StreamPos_t)){
        return _TStreamCacheClip_unpackUIntWithTag(clip,(hpatch_StreamPos_t*)out_v,kTagBit);
    }else{
        hpatch_StreamPos_t v;
        hpatch_BOOL ret=_TStreamCacheClip_unpackUIntWithTag(clip,&v,kTagBit);
        Type_t tv=(Type_t)v;
        *out_v=tv;
        return ret&&(tv==v);
    }
}

inline static hpatch_BOOL _clip_unpackToUInt32(uint32_t* out_v,TStreamCacheClip* clip,hpatch_uint kTagBit=0){
    return __clip_unpackToType_t<uint32_t>(out_v,clip,kTagBit);
}
inline static hpatch_BOOL _clip_unpackToSize_t(size_t* out_v,TStreamCacheClip* clip){
    return __clip_unpackToType_t<size_t>(out_v,clip);
}
inline static hpatch_BOOL _clip_unpackToByte(hpatch_byte* out_v,TStreamCacheClip* clip){
    return _TStreamCacheClip_readDataTo(clip,out_v,out_v+1);
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
    bool open(const hpatch_TStreamInput* base,hpatch_StreamPos_t newStreamSize,
              hpatch_TChecksum* checksumPlugin){
        assert(_checksum==0);
        if (newStreamSize>base->streamSize)
            return false;
        _base=base;
        _checksumPlugin=checksumPlugin;
        this->streamImport=this;
        this->streamSize=newStreamSize;
        this->read=_read;
        
        _checksumByteSize=_checksumPlugin->checksumByteSize();
        _checksum=_checksumPlugin->open(_checksumPlugin);
        _checksumBuf=(TByte*)malloc(_checksumByteSize);
        if (_checksum)
            _checksumPlugin->begin(_checksum);
        return _checksum!=0;
    }
    bool reopen(hpatch_StreamPos_t newStreamSize){
        if (newStreamSize>_base->streamSize)
            return false;
        this->streamSize=newStreamSize;
        if (_checksum)
            _checksumPlugin->begin(_checksum);
        return _checksum!=0;
    }
    TByte* finish(){
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
    hpatch_BOOL rt=_TStreamCacheClip_unpackUIntWithTag(sclip,&v,0);
    if (rt){
        *result=(TUInt)v;
        if ((sizeof(TUInt)<sizeof(hpatch_StreamPos_t))&&((*result)!=v))
            return hpatch_FALSE;
    }
    return rt;
}

} //namespace sync_private
using namespace sync_private;


TSyncClient_resultType _checkNewSyncInfoType(TStreamCacheClip* newSyncInfo_clip,hpatch_BOOL* out_newIsDir){
    char  tempType[hpatch_kMaxPluginTypeLength+1];
    TSyncClient_resultType result=kSyncClient_ok;
    int _inClear=0;
    check(_TStreamCacheClip_readType_end(newSyncInfo_clip,'&',tempType),
          kSyncClient_newSyncInfoTypeError);
    if (0==strcmp(tempType,"HSyni23"))
        *out_newIsDir=hpatch_FALSE;
#if (_IS_NEED_DIR_DIFF_PATCH)
    else if (0==strcmp(tempType,"HDirSyni23"))
        *out_newIsDir=hpatch_TRUE;
#endif
    else //unknow type
        check(hpatch_FALSE,kSyncClient_newSyncInfoTypeError);
clear:
    _inClear=1;
    return result;
}

TSyncClient_resultType checkNewSyncInfoType(const hpatch_TStreamInput* newSyncInfo,hpatch_BOOL* out_newIsDir){
    TStreamCacheClip    clip;
    TByte temp_cache[hpatch_kMaxPluginTypeLength+1];
    _TStreamCacheClip_init(&clip,newSyncInfo,0,newSyncInfo->streamSize,temp_cache,sizeof(temp_cache));
    return _checkNewSyncInfoType(&clip,out_newIsDir);
}

TSyncClient_resultType checkNewSyncInfoType_by_file(const char* newSyncInfoFile,hpatch_BOOL* out_newIsDir){
    hpatch_TFileStreamInput  newSyncInfo;
    hpatch_TFileStreamInput_init(&newSyncInfo);
    TSyncClient_resultType result=kSyncClient_ok;
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
        if (v>sp.curIndex) return false;
        sp.sameIndex=sp.curIndex-v;
        pre=sp.curIndex;
    }
    return true;
}

static bool _clip_readSavedSize(uint32_t* value,uint32_t* tag,TStreamCacheClip* codeClip){
    const hpatch_byte* buf=_TStreamCacheClip_accessData(codeClip,1);
    if (buf==0) return false;
    *tag=buf[0]>>7;
    return _clip_unpackToUInt32(value,codeClip,1);
}
static bool readSavedSizesTo(TStreamCacheClip* codeClip,TNewDataSyncInfo* self){
    uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(self);
    uint32_t backSize=0;
    hpatch_StreamPos_t sumSavedSize=0;
    for (uint32_t i=0; i<kBlockCount; ++i){
        uint32_t savedSize;
        uint32_t highTag;
        if (!_clip_readSavedSize(&savedSize,&highTag,codeClip)) return false;
        if (highTag==0){
            savedSize+=backSize;
            backSize=savedSize;
        }else if (savedSize>0){
            savedSize=backSize-savedSize;
            backSize=savedSize;
        }else{
            backSize=self->kSyncBlockSize;
        }
        self->savedSizes[i]=savedSize;
        sumSavedSize+=(savedSize>0)?savedSize:TNewDataSyncInfo_newDataBlockSize(self,i);
    }
    if (sumSavedSize>self->newSyncDataSize) return false;
    return true;
}

static bool readPartHashTo(TStreamCacheClip* clip,TByte* partHash,size_t partBits,
                           TSameNewBlockPair* samePairList,size_t samePairCount,uint32_t kBlockCount){
    uint32_t curPair=0;
    TByte* curPartChecksum=partHash;
    const size_t partBytes=_bitsToBytes(partBits);
    size_t bitsValue=0;
    size_t bitsCount=0;
    const size_t highBit=(partBits&7)?(partBits&7):8;
    for (size_t i=0; i<kBlockCount; ++i,curPartChecksum+=partBytes){
        if ((curPair<samePairCount)&&(i==samePairList[curPair].curIndex)){
            size_t sameIndex=samePairList[curPair].sameIndex;
            memcpy(curPartChecksum,partHash+sameIndex*partBytes,partBytes);
            ++curPair;
        }else{
            for (size_t j=0;j<partBytes;++j){
                if (bitsCount<8){//read a byte
                    hpatch_byte v;
                    if (!_TStreamCacheClip_readDataTo(clip,&v,(&v)+1))
                            return false;
                    bitsValue|=(((size_t)v)<<bitsCount);
                    bitsCount+=8;
                }
                if (j==0){
                    curPartChecksum[j]=bitsValue&((1<<highBit)-1);
                    bitsValue>>=highBit;
                    bitsCount-=highBit;
                }else{
                    curPartChecksum[j]=(hpatch_byte)bitsValue;
                    bitsValue>>=8;
                    bitsCount-=8;
                }
            }
        }
    }
    if (bitsValue!=0) return false;
    if (curPair!=samePairCount) return false;
    return true;
}

    typedef struct{
        hsync_TDictDecompress*     decompressPlugin;
        hsync_dictDecompressHandle decompressHandle;
    } _TDictDecompressInputStream;

#if (_IS_NEED_DIR_DIFF_PATCH)
static hpatch_BOOL _readDirHead(TNewDataSyncInfo_dir* dirInfo,TStreamCacheClip* clip){
    if (!_clip_unpackToSize_t(&dirInfo->dir_newExecuteCount,clip)) return hpatch_FALSE;
    if (!_clip_unpackToSize_t(&dirInfo->dir_newPathCount,clip)) return hpatch_FALSE;
    if (!_clip_unpackToSize_t(&dirInfo->dir_newPathSumCharSize,clip)) return hpatch_FALSE;
    return hpatch_TRUE;
}
#endif

static TSyncClient_resultType
    _TNewDataSyncInfo_open(TNewDataSyncInfo* self,const hpatch_TStreamInput* newSyncInfo,
                           ISyncInfoListener *listener){
    assert(self->_import==0);
    hsync_TDictDecompress* decompressPlugin=0;
    hpatch_TChecksum*   strongChecksumPlugin=0;
    TStreamCacheClip    clip;
    TSyncClient_resultType result=kSyncClient_ok;
    int _inClear=0;

    hpatch_BOOL newIsDir_byType=hpatch_FALSE;
    uint32_t    kBlockCount=0;
    const char* checksumType=0;
    char  compressType[hpatch_kMaxPluginTypeLength+1];
    hpatch_StreamPos_t headEndPos=0;
    hpatch_StreamPos_t privateExternDataSize=0;
    hpatch_StreamPos_t externDataSize=0;
    hpatch_StreamPos_t uncompressDataSize=0;
    hpatch_StreamPos_t compressDataSize=0;
    _TDictDecompressInputStream decompresser;
    TChecksumInputStream  checksumInputStream;
    const bool isChecksumNewSyncInfo=true;
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

        }
        {//read strongChecksumType
            check(_TStreamCacheClip_readType_end(&clip,'\0',tempType),kSyncClient_noStrongChecksumPluginError);
            strongChecksumPlugin=listener->findChecksumPlugin(listener,tempType);
            check(strongChecksumPlugin!=0,kSyncClient_noStrongChecksumPluginError);
            checksumType=strongChecksumPlugin->checksumType();
            check(0==strcmp(tempType,checksumType),kSyncClient_noStrongChecksumPluginError);
            self->_strongChecksumPlugin=strongChecksumPlugin;
        }
        check(_clip_unpackToSize_t(&self->dictSize,&clip), kSyncClient_newSyncInfoDataError);
        if (strlen(compressType)>0){
            decompressPlugin=listener->findDecompressPlugin(listener,compressType,self->dictSize);
            check(decompressPlugin!=0,kSyncClient_noDecompressPluginError);
            self->_decompressPlugin=decompressPlugin;
        }
        
        check(_clip_unpackToByte(&self->decompressInfoSize,&clip),kSyncClient_newSyncInfoDataError);
        check(self->decompressInfoSize<=kDecompressInfoMaxSize,kSyncClient_newSyncInfoDataError);
        check(_TStreamCacheClip_readDataTo(&clip,self->decompressInfo,
                self->decompressInfo+self->decompressInfoSize),kSyncClient_newSyncInfoDataError);
        
        check(_clip_unpackUIntTo(&self->newSyncDataSize,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackUIntTo(&self->newSyncDataOffsert,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackUIntTo(&self->newDataSize,&clip),kSyncClient_newSyncInfoDataError);
        if (self->_decompressPlugin){
            static const hpatch_StreamPos_t _kDictSizeSafeLimit =(1<<20)*16;
            check((self->dictSize/2)<(self->newDataSize>=_kDictSizeSafeLimit?self->newDataSize:_kDictSizeSafeLimit),
                kSyncClient_newSyncInfoDataError);
            decompressPlugin=listener->findDecompressPlugin(listener,compressType,self->dictSize);
            check(decompressPlugin!=0,kSyncClient_noDecompressPluginError);
            self->_decompressPlugin=decompressPlugin;
        }

        check(_clip_unpackToUInt32(&self->kSyncBlockSize,&clip),kSyncClient_newSyncInfoDataError);
        check((self->kSyncBlockSize<=(self->newDataSize<=_kSyncBlockSize_min_limit?_kSyncBlockSize_min_limit:self->newDataSize))
              &&(self->kSyncBlockSize>=_kSyncBlockSize_min_limit),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackToSize_t(&self->kStrongChecksumByteSize,&clip),kSyncClient_newSyncInfoDataError);
        check(strongChecksumPlugin->checksumByteSize()==self->kStrongChecksumByteSize,
              kSyncClient_strongChecksumByteSizeError);
        check(_clip_unpackToSize_t(&self->savedStrongChecksumBits,&clip),kSyncClient_newSyncInfoDataError);
        self->savedStrongChecksumByteSize=_bitsToBytes(self->savedStrongChecksumBits);
        check((self->savedStrongChecksumByteSize<=self->kStrongChecksumByteSize),
              kSyncClient_strongChecksumByteSizeError);
        check(_clip_unpackToSize_t(&self->savedRollHashBits,&clip),kSyncClient_newSyncInfoDataError);
        check((size_t)(self->savedRollHashBits-1)<sizeof(uint64_t)*8,kSyncClient_newSyncInfoDataError);
        self->savedRollHashByteSize=_bitsToBytes(self->savedRollHashBits);
        check(_clip_unpackToUInt32(&self->samePairCount,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackToByte(&isSavedSizes,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackUIntTo(&privateExternDataSize,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackUIntTo(&externDataSize,&clip),kSyncClient_newSyncInfoDataError);
        
        check(_clip_unpackUIntTo(&uncompressDataSize,&clip),kSyncClient_newSyncInfoDataError);
        check(_clip_unpackUIntTo(&compressDataSize,&clip),kSyncClient_newSyncInfoDataError);
        check((compressDataSize==0)||(compressDataSize<uncompressDataSize),kSyncClient_newSyncInfoDataError);
        if (compressDataSize>0) check(decompressPlugin!=0,kSyncClient_newSyncInfoDataError);
        
        check(_clip_unpackToByte(&self->isDirSyncInfo,&clip),kSyncClient_newSyncInfoDataError);
        check(newIsDir_byType==self->isDirSyncInfo, kSyncClient_newSyncInfoTypeError);
#if (_IS_NEED_DIR_DIFF_PATCH)
        if (self->isDirSyncInfo){
            check(_clip_unpackToSize_t(&self->dirInfoSavedSize,&clip),kSyncClient_newSyncInfoDataError);
            check(self->dirInfoSavedSize<self->newDataSize,kSyncClient_newSyncInfoDataError);
            check(_readDirHead(&self->dirInfo,&clip),kSyncClient_newSyncInfoDataError);
        }
#endif
        check(_clip_unpackUIntTo(&self->newSyncInfoSize,&clip),kSyncClient_newSyncInfoDataError);
        check(newSyncInfo->streamSize==self->newSyncInfoSize,kSyncClient_newSyncInfoDataError);
        {
            hpatch_StreamPos_t v=TNewDataSyncInfo_blockCount(self);
            kBlockCount=(uint32_t)v;
            check((kBlockCount==v),kSyncClient_newSyncInfoDataError);
        }
        headEndPos=_TStreamCacheClip_readPosOfSrcStream(&clip);
        self->infoChecksumEndPos= headEndPos + privateExternDataSize + externDataSize
                                 + ((compressDataSize>0)?compressDataSize:uncompressDataSize)
                                 + self->kStrongChecksumByteSize*2;
    }
    hpatch_StreamPos_t memSize;
    {//mem
        size_t headMaxSize=sizeof(hpatch_StreamPos_t)*5+ strlen(compressType)+1+strlen(checksumType)+1
                +self->kStrongChecksumByteSize*2+checkChecksumBufByteSize(self->kStrongChecksumByteSize);
        TByte* curMem=0;
        memSize=headMaxSize;
        memSize+=self->samePairCount*sizeof(TSameNewBlockPair);
        if (isSavedSizes)
            memSize+=sizeof(uint32_t)*(hpatch_StreamPos_t)kBlockCount;
#if (_IS_NEED_DIR_DIFF_PATCH)
        if (self->isDirSyncInfo){
            self->dirInfo.dir_newNameList_isCString=hpatch_TRUE;
            memSize+=self->dirInfo.dir_newPathSumCharSize + sizeof(const char*)*self->dirInfo.dir_newPathCount
                    +sizeof(hpatch_StreamPos_t)*self->dirInfo.dir_newPathCount
                    +sizeof(size_t)*self->dirInfo.dir_newExecuteCount;
        }
#endif
        memSize+=(self->savedRollHashByteSize+self->savedStrongChecksumByteSize)*(hpatch_StreamPos_t)kBlockCount;

        const hpatch_StreamPos_t _kSafeTempMemSize=uncompressDataSize+compressDataSize+headMaxSize+1024;
        if ((compressDataSize>0)&&(memSize<_kSafeTempMemSize))
            memSize=_kSafeTempMemSize;
        check(memSize==(size_t)memSize,kSyncClient_memError);
        curMem=(TByte*)malloc((size_t)memSize);
        check(curMem!=0,kSyncClient_memError);
        self->_import=curMem;

        if (decompressPlugin!=0){
            self->compressType=(const char*)curMem;
            curMem+=strlen(compressType)+1;
            memcpy((TByte*)self->compressType,compressType,strlen(compressType)+1);
        }
        self->strongChecksumType=(const char*)curMem;
        curMem+=strlen(checksumType)+1;
        memcpy((TByte*)self->strongChecksumType,checksumType,curMem-(TByte*)self->strongChecksumType);

        curMem=(TByte*)_hpatch_align_upper(curMem,sizeof(hpatch_StreamPos_t));
        self->infoChecksum=curMem;
        curMem+=self->kStrongChecksumByteSize;
        self->infoPartHashChecksum=curMem;
        curMem+=self->kStrongChecksumByteSize;
        self->savedNewDataCheckChecksum=curMem;
        curMem+=checkChecksumBufByteSize(self->kStrongChecksumByteSize);
        
        curMem=(TByte*)_hpatch_align_upper(curMem,sizeof(hpatch_StreamPos_t));
        self->samePairList=(TSameNewBlockPair*)curMem;
        curMem+=self->samePairCount*sizeof(TSameNewBlockPair);
        if (isSavedSizes){
            self->savedSizes=(uint32_t*)curMem;
            curMem+=sizeof(uint32_t)*(size_t)kBlockCount;
        }
#if (_IS_NEED_DIR_DIFF_PATCH)
        if (self->isDirSyncInfo){
            curMem=(TByte*)_hpatch_align_upper(curMem,sizeof(hpatch_StreamPos_t));
            self->dirInfo.dir_newSizeList=(hpatch_StreamPos_t*)curMem;
            curMem+=sizeof(hpatch_StreamPos_t)*self->dirInfo.dir_newPathCount;
            self->dirInfo.dir_utf8NewNameList=(const char**)curMem;
            curMem+=sizeof(const char*)*self->dirInfo.dir_newPathCount;
            self->dirInfo.dir_newExecuteIndexList=(size_t*)curMem;
            curMem+=sizeof(size_t)*self->dirInfo.dir_newExecuteCount;
            if (self->dirInfo.dir_newPathCount>0){
                ((const char**)self->dirInfo.dir_utf8NewNameList)[0]=(char*)curMem;
                curMem+=self->dirInfo.dir_newPathSumCharSize;
            }
        }
#endif

        curMem=(TByte*)_hpatch_align_upper(curMem,sizeof(hpatch_StreamPos_t));
        self->rollHashs=curMem;
        curMem+=self->savedRollHashByteSize*(size_t)kBlockCount;
        curMem=(TByte*)_hpatch_align_upper(curMem,sizeof(hpatch_StreamPos_t));
        self->partChecksums=curMem;
        curMem+=self->savedStrongChecksumByteSize*(size_t)kBlockCount;
        assert(curMem<=(TByte*)self->_import+memSize);
    }
    if (isChecksumNewSyncInfo){
        //reload clip for infoFullChecksum
        check(checksumInputStream.open(newSyncInfo,self->infoChecksumEndPos-self->kStrongChecksumByteSize,
                                       strongChecksumPlugin), kSyncClient_strongChecksumOpenError);
        _TStreamCacheClip_init(&clip,&checksumInputStream,0,checksumInputStream.streamSize,
                               temp_cache,kFileIOBufBetterSize);
        check(_TStreamCacheClip_skipData(&clip,headEndPos), //checksum head data[0--headEndPos)
              kSyncClient_newSyncInfoDataError);
    }
    {//load saved newDataCheckChecksum
        check(_TStreamCacheClip_readDataTo(&clip,self->savedNewDataCheckChecksum,self->savedNewDataCheckChecksum
                                           +self->kStrongChecksumByteSize),kSyncClient_newSyncInfoDataError);
    }
    if (privateExternDataSize>0){
        check(_TStreamCacheClip_skipData(&clip,privateExternDataSize), //reserved ,now unknow
              kSyncClient_newSyncInfoDataError);
    }
    if (externDataSize>0){
        check(_TStreamCacheClip_skipData(&clip,externDataSize), //reserved ,now unknow
              kSyncClient_newSyncInfoDataError);
    }
    
    {// compressed? buf
        #define _clear_decompresser(_decompresser) \
                    if (_decompresser.decompressHandle){   \
                        decompressPlugin->dictDecompressClose(decompressPlugin,_decompresser.decompressHandle); \
                        _decompresser.decompressHandle=0; }
        TStreamCacheClip _cmCodeClip;
        TStreamCacheClip* codeClip=0;
        if (compressDataSize>0) {
            TByte*  decompressBuf=(TByte*)self->_import+(memSize-uncompressDataSize);
            hpatch_byte* codeBuf=decompressBuf-compressDataSize;
            hpatch_StreamPos_t curPos=_TStreamCacheClip_readPosOfSrcStream(&clip);
            const hpatch_TStreamInput* inStream=isChecksumNewSyncInfo?&checksumInputStream:newSyncInfo;
            
            check(inStream->read(inStream,curPos,codeBuf,codeBuf+compressDataSize),kSyncClient_newSyncInfoDataError);
            decompresser.decompressPlugin=decompressPlugin;
            decompresser.decompressHandle=decompressPlugin->dictDecompressOpen(decompressPlugin,1,(size_t)uncompressDataSize,0,0);
            check((decompresser.decompressHandle!=0),kSyncClient_decompressOpenError);
            check(decompressPlugin->dictDecompress(decompresser.decompressHandle,0,codeBuf,codeBuf+compressDataSize,
                                                   decompressBuf,decompressBuf+uncompressDataSize),
                  kSyncClient_decompressError);
            
            codeClip=&_cmCodeClip;
            _TStreamCacheClip_init(codeClip,0,0,0,decompressBuf,(size_t)uncompressDataSize);
            codeClip->cacheBegin=0;
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
        if (compressDataSize>0)
            _clear_decompresser(decompresser);
    }
    if (isChecksumNewSyncInfo){//info Checksum
        check(newSyncInfo->read(newSyncInfo,checksumInputStream.streamSize,self->infoChecksum,
                                self->infoChecksum+self->kStrongChecksumByteSize),
              kSyncClient_newSyncInfoDataError);

        TByte* strongChecksumInfo=checksumInputStream.finish();
        assert(self->kStrongChecksumByteSize==checksumInputStream.checksumByteSize());
        check(0==memcmp(strongChecksumInfo,self->infoChecksum,self->kStrongChecksumByteSize),
              kSyncClient_newSyncInfoChecksumError);

        check(checksumInputStream.reopen(newSyncInfo->streamSize-self->kStrongChecksumByteSize),
              kSyncClient_strongChecksumOpenError);
    }
    {
        hpatch_StreamPos_t curPos=_TStreamCacheClip_readPosOfSrcStream(&clip);
        const hpatch_TStreamInput* inStream=isChecksumNewSyncInfo?&checksumInputStream:newSyncInfo;
        _TStreamCacheClip_init(&clip,inStream,curPos+compressDataSize,inStream->streamSize,
                               temp_cache,kFileIOBufBetterSize);

        _TStreamCacheClip_skipData(&clip,self->kStrongChecksumByteSize);
    }

    //rollHashs
    check(readPartHashTo(&clip,self->rollHashs,self->savedRollHashBits,self->samePairList,
                         self->samePairCount,kBlockCount), kSyncClient_newSyncInfoDataError);
    //partStrongChecksums
    check(readPartHashTo(&clip,self->partChecksums,self->savedStrongChecksumBits,
                         self->samePairList,self->samePairCount,
                         kBlockCount), kSyncClient_newSyncInfoDataError);

    if (isChecksumNewSyncInfo){ //info partHash Checksum
        const hpatch_StreamPos_t infoChecksumPos=_TStreamCacheClip_readPosOfSrcStream(&clip);
        check(newSyncInfo->read(newSyncInfo,infoChecksumPos,self->infoPartHashChecksum,
                                self->infoPartHashChecksum+self->kStrongChecksumByteSize),
              kSyncClient_newSyncInfoDataError);
        
        TByte* strongChecksumInfo=checksumInputStream.finish();
        assert(self->kStrongChecksumByteSize==checksumInputStream.checksumByteSize());
        check(0==memcmp(strongChecksumInfo,self->infoPartHashChecksum,self->kStrongChecksumByteSize),
              kSyncClient_newSyncInfoChecksumError);
    }
    check(_TStreamCacheClip_readPosOfSrcStream(&clip)+self->kStrongChecksumByteSize
          ==newSyncInfo->streamSize, kSyncClient_newSyncInfoDataError);
clear:
    _inClear=1;
    _clear_decompresser(decompresser);
    if (result!=kSyncClient_ok) TNewDataSyncInfo_close(self);
    if (temp_cache) free(temp_cache);
    return result;
}

#if (_IS_NEED_DIR_DIFF_PATCH)
TSyncClient_resultType TNewDataSyncInfo_dir_load(TNewDataSyncInfo_dir* self,const hpatch_byte* buf,size_t bufSize){
    TSyncClient_resultType result=kSyncClient_ok;
    int _inClear=0;
    TStreamCacheClip _clip;
    TStreamCacheClip* clip=&_clip;
    _TStreamCacheClip_init(clip,0,0,0,(hpatch_byte*)buf,bufSize);
    clip->cacheBegin=0;

    check(_readDirHead(self,clip),kSyncClient_savedNewDirInfoDataError);

    //dir_newExecuteIndexList
    check(readIncListTo(clip,self->dir_newExecuteIndexList,self->dir_newExecuteCount,
                        self->dir_newPathCount),kSyncClient_savedNewDirInfoDataError);
    //dir_newSizeList
    check(readListTo(clip,self->dir_newSizeList,self->dir_newPathCount),
            kSyncClient_savedNewDirInfoDataError);
    //dir_utf8NewNameList
    if (self->dir_newPathCount>0){
        char* charsBuf=((char**)self->dir_utf8NewNameList)[0];
        check(_TStreamCacheClip_readDataTo(clip,(TByte*)charsBuf,
                                           (TByte*)charsBuf+self->dir_newPathSumCharSize),
              kSyncClient_savedNewDirInfoDataError);
        formatDirTagForLoad(charsBuf,charsBuf+self->dir_newPathSumCharSize);
        check(clipCStrsTo(charsBuf,charsBuf+self->dir_newPathSumCharSize,
                            (const char**)self->dir_utf8NewNameList,self->dir_newPathCount),
                kSyncClient_savedNewDirInfoDataError);
    }
clear:
    _inClear=1;
    return result;
}
#endif

void TNewDataSyncInfo_close(TNewDataSyncInfo* self){
    if (self==0) return;
    if (self->_import!=0) free(self->_import);
    TNewDataSyncInfo_init(self);
}

TSyncClient_resultType TNewDataSyncInfo_open(TNewDataSyncInfo* self,const hpatch_TStreamInput* newSyncInfo,
                                             ISyncInfoListener* listener){
    TSyncClient_resultType result=_TNewDataSyncInfo_open(self,newSyncInfo,listener);
    if ((result==kSyncClient_ok)&&listener->onLoadedNewSyncInfo)
        listener->onLoadedNewSyncInfo(listener,self);
    return result;
}

TSyncClient_resultType TNewDataSyncInfo_open_by_file(TNewDataSyncInfo* self,const char* newSyncInfoFile,
                                                     ISyncInfoListener *listener){
    hpatch_TFileStreamInput  newSyncInfo;
    hpatch_TFileStreamInput_init(&newSyncInfo);
    TSyncClient_resultType result=kSyncClient_ok;
    int _inClear=0;
    check(hpatch_TFileStreamInput_open(&newSyncInfo,newSyncInfoFile), kSyncClient_newSyncInfoOpenError);
    result=_TNewDataSyncInfo_open(self,&newSyncInfo.base,listener);
clear:
    _inClear=1;
    check(hpatch_TFileStreamInput_close(&newSyncInfo), kSyncClient_newSyncInfoCloseError);
    if ((result==kSyncClient_ok)&&listener->onLoadedNewSyncInfo)
        listener->onLoadedNewSyncInfo(listener,self);
    return result;
}

