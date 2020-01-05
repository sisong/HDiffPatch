//  sync_info_server.cpp
//  sync_server
//  Created by housisong on 2019-09-17.
/*
 The MIT License (MIT)
 Copyright (c) 2019-2019 HouSisong
 
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
#include "sync_info_server.h"
using namespace hdiff_private;

#define rollHashSize(self) (self->is32Bit_rollHash?sizeof(uint32_t):sizeof(uint64_t))

bool TNewDataSyncInfo_saveTo(TNewDataSyncInfo* self,const hpatch_TStreamOutput* out_stream,
                             hpatch_TChecksum* strongChecksumPlugin,const hdiff_TCompress* compressPlugin){
    const char* kSyncUpdateTypeVersion = "SyncUpdate19";
    if (compressPlugin){
        checkv(0==strcmp(compressPlugin->compressType(),self->compressType));
    }else{
        checkv(self->compressType==0); }
    checkv(0==strcmp(strongChecksumPlugin->checksumType(),self->strongChecksumType));
    CChecksum checksumInfo(strongChecksumPlugin);
    
    std::vector<TByte> head;
    {//head
        pushTypes(head,kSyncUpdateTypeVersion,compressPlugin,strongChecksumPlugin);
        packUInt(head,self->kStrongChecksumByteSize);
        packUInt(head,self->kMatchBlockSize);
        packUInt(head,self->samePairCount);
        pushUInt(head,self->is32Bit_rollHash);
        packUInt(head,self->newDataSize);
        packUInt(head,self->newSyncDataSize);
    }
    std::vector<TByte> privateExternData;//now empty
    {//privateExternData size
        //head +
        packUInt(head,privateExternData.size());
    }
    
    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(self);
    std::vector<TByte> buf;
    {//samePairList
        uint32_t pre=0;
        for (size_t i=0;i<self->samePairCount;++i){
            const TSameNewDataPair& sp=self->samePairList[i];
            packUInt(buf,(uint32_t)(sp.curIndex-pre));
            packUInt(buf,(uint32_t)(sp.curIndex-sp.sameIndex));
            pre=sp.curIndex;
        }
    }
    if (compressPlugin){ //savedSizes
        uint32_t curPair=0;
        hpatch_StreamPos_t sumSavedSize=0;
        for (uint32_t i=0; i<kBlockCount; ++i){
            uint32_t savedSize=self->savedSizes[i];
            sumSavedSize+=savedSize;
            if ((curPair<self->samePairCount)
                &&(i==self->samePairList[curPair].curIndex)){
                assert(savedSize==self->savedSizes[self->samePairList[curPair].sameIndex]);
                ++curPair;
            }else{
                if (savedSize==TNewDataSyncInfo_newDataBlockSize(self,i))
                    savedSize=0;
                packUInt(buf,savedSize);
            }
        }
        assert(curPair==self->samePairCount);
        assert(sumSavedSize==self->newSyncDataSize);
    }
    
    {//compress buf
        std::vector<TByte> cmbuf;
        if (compressPlugin){
            cmbuf.resize((size_t)compressPlugin->maxCompressedSize(buf.size()));
            size_t compressedSize=hdiff_compress_mem(compressPlugin,cmbuf.data(),cmbuf.data()+cmbuf.size(),
                                                     buf.data(),buf.data()+buf.size());
            checkv(compressedSize>0);
            if (compressedSize>=buf.size()) compressedSize=0; //not compressed
            cmbuf.resize(compressedSize);
        }
        
        //head +
        packUInt(head,buf.size());
        packUInt(head,cmbuf.size());
        if (cmbuf.size()>0){
            swapClear(buf);
            buf.swap(cmbuf);
        }
    }
    {//newSyncInfoSize
        self->newSyncInfoSize = head.size() + sizeof(self->newSyncInfoSize)
                                + privateExternData.size() + buf.size();
        self->newSyncInfoSize +=(rollHashSize(self)+kPartStrongChecksumByteSize)
                                *(hpatch_StreamPos_t)(kBlockCount-self->samePairCount);
        self->newSyncInfoSize += kPartStrongChecksumByteSize;
        //head +
        pushUInt(head,self->newSyncInfoSize);
        //end head info
    }

    #define _outBuf(_buf)    if (!_buf.empty()){ \
            checksumInfo.append(_buf); \
            writeStream(out_stream,outPos,_buf);  _buf.clear(); }
    hpatch_StreamPos_t outPos=0;
    //out head buf
    _outBuf(head);              swapClear(head);
    _outBuf(privateExternData); swapClear(privateExternData);
    _outBuf(buf);
    
    {//rollHashs
        uint32_t curPair=0;
        bool is32Bit_rollHash=(0!=self->is32Bit_rollHash);
        uint32_t* rhashs32=(uint32_t*)self->rollHashs;
        uint64_t* rhashs64=(uint64_t*)self->rollHashs;
        for (size_t i=0; i<kBlockCount; ++i){
            if ((curPair<self->samePairCount)
                &&(i==self->samePairList[curPair].curIndex)){ ++curPair; continue; }
            if (is32Bit_rollHash)
                pushUInt(buf,rhashs32[i]);
            else
                pushUInt(buf,rhashs64[i]);
            if (buf.size()>=hpatch_kFileIOBufBetterSize)
                _outBuf(buf);
        }
        assert(curPair==self->samePairCount);
    }
    {//partStrongChecksums
        uint32_t curPair=0;
        for (size_t i=0; i<kBlockCount; ++i){
            if ((curPair<self->samePairCount)
                &&(i==self->samePairList[curPair].curIndex)){ ++curPair; continue; }
            pushBack(buf,self->partChecksums+i*(size_t)kPartStrongChecksumByteSize,
                     kPartStrongChecksumByteSize);
            if (buf.size()>=hpatch_kFileIOBufBetterSize)
                _outBuf(buf);
        }
        assert(curPair==self->samePairCount);
    }
    _outBuf(buf); swapClear(buf);
    
    {// out infoPartChecksum
        checksumInfo.appendEnd();
        toPartChecksum(self->infoPartChecksum,checksumInfo.checksum.data(),checksumInfo.checksum.size());
        writeStream(out_stream,outPos,self->infoPartChecksum,kPartStrongChecksumByteSize);
        assert(outPos==self->newSyncInfoSize);
    }
    return true;
}

CNewDataSyncInfo::CNewDataSyncInfo(hpatch_TChecksum* strongChecksumPlugin,const hdiff_TCompress* compressPlugin,
                                   hpatch_StreamPos_t newDataSize,uint32_t kMatchBlockSize){
    TNewDataSyncInfo_init(this);
    if (compressPlugin){
        this->_compressType.assign(compressPlugin->compressType());
        this->compressType=this->_compressType.c_str();
    }
    this->_strongChecksumType.assign(strongChecksumPlugin->checksumType());
    this->strongChecksumType=this->_strongChecksumType.c_str();
    this->kStrongChecksumByteSize=(uint32_t)strongChecksumPlugin->checksumByteSize();
    this->kMatchBlockSize=kMatchBlockSize;
    this->newDataSize=newDataSize;
    this->is32Bit_rollHash=isCanUse32bitRollHash(newDataSize,kMatchBlockSize);
    //mem
    const size_t kBlockCount=this->blockCount();
    hpatch_StreamPos_t memSize=kBlockCount*( (hpatch_StreamPos_t)0
                                            +sizeof(TSameNewDataPair)+kPartStrongChecksumByteSize
                                            +(this->is32Bit_rollHash?sizeof(uint32_t):sizeof(uint64_t))
                                            +(compressPlugin?sizeof(uint32_t):0));
    memSize+=kPartStrongChecksumByteSize;
    checkv(memSize==(size_t)memSize);
    _mem.realloc((size_t)memSize);
    TByte* curMem=_mem.data();
    
    this->infoPartChecksum=curMem; curMem+=kPartStrongChecksumByteSize;
    this->partChecksums=curMem; curMem+=kBlockCount*kPartStrongChecksumByteSize;
    this->samePairCount=0;
    this->samePairList=(TSameNewDataPair*)curMem; curMem+=kBlockCount*sizeof(TSameNewDataPair);
    this->rollHashs=curMem; curMem+=kBlockCount*(this->is32Bit_rollHash?sizeof(uint32_t):sizeof(uint64_t));
    if (compressPlugin){
        this->savedSizes=(uint32_t*)curMem; curMem+=kBlockCount*sizeof(uint32_t);
    }
    assert(curMem==_mem.data_end());
}
