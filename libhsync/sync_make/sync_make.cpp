//  sync_make.cpp
//  sync_make
//  Created by housisong on 2019-09-17.
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
#include "sync_make.h"
#include "sync_make_hash_clash.h"
#include "sync_info_make.h"
#include "match_in_new.h"
#include "..\..\libParallel\parallel_channel.h"
#include <vector>
using namespace hdiff_private;
using namespace sync_private;

struct _TCreateDatas {
    const hpatch_TStreamInput*  newData;
    hsync_TDictCompress*        compressPlugin;
    hsync_THsynz*               hsynzPlugin;
    TNewDataSyncInfo*           out_hsyni;
    const hpatch_TStreamOutput* out_hsynz;
    hpatch_StreamPos_t          curOutPos;
    hpatch_checksumHandle       checkChecksum;
};

struct _TCompress{
    inline _TCompress(hsync_TDictCompress* _compressPlugin,uint32_t kSyncBlockCount,
                      uint32_t _kSyncBlockSize,TNewDataSyncInfo* out_hsyni)
    :compressPlugin(_compressPlugin),dictCompressHandle(0),kSyncBlockSize(_kSyncBlockSize),
    kMaxCompressedSize(_compressPlugin?_compressPlugin->maxCompressedSize(_kSyncBlockSize):0),
    cmBuf(0),cmBufPos(0){
        assert(out_hsyni->decompressInfoSize==0);
        if (compressPlugin!=0){
            dictCompressHandle=compressPlugin->dictCompressOpen(compressPlugin,kSyncBlockCount,_kSyncBlockSize);
            checkv(dictCompressHandle!=0);
            if (compressPlugin->dictCompressInfo){
                size_t decompressInfoSize=compressPlugin->dictCompressInfo(compressPlugin,dictCompressHandle,
                                            &out_hsyni->decompressInfo[0],&out_hsyni->decompressInfo[0]+sizeof(out_hsyni->decompressInfo));
                checkv(decompressInfoSize!=kDictCompressError);
                if (decompressInfoSize!=kDictCompressCancel){
                    assert(decompressInfoSize==(uint8_t)decompressInfoSize);
                    checkv(decompressInfoSize<=sizeof(out_hsyni->decompressInfo));
                    out_hsyni->decompressInfoSize=(uint8_t)decompressInfoSize;
                }
            }
        }
    }
    inline ~_TCompress(){ if (dictCompressHandle) compressPlugin->dictCompressClose(compressPlugin,dictCompressHandle); }
    inline size_t doCompress(size_t blockIndex,const TByte* data,const TByte* dataEnd){
        if (dictCompressHandle==0)
            return 0;//not compress
        assert(cmBuf!=0);
        hpatch_byte* curBuf=cmBuf+cmBufPos;
        size_t result=compressPlugin->dictCompress(dictCompressHandle,blockIndex,curBuf,
                                                   curBuf+kMaxCompressedSize,data,dataEnd);
        checkv(result!=kDictCompressError);
        if (result==kDictCompressCancel){
            result=0; //cancel compress
            size_t dataSize=dataEnd-data;
            memcpy(curBuf,data,dataSize);
            cmBufPos+=dataSize;
        }else{
            checkv((result<=kMaxCompressedSize)
                 &&(result<=2*kSyncBlockSize)); //for decompress memroy size ctrl
            cmBufPos+=result;
        }
        return result;
    }
    hsync_TDictCompress* compressPlugin;
    hsync_dictCompressHandle dictCompressHandle;
    const size_t    kSyncBlockSize;
    const size_t    kMaxCompressedSize;
    hpatch_byte*    cmBuf;
    size_t          cmBufPos;
};

    struct TWorkBuf{
        uint32_t            blockBegin;
        uint32_t            blockEnd;
        unsigned char*      buf;
        size_t              cmSize;
#if (_IS_USED_MULTITHREAD)
        struct TWorkBuf*    next;
#endif
    };

#if (_IS_USED_MULTITHREAD)
namespace{

    inline static void _insertNode(TWorkBuf** insertBuf,TWorkBuf* workBuf){
        while ((*insertBuf)&&((*insertBuf)->blockBegin<workBuf->blockBegin))
            insertBuf=&((*insertBuf)->next);
        workBuf->next=*insertBuf;
        *insertBuf=workBuf;
    }
    inline static TWorkBuf* _popNode(TWorkBuf** popBuf){
        TWorkBuf* workBuf=*popBuf;
        assert(workBuf!=0);
        *popBuf=workBuf->next;
        workBuf->next=0;
        return workBuf;
    }

    struct TMt:public TMtByChannel{
        CHLocker readLocker;
        CHLocker writeLocker;
        TWorkBuf* workFilished;
        uint32_t  blockEndBack;
        inline TMt():workFilished(0),blockEndBack(0){}
    };
}
#endif


static void _saveCompressedData(_TCreateDatas& cd,TWorkBuf* workData,void* _mt=0){
    const bool is_hsynz_readed_data=(cd.out_hsynz&&cd.hsynzPlugin&&cd.hsynzPlugin->hsynz_readed_data);
    const uint32_t kSyncBlockSize=cd.out_hsyni->kSyncBlockSize;
#if (_IS_USED_MULTITHREAD)
    TMt* mt=(TMt*)_mt;
    CAutoLocker _autoLocker(mt?mt->writeLocker.locker:0);
#endif //_IS_USED_MULTITHREAD

    while(true){
#if (_IS_USED_MULTITHREAD)
        if (mt){//save data order by blockBegin
            if ((workData!=0)&&(workData->blockBegin!=mt->blockEndBack)){
                _insertNode(&mt->workFilished,workData);
                workData=0;
            }
            if ((workData==0)&&(mt->workFilished!=0)&&(mt->workFilished->blockBegin==mt->blockEndBack))
                workData=_popNode(&mt->workFilished);
        }
#endif 
        if (workData==0) break;

        size_t backZeroLen=0;
        const uint32_t kSyncBlockCount=(workData->blockEnd-workData->blockBegin);
        const size_t dataLens=(size_t)kSyncBlockSize*kSyncBlockCount;
        {
            const hpatch_StreamPos_t curReadPos=(hpatch_StreamPos_t)workData->blockBegin*kSyncBlockSize;
            if (curReadPos+dataLens>cd.newData->streamSize)
                backZeroLen=(size_t)(curReadPos+dataLens-cd.newData->streamSize);
        }
        if (is_hsynz_readed_data)
            cd.hsynzPlugin->hsynz_readed_data(cd.hsynzPlugin,workData->buf,dataLens-backZeroLen);
        if (cd.out_hsynz){
            size_t writeSize=workData->cmSize;
            if (writeSize>0){
                hpatch_byte* cmBuf=workData->buf+dataLens;
                writeStream(cd.out_hsynz,cd.curOutPos,cmBuf,writeSize);
            }else{
                writeSize=dataLens-backZeroLen;
                writeStream(cd.out_hsynz,cd.curOutPos,workData->buf,writeSize);
            }
        }

#if (_IS_USED_MULTITHREAD)
        if (mt){
            assert(mt->blockEndBack==workData->blockBegin);
            mt->blockEndBack=workData->blockEnd;
            if (!mt->read_chan.send(workData,true)){ mt->on_error(); break; }
        }
#endif
        workData=0;
    }
}

static void _create_sync_data_part(_TCreateDatas& cd,TWorkBuf* workData,
                                   CChecksum& checksumBlockData,_TCompress& compress,void* _mt=0){
    TNewDataSyncInfo*   out_hsyni=cd.out_hsyni;
    const uint32_t      kSyncBlockSize=out_hsyni->kSyncBlockSize;
    const uint32_t      kSyncBlockCount=(workData->blockEnd-workData->blockBegin);

    size_t backZeroLen=0;
    const size_t dataLens=(size_t)kSyncBlockSize*kSyncBlockCount;
    compress.cmBuf=workData->buf+dataLens;
    compress.cmBufPos=0;
    {//read data
        const hpatch_StreamPos_t curReadPos=(hpatch_StreamPos_t)workData->blockBegin*kSyncBlockSize;
        if (curReadPos+dataLens>cd.newData->streamSize){
            backZeroLen=(size_t)(curReadPos+dataLens-cd.newData->streamSize);
            assert(backZeroLen<kSyncBlockSize);
        }
        {
#if (_IS_USED_MULTITHREAD)
            TMt* mt=(TMt*)_mt;
            CAutoLocker _autoLocker(mt?mt->readLocker.locker:0);
#endif
            checkv(cd.newData->read(cd.newData,curReadPos,workData->buf,workData->buf+dataLens-backZeroLen));
        }
        if (backZeroLen)
            memset(workData->buf+dataLens-backZeroLen,0,backZeroLen);
    }

    hpatch_byte* dataBuf=workData->buf;
    for (uint32_t i=workData->blockBegin;i<workData->blockEnd;++i,dataBuf+=kSyncBlockSize) {
        size_t srcDataLen=(i+1<workData->blockEnd)?kSyncBlockSize:kSyncBlockSize-backZeroLen;
        //compress
        size_t compressedSize=compress.doCompress(i,dataBuf,dataBuf+srcDataLen);
        checkv(compressedSize==(uint32_t)compressedSize);
        if (out_hsyni->savedSizes) //save compressedSize
            out_hsyni->savedSizes[i]=(uint32_t)compressedSize;

        const uint64_t rollHash=roll_hash_start((uint64_t*)0,dataBuf,kSyncBlockSize);
        //strong hash
        checksumBlockData.appendBegin();
        checksumBlockData.append(dataBuf,dataBuf+kSyncBlockSize);
        checksumBlockData.appendEnd();
        {//save hash
            hpatch_TChecksum* strongChecksumPlugin=out_hsyni->_strongChecksumPlugin;
            const size_t   checksumByteSize=strongChecksumPlugin->checksumByteSize();
            const uint64_t _partRollHash=toSavedPartRollHash(rollHash,out_hsyni->savedRollHashBits);
            writeRollHashBytes(out_hsyni->rollHashs+i*out_hsyni->savedRollHashByteSize,
                               _partRollHash,out_hsyni->savedRollHashByteSize);
            checkChecksumAppendData(cd.out_hsyni->savedNewDataCheckChecksum,i,
                                    strongChecksumPlugin,cd.checkChecksum,
                                    checksumBlockData.checksum.data(),checksumByteSize);
            toPartChecksum(checksumBlockData.checksum.data(),out_hsyni->savedStrongChecksumBits,
                           checksumBlockData.checksum.data(),checksumByteSize);
            memcpy(out_hsyni->partChecksums+i*out_hsyni->savedStrongChecksumByteSize,
                   checksumBlockData.checksum.data(),out_hsyni->savedStrongChecksumByteSize);
        }
    }

    workData->cmSize=compress.cmBufPos;
    _saveCompressedData(cd,workData,_mt);
}

void _private_create_sync_data(TNewDataSyncInfo*           newSyncInfo,
                               const hpatch_TStreamInput*  newData,
                               const hpatch_TStreamOutput* out_hsyni,
                               const hpatch_TStreamOutput* out_hsynz,
                               hsync_TDictCompress* compressPlugin,
                               hsync_THsynz* hsynzPlugin,size_t threadNum){
    const uint32_t kSyncBlockSize= newSyncInfo->kSyncBlockSize;
    checkv(kSyncBlockSize>=kSyncBlockSize_min);
    if (compressPlugin) checkv(out_hsynz!=0);
    hsync_THsynz _hsynzPlugin;
    if ((hsynzPlugin==0)&&((compressPlugin!=0)||newSyncInfo->isDirSyncInfo)){
        getHsynzPluginDefault(&_hsynzPlugin);
        hsynzPlugin=&_hsynzPlugin;
    }
    {//check checksumByteSize
        const size_t checksumByteSize=newSyncInfo->_strongChecksumPlugin->checksumByteSize();
        checkv((checksumByteSize==(uint32_t)checksumByteSize)
            &&(checksumByteSize*8>=kStrongChecksumBits_min));
    }

    CChecksum      _checkChecksum(newSyncInfo->_strongChecksumPlugin,false);
    _TCreateDatas  createDatas;
    createDatas.newData=newData;
    createDatas.compressPlugin=compressPlugin;
    createDatas.out_hsyni=newSyncInfo;
    createDatas.out_hsynz=out_hsynz;
    createDatas.hsynzPlugin=hsynzPlugin;
    createDatas.curOutPos=0;
    createDatas.checkChecksum=_checkChecksum._handle;
    const bool is_hsynzPlugin=(out_hsynz&&hsynzPlugin);
    if (is_hsynzPlugin){
        createDatas.curOutPos=hsynzPlugin->hsynz_write_head(hsynzPlugin,out_hsynz,createDatas.curOutPos,newSyncInfo->isDirSyncInfo,
                                newData->streamSize,kSyncBlockSize,newSyncInfo->_strongChecksumPlugin,compressPlugin);
        newSyncInfo->newSyncDataOffsert=createDatas.curOutPos;
    }
    const uint32_t kBlockCount=(uint32_t)getSyncBlockCount(newData->streamSize,kSyncBlockSize);
    newSyncInfo->dictSize=compressPlugin?compressPlugin->limitDictSizeByData(compressPlugin,kBlockCount,kSyncBlockSize):0;

    checkChecksumInit(createDatas.out_hsyni->savedNewDataCheckChecksum,
                      createDatas.out_hsyni->kStrongChecksumByteSize);
    
#if (_IS_USED_MULTITHREAD)
    /*if (threadNum>1) {
        //todo: 
    }else*/
#endif
    {
        const size_t kMaxCompressedSize=compressPlugin?compressPlugin->maxCompressedSize(kSyncBlockSize):0;
        uint32_t bestWorkBlockCount=(4*(1<<20)+kSyncBlockSize+kMaxCompressedSize-1)/(kSyncBlockSize+kMaxCompressedSize);
        bestWorkBlockCount=(bestWorkBlockCount<=kBlockCount)?bestWorkBlockCount:kBlockCount;
        TAutoMem    membuf((kSyncBlockSize+kMaxCompressedSize)*bestWorkBlockCount);
        CChecksum   checksumBlockData(newSyncInfo->_strongChecksumPlugin,false);
        _TCompress  compress(compressPlugin,kBlockCount,kSyncBlockSize,createDatas.out_hsyni);
        TWorkBuf   workData={0}; workData.buf=membuf.data();
        for (uint32_t ib=0; ib<kBlockCount; ib+=bestWorkBlockCount){
            workData.blockBegin=ib;
            workData.blockEnd=(ib+bestWorkBlockCount<=kBlockCount)?ib+bestWorkBlockCount:kBlockCount;
            _create_sync_data_part(createDatas,&workData,checksumBlockData,compress);
        }
        
    }
    checkChecksumEnd(createDatas.out_hsyni->savedNewDataCheckChecksum,
                     createDatas.out_hsyni->kStrongChecksumByteSize);
    if (is_hsynzPlugin){
        createDatas.curOutPos=hsynzPlugin->hsynz_write_foot(hsynzPlugin,out_hsynz,createDatas.curOutPos,
                                createDatas.out_hsyni->savedNewDataCheckChecksum,createDatas.out_hsyni->kStrongChecksumByteSize);
    }
    newSyncInfo->newSyncDataSize=createDatas.curOutPos;
    matchNewDataInNew(createDatas.out_hsyni);
    
    //save to out_hsyni
    TNewDataSyncInfo_saveTo(newSyncInfo,out_hsyni,compressPlugin);
}

void create_sync_data(const hpatch_TStreamInput*  newData,
                      const hpatch_TStreamOutput* out_hsyni,
                      const hpatch_TStreamOutput* out_hsynz,
                      hpatch_TChecksum*           strongChecksumPlugin,
                      hsync_TDictCompress*  compressPlugin,hsync_THsynz* hsynzPlugin,
                      uint32_t kSyncBlockSize,size_t kSafeHashClashBit,size_t threadNum){
    CNewDataSyncInfo newSyncInfo(strongChecksumPlugin,compressPlugin,
                                 newData->streamSize,kSyncBlockSize,kSafeHashClashBit);
    _private_create_sync_data(&newSyncInfo,newData,out_hsyni,out_hsynz,compressPlugin,hsynzPlugin,threadNum);
}

void create_sync_data(const hpatch_TStreamInput*  newData,
                      const hpatch_TStreamOutput* out_hsyni,
                      hpatch_TChecksum*           strongChecksumPlugin,
                      hsync_TDictCompress*  compressPlugin,
                      uint32_t kSyncBlockSize,size_t kSafeHashClashBit,size_t threadNum){
    create_sync_data(newData,out_hsyni,0,strongChecksumPlugin,compressPlugin,0,
                     kSyncBlockSize,kSafeHashClashBit,threadNum);
}
