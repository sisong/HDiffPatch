//  sync_info_make.cpp
//  sync_make
//  Created by housisong on 2019-09-17.
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
#include "sync_info_make.h"
#include "sync_make_hash_clash.h" // isCanUse32bitRollHash
using namespace hdiff_private;
namespace sync_private{

#define _outBuf(_buf_begin,_buf_end) { \
            checksumInfo.append(_buf_begin,_buf_end); \
            writeStream(out_stream,outPos,_buf_begin,_buf_end); }
#define _flushV(_v)  if (!_v.empty()){ _outBuf(_v.data(),_v.data()+_v.size()); _v.clear(); }
#define _flushV_end(_v)  if (!_v.empty()){ _flushV(_v); swapClear(_v); }

    static void saveSamePairList(std::vector<TByte> &buf,
                                 const TSameNewBlockPair* samePairList, size_t samePairCount) {
        uint32_t pre=0;
        for (size_t i=0;i<samePairCount;++i){
            const TSameNewBlockPair& sp=samePairList[i];
            packUInt(buf,(uint32_t)(sp.curIndex-pre));
            packUInt(buf,(uint32_t)(sp.curIndex-sp.sameIndex));
            pre=sp.curIndex;
        }
    }
    
    static void saveSavedSizes(std::vector<TByte> &buf, TNewDataSyncInfo *self) {
        const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(self);
        hpatch_StreamPos_t sumSavedSize=0;
        for (uint32_t i=0; i<kBlockCount; ++i){
            uint32_t savedSize=self->savedSizes[i];
            sumSavedSize+=savedSize;
            if (savedSize==TNewDataSyncInfo_newDataBlockSize(self,i))
                savedSize=0;
            packUInt(buf,savedSize);
        }
        assert(sumSavedSize==self->newSyncDataSize);
    }
    
    static void _compressBuf(std::vector<TByte> &buf, const hsync_TDictCompress *compressPlugin) {
        std::vector<TByte> cmbuf;
        cmbuf.resize((size_t)compressPlugin->maxCompressedSize(buf.size()));
        hsync_dictCompressHandle  compressHandle=compressPlugin->dictCompressOpen(compressPlugin);
        checkv(compressHandle!=0);
        size_t compressedSize=compressPlugin->dictCompress(compressHandle,cmbuf.data(),cmbuf.data()+cmbuf.size(),
                                                           buf.data(),buf.data(),buf.data()+buf.size());
        compressPlugin->dictCompressClose(compressPlugin,compressHandle);
        if ((compressedSize>0)&&(compressedSize<buf.size())){
            cmbuf.resize(compressedSize);
            buf.swap(cmbuf);
        }
    }
    
    static void savePartHash(const hpatch_TStreamOutput *out_stream,hpatch_StreamPos_t &outPos,
                             uint32_t kBlockCount,const TByte* partHash,size_t partSize,
                             const TSameNewBlockPair* samePairList,uint32_t samePairCount,CChecksum &checksumInfo) {
        std::vector<TByte> buf;
        uint32_t curPair=0;
        for (size_t i=0; i<kBlockCount; ++i,partHash+=partSize){
            if ((curPair<samePairCount)&&(i==samePairList[curPair].curIndex))
                { ++curPair; continue; }
            pushBack(buf,partHash,partSize);
            if (buf.size()>=hpatch_kFileIOBufBetterSize)
                _flushV(buf);
        }
        _flushV(buf);
        assert(curPair==samePairCount);
    }
    
void TNewDataSyncInfo_saveTo(TNewDataSyncInfo* self,const hpatch_TStreamOutput* out_stream,
                             const hsync_TDictCompress* compressPlugin,hsync_dictCompressHandle dictHandle,size_t dictSize){
#if ( ! (_IS_NEED_DIR_DIFF_PATCH) )
    checkv(!self->isDirSyncInfo);
#endif
    const char* kVersionType=self->isDirSyncInfo?"HDirSync20":"HSync20";
    if (compressPlugin)
        checkv(0==strcmp(compressPlugin->compressType(),self->compressType));
    else
        checkv(self->compressType==0);
    hpatch_TChecksum* strongChecksumPlugin=self->_strongChecksumPlugin;
    checkv(0==strcmp(strongChecksumPlugin->checksumType(),self->strongChecksumType));

    const size_t privateExternDataSize=0; //reserved ,now empty
    const size_t externDataSize=0;//reserved ,now empty
    const uint8_t isSavedSizes=(self->savedSizes)!=0?1:0;
    std::vector<TByte> buf;
    
    saveSamePairList(buf,self->samePairList,self->samePairCount);
    if (isSavedSizes)
        saveSavedSizes(buf,self);

#if (_IS_NEED_DIR_DIFF_PATCH)
    size_t dir_newPathSumCharSize=0;
    if (self->isDirSyncInfo){
        checkv(!self->dir_newNameList_isCString);
        dir_newPathSumCharSize=pushNameList(buf,self->dir_utf8NewRootPath,
                                            (std::string*)self->dir_utf8NewNameList,self->dir_newPathCount);
        packList(buf,self->dir_newSizeList,self->dir_newPathCount);
        packIncList(buf,self->dir_newExecuteIndexList,self->dir_newExecuteCount);
    }
#endif

    //compress buf
    size_t uncompressDataSize=buf.size();
    size_t compressDataSize=0;
    if (compressPlugin){
        _compressBuf(buf,compressPlugin);
        if (buf.size()<uncompressDataSize)
            compressDataSize=buf.size();
    }
    
    std::vector<TByte> head;
    {//head
        pushTypes(head,kVersionType,compressPlugin?compressPlugin->compressType():0,strongChecksumPlugin);
        packUInt(head,self->kStrongChecksumByteSize);
        packUInt(head,self->savedStrongChecksumByteSize);
        packUInt(head,self->savedRollHashByteSize);
        packUInt(head,self->kMatchBlockSize);
        packUInt(head,self->samePairCount);
        pushUInt(head,self->isDirSyncInfo);
        pushUInt(head,isSavedSizes);
        packUInt(head,self->newDataSize);
        packUInt(head,self->newSyncDataSize);
        packUInt(head,dictSize);
        packUInt(head,privateExternDataSize);
        packUInt(head,externDataSize);
        packUInt(head,uncompressDataSize);
        packUInt(head,compressDataSize);
        
#if (_IS_NEED_DIR_DIFF_PATCH)
        if (self->isDirSyncInfo){
            packUInt(head,dir_newPathSumCharSize);
            packUInt(head,self->dir_newPathCount);
            packUInt(head,self->dir_newExecuteCount);
        }
#endif
        
        {//newSyncInfoSize
            self->newSyncInfoSize = head.size() + sizeof(self->newSyncInfoSize)
                                    + privateExternDataSize + externDataSize + buf.size();
            self->newSyncInfoSize +=(self->savedRollHashByteSize+self->savedStrongChecksumByteSize)
                                    *(TNewDataSyncInfo_blockCount(self)-self->samePairCount);
            self->newSyncInfoSize += self->kStrongChecksumByteSize+self->kStrongChecksumByteSize;
        }
        pushUInt(head,self->newSyncInfoSize);
        //end head info
    }
    hpatch_StreamPos_t kBlockCount=TNewDataSyncInfo_blockCount(self);
    checkv(kBlockCount==(uint32_t)kBlockCount);

    CChecksum checksumInfo(strongChecksumPlugin);
    hpatch_StreamPos_t outPos=0;
    //out head buf
    _flushV_end(head);
    //adding newDataCheckChecksum
    _outBuf(self->savedNewDataCheckChecksum,self->savedNewDataCheckChecksum+self->kStrongChecksumByteSize);
    assert(privateExternDataSize==0);//out privateExternData //reserved ,now empty
    assert(externDataSize==0);//reserved ,now empty
    _flushV_end(buf);
    
    savePartHash(out_stream,outPos,(uint32_t)kBlockCount,self->rollHashs,self->savedRollHashByteSize,
                 self->samePairList,self->samePairCount,checksumInfo);
    savePartHash(out_stream,outPos,(uint32_t)kBlockCount,self->partChecksums,self->savedStrongChecksumByteSize,
                 self->samePairList,self->samePairCount,checksumInfo);
    
    {// out infoFullChecksum
        checksumInfo.appendEnd();
        assert(checksumInfo.checksum.size()==self->kStrongChecksumByteSize);
        memcpy(self->infoFullChecksum,checksumInfo.checksum.data(),checksumInfo.checksum.size());
        writeStream(out_stream,outPos,self->infoFullChecksum,checksumInfo.checksum.size());
        assert(outPos==self->newSyncInfoSize);
    }
}

CNewDataSyncInfo::CNewDataSyncInfo(hpatch_TChecksum* strongChecksumPlugin,const hsync_TDictCompress* compressPlugin,
                                   hpatch_StreamPos_t newDataSize,uint32_t kMatchBlockSize,size_t kSafeHashClashBit){
    TNewDataSyncInfo_init(this);
    if (compressPlugin){
        this->_compressType.assign(compressPlugin->compressType());
        this->compressType=this->_compressType.c_str();
    }
    this->_strongChecksumPlugin=strongChecksumPlugin;
    this->_strongChecksumType.assign(strongChecksumPlugin->checksumType());
    this->strongChecksumType=this->_strongChecksumType.c_str();
    this->kStrongChecksumByteSize=(uint32_t)strongChecksumPlugin->checksumByteSize();
    this->kMatchBlockSize=kMatchBlockSize;
    this->newDataSize=newDataSize;
    getNeedHashByte(kSafeHashClashBit,newDataSize,kMatchBlockSize,this->kStrongChecksumByteSize,
                    &this->savedRollHashByteSize,&this->savedStrongChecksumByteSize);
    //mem
    const size_t kBlockCount=this->blockCount();
    hpatch_StreamPos_t memSize=kBlockCount*( (hpatch_StreamPos_t)0
                                            +sizeof(TSameNewBlockPair)+this->savedStrongChecksumByteSize
                                            +this->savedRollHashByteSize +(compressPlugin?sizeof(uint32_t):0));
    memSize+=this->kStrongChecksumByteSize+checkChecksumBufByteSize(this->kStrongChecksumByteSize);
    checkv(memSize==(size_t)memSize);
    _mem.realloc((size_t)memSize);
    TByte* curMem=_mem.data();
    
    this->infoFullChecksum=curMem; curMem+=this->kStrongChecksumByteSize;
    this->savedNewDataCheckChecksum=curMem; curMem+=checkChecksumBufByteSize(this->kStrongChecksumByteSize);
    this->partChecksums=curMem; curMem+=kBlockCount*this->savedStrongChecksumByteSize;
    this->samePairCount=0;
    this->samePairList=(TSameNewBlockPair*)curMem; curMem+=kBlockCount*sizeof(TSameNewBlockPair);
    this->rollHashs=curMem; curMem+=kBlockCount*this->savedRollHashByteSize;
    if (compressPlugin){
        this->savedSizes=(uint32_t*)curMem; curMem+=kBlockCount*sizeof(uint32_t);
    }
    assert(curMem==_mem.data_end());
}

}//namespace sync_private

