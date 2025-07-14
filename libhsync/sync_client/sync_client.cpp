//  sync_client.cpp
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
#include "sync_client_private.h"
#include "../../file_for_patch.h"
#include "match_in_old.h"
#include "sync_diff_data.h"
#include <stdexcept>
namespace sync_private{

#define check(v,errorCode) \
            do{ if (!(v)) { if (result==kSyncClient_ok) result=errorCode; \
                            if (!_inClear) goto clear; } }while(0)
void checkChecksumInit(unsigned char* checkChecksumBuf,size_t kStrongChecksumByteSize){
        unsigned char* d_xor=checkChecksumBuf+kStrongChecksumByteSize;
        assert(kStrongChecksumByteSize>0);
        memset(d_xor,0xFF,kStrongChecksumByteSize); }

    static hpatch_inline
    void _writeUInt32(unsigned char* dst,uint32_t v){
        dst[0]=(unsigned char)v;       dst[1]=(unsigned char)(v>>8);
        dst[2]=(unsigned char)(v>>16); dst[3]=(unsigned char)(v>>24); }

void checkChecksumAppendData(unsigned char* checkChecksumBuf,uint32_t checksumIndex,
                             hpatch_TChecksum* checkChecksumPlugin,hpatch_checksumHandle checkChecksum,
                             const unsigned char* blockChecksum,const unsigned char* blockData,size_t blockDataSize){
    const size_t kStrongChecksumByteSize=checkChecksumPlugin->checksumByteSize();
    unsigned char* d_xor=checkChecksumBuf+kStrongChecksumByteSize;
    unsigned char* d_xor_end=d_xor+kStrongChecksumByteSize;
    unsigned char* _ccBuf=checkChecksumBuf+kStrongChecksumByteSize*2;

    unsigned char _checksumIndex[4*3];
    _writeUInt32(&_checksumIndex[0],checksumIndex);
    const uint64_t _fchecksum=fast_adler64_start(&_checksumIndex[0],4);
    _writeUInt32(&_checksumIndex[4],(uint32_t)_fchecksum);
    _writeUInt32(&_checksumIndex[4*2],(uint32_t)(_fchecksum>>32));

    checkChecksumPlugin->begin(checkChecksum);
    checkChecksumPlugin->append(checkChecksum,&_checksumIndex[0],(&_checksumIndex[0])+4*3);
    checkChecksumPlugin->append(checkChecksum,blockChecksum,blockChecksum+kStrongChecksumByteSize);
    checkChecksumPlugin->end(checkChecksum,_ccBuf,_ccBuf+kStrongChecksumByteSize);

    while (d_xor<d_xor_end){
        (*d_xor++)^=*_ccBuf++;
    }
}

void checkChecksumEndTo(unsigned char* dst,unsigned char* checkChecksumBuf,
                        hpatch_TChecksum* checkChecksumPlugin,hpatch_checksumHandle checkChecksum){
    const size_t kStrongChecksumByteSize=checkChecksumPlugin->checksumByteSize();
    const unsigned char* d_xor=checkChecksumBuf+kStrongChecksumByteSize;
    const unsigned char* d_xor_end=d_xor+kStrongChecksumByteSize;
    while (d_xor<d_xor_end){
        *dst++=(*d_xor++);
    }
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
    
static void _indexOfCompressedSyncBlock(uint32_t* _followCompressedIndex,const TNewDataSyncInfo* self,
                                        const hpatch_StreamPos_t* newBlockDataInOldPoss,uint32_t indexBegin){
    uint32_t blockCount=(uint32_t)TNewDataSyncInfo_blockCount(self);
    uint32_t& followCompressedIndex=*_followCompressedIndex;
    if (self->savedSizes&&(followCompressedIndex<blockCount)){
        indexBegin=(indexBegin>=followCompressedIndex)?indexBegin:followCompressedIndex;
        for (uint32_t i=indexBegin;i<blockCount;++i){
            if ((self->savedSizes[i])&&(newBlockDataInOldPoss[i]==kBlockType_needSync)){
                followCompressedIndex=i;
                return;
            }
        }
    }
    followCompressedIndex=blockCount;
}

    
#define _checkSumNewDataBuf() { \
    if (newDataSize<kSyncBlockSize)/*for backZeroLen*/ \
        memset(dataBuf+newDataSize,0,kSyncBlockSize-newDataSize);  \
    strongChecksumPlugin->begin(checksumSync);  \
    strongChecksumPlugin->append(checksumSync,dataBuf,dataBuf+kSyncBlockSize); \
    strongChecksumPlugin->end(checksumSync,checksumSync_buf+newSyncInfo->savedStrongChecksumByteSize,    \
                              checksumSync_buf+newSyncInfo->savedStrongChecksumByteSize \
                                  +kStrongChecksumByteSize);\
    toPartChecksum(checksumSync_buf,newSyncInfo->savedStrongChecksumBits, \
                   checksumSync_buf+newSyncInfo->savedStrongChecksumByteSize, \
                   kStrongChecksumByteSize);   \
    check(0==memcmp(checksumSync_buf,   \
                    newSyncInfo->partChecksums+i*newSyncInfo->savedStrongChecksumByteSize,   \
                    newSyncInfo->savedStrongChecksumByteSize),kSyncClient_checksumSyncDataError);    \
}


static TSyncClient_resultType writeToNewOrDiff(_TWriteDatas& wd,bool& isNeed_readSyncDataEnd) {
    _IWriteToNewOrDiff_by wr_by={0};
    wr_by.checkChecksumAppendData=checkChecksumAppendData;
    return _writeToNewOrDiff_by(&wr_by,wd,isNeed_readSyncDataEnd);
}

TSyncClient_resultType _writeToNewOrDiff_by(_IWriteToNewOrDiff_by* wr_by,_TWriteDatas& wd,bool& isNeed_readSyncDataEnd) {
    const TNewDataSyncInfo* newSyncInfo=wd.newSyncInfo;
    IReadSyncDataListener*  syncDataListener=wd.syncDataListener;
    hpatch_TChecksum*       fileChecksumPlugin=newSyncInfo->fileChecksumPlugin;
    const size_t fileChecksumByteSize=fileChecksumPlugin?fileChecksumPlugin->checksumByteSize():0;
    hpatch_TChecksum*       strongChecksumPlugin=newSyncInfo->strongChecksumPlugin;
    const size_t            kStrongChecksumByteSize=strongChecksumPlugin->checksumByteSize();
    TSyncClient_resultType result=kSyncClient_ok;
    int _inClear=0;
    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    const uint32_t kSyncBlockSize=newSyncInfo->kSyncBlockSize;
    TByte*             _memBuf=0;
    TByte*             dataBuf=0;
    TByte*             checksumSync_buf=0;
    hsync_dictDecompressHandle decompressHandle=0;
    hpatch_checksumHandle checksumSync=0;
    hpatch_StreamPos_t posInNewSyncData=newSyncInfo->newSyncDataOffsert;
    hpatch_StreamPos_t posInNeedSyncData=0;
    hpatch_StreamPos_t outNewDataPos=0;
    const hpatch_StreamPos_t oldDataSize=wd.oldStream->streamSize;
    bool isOnDiffContinue =(wd.continueDiffData!=0);
    bool isOnNewDataContinue =(wd.newDataContinue!=0);
    isNeed_readSyncDataEnd=false;
    uint32_t followCompressedIndex=0;
    _indexOfCompressedSyncBlock(&followCompressedIndex,newSyncInfo,wd.newBlockDataInOldPoss,0);
    if (followCompressedIndex>=kBlockCount)
        wd.decompressPlugin=0;//no blocks need decompress
    const size_t _kMaxCompressedSize=wd.decompressPlugin?(size_t)(wd.decompressPlugin->maxCompressedSize(kSyncBlockSize)+2):0;
    const size_t _memSize=(kSyncBlockSize+_kMaxCompressedSize+1+1)
                        +newSyncInfo->savedStrongChecksumByteSize+kStrongChecksumByteSize
                        +(wd.decompressPlugin&&wd.decompressPlugin->needFillAlignCode?
                                wd.decompressPlugin->needFillAlignCode(0,0):0);
    _memBuf=(TByte*)malloc(_memSize);
    check(_memBuf!=0,kSyncClient_memError);
    checksumSync_buf=_memBuf;
    dataBuf=_memBuf+(newSyncInfo->savedStrongChecksumByteSize+kStrongChecksumByteSize)+1;
    {//checksum newSyncData
        checksumSync=strongChecksumPlugin->open(strongChecksumPlugin);
        check(checksumSync!=0,kSyncClient_strongChecksumOpenError);
    }
    if (wd.decompressPlugin){
        decompressHandle=wd.decompressPlugin->dictDecompressOpen(wd.decompressPlugin,kBlockCount,kSyncBlockSize,
                            newSyncInfo->decompressInfo,newSyncInfo->decompressInfo+newSyncInfo->decompressInfoSize);
        check(decompressHandle,kSyncClient_decompressOpenError);
    }
    for (uint32_t needbackup_nextBlocki=-1,backuped_nextBlocki=-1,i=0;i<kBlockCount;++i){
        const hpatch_byte skipBitsInFirstCodeByte=TNewDataSyncInfo_skipBitsInFirstCodeByte(newSyncInfo,i);
        const bool isCompressedBlock=TNewDataSyncInfo_syncBlockIsCompressed(newSyncInfo,i);
        hpatch_byte lastByteHalfBits;
        const uint32_t syncSize=TNewDataSyncInfo_syncBlockSize(newSyncInfo,i,&lastByteHalfBits);
        const uint32_t newDataSize=TNewDataSyncInfo_newDataBlockSize(newSyncInfo,i);
        check(syncSize<=(isCompressedBlock?_kMaxCompressedSize:kSyncBlockSize),kSyncClient_newSyncInfoDataError);
        const hpatch_StreamPos_t curSyncPos=wd.newBlockDataInOldPoss[i];
        const bool isNeedSync=(curSyncPos==kBlockType_needSync);
        const uint32_t _isNeedBackupHalf=((isNeedSync&&(needbackup_nextBlocki==i))?1:0);
        if (isOnNewDataContinue&&(outNewDataPos+newDataSize>wd.newDataContinue->streamSize))
            isOnNewDataContinue=hpatch_FALSE;
        if (isOnNewDataContinue){ //copy from newDataContinue
            check(wd.newDataContinue->read(wd.newDataContinue,outNewDataPos,dataBuf,dataBuf+newDataSize),
                  kSyncClient_newFileReopenReadError);
            if (isCompressedBlock)
                _indexOfCompressedSyncBlock(&followCompressedIndex,newSyncInfo,wd.newBlockDataInOldPoss,i+1);
        }else if (isNeedSync){ //download or read from diff data
            TByte* const buf=isCompressedBlock?(dataBuf+kSyncBlockSize+1):dataBuf; //+1 for backup 1 byte compressed code when lastByteHalfBits!=0
            const uint32_t _isBackupedHalf=(backuped_nextBlocki==i)?1:0; //buckuped byte data is hit
            if (_isBackupedHalf||_isNeedBackupHalf) assert(skipBitsInFirstCodeByte);
            const size_t _isReLoadNewHalf=((skipBitsInFirstCodeByte&&(!_isBackupedHalf))?1:0);
            const size_t _isReLoadSyncHalf=((_isNeedBackupHalf&&(!_isBackupedHalf))?1:0);
            /*if ((wd.out_newStream)||(wd.out_diffStream))*/{//download data
                if (isOnDiffContinue){ //read from local data
                    if (!wd.continueDiffData->readSyncData(wd.continueDiffData,i,posInNewSyncData-_isReLoadNewHalf,
                                                           posInNeedSyncData-_isReLoadSyncHalf,buf,syncSize-_isBackupedHalf))
                        isOnDiffContinue=false; // swap to download
                }
                if (!isOnDiffContinue){ //downloaded data or read from diff data
                    if (!isNeed_readSyncDataEnd){
                        isNeed_readSyncDataEnd=true;
                        if (syncDataListener->readSyncDataBegin)
                            check(syncDataListener->readSyncDataBegin(syncDataListener,wd.needSyncInfo,
                                                        i,posInNewSyncData-_isReLoadNewHalf,posInNeedSyncData),kSyncClient_readSyncDataBeginError);
                    }
                    check(syncDataListener->readSyncData(syncDataListener,i,posInNewSyncData-_isReLoadNewHalf,
                                                posInNeedSyncData-_isReLoadSyncHalf,buf,syncSize-_isBackupedHalf),kSyncClient_readSyncDataError);
                }
                if (wd.out_diffStream){ //out diff
                    if (!isOnDiffContinue){ //save downloaded data
                        check(wd.out_diffStream->write(wd.out_diffStream,wd.outDiffDataPos+posInNeedSyncData-_isReLoadSyncHalf,
                                                       buf,buf+syncSize-_isBackupedHalf),kSyncClient_saveDiffError);
                    }
                }
            }
            const hpatch_byte backup_last_code=buf[syncSize-_isBackupedHalf-1];
            if (/*wd.out_newStream&&*/isCompressedBlock){// need deccompress
                assert(followCompressedIndex==i);
                size_t alignCodeSize=0;
                if (lastByteHalfBits&&wd.decompressPlugin->needFillAlignCode)
                    alignCodeSize=wd.decompressPlugin->needFillAlignCode(&buf[syncSize-_isBackupedHalf-1],lastByteHalfBits);
                check(wd.decompressPlugin->dictDecompress(decompressHandle,i,buf-_isBackupedHalf,buf-_isBackupedHalf+syncSize+alignCodeSize,
                                                          dataBuf,dataBuf+newDataSize,skipBitsInFirstCodeByte),kSyncClient_decompressError);
                _indexOfCompressedSyncBlock(&followCompressedIndex,newSyncInfo,wd.newBlockDataInOldPoss,i+1);
            }
            buf[-1]=backup_last_code;//backup last half byte code for next block
            backuped_nextBlocki=lastByteHalfBits?(i+1):-1;
        }else{//copy from old
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

        if (decompressHandle&&(followCompressedIndex<kBlockCount))
            wd.decompressPlugin->dictUncompress(decompressHandle,i,followCompressedIndex,dataBuf,dataBuf+newDataSize);

        /*if (wd.out_newStream)*/{//write
            bool isNeedChecksumAppend=isNeedSync|wd.isLocalPatch|(wd.continueDiffData!=0);
            if (isNeedChecksumAppend|isOnNewDataContinue)
                _checkSumNewDataBuf();
            if (newSyncInfo->fileChecksumPlugin&&
                 (newSyncInfo->isNotCheckChecksumNewWhenMatch||isNeedChecksumAppend))
                wr_by->checkChecksumAppendData(newSyncInfo->savedNewDataCheckChecksum,i,
                                               fileChecksumPlugin,wd.checkChecksum,
                                               checksumSync_buf+wd.newSyncInfo->savedStrongChecksumByteSize,
                                               dataBuf,newDataSize);
            if ((!isOnNewDataContinue)&&wd.out_newStream){
                check(wd.out_newStream->write(wd.out_newStream,outNewDataPos,dataBuf,
                                              dataBuf+newDataSize), kSyncClient_writeNewDataError);
            }
        }
        
        outNewDataPos+=newDataSize;
        posInNewSyncData+=syncSize-(skipBitsInFirstCodeByte?1:0);//delete one byte when adjacent blocks have half byte
        posInNeedSyncData+=isNeedSync?(syncSize-_isNeedBackupHalf):0; //delete one byte when adjacent need sync blocks have half byte
        needbackup_nextBlocki=(isNeedSync&&lastByteHalfBits)?(i+1):-1;
    }//for i
    if (wd.out_diffStream) wd.outDiffDataPos+=posInNeedSyncData;
    check(outNewDataPos==newSyncInfo->newDataSize,kSyncClient_newDataSizeError);
    if (newSyncInfo->newSyncDataSize==0)
        assert(posInNewSyncData<=newSyncInfo->newDataSize);
    else
        assert(posInNewSyncData<=newSyncInfo->newSyncDataSize);//checked in readSavedSizesTo()
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
                                 hpatch_BOOL* out_isNeedSync,uint32_t* out_syncSize,hpatch_byte* out_lastByteHalfBits){
    const TNeedSyncInfosImport* self=(const TNeedSyncInfosImport*)needSyncInfos->import;
    assert(blockIndex<self->blockCount);
    *out_isNeedSync=(self->newBlockDataInOldPoss[blockIndex]==kBlockType_needSync);
    *out_syncSize=TNewDataSyncInfo_syncBlockSize(self->newSyncInfo,blockIndex,out_lastByteHalfBits);
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
    
    uint32_t halfByte_nextBlocki=-1;//no match
    for (uint32_t i=0; i<kBlockCount; ++i){
        if (newBlockDataInOldPoss[i]==kBlockType_needSync){
            ++out_nsi->needSyncBlockCount;
            hpatch_byte lastByteHalfBits;
            out_nsi->needSyncSumSize+=TNewDataSyncInfo_syncBlockSize(newSyncInfo,i,&lastByteHalfBits);
            if (halfByte_nextBlocki==i)
                out_nsi->needSyncSumSize--; //delete one byte when adjacent blocks have half byte
            if (lastByteHalfBits)
                halfByte_nextBlocki=i+1;
        }
    }
}

TSyncClient_resultType 
    _sync_patch(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,TSyncDiffData* diffData,
                const hpatch_TStreamInput* oldStream,const TNewDataSyncInfo* newSyncInfo,
                const hpatch_TStreamOutput* out_newStream,const hpatch_TStreamInput* newDataContinue,
                const hpatch_TStreamOutput* out_diffStream,TSyncDiffType diffType,
                const hpatch_TStreamInput* diffContinue,int threadNum){
    _ISyncPatch_by sp_by={0};
    sp_by.matchNewDataInOld=matchNewDataInOld;
    sp_by.writeToNewOrDiff=writeToNewOrDiff;
    sp_by.checkChecksumInit=checkChecksumInit;
    sp_by.checkChecksumEndTo=checkChecksumEndTo;
    return _sync_patch_by(&sp_by, listener,syncDataListener,diffData,oldStream,newSyncInfo,
                          out_newStream,newDataContinue,out_diffStream,diffType,diffContinue,threadNum);
}


TSyncClient_resultType 
    _sync_patch_by(_ISyncPatch_by* sp_by,
                   ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,TSyncDiffData* diffData,
                   const hpatch_TStreamInput* oldStream,const TNewDataSyncInfo* newSyncInfo,
                   const hpatch_TStreamOutput* out_newStream,const hpatch_TStreamInput* newDataContinue,
                   const hpatch_TStreamOutput* out_diffStream,TSyncDiffType diffType,
                   const hpatch_TStreamInput* diffContinue,int threadNum){
    assert(listener!=0);
    hsync_TDictDecompress* decompressPlugin=0;
    hpatch_TChecksum*   fileChecksumPlugin=newSyncInfo->fileChecksumPlugin;
    const size_t fileChecksumByteSize=fileChecksumPlugin?fileChecksumPlugin->checksumByteSize():0;
    hpatch_checksumHandle checkChecksum=0;
    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    TNeedSyncInfosImport needSyncInfo; memset(&needSyncInfo,0,sizeof(needSyncInfo));
    hpatch_StreamPos_t* newBlockDataInOldPoss=0;
    hpatch_StreamPos_t  outDiffDataPos=0;
    TSyncClient_resultType result=kSyncClient_ok;
    int _inClear=0;
    TSyncDiffData continueDiffData; memset(&continueDiffData,0,sizeof(continueDiffData));
    const bool isNeedOut=(out_newStream!=0)||((out_diffStream!=0)&&(diffType!=kSyncDiff_info));
    bool isLoadedOldPoss=false;
    bool isReMatchInOld=false;
    bool isNeed_readSyncDataEnd=false;
    
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
    //fileChecksumPlugin
    if (newSyncInfo->fileChecksumPlugin){
        sp_by->checkChecksumInit(newSyncInfo->savedNewDataCheckChecksum,fileChecksumByteSize);
        checkChecksum=fileChecksumPlugin->open(fileChecksumPlugin);
        check(checkChecksum!=0,kSyncClient_strongChecksumOpenError);
    }
    //matched in oldData
    newBlockDataInOldPoss=(hpatch_StreamPos_t*)malloc(kBlockCount*(size_t)sizeof(hpatch_StreamPos_t));
    check(newBlockDataInOldPoss!=0,kSyncClient_memError);
    if (diffData!=0){ //local patch
        assert(syncDataListener==0);
        assert(diffContinue==0);
        check(_TSyncDiffData_readOldPoss(diffData,newBlockDataInOldPoss,kBlockCount,newSyncInfo->kSyncBlockSize,
                                         oldStream->streamSize,newSyncInfo->savedNewDataCheckChecksum,
                                         fileChecksumByteSize), kSyncClient_loadDiffError);
        isLoadedOldPoss=(diffData->diffType!=kSyncDiff_data);
        if (!isLoadedOldPoss) isReMatchInOld=true;
        syncDataListener=diffData;
    }else{
        assert(syncDataListener!=0);
        if (diffContinue!=0){ // try continue local diff
            assert(out_diffStream!=0);
            if (_TSyncDiffData_load(&continueDiffData,diffContinue)){
                if (_TSyncDiffData_readOldPoss(&continueDiffData,newBlockDataInOldPoss,kBlockCount,
                                               newSyncInfo->kSyncBlockSize,oldStream->streamSize,
                                               newSyncInfo->savedNewDataCheckChecksum,fileChecksumByteSize)){
                    isLoadedOldPoss=(continueDiffData.diffType!=kSyncDiff_data);//ok diffContinue
                    if (!isLoadedOldPoss) isReMatchInOld=true;
                }else
                    diffContinue=0; // not use diffContinue
            }else
                diffContinue=0; // not use diffContinue
        }
    }
    if (!isLoadedOldPoss){
        try{
            sp_by->matchNewDataInOld(newBlockDataInOldPoss,newSyncInfo,oldStream,
                                     ((diffType==kSyncDiff_data)||isReMatchInOld)?1:threadNum);
            if (isReMatchInOld){
                sp_by->checkChecksumInit(newSyncInfo->savedNewDataCheckChecksum,
                                         fileChecksumByteSize);
            }
        }catch(const std::exception& e){
            fprintf(stderr,"matchNewDataInOld() run an error: %s",e.what());
            result=kSyncClient_matchNewDataInOldError;
        }
    }

    check(result==kSyncClient_ok,result);
    if ((out_diffStream!=0)){
        if (diffContinue==0){
            check(_saveSyncDiffData(newBlockDataInOldPoss,kBlockCount,newSyncInfo->kSyncBlockSize,oldStream->streamSize,
                                    newSyncInfo->savedNewDataCheckChecksum,fileChecksumByteSize,
                                    out_diffStream,diffType,&outDiffDataPos),kSyncClient_saveDiffError);
        }else{ // continue local diff
            outDiffDataPos=continueDiffData.diffDataPos0;
        }
    }
    getNeedSyncInfo(newBlockDataInOldPoss,newSyncInfo,&needSyncInfo);
    if (diffContinue)
        needSyncInfo.localDiffDataSize=(diffContinue->streamSize-continueDiffData.diffDataPos0);
    if (newDataContinue)
        needSyncInfo.localNewDataSize=newDataContinue->streamSize;

    if (listener->onNeedSyncInfo)
        listener->onNeedSyncInfo(listener,&needSyncInfo);
    if (syncDataListener->onNeedSyncInfo)
        syncDataListener->onNeedSyncInfo(syncDataListener,&needSyncInfo);
    
    if (isNeedOut) 
        check(syncDataListener->readSyncData!=0,kSyncClient_noReadSyncDataError);
    if (isNeedOut){
        _TWriteDatas writeDatas;
        writeDatas.out_newStream=out_newStream;
        writeDatas.newDataContinue=newDataContinue;
        writeDatas.out_diffStream=(diffType==kSyncDiff_info)?0:out_diffStream;
        writeDatas.outDiffDataPos=outDiffDataPos;
        writeDatas.oldStream=oldStream;
        writeDatas.newSyncInfo=newSyncInfo;
        writeDatas.needSyncInfo=&needSyncInfo;
        writeDatas.newBlockDataInOldPoss=newBlockDataInOldPoss;
        writeDatas.needSyncBlockCount=needSyncInfo.needSyncBlockCount;
        writeDatas.decompressPlugin=decompressPlugin;
        writeDatas.checkChecksum=checkChecksum;
        writeDatas.syncDataListener=syncDataListener;
        writeDatas.continueDiffData=diffContinue?&continueDiffData:0;
        writeDatas.isLocalPatch=(diffData!=0);
        result=sp_by->writeToNewOrDiff(writeDatas,isNeed_readSyncDataEnd);
        
        if ((result==kSyncClient_ok)&&newSyncInfo->fileChecksumPlugin){
            sp_by->checkChecksumEndTo(newSyncInfo->savedNewDataCheckChecksum+fileChecksumByteSize,
                                      newSyncInfo->savedNewDataCheckChecksum,fileChecksumPlugin,checkChecksum);
            check(0==memcmp(newSyncInfo->savedNewDataCheckChecksum+fileChecksumByteSize,
                            newSyncInfo->savedNewDataCheckChecksum,fileChecksumByteSize),
                  kSyncClient_newDataCheckChecksumError);
        }
    }
    
clear:
    _inClear=1;
    if (isNeed_readSyncDataEnd&&(syncDataListener->readSyncDataEnd))
        syncDataListener->readSyncDataEnd(syncDataListener);
    if (checkChecksum) fileChecksumPlugin->close(fileChecksumPlugin,checkChecksum);
    if (newBlockDataInOldPoss) free(newBlockDataInOldPoss);
    return result;
}

static TSyncClient_resultType
    _sync_patch_file2file(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                          TSyncDiffData* diffData,const char* oldFile,const char* newSyncInfoFile,
                          const char* outNewFile,hpatch_BOOL isOutNewContinue,
                          const hpatch_TStreamOutput* out_diffStream,TSyncDiffType diffType,
                          const hpatch_TStreamInput* diffContinue,int threadNum){
    TSyncClient_resultType result=kSyncClient_ok;
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
                       outNewFile?&out_newData.base:0,newDataContinue,
                       out_diffStream,diffType,diffContinue,threadNum);
clear:
    _inClear=1;
    check(hpatch_TFileStreamOutput_close(&out_newData),kSyncClient_newFileCloseError);
    check(hpatch_TFileStreamInput_close(&oldData),kSyncClient_oldFileCloseError);
    TNewDataSyncInfo_close(&newSyncInfo);
    return result;
}

    struct _TRange{
        hpatch_StreamPos_t first;
        hpatch_StreamPos_t second;
    };
    static const hpatch_StreamPos_t kEmptyEndPos=~(hpatch_StreamPos_t)0;
    static inline bool _isCanCombine(const _TRange& back_range,hpatch_StreamPos_t rangeBegin){
        return ((rangeBegin>0)&(back_range.second+1==rangeBegin));
    }
    static inline void _setRange(_TRange& next_ranges,hpatch_StreamPos_t rangeBegin,hpatch_StreamPos_t rangeEnd){
        assert(rangeBegin<rangeEnd);
        hpatch_StreamPos_t rangLast=(rangeEnd!=kEmptyEndPos)?(rangeEnd-1):kEmptyEndPos;
        next_ranges.first=rangeBegin;
        next_ranges.second=rangLast;
    }

} //namespace sync_private
using namespace  sync_private;

size_t TNeedSyncInfos_getNextRanges(const TNeedSyncInfos* nsi,hpatch_StreamPos_t* _dstRanges,size_t maxGetRangeLen,
                                    uint32_t* _curBlockIndex,hpatch_StreamPos_t* _curPosInNewSyncData){
    _TRange* out_ranges=(_TRange*)_dstRanges;
    _TRange  backRange={0,0};
    uint32_t& blockIndex=*_curBlockIndex;
    hpatch_StreamPos_t& posInNewSyncData=*_curPosInNewSyncData;
    uint32_t halfByte_nextBlocki=-1; //no match
    size_t result=0;
    while (blockIndex<nsi->blockCount){
        hpatch_BOOL isNeedSync;
        uint32_t    syncSize;
        hpatch_byte lastByteHalfBits;
        nsi->getBlockInfoByIndex(nsi,blockIndex,&isNeedSync,&syncSize,&lastByteHalfBits);
        if (isNeedSync){
            if ((result>0)&&_isCanCombine(backRange,posInNewSyncData)){
                backRange.second+=syncSize;
                if (halfByte_nextBlocki==blockIndex)
                    backRange.second--; //delete one byte when adjacent blocks have half byte
            }else if (result>=maxGetRangeLen){
                break; //finish
            }else{
                _setRange(backRange,posInNewSyncData,posInNewSyncData+syncSize);
                ++result;
            }
            if (out_ranges) out_ranges[result-1]=backRange;
        }
        posInNewSyncData+=syncSize;
        if (halfByte_nextBlocki==blockIndex)
            posInNewSyncData--; //delete one byte when adjacent blocks have half byte
        if (lastByteHalfBits)
            halfByte_nextBlocki=blockIndex+1;
        ++blockIndex;
    }
    return result;
}

TSyncClient_resultType sync_patch(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                                  const hpatch_TStreamInput* oldStream,const TNewDataSyncInfo* newSyncInfo,
                                  const hpatch_TStreamOutput* out_newStream,const hpatch_TStreamInput* newDataContinue,
                                  const hpatch_TStreamOutput* out_diffInfoStream,const hpatch_TStreamInput* diffInfoContinue,int threadNum){
    return _sync_patch(listener,syncDataListener,0,oldStream,newSyncInfo,out_newStream,newDataContinue,
                       out_diffInfoStream,kSyncDiff_info,diffInfoContinue,threadNum);
}

TSyncClient_resultType sync_local_diff(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                                       const hpatch_TStreamInput* oldStream,const TNewDataSyncInfo* newSyncInfo,
                                       const hpatch_TStreamOutput* out_diffStream,TSyncDiffType diffType,
                                       const hpatch_TStreamInput* diffContinue,int threadNum){
    return _sync_patch(listener,syncDataListener,0,oldStream,newSyncInfo,0,0,
                       out_diffStream,diffType,diffContinue,threadNum);
}


TSyncClient_resultType sync_local_patch(ISyncInfoListener* listener,const hpatch_TStreamInput* in_diffStream,
                                        const hpatch_TStreamInput* oldStream,const TNewDataSyncInfo* newSyncInfo,
                                        const hpatch_TStreamOutput* out_newStream,const hpatch_TStreamInput* newDataContinue,int threadNum){
    TSyncDiffData diffData;
    if (!_TSyncDiffData_load(&diffData,in_diffStream)) return kSyncClient_loadDiffError;
    return _sync_patch(listener,0,&diffData,oldStream,newSyncInfo,out_newStream,newDataContinue,
                      0,kSyncDiff_default,0,threadNum);
}


TSyncClient_resultType sync_patch_file2file(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                                            const char* oldFile,const char* newSyncInfoFile,
                                            const char* outNewFile,hpatch_BOOL isOutNewContinue,
                                            const char* cacheDiffInfoFile,int threadNum){
    TSyncClient_resultType result=kSyncClient_ok;
    int _inClear=0;
    hpatch_TFileStreamOutput out_diffInfo;
    hpatch_TFileStreamOutput_init(&out_diffInfo);
    const hpatch_TStreamInput* diffContinue=0;
    hpatch_TStreamInput _diffContinue;
    if (cacheDiffInfoFile){
        hpatch_BOOL isOutDiffContinue=hpatch_TRUE;
        check(_open_continue_out(isOutDiffContinue,cacheDiffInfoFile,&out_diffInfo,&_diffContinue),
            isOutDiffContinue?kSyncClient_diffFileReopenWriteError:kSyncClient_diffFileCreateError);
        if (isOutDiffContinue) diffContinue=&_diffContinue;
    }

    result=_sync_patch_file2file(listener,syncDataListener,0,oldFile,newSyncInfoFile,
                                 outNewFile,isOutNewContinue,
                                 cacheDiffInfoFile?&out_diffInfo.base:0,kSyncDiff_info,diffContinue,threadNum);
clear:
    _inClear=1;
    check(hpatch_TFileStreamOutput_close(&out_diffInfo),kSyncClient_diffFileCloseError);
    return result;
}


TSyncClient_resultType sync_local_diff_file2file(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                                                 const char* oldFile,const char* newSyncInfoFile,
                                                 const char* outDiffFile,TSyncDiffType diffType,
                                                 hpatch_BOOL isOutDiffContinue,int threadNum){
    TSyncClient_resultType result=kSyncClient_ok;
    int _inClear=0;
    hpatch_TFileStreamOutput out_diff;
    hpatch_TFileStreamOutput_init(&out_diff);
    const hpatch_TStreamInput* diffContinue=0;
    hpatch_TStreamInput _diffContinue;
    
    check(_open_continue_out(isOutDiffContinue,outDiffFile,&out_diff,&_diffContinue),
          isOutDiffContinue?kSyncClient_diffFileReopenWriteError:kSyncClient_diffFileCreateError);
    if (isOutDiffContinue) diffContinue=&_diffContinue;
    
    result=_sync_patch_file2file(listener,syncDataListener,0,oldFile,newSyncInfoFile,0,hpatch_FALSE,
                                 &out_diff.base,diffType,diffContinue,threadNum);
    
clear:
    _inClear=1;
    check(hpatch_TFileStreamOutput_close(&out_diff),kSyncClient_diffFileCloseError);
    return result;
}

TSyncClient_resultType sync_local_patch_file2file(ISyncInfoListener* listener,const char* inDiffFile,
                                                  const char* oldFile,const char* newSyncInfoFile,
                                                  const char* outNewFile,hpatch_BOOL isOutNewContinue,int threadNum){
    TSyncClient_resultType result=kSyncClient_ok;
    int _inClear=0;
    TSyncDiffData diffData;
    hpatch_TFileStreamInput in_diffData;
    hpatch_TFileStreamInput_init(&in_diffData);
    check(hpatch_TFileStreamInput_open(&in_diffData,inDiffFile),
          kSyncClient_diffFileOpenError);
    check(_TSyncDiffData_load(&diffData,&in_diffData.base),kSyncClient_loadDiffError);
    result=_sync_patch_file2file(listener,0,&diffData,oldFile,newSyncInfoFile,outNewFile,isOutNewContinue,
                                 0,kSyncDiff_default,0,threadNum);
clear:
    _inClear=1;
    check(hpatch_TFileStreamInput_close(&in_diffData),kSyncClient_diffFileCloseError);
    return result;
}
