//  sync_make.cpp
//  sync_make
//  Created by housisong on 2019-09-17.
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
#include "sync_make.h"
#include "sync_make_hash_clash.h"
#include "sync_info_make.h"
#include "match_in_new.h"
#include "../../libParallel/parallel_channel.h"
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
    inline _TCompress():compressPlugin(0),dictCompressHandle(0),kSyncBlockSize(0),
                        kMaxCompressedSize(0),cmBuf(0),cmBufPos(0),blockEnd(0){}
    inline _TCompress(hsync_TDictCompress* _compressPlugin,uint32_t kSyncBlockCount,
                      uint32_t _kSyncBlockSize,TNewDataSyncInfo* out_hsyni)
    :compressPlugin(0),dictCompressHandle(0),kSyncBlockSize(0),
     kMaxCompressedSize(0),cmBuf(0),cmBufPos(0),blockEnd(0){ 
        init(_compressPlugin,kSyncBlockCount,_kSyncBlockSize,out_hsyni); }
    void init(hsync_TDictCompress* _compressPlugin,uint32_t kSyncBlockCount,
              uint32_t _kSyncBlockSize,TNewDataSyncInfo* out_hsyni){
        checkv(kSyncBlockSize==0);
        compressPlugin=(kSyncBlockCount>0)?_compressPlugin:0;
        kSyncBlockSize=_kSyncBlockSize;
        kMaxCompressedSize=compressPlugin?(size_t)compressPlugin->maxCompressedSize(_kSyncBlockSize):0;
        if (compressPlugin!=0){
            dictCompressHandle=compressPlugin->dictCompressOpen(compressPlugin,kSyncBlockCount,_kSyncBlockSize);
            checkv(dictCompressHandle!=0);
            if (out_hsyni&&(out_hsyni->decompressInfoSize==0)&&compressPlugin->dictCompressInfo){
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
    inline size_t doCompress(uint32_t blockIndex,const TByte* in_data,size_t in_dataSize,size_t in_borderSize){
        if (dictCompressHandle==0)
            return 0;//not compress
        blockEnd=blockIndex+1;
        assert(cmBuf!=0);
        hpatch_byte* curBuf=cmBuf+cmBufPos;
        size_t result=compressPlugin->dictCompress(dictCompressHandle,blockIndex,curBuf,
                                                   curBuf+kMaxCompressedSize,in_data,in_dataSize,in_borderSize);
        checkv(result!=kDictCompressError);
        if (result==kDictCompressCancel){
            result=0; //cancel compress
            memcpy(curBuf,in_data,in_dataSize);
            cmBufPos+=in_dataSize;
        }else{
            checkv((result<=kMaxCompressedSize)
                 &&(result<=2*kSyncBlockSize)); //for decompress memroy size ctrl
            cmBufPos+=result;
        }
        return result;
    }
    
    inline void  resertDict(const hpatch_TStreamInput* data,hpatch_StreamPos_t curReadPos,size_t blockIndex){
        if (dictCompressHandle==0)
            return;//not compress
        if (blockEnd==blockIndex)
            return;//not need reset dict
        size_t dictSize=0;
        hpatch_byte* dictBuf=compressPlugin->getResetDictBuffer(dictCompressHandle,blockIndex,&dictSize);
        if (dictSize==0) return;
        checkv((dictBuf!=0)&&(dictSize<=curReadPos));
        checkv(data->read(data,curReadPos-dictSize,dictBuf,dictBuf+dictSize));
    }

    hsync_TDictCompress* compressPlugin;
    hsync_dictCompressHandle dictCompressHandle;
    size_t          kSyncBlockSize;
    size_t          kMaxCompressedSize;
    hpatch_byte*    cmBuf;
    size_t          cmBufPos;
    uint32_t        blockEnd;
};

    struct TWorkBuf{
        uint32_t            blockBegin;
        uint32_t            blockEnd;
        unsigned char*      buf;
        size_t              in_borderSize;
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

    struct TMt;
    struct TThreadData{
        CChecksum   checksumBlockData;
        _TCompress  compress;
    };
    

    struct TMt:public TMtByChannel{
        _TCreateDatas& cd;
        uint32_t bestWorkBlockCount;
        CHLocker readLocker;
        CHLocker writeLocker;
        CHLocker workpLocker;
        CHLocker checkLocker;
        TWorkBuf* workFilished;
        volatile uint32_t workBlockEnd;
        volatile uint32_t savedBlockEnd;
        const uint32_t kBlockCount;
        std::vector<TThreadData> threadDataList;
        inline TMt(size_t threadNum,_TCreateDatas& _cd,
                   uint32_t _bestWorkBlockCount,hpatch_TChecksum* checksumPlugin,
                   hsync_TDictCompress* compressPlugin,uint32_t kSyncBlockCount,
                   uint32_t kSyncBlockSize,TNewDataSyncInfo* out_hsyni)
        :cd(_cd),bestWorkBlockCount(_bestWorkBlockCount),
         workFilished(0),workBlockEnd(0),savedBlockEnd(0),kBlockCount(kSyncBlockCount){
            threadDataList.resize(threadNum);
            for (size_t i=0;i<threadNum;++i){
                TThreadData& td=threadDataList[i];
                td.checksumBlockData.init(checksumPlugin,false);
                td.compress.init(compressPlugin,kSyncBlockCount,kSyncBlockSize,out_hsyni);
            }
        }
        inline TWorkBuf* getWorkPart(){
            if (kBlockCount==workBlockEnd) return 0;
            TWorkBuf* workBuf=(TWorkBuf*)read_chan.accept(true);
            if ((workBuf==0)||(kBlockCount==workBlockEnd)) return 0;

            CAutoLocker _auto_locker(workpLocker.locker);
            if (kBlockCount==workBlockEnd) return 0;
            workBuf->blockBegin=workBlockEnd;
            workBlockEnd+=bestWorkBlockCount;
            workBlockEnd=(workBlockEnd<=kBlockCount)?workBlockEnd:kBlockCount;
            workBuf->blockEnd=workBlockEnd;
            return workBuf;
        }
    };
}
#endif


static void _saveCompressedData(_TCreateDatas& cd,TWorkBuf* workData,void* _mt=0){
    const bool is_hsynz_readed_data=(cd.out_hsynz&&cd.hsynzPlugin&&cd.hsynzPlugin->hsynz_readed_data);
    const uint32_t kSyncBlockSize=cd.out_hsyni->kSyncBlockSize;
#if (_IS_USED_MULTITHREAD)
  TMt* mt=(TMt*)_mt;
  TWorkBuf* delWorkBufList=0;
  {//in lock
    CAutoLocker _autoLocker(mt?mt->writeLocker.locker:0);
#endif //_IS_USED_MULTITHREAD

    while(true){
#if (_IS_USED_MULTITHREAD)
        if (mt){//save data order by blockBegin
            if ((workData!=0)&&(workData->blockBegin!=mt->savedBlockEnd)){
                _insertNode(&mt->workFilished,workData);
                workData=0;
            }
            if ((workData==0)&&(mt->workFilished!=0)&&(mt->workFilished->blockBegin==mt->savedBlockEnd))
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
                hpatch_byte* cmBuf=workData->buf+dataLens+workData->in_borderSize;
                writeStream(cd.out_hsynz,cd.curOutPos,cmBuf,writeSize);
            }else{
                writeSize=dataLens-backZeroLen;
                writeStream(cd.out_hsynz,cd.curOutPos,workData->buf,writeSize);
            }
        }
#if (_IS_USED_MULTITHREAD)
        if (mt){
            assert(mt->savedBlockEnd==workData->blockBegin);
            mt->savedBlockEnd=workData->blockEnd;
            workData->blockBegin=0;
            _insertNode(&delWorkBufList,workData);
        }
#endif
        workData=0;
    }
#if (_IS_USED_MULTITHREAD)
  }//out lock
    if (mt){
        while (delWorkBufList){
            workData=_popNode(&delWorkBufList);
            if (!mt->read_chan.send(workData,true)){ mt->on_error(); break; }
        }
    }
#endif
}

static void _create_sync_data_part(_TCreateDatas& cd,TWorkBuf* workData,
                                   CChecksum& checksumBlockData,_TCompress& compress,void* _mt=0){
    TNewDataSyncInfo*   out_hsyni=cd.out_hsyni;
    const uint32_t      kSyncBlockSize=out_hsyni->kSyncBlockSize;
    const uint32_t      kSyncBlockCount=(workData->blockEnd-workData->blockBegin);
    hpatch_StreamPos_t curReadPos=(hpatch_StreamPos_t)workData->blockBegin*kSyncBlockSize;

    size_t backZeroLen=0;
    const size_t dataLens=(size_t)kSyncBlockSize*kSyncBlockCount;
    compress.cmBuf=workData->buf+dataLens+workData->in_borderSize;
    compress.cmBufPos=0;
    {//read data
        if (curReadPos+dataLens>cd.newData->streamSize){
            backZeroLen=(size_t)(curReadPos+dataLens-cd.newData->streamSize);
            assert(backZeroLen<kSyncBlockSize);
        }
        {
            size_t readLen=dataLens-backZeroLen+workData->in_borderSize;
            if (curReadPos+readLen>cd.newData->streamSize)
                    readLen=cd.newData->streamSize-curReadPos;
#if (_IS_USED_MULTITHREAD)
            TMt* mt=(TMt*)_mt;
            CAutoLocker _autoLocker(mt?mt->readLocker.locker:0);
            if (mt)
                compress.resertDict(cd.newData,curReadPos,workData->blockBegin);
#endif
            checkv(cd.newData->read(cd.newData,curReadPos,workData->buf,workData->buf+readLen));
        }
        if (backZeroLen)
            memset(workData->buf+dataLens-backZeroLen,0,backZeroLen);
    }

    hpatch_byte* dataBuf=workData->buf;
    for (uint32_t i=workData->blockBegin;i<workData->blockEnd;++i,dataBuf+=kSyncBlockSize,curReadPos+=kSyncBlockSize) {
        size_t srcDataLen=(i+1<workData->blockEnd)?kSyncBlockSize:kSyncBlockSize-backZeroLen;
        size_t cur_borderSize=workData->in_borderSize;
        if (curReadPos+srcDataLen+cur_borderSize>cd.newData->streamSize)
            cur_borderSize=cd.newData->streamSize-(curReadPos+srcDataLen);
        //compress
        size_t compressedSize=compress.doCompress(i,dataBuf,srcDataLen,cur_borderSize);
        checkv(compressedSize==(uint32_t)compressedSize);
        if (out_hsyni->savedSizes) //save compressedSize
            out_hsyni->savedSizes[i]=(uint32_t)compressedSize;

        const uint64_t rollHash=roll_hash_start(dataBuf,kSyncBlockSize);
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
            {
#if (_IS_USED_MULTITHREAD)
                TMt* mt=(TMt*)_mt;
                CAutoLocker _autoLocker(mt?mt->checkLocker.locker:0);
#endif
                checkChecksumAppendData(cd.out_hsyni->savedNewDataCheckChecksum,i,
                                        strongChecksumPlugin,cd.checkChecksum,
                                        checksumBlockData.checksum.data(),checksumByteSize);
            }
            toPartChecksum(checksumBlockData.checksum.data(),out_hsyni->savedStrongChecksumBits,
                           checksumBlockData.checksum.data(),checksumByteSize);
            memcpy(out_hsyni->partChecksums+i*out_hsyni->savedStrongChecksumByteSize,
                   checksumBlockData.checksum.data(),out_hsyni->savedStrongChecksumByteSize);
        }
    }

    workData->cmSize=compress.cmBufPos;
    _saveCompressedData(cd,workData,_mt);
}

#if (_IS_USED_MULTITHREAD)
static void _create_sync_data_mt(int threadIndex,void* _mt){
    TMt& mt=*(TMt*)_mt;
    TMtByChannel::TAutoThreadEnd __auto_thread_end(mt);
    TThreadData& td=mt.threadDataList[threadIndex];
    while (true){
        TWorkBuf* workData=mt.getWorkPart();
        if (workData==0)
            break; //work finish
        try{
            _create_sync_data_part(mt.cd,workData,td.checksumBlockData,td.compress,&mt);
        }catch(...){
            mt.on_error();
        }
    }
}
#endif

void _private_create_sync_data(TNewDataSyncInfo*           newSyncInfo,
                               const hpatch_TStreamInput*  newData,
                               const hpatch_TStreamOutput* out_hsyni,
                               const hpatch_TStreamOutput* out_hsynz,
                               hsync_TDictCompress* compressPlugin,
                               hsync_THsynz* hsynzPlugin,size_t threadNum){
    const uint32_t kSyncBlockSize= newSyncInfo->kSyncBlockSize;
    hpatch_TChecksum* checksumPlugin=newSyncInfo->_strongChecksumPlugin;
    checkv(kSyncBlockSize>=_kSyncBlockSize_min_limit);
    if (compressPlugin) checkv(out_hsynz!=0);
    hsync_THsynz _hsynzPlugin;
    if ((hsynzPlugin==0)&&((compressPlugin!=0)||newSyncInfo->isDirSyncInfo)){
        getHsynzPluginDefault(&_hsynzPlugin);
        hsynzPlugin=&_hsynzPlugin;
    }
    {//check checksumByteSize
        const size_t checksumByteSize=checksumPlugin->checksumByteSize();
        checkv((checksumByteSize<=_kStrongChecksumByteSize_max_limit)&&(checksumByteSize>=kStrongChecksumByteSize_min));
    }

    CChecksum      _checkChecksum(checksumPlugin,false);
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
                                                            newData->streamSize,kSyncBlockSize,checksumPlugin,compressPlugin);
        newSyncInfo->newSyncDataOffsert=createDatas.curOutPos;
    }
    const uint32_t kBlockCount=(uint32_t)getSyncBlockCount(newData->streamSize,kSyncBlockSize);
    newSyncInfo->dictSize=compressPlugin?compressPlugin->limitDictSizeByData(compressPlugin,kBlockCount,kSyncBlockSize):0;

    checkChecksumInit(createDatas.out_hsyni->savedNewDataCheckChecksum,
                      createDatas.out_hsyni->kStrongChecksumByteSize);
    
    const size_t in_borderSize=(compressPlugin&&compressPlugin->getDictCompressBorder)?compressPlugin->getDictCompressBorder():0;
    const hpatch_StreamPos_t kMaxCompressedSize=compressPlugin?compressPlugin->maxCompressedSize(kSyncBlockSize):0;
    const size_t _kBestWorkBufSize=2*(1<<20);
    uint32_t bestWorkBlockCount=(uint32_t)((_kBestWorkBufSize+kSyncBlockSize+kMaxCompressedSize-1)
                                          /(kSyncBlockSize+kMaxCompressedSize));
    bestWorkBlockCount=(bestWorkBlockCount<=kBlockCount)?bestWorkBlockCount:kBlockCount;
 #if (_IS_USED_MULTITHREAD)
    while ((hpatch_StreamPos_t)bestWorkBlockCount*threadNum>kBlockCount)
        --threadNum;
    if ((threadNum>1)&&(bestWorkBlockCount<=kBlockCount/2)){
        if (compressPlugin)
            bestWorkBlockCount=(uint32_t)compressPlugin->getBestWorkBlockCount(compressPlugin,kBlockCount,kSyncBlockSize,bestWorkBlockCount);
        const uint32_t maxWorkBlockCount=(uint32_t)((kBlockCount+threadNum-1)/threadNum);
        bestWorkBlockCount=(bestWorkBlockCount<=maxWorkBlockCount)?bestWorkBlockCount:maxWorkBlockCount;
        
        const size_t kWorkBufCount=threadNum+(threadNum/4)+1;
        const size_t kWorkMemBufSize=(kSyncBlockSize+(size_t)kMaxCompressedSize)*bestWorkBlockCount+in_borderSize;
        TAutoMem    membuf((sizeof(TWorkBuf)+kWorkMemBufSize)*kWorkBufCount);

        TMt mt(threadNum,createDatas,bestWorkBlockCount,checksumPlugin,compressPlugin,
               kBlockCount,kSyncBlockSize,createDatas.out_hsyni);
        { //mem buf
            TWorkBuf* workBufList=(TWorkBuf*)membuf.data();
            hpatch_byte* workMemBuf=(hpatch_byte*)(workBufList+kWorkBufCount);
            memset(workBufList,0,sizeof(TWorkBuf)*kWorkBufCount);
            for (size_t i=0;i<kWorkBufCount;++i,workBufList++,workMemBuf+=kWorkMemBufSize){
                workBufList->buf=workMemBuf;
                workBufList->in_borderSize=in_borderSize;
                checkv(mt.read_chan.send(workBufList,true));
            }
        }

        checkv(mt.start_threads((int)threadNum,_create_sync_data_mt,&mt,true));
        mt.wait_all_thread_end();
        checkv(!mt.is_on_error());
        checkv(mt.savedBlockEnd==mt.kBlockCount);
    }else
#endif
    {
        TAutoMem    membuf((kSyncBlockSize+(size_t)kMaxCompressedSize)*bestWorkBlockCount+in_borderSize);
        CChecksum   checksumBlockData(checksumPlugin,false);
        _TCompress  compress(compressPlugin,kBlockCount,kSyncBlockSize,createDatas.out_hsyni);
        TWorkBuf    workData={0};
        workData.buf=membuf.data();
        workData.in_borderSize=in_borderSize;
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
