//  zsync_info_client.cpp
//  zsync_client for hsynz
//  Created by housisong on 2025-06-28.
/*
 The MIT License (MIT)
 Copyright (c) 2025 HouSisong
 
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
#include "zsync_info_client.h"
#include "zsync_client_type_private.h"
#include "../../file_for_patch.h"
#include "../../libHDiffPatch/HPatch/patch_private.h"
#include "../../_atosize.h"
#include "../../_hextobytes.h"
#include <stdio.h> //sscanf

namespace sync_private{
#ifdef  check
# undef check
#endif
#define check(v,errorCode) \
            do{ if (!(v)) { if (result==kSyncClient_ok) result=errorCode; \
                            if (!_inClear) goto clear; } }while(0)

static const char* kZsync_VERSION="0.6.3"; //now supported zsync version

} //namespace sync_private
using namespace sync_private;

static char* _readZsyncStr(TStreamCacheClip* newSyncInfo_clip,char* out_str,size_t strBufLen){
    if (!_TStreamCacheClip_readStr_end(newSyncInfo_clip,'\n',out_str,strBufLen))
        return 0; //error
    int strLen=(int)strlen(out_str);
    if (strLen==0) return out_str; //key-value empty, end loop
    while (strLen>0){
        char& c=out_str[--strLen];
        if ((c=='\r')||(c=='\n')||(c==' ')) c=0;//remove end '\r' '\n' ' '
        else break;
    }
    char* pv = strchr(out_str,':');
    if (pv==0) return 0; //error
    *pv++=0; //split key and value
    if (*pv==' ') ++pv; //remove value begin ' '
    return pv;
}

static const size_t kMaxZsyncStrLine=1024;
static
TSyncClient_resultType _checkNewZsyncInfoType(TStreamCacheClip* newSyncInfo_clip){
    char  key[kMaxZsyncStrLine];
    TSyncClient_resultType result=kSyncClient_ok;
    int _inClear=0;
    const char* value=_readZsyncStr(newSyncInfo_clip,key,sizeof(key));
    check(value!=0,kSyncClient_newZsyncInfoTypeError);
    check(0==strcmp(key,"zsync"),kSyncClient_newZsyncInfoTypeError);
    check(0!=strcmp(value,"0.0.4"),kSyncClient_newZsyncInfoTypeError);//not compatible with zsync 0.0.4
clear:
    _inClear=1;
    return result;
}

TSyncClient_resultType checkNewZsyncInfoType(const hpatch_TStreamInput* newSyncInfo){
    TStreamCacheClip    clip;
    TByte temp_cache[hpatch_kStreamCacheSize];
    static_assert(hpatch_kStreamCacheSize>=kMaxZsyncStrLine,"hpatch_kStreamCacheSize must be >= kMaxZsyncStrLine");
    _TStreamCacheClip_init(&clip,newSyncInfo,0,newSyncInfo->streamSize,temp_cache,sizeof(temp_cache));
    return _checkNewZsyncInfoType(&clip);
}

TSyncClient_resultType checkNewZsyncInfoType_by_file(const char* newSyncInfoFile){
    hpatch_TFileStreamInput  newSyncInfo;
    hpatch_TFileStreamInput_init(&newSyncInfo);
    TSyncClient_resultType result=kSyncClient_ok;
    int _inClear=0;
    check(hpatch_TFileStreamInput_open(&newSyncInfo,newSyncInfoFile), kSyncClient_newZsyncInfoOpenError);
    result=checkNewZsyncInfoType(&newSyncInfo.base);
clear:
    _inClear=1;
    check(hpatch_TFileStreamInput_close(&newSyncInfo), kSyncClient_newZsyncInfoCloseError);
    return result;
}


static bool read2PartHashTo(TStreamCacheClip* clip,TByte* rollHashs,size_t rollBytes,
                            TByte* checksums,size_t checksumBytes,uint32_t kBlockCount){
    for (size_t i=0; i<kBlockCount; ++i,rollHashs+=rollBytes,checksums+=checksumBytes){
        if (!_TStreamCacheClip_readDataTo(clip,rollHashs,rollHashs+rollBytes)) return false;
        if (!_TStreamCacheClip_readDataTo(clip,checksums,checksums+checksumBytes)) return false;
    }
    return true;
}


    #define _DEF_insertOutBits(){ \
            if ((insert>=kBlockCount)||(curBits==0)||(curBits>=(1<<(32-3)))) return false;  \
            if (insert==0) self->newSyncDataOffsert=curBlockBitsPos>>3;                     \
            savedBitsInfos[insert].skipBitsInFirstCodeByte=curBlockBitsPos&((1<<3)-1);      \
            savedBitsInfos[insert].bitsSize=(uint32_t)curBits;  \
            insert++;   \
            curBlockBitsPos+=curBits;   \
            curOuts=0;  \
            curBits=0; }

static bool readSavedBitsTo(const TByte* zmap2_data,size_t zmap2_blocks,TNewDataZsyncInfo* self){
    size_t kBlockSize=self->kSyncBlockSize;
    size_t kBlockCount=(size_t)TNewDataSyncInfo_blockCount(self);
    hpatch_StreamPos_t sumOuts=0;
    hpatch_StreamPos_t curBlockBitsPos=0;
    savedBitsInfo_t* savedBitsInfos=self->savedBitsInfos;
    size_t insert=0,curOuts=0,curBits=0;
    for (size_t i=0;i<zmap2_blocks;++i,zmap2_data+=4){
        const uint16_t bits=(((uint16_t)zmap2_data[0])<<8)|zmap2_data[1];
        uint16_t       outs=(((uint16_t)zmap2_data[2])<<8)|zmap2_data[3];
        const bool isBolckStart=((outs>>15)==0);
        outs&=((1<<15)-1);
        sumOuts+=outs;
        if (outs==0){
            if (((insert==0)&&(curOuts==0))||(insert>=kBlockCount)){ assert(isBolckStart); curBlockBitsPos+=bits; continue; }
        }
        if (curOuts+outs>kBlockSize){
            if (curOuts!=kBlockSize) return false;
            _DEF_insertOutBits();
        }
        curOuts+=outs;
        curBits+=bits;
    }
    if (curOuts>0){
        if (curOuts>kBlockSize) return false;
        _DEF_insertOutBits();
    }
    self->newSyncDataSize=((curBlockBitsPos+7)>>3)+8; //8 is gzip file foot size
    return (sumOuts==self->newDataSize)&&(insert==kBlockCount)&&(curOuts==0)&&(curBits==0);
}

static TSyncClient_resultType
    _TNewDataZsyncInfo_open(TNewDataZsyncInfo* self,const hpatch_TStreamInput* newSyncInfo,hpatch_BOOL isIgnoreCompressInfo,
                            ISyncInfoListener *listener){
    assert((self->_import==0)&&(self->_extraMem==0));
    TSyncClient_resultType result=kSyncClient_ok;
    int _inClear=0;
    size_t checksumBytes=20;
    hpatch_byte _savedNewDataCheckChecksum[20];
    const char* checksumType="";
    TStreamCacheClip    clip;
    hpatch_uint32_t     kBlockCount=0;
    TByte* temp_cache=0;
    TByte* zmap2_data=0;
    size_t zmap2_blocks;
    const char*  zmap2_data_compressType="gzipD";
    const size_t zmap2_data_compressDictSize=32*1024;
    const size_t kFileIOBufBetterSize=hpatch_kFileIOBufBetterSize;

    self->strongChecksumPlugin=listener->findChecksumPlugin(listener,"md4"); //for oldData
    check(self->strongChecksumPlugin!=0,kSyncClient_noStrongChecksumPluginError);
    self->isNotCChecksumNewMTParallel=hpatch_TRUE; //unsupport parallel checksum (when _matchRange), must check by order (when _writeToNewOrDiff_by)

    temp_cache=(TByte*)malloc(kFileIOBufBetterSize);
    check(temp_cache!=0,kSyncClient_memError);
    _TStreamCacheClip_init(&clip,newSyncInfo,0,newSyncInfo->streamSize,
                            temp_cache,kFileIOBufBetterSize);

    self->fileChecksumPlugin=0;
    self->isSeqMatch=hpatch_FALSE;
    self->savedStrongChecksumByteSize=16;//for md4
    self->savedRollHashByteSize=4; //for adler32


    result=_checkNewZsyncInfoType(&clip); //check type
    check(result==kSyncClient_ok,result);
    while (1){
        char  key[kMaxZsyncStrLine];
        const char* value=_readZsyncStr(&clip,key,sizeof(key));
        check(value!=0,kSyncClient_newZsyncInfoDataError);
        if (value==key){ assert(strlen(key)==0); break; }//end of key-value

        if (0==strcmp(key,"Min-Version")){
            check(strcmp(value,kZsync_VERSION)<=0,kSyncClient_newZsyncInfoMinVersionError);
        }else if (0==strcmp(key,"Length")){
            check(a_to_u64(value,strlen(value),&self->newDataSize),kSyncClient_newZsyncInfoDataError);
        }else if (0==strcmp(key,"Blocksize")){
            hpatch_uint64_t blockSize;
            check(a_to_u64(value,strlen(value),&blockSize)
                    &&(blockSize==(hpatch_uint32_t)blockSize)
                    &&(blockSize>0)&&(0==(blockSize&(blockSize-1))),kSyncClient_newZsyncInfoBlockSizeError);
            self->kSyncBlockSize=(hpatch_uint32_t)blockSize;
        }else if (0==strcmp(key,"Hash-Lengths")){
            int sequentialMatch,savedRollHashBytes,savedStrongChecksumBytes;
            check((sscanf(value,"%d,%d,%d",&sequentialMatch,&savedRollHashBytes,&savedStrongChecksumBytes)==3)
                    &&(sequentialMatch>=1)&&(sequentialMatch<=2)
                    &&(savedRollHashBytes>=1)&&(savedRollHashBytes<=4)
                    &&(savedStrongChecksumBytes>=3)&&(savedStrongChecksumBytes<=16),kSyncClient_newZsyncInfoDataError);
            self->savedStrongChecksumByteSize=savedStrongChecksumBytes;
            self->savedStrongChecksumBits=savedStrongChecksumBytes*8;
            self->savedRollHashByteSize=savedRollHashBytes;
            self->savedRollHashBits=savedRollHashBytes*8;
            self->isSeqMatch=(sequentialMatch>1)?hpatch_TRUE:hpatch_FALSE;
        }else if (0==strcmp(key,"Z-Map2")){
            hpatch_uint64_t _zmBlocks;
            const size_t _kZmOneSize=4;
            check(a_to_u64(value,strlen(value),&_zmBlocks)
                    &&(_zmBlocks==(((size_t)(_zmBlocks*_kZmOneSize))/_kZmOneSize)),kSyncClient_newZsyncInfoZmap2BlocksError);
            zmap2_blocks=(size_t)_zmBlocks;
            if (!isIgnoreCompressInfo){
                zmap2_data=(TByte*)malloc(zmap2_blocks*_kZmOneSize);
                check(zmap2_data!=0,kSyncClient_memError);
                check(_TStreamCacheClip_readDataTo(&clip,zmap2_data,zmap2_data+zmap2_blocks*_kZmOneSize), kSyncClient_newZsyncInfoDataError);
            }else{
                check(_TStreamCacheClip_skipData(&clip,zmap2_blocks*_kZmOneSize), kSyncClient_newZsyncInfoDataError);
                zmap2_blocks=0;
            }
        }else if (0==strcmp(key,"SHA-1")){
            self->fileChecksumPlugin=listener->findChecksumPlugin(listener,"sha1"); //for newData
            check(self->fileChecksumPlugin!=0,kSyncClient_noStrongChecksumPluginError);
            check(checksumBytes==self->fileChecksumPlugin->checksumByteSize(),kSyncClient_noStrongChecksumPluginError);
            checksumType=self->fileChecksumPlugin->checksumType();
            check(strlen(value)==checksumBytes*2,kSyncClient_newZsyncInfoDataError);
            check(hexs_to_bytes(value,checksumBytes*2,_savedNewDataCheckChecksum),kSyncClient_newZsyncInfoDataError);
        }else{ //ignore key-value
            continue;
        }
    }
    check((self->newDataSize>0)&&(self->kSyncBlockSize>0),kSyncClient_newZsyncInfoDataError);
    {
        hpatch_StreamPos_t v=TNewDataSyncInfo_blockCount(self);
        kBlockCount=(uint32_t)v;
        check((kBlockCount==v),kSyncClient_newZsyncInfoBlockSizeTooSmallError);//unsupport too large kBlockCount, need rise BlockSize when make
        if (zmap2_data)
            check((kBlockCount<=zmap2_blocks),kSyncClient_newZsyncInfoZmap2BlocksSmallError);
    }
    hpatch_StreamPos_t savedHashDataSize;
    {
        savedHashDataSize=(self->savedRollHashByteSize+self->savedStrongChecksumByteSize)*(hpatch_StreamPos_t)kBlockCount;
        self->newSyncInfoSize=_TStreamCacheClip_readPosOfSrcStream(&clip)+savedHashDataSize;
        check(newSyncInfo->streamSize>=self->newSyncInfoSize,kSyncClient_newSyncInfoDataError);
    }
    hpatch_StreamPos_t memSize;
    {
        TByte* curMem=0;
        memSize=sizeof(hpatch_StreamPos_t)*3+z_checkChecksumBufByteSize(checksumBytes);
        memSize+=strlen(checksumType)+1; //checksumType
        memSize+=savedHashDataSize+self->savedRollHashByteSize; //adding 1 empty rollHash for isSeqMatch
        if (zmap2_data)
            memSize +=strlen(zmap2_data_compressType)+1; //compressType

        check(memSize==(size_t)memSize,kSyncClient_memError);
        curMem=(TByte*)malloc((size_t)memSize);
        check(curMem!=0,kSyncClient_memError);
        self->_import=curMem;
        
        self->strongChecksumType=(const char*)curMem;
        curMem+=strlen(checksumType)+1;
        memcpy((TByte*)self->strongChecksumType,checksumType,curMem-(TByte*)self->strongChecksumType);
        {
            curMem=(TByte*)_hpatch_align_upper(curMem,sizeof(hpatch_StreamPos_t));
            self->savedNewDataCheckChecksum=(unsigned char*)curMem;
            curMem+=z_checkChecksumBufByteSize(checksumBytes);
            if (self->fileChecksumPlugin)
                memcpy(self->savedNewDataCheckChecksum,_savedNewDataCheckChecksum,checksumBytes);
            else
                memset(self->savedNewDataCheckChecksum,0,checksumBytes);//be zero for null checksum
        }
        
        if (zmap2_data){
            self->compressType=(char*)curMem;
            curMem+=strlen(zmap2_data_compressType)+1;
            memcpy((char*)self->compressType,zmap2_data_compressType,strlen(zmap2_data_compressType)+1);
            self->dictSize=zmap2_data_compressDictSize;
        }

        curMem=(TByte*)_hpatch_align_upper(curMem,sizeof(hpatch_StreamPos_t));
        self->rollHashs=curMem;
        curMem+=self->savedRollHashByteSize*((size_t)kBlockCount+1);//adding 1 empty rollHash for isSeqMatch
        memset(curMem-self->savedRollHashByteSize,0,self->savedRollHashByteSize); //a empty rollHash
        curMem=(TByte*)_hpatch_align_upper(curMem,sizeof(hpatch_StreamPos_t));
        self->partChecksums=curMem;
        curMem+=self->savedStrongChecksumByteSize*(size_t)kBlockCount;
        assert(curMem<=(TByte*)self->_import+memSize);
    }

    if (zmap2_data){//zmap2 compressed bit size
        self->_decompressPlugin=listener->findDecompressPlugin(listener,self->compressType,self->dictSize);
        check(self->_decompressPlugin!=0,kSyncClient_noDecompressPluginError);
        self->savedBitsInfos=(savedBitsInfo_t*)zmap2_data; //used same mem buf and safe
        self->_extraMem=zmap2_data; //free mem by close
        zmap2_data=0;
        check(readSavedBitsTo((const TByte*)self->_extraMem,zmap2_blocks,self), //zmap2_data convert to savedBitsInfos
            kSyncClient_newZsyncInfoZMap2DataError);
        self->isSavedBitsSizes=hpatch_TRUE;
    }

    //rollHashs & partStrongChecksums
    check(read2PartHashTo(&clip, self->rollHashs,self->savedRollHashByteSize,
                          self->partChecksums,self->savedStrongChecksumByteSize,
                          kBlockCount), kSyncClient_newZsyncInfoDataError);

clear:
    _inClear=1;
    if (result!=kSyncClient_ok) TNewDataZsyncInfo_close(self);
    if (zmap2_data) free(zmap2_data);
    if (temp_cache) free(temp_cache);
    return result;
}

void TNewDataZsyncInfo_close(TNewDataZsyncInfo* self){
    TNewDataSyncInfo_close(self);
}

TSyncClient_resultType TNewDataZsyncInfo_open(TNewDataZsyncInfo* self,const hpatch_TStreamInput* newSyncInfo,hpatch_BOOL isIgnoreCompressInfo,
                                              ISyncInfoListener* listener){
    TSyncClient_resultType result=_TNewDataZsyncInfo_open(self,newSyncInfo,isIgnoreCompressInfo,listener);
    if ((result==kSyncClient_ok)&&listener->onLoadedNewSyncInfo)
        listener->onLoadedNewSyncInfo(listener,self);
    return result;
}

TSyncClient_resultType TNewDataZsyncInfo_open_by_file(TNewDataZsyncInfo* self,const char* newSyncInfoFile,hpatch_BOOL isIgnoreCompressInfo,
                                                      ISyncInfoListener *listener){
    hpatch_TFileStreamInput  newSyncInfo;
    hpatch_TFileStreamInput_init(&newSyncInfo);
    TSyncClient_resultType result=kSyncClient_ok;
    int _inClear=0;
    check(hpatch_TFileStreamInput_open(&newSyncInfo,newSyncInfoFile), kSyncClient_newZsyncInfoOpenError);
    result=_TNewDataZsyncInfo_open(self,&newSyncInfo.base,isIgnoreCompressInfo,listener);
clear:
    _inClear=1;
    check(hpatch_TFileStreamInput_close(&newSyncInfo), kSyncClient_newZsyncInfoCloseError);
    if ((result==kSyncClient_ok)&&listener->onLoadedNewSyncInfo)
        listener->onLoadedNewSyncInfo(listener,self);
    return result;
}

