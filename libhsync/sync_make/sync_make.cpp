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
    :compressPlugin(_compressPlugin),dictCompressHandle(0),kSyncBlockSize(_kSyncBlockSize){
        assert(out_hsyni->decompressInfoSize==0);
        if (compressPlugin!=0){
            dictCompressHandle=compressPlugin->dictCompressOpen(compressPlugin,kSyncBlockCount,_kSyncBlockSize);
            checkv(dictCompressHandle!=0);
            cmbuf.resize((size_t)(compressPlugin->maxCompressedSize(kSyncBlockSize)));
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
        if (dictCompressHandle==0) return 0;
        size_t result=compressPlugin->dictCompress(dictCompressHandle,blockIndex,cmbuf.data(),
                                                   cmbuf.data()+cmbuf.size(),data,dataEnd);
        checkv(result!=kDictCompressError);
        if (result==kDictCompressCancel)
            result=0; //cancel compress
        checkv(result<=2*kSyncBlockSize); //for decompress memroy size ctrl
        return result;
    }
    hsync_TDictCompress* compressPlugin;
    hsync_dictCompressHandle   dictCompressHandle;
    size_t                     kSyncBlockSize;
    std::vector<TByte> cmbuf;
};

static void mt_create_sync_data(_TCreateDatas& cd,void* _mt=0,int threadIndex=0){
    TNewDataSyncInfo*       out_hsyni=cd.out_hsyni;
    const uint32_t          kSyncBlockSize=out_hsyni->kSyncBlockSize;
    hpatch_TChecksum*       strongChecksumPlugin=out_hsyni->_strongChecksumPlugin;
    const uint32_t          kBlockCount=(uint32_t)getSyncBlockCount(out_hsyni->newDataSize,kSyncBlockSize);
    _TCompress              compress(cd.compressPlugin,kBlockCount,kSyncBlockSize,out_hsyni);
    std::vector<TByte>      buf(kSyncBlockSize);
    const size_t            checksumByteSize=strongChecksumPlugin->checksumByteSize();
    checkv((checksumByteSize==(uint32_t)checksumByteSize)
          &&(checksumByteSize*8>=kStrongChecksumBits_min));
    CChecksum checksumBlockData(strongChecksumPlugin,false);
    
    const bool is_hsynz_readed_data=(cd.out_hsynz&&cd.hsynzPlugin&&cd.hsynzPlugin->hsynz_readed_data);
    hpatch_StreamPos_t curReadPos=0;
    hpatch_byte* const dataBuf=buf.data();
    for (uint32_t i=0; i<kBlockCount; ++i,curReadPos+=kSyncBlockSize) {
        size_t dataLen=kSyncBlockSize;
        if (i+1==kBlockCount) dataLen=(size_t)(cd.newData->streamSize-curReadPos);
        {//read data can by locker or by order
            checkv(cd.newData->read(cd.newData,curReadPos,dataBuf,dataBuf+dataLen));
            if (is_hsynz_readed_data)
                cd.hsynzPlugin->hsynz_readed_data(cd.hsynzPlugin,dataBuf,dataLen);
        }
        {
            size_t backZeroLen=kSyncBlockSize-dataLen;
            if (backZeroLen>0)
                memset(dataBuf+dataLen,0,backZeroLen);
        }
        
        const uint64_t rollHash=roll_hash_start((uint64_t*)0,dataBuf,kSyncBlockSize);
        //strong hash
        checksumBlockData.appendBegin();
        checksumBlockData.append(dataBuf,dataBuf+kSyncBlockSize);
        checksumBlockData.appendEnd();
        //compress
        size_t compressedSize=compress.doCompress(i,dataBuf,dataBuf+dataLen);
        checkv(compressedSize==(uint32_t)compressedSize);
        
        {//save data order by workIndex
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
            
            if (out_hsyni->savedSizes)
                out_hsyni->savedSizes[i]=(uint32_t)compressedSize;
            if (compressedSize>0){
                out_hsyni->newSyncDataSize+=compressedSize;
                if (cd.out_hsynz)
                    writeStream(cd.out_hsynz,cd.curOutPos,compress.cmbuf.data(),compressedSize);
            }else{
                out_hsyni->newSyncDataSize+=dataLen;
                if (cd.out_hsynz)
                    writeStream(cd.out_hsynz,cd.curOutPos,dataBuf,dataLen);
            }
        }
    }
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
        newSyncInfo->newSyncDataSize=createDatas.curOutPos;
    }
    const uint32_t kBlockCount=(uint32_t)getSyncBlockCount(newData->streamSize,kSyncBlockSize);
    newSyncInfo->dictSize=compressPlugin?compressPlugin->limitDictSizeByData(compressPlugin,kBlockCount,kSyncBlockSize):0;

    checkChecksumInit(createDatas.out_hsyni->savedNewDataCheckChecksum,
                      createDatas.out_hsyni->kStrongChecksumByteSize);
    {
        mt_create_sync_data(createDatas);
    }
    checkChecksumEnd(createDatas.out_hsyni->savedNewDataCheckChecksum,
                     createDatas.out_hsyni->kStrongChecksumByteSize);
    if (is_hsynzPlugin){
        createDatas.curOutPos=hsynzPlugin->hsynz_write_foot(hsynzPlugin,out_hsynz,createDatas.curOutPos,
                                createDatas.out_hsyni->savedNewDataCheckChecksum,createDatas.out_hsyni->kStrongChecksumByteSize);
    }
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
