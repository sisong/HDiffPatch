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
#include "sync_patch_hash_clash.h" // isCanUse32bitRollHash
using namespace hdiff_private;
namespace sync_private{

#define rollHashSize(self) (self->is32Bit_rollHash?sizeof(uint32_t):sizeof(uint64_t))

#define _outBuf(_buf_begin,_buf_end) { \
            checksumInfo.append(_buf_begin,_buf_end); \
            writeStream(out_stream,outPos,_buf_begin,_buf_end); }
#define _outV(_v)  if (!_v.empty()){ _outBuf(_v.data(),_v.data()+_v.size()); }
#define _outV_clear(_v)  if (!_v.empty()){ _outV(_v); swapClear(_v); }

    static void saveSamePairList(std::vector<TByte> &buf, TNewDataSyncInfo *self) {
        uint32_t pre=0;
        for (size_t i=0;i<self->samePairCount;++i){
            const TSameNewBlockPair& sp=self->samePairList[i];
            packUInt(buf,(uint32_t)(sp.curIndex-pre));
            packUInt(buf,(uint32_t)(sp.curIndex-sp.sameIndex));
            pre=sp.curIndex;
        }
    }
    
    static void saveSavedSizes(std::vector<TByte> &buf, TNewDataSyncInfo *self) {
        const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(self);
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
    
    static void compressBuf(std::vector<TByte> &buf, const hdiff_TCompress *compressPlugin) {
        std::vector<TByte> cmbuf;
        cmbuf.resize((size_t)compressPlugin->maxCompressedSize(buf.size()));
        size_t compressedSize=hdiff_compress_mem(compressPlugin,cmbuf.data(),cmbuf.data()+cmbuf.size(),
                                                 buf.data(),buf.data()+buf.size());
        checkv(compressedSize>0);
        if (compressedSize<buf.size()){
            cmbuf.resize(compressedSize);
            buf.swap(cmbuf);
        }
    }
    
    static void saveRollHashs(const hpatch_TStreamOutput *out_stream, TNewDataSyncInfo *self,
                              CChecksum &checksumInfo, hpatch_StreamPos_t &outPos) {
        std::vector<TByte> buf;
        const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(self);
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
                _outV(buf);
        }
        _outV(buf);
        assert(curPair==self->samePairCount);
    }
    
    static void savePartStrongChecksums(const hpatch_TStreamOutput *out_stream, TNewDataSyncInfo *self,
                                        CChecksum &checksumInfo, hpatch_StreamPos_t &outPos) {
        std::vector<TByte> buf;
        const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(self);
        uint32_t curPair=0;
        for (size_t i=0; i<kBlockCount; ++i){
            if ((curPair<self->samePairCount)
                &&(i==self->samePairList[curPair].curIndex)){ ++curPair; continue; }
            pushBack(buf,self->partChecksums+i*(size_t)kPartStrongChecksumByteSize,
                     kPartStrongChecksumByteSize);
            if (buf.size()>=hpatch_kFileIOBufBetterSize)
                _outV(buf);
        }
        _outV(buf);
        assert(curPair==self->samePairCount);
    }
    
void TNewDataSyncInfo_saveTo(TNewDataSyncInfo* self,const hpatch_TStreamOutput* out_stream,
                             const hdiff_TCompress* compressPlugin){
    const char* kVersionType="HSync20";
    if (compressPlugin)
        checkv(0==strcmp(compressPlugin->compressType(),self->compressType));
    else
        checkv(self->compressType==0);
    hpatch_TChecksum* strongChecksumPlugin=self->_strongChecksumPlugin;
    checkv(0==strcmp(strongChecksumPlugin->checksumType(),self->strongChecksumType));
#if (!_IS_NEED_DIR_DIFF_PATCH)
    checkv(!self->isDirSyncInfo);
#endif

    const size_t privateExternDataSize=0; //reserved ,now empty
    const size_t externDataSize=self->externData_end-self->externData_begin;
    const uint8_t isSavedSizes=(self->savedSizes)!=0?1:0;
    std::vector<TByte> buf;
    saveSamePairList(buf,self);
    if (isSavedSizes)
        saveSavedSizes(buf,self);

#if (_IS_NEED_DIR_DIFF_PATCH)
    size_t dir_newPathSumCharSize=0;
    //todo:
#endif

    //compress buf
    size_t uncompressDataSize=buf.size();
    size_t compressDataSize=0;
    if (compressPlugin){
        compressBuf(buf,compressPlugin);
        if (buf.size()<uncompressDataSize)
            compressDataSize=buf.size();
    }
    
    std::vector<TByte> head;
    {//head
        pushTypes(head,kVersionType,compressPlugin,strongChecksumPlugin);
        packUInt(head,self->kStrongChecksumByteSize);
        packUInt(head,self->kMatchBlockSize);
        packUInt(head,self->samePairCount);
        pushUInt(head,self->isDirSyncInfo);
        pushUInt(head,self->is32Bit_rollHash);
        pushUInt(head,isSavedSizes);
        packUInt(head,self->newDataSize);
        packUInt(head,self->newSyncDataSize);
        packUInt(head,privateExternDataSize);
        packUInt(head,externDataSize);
        packUInt(head,uncompressDataSize);
        packUInt(head,compressDataSize);
        
        if (self->isDirSyncInfo){
#if (_IS_NEED_DIR_DIFF_PATCH)
            packUInt(head,dir_newPathSumCharSize);
            packUInt(head,self->dir_subPathCount);
            packUInt(head,self->dir_newExecuteCount);
#endif
        }
        
        {//newSyncInfoSize
            self->newSyncInfoSize = head.size() + sizeof(self->newSyncInfoSize)
                                    + privateExternDataSize + externDataSize + buf.size();
            self->newSyncInfoSize +=(rollHashSize(self)+kPartStrongChecksumByteSize)
                                    *(TNewDataSyncInfo_blockCount(self)-self->samePairCount);
            self->newSyncInfoSize += kPartStrongChecksumByteSize;
        }
        pushUInt(head,self->newSyncInfoSize);
        //end head info
    }

    CChecksum checksumInfo(strongChecksumPlugin);
    hpatch_StreamPos_t outPos=0;
    //out head buf
    _outV_clear(head);
    //out privateExternData //reserved ,now empty
    _outBuf(self->externData_end,self->externData_begin);
    _outV_clear(buf);
    
    saveRollHashs(out_stream,self,checksumInfo,outPos);
    savePartStrongChecksums(out_stream,self,checksumInfo,outPos);;
    
    {// out infoPartChecksum
        checksumInfo.appendEnd();
        toSyncPartChecksum(self->infoPartChecksum,checksumInfo.checksum.data(),checksumInfo.checksum.size());
        writeStream(out_stream,outPos,self->infoPartChecksum,kPartStrongChecksumByteSize);
        assert(outPos==self->newSyncInfoSize);
    }
}

CNewDataSyncInfo::CNewDataSyncInfo(hpatch_TChecksum* strongChecksumPlugin,const hdiff_TCompress* compressPlugin,
                                   hpatch_StreamPos_t newDataSize,uint32_t kMatchBlockSize){
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
    this->is32Bit_rollHash=isCanUse32bitRollHash(newDataSize,kMatchBlockSize);
    //mem
    const size_t kBlockCount=this->blockCount();
    hpatch_StreamPos_t memSize=kBlockCount*( (hpatch_StreamPos_t)0
                                            +sizeof(TSameNewBlockPair)+kPartStrongChecksumByteSize
                                            +(this->is32Bit_rollHash?sizeof(uint32_t):sizeof(uint64_t))
                                            +(compressPlugin?sizeof(uint32_t):0));
    memSize+=kPartStrongChecksumByteSize;
    checkv(memSize==(size_t)memSize);
    _mem.realloc((size_t)memSize);
    TByte* curMem=_mem.data();
    
    this->infoPartChecksum=curMem; curMem+=kPartStrongChecksumByteSize;
    this->partChecksums=curMem; curMem+=kBlockCount*kPartStrongChecksumByteSize;
    this->samePairCount=0;
    this->samePairList=(TSameNewBlockPair*)curMem; curMem+=kBlockCount*sizeof(TSameNewBlockPair);
    this->rollHashs=curMem; curMem+=kBlockCount*(this->is32Bit_rollHash?sizeof(uint32_t):sizeof(uint64_t));
    if (compressPlugin){
        this->savedSizes=(uint32_t*)curMem; curMem+=kBlockCount*sizeof(uint32_t);
    }
    assert(curMem==_mem.data_end());
}

}//namespace sync_private

