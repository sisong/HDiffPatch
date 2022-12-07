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
    const hsync_TDictCompress*  compressPlugin;
    hsync_dictCompressHandle    sharedDictCompress;
    size_t                      dictSize;
    TNewDataSyncInfo*           out_hsyni;
    const hpatch_TStreamOutput* out_hsynz;
    hpatch_StreamPos_t          curOutPos;
};


struct _TCompress_auto_free{
    inline _TCompress_auto_free(const hsync_TDictCompress* _compressPlugin,hsync_dictCompressHandle dictCompress)
    :compressPlugin(_compressPlugin),self_dictCompressHandle(dictCompress){ }
    inline ~_TCompress_auto_free(){
        if (self_dictCompressHandle)
            compressPlugin->dictCompressClose(compressPlugin,self_dictCompressHandle); }
    const hsync_TDictCompress* compressPlugin;
    hsync_dictCompressHandle   self_dictCompressHandle;
};

struct _TCompress{
    inline _TCompress(const hsync_TDictCompress* _compressPlugin,hsync_dictCompressHandle _dictCompress,
                      uint32_t kMatchBlockSize)
    :compressPlugin(_compressPlugin),dictCompressHandle(_dictCompress){
        if (dictCompressHandle!=0)
            cmbuf.resize(compressPlugin->maxCompressedSize(kMatchBlockSize));
    }
    inline size_t doCompress(const TByte* data,const TByte* dictEnd,const TByte* dataEnd){
        if (dictCompressHandle==0) return 0;
        return compressPlugin->dictCompress(dictCompressHandle,cmbuf.data(),
                                            cmbuf.data()+cmbuf.size(),data,dictEnd,dataEnd);
    }
    const hsync_TDictCompress* compressPlugin;
    hsync_dictCompressHandle   dictCompressHandle;
    std::vector<TByte> cmbuf;
};

static void mt_create_sync_data(_TCreateDatas& cd,void* _mt=0,int threadIndex=0){
    TNewDataSyncInfo*       out_hsyni=cd.out_hsyni;
    const uint32_t          kMatchBlockSize=out_hsyni->kMatchBlockSize;
    _TCompress              compress(cd.compressPlugin,cd.sharedDictCompress,kMatchBlockSize);
    hpatch_TChecksum*       strongChecksumPlugin=out_hsyni->_strongChecksumPlugin;
    const uint32_t          kBlockCount=(uint32_t)getSyncBlockCount(out_hsyni->newDataSize,kMatchBlockSize);
    std::vector<TByte>      buf(kMatchBlockSize);
    const size_t            checksumByteSize=strongChecksumPlugin->checksumByteSize();
    checkv((checksumByteSize==(uint32_t)checksumByteSize)
          &&(checksumByteSize>=kStrongChecksumByteSize_min)
          &&(checksumByteSize%sizeof(uint32_t)==0));
    CChecksum checksumBlockData(strongChecksumPlugin,false);

    hpatch_StreamPos_t curReadPos=0;
    for (uint32_t i=0; i<kBlockCount; ++i,curReadPos+=kMatchBlockSize) {
        size_t dataLen=kMatchBlockSize;
        if (i==kBlockCount-1) dataLen=(size_t)(cd.newData->streamSize-curReadPos);
        {//read data can by locker or by order
            checkv(cd.newData->read(cd.newData,curReadPos,buf.data(),buf.data()+dataLen));
        }
        size_t backZeroLen=kMatchBlockSize-dataLen;
        if (backZeroLen>0)
            memset(buf.data()+dataLen,0,backZeroLen);
        
        uint64_t rollHash=roll_hash_start((uint64_t*)0,buf.data(),kMatchBlockSize);
        //strong hash
        checksumBlockData.appendBegin();
        checksumBlockData.append(buf.data(),buf.data()+kMatchBlockSize);
        checksumBlockData.appendEnd();
        //compress
        size_t compressedSize=compress.doCompress(buf.data(),buf.data(),buf.data()+dataLen);
        checkv(compressedSize==(uint32_t)compressedSize);
        
        {//save data order by workIndex
            toSavedPartRollHash(out_hsyni->rollHashs+i*out_hsyni->savedRollHashByteSize,
                                rollHash,out_hsyni->savedRollHashByteSize);
            checkChecksumAppendData(cd.out_hsyni->savedNewDataCheckChecksum,i,
                                    checksumBlockData.checksum.data(),checksumByteSize);
            toPartChecksum(checksumBlockData.checksum.data(),out_hsyni->savedStrongChecksumByteSize,
                           checksumBlockData.checksum.data(),checksumByteSize);
            memcpy(out_hsyni->partChecksums+i*out_hsyni->savedStrongChecksumByteSize,
                   checksumBlockData.checksum.data(),out_hsyni->savedStrongChecksumByteSize);
            if (cd.compressPlugin){
                if (compressedSize>0)
                    out_hsyni->savedSizes[i]=(uint32_t)compressedSize;
                else
                    out_hsyni->savedSizes[i]=(uint32_t)dataLen;
            }
            
            if (compressedSize>0){
                if (cd.out_hsynz)
                    writeStream(cd.out_hsynz,cd.curOutPos,compress.cmbuf.data(),compressedSize);
                out_hsyni->newSyncDataSize+=compressedSize;
            }else{
                if (cd.out_hsynz)
                    writeStream(cd.out_hsynz,cd.curOutPos, buf.data(),dataLen);
                out_hsyni->newSyncDataSize+=dataLen;
            }
        }
    }
}

void _private_create_sync_data(TNewDataSyncInfo*           newSyncInfo,
                               const hpatch_TStreamInput*  newData,
                               const hpatch_TStreamOutput* out_hsyni,
                               const hpatch_TStreamOutput* out_hsynz,
                               const hsync_TDictCompress* compressPlugin,size_t threadNum){
    _TCreateDatas  createDatas;
    createDatas.newData=newData;
    createDatas.compressPlugin=compressPlugin;
    createDatas.sharedDictCompress=0;
    createDatas.out_hsyni=newSyncInfo;
    createDatas.out_hsynz=out_hsynz;
    createDatas.dictSize=0;
    createDatas.curOutPos=0;
    
    const uint32_t kMatchBlockSize=createDatas.out_hsyni->kMatchBlockSize;
    checkv(kMatchBlockSize>=kMatchBlockSize_min);
    if (createDatas.compressPlugin) checkv(createDatas.out_hsynz!=0);
    
    if (compressPlugin){//init dict
        createDatas.sharedDictCompress=compressPlugin->dictCompressOpen(compressPlugin);
        checkv(createDatas.sharedDictCompress!=0);
    }
    _TCompress_auto_free _auto_free_sharedDictCompress(compressPlugin,createDatas.sharedDictCompress);

    checkChecksumInit(createDatas.out_hsyni->savedNewDataCheckChecksum,
                      createDatas.out_hsyni->kStrongChecksumByteSize);
    {
        mt_create_sync_data(createDatas);
    }
    checkChecksumEnd(createDatas.out_hsyni->savedNewDataCheckChecksum,
                      createDatas.out_hsyni->kStrongChecksumByteSize);
    matchNewDataInNew(createDatas.out_hsyni);
    
    //save to out_hsyni
    TNewDataSyncInfo_saveTo(newSyncInfo,out_hsyni,compressPlugin,createDatas.sharedDictCompress,createDatas.dictSize);
}

void create_sync_data(const hpatch_TStreamInput*  newData,
                      const hpatch_TStreamOutput* out_hsyni,
                      const hpatch_TStreamOutput* out_hsynz,
                      hpatch_TChecksum*           strongChecksumPlugin,
                      const hsync_TDictCompress*  compressPlugin,
                      uint32_t kMatchBlockSize,size_t kSafeHashClashBit,size_t threadNum){
    CNewDataSyncInfo newSyncInfo(strongChecksumPlugin,compressPlugin,
                                 newData->streamSize,kMatchBlockSize,kSafeHashClashBit);
    _private_create_sync_data(&newSyncInfo,newData,out_hsyni,out_hsynz,compressPlugin,threadNum);
}

void create_sync_data(const hpatch_TStreamInput*  newData,
                      const hpatch_TStreamOutput* out_hsyni,
                      hpatch_TChecksum*           strongChecksumPlugin,
                      const hsync_TDictCompress*  compressPlugin,
                      uint32_t kMatchBlockSize,size_t kSafeHashClashBit,size_t threadNum){
    create_sync_data(newData,out_hsyni,0,strongChecksumPlugin,compressPlugin,
                     kMatchBlockSize,kSafeHashClashBit,threadNum);
}
