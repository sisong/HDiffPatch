//  sync_server.cpp
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
#include "sync_server.h"
#include <stdexcept>
#include <vector>
#include <string>
#include "../../file_for_patch.h"
#include "../../libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.h"
#include "match_in_new.h"

#define checki(value,info) { if (!(value)) { throw std::runtime_error(info); } }
#define check(value) checki(value,"check "#value" error!")

static void writeStream(const hpatch_TStreamOutput* out_stream,hpatch_StreamPos_t& outPos,
                       const TByte* buf,size_t byteSize){
    check(out_stream->write(out_stream,outPos,buf,buf+byteSize));
    outPos+=byteSize;
}
inline static void writeStream(const hpatch_TStreamOutput* out_stream,hpatch_StreamPos_t& outPos,
                              const std::vector<TByte>& buf){
    writeStream(out_stream,outPos,buf.data(),buf.size());
}

inline static void writeData(std::vector<TByte>& out_buf,const TByte* buf,size_t byteSize){
    out_buf.insert(out_buf.end(),buf,buf+byteSize);
}
inline static void writeCStr(std::vector<TByte>& out_buf,const char* str){
    writeData(out_buf,(const TByte*)str,strlen(str));
}

static void writeUInt(std::vector<TByte>& out_buf,
                      hpatch_StreamPos_t v,size_t byteSize){
    assert(sizeof(hpatch_StreamPos_t)>=byteSize);
    TByte buf[sizeof(hpatch_StreamPos_t)];
    size_t len=sizeof(hpatch_StreamPos_t);
    if (len>byteSize) len=byteSize;
    for (size_t i=0; i<len; ++i) {
        buf[i]=(TByte)v;
        v>>=8;
    }
    writeData(out_buf,buf,len);
}

template <class TUInt> inline static
void writeUInt(std::vector<TByte>& out_buf,TUInt v){
    writeUInt(out_buf,v,sizeof(v));
}

static void packUInt(std::vector<TByte>& out_buf,hpatch_StreamPos_t v){
    TByte buf[hpatch_kMaxPackedUIntBytes];
    TByte* curBuf=buf;
    check(hpatch_packUInt(&curBuf,buf+sizeof(buf),v));
    writeData(out_buf,buf,(curBuf-buf));
}

class _TAutoClose_checksumHandle{
public:
    inline _TAutoClose_checksumHandle(hpatch_TChecksum* cp,hpatch_checksumHandle ch)
    :_cp(cp),_ch(ch){}
    inline ~_TAutoClose_checksumHandle(){ _cp->close(_cp,_ch); }
private:
    hpatch_TChecksum*     _cp;
    hpatch_checksumHandle _ch;
};

inline static void _clear(std::vector<TByte>& buf){
    std::vector<TByte> _temp;
    _temp.swap(buf);
}

static bool TNewDataSyncInfo_saveTo(TNewDataSyncInfo*      self,
                                    const hpatch_TStreamOutput* out_stream,
                                    hpatch_TChecksum*      strongChecksumPlugin,
                                    const hdiff_TCompress* compressPlugin){
    const char* kSyncUpdateTypeVersion = "SyncUpdate19";
    if (compressPlugin){
        check(0==strcmp(compressPlugin->compressType(),self->compressType));
    }else{
        check(self->compressType==0); }
    check(0==strcmp(strongChecksumPlugin->checksumType(),self->strongChecksumType));
    hpatch_checksumHandle checksumInfo=strongChecksumPlugin->open(strongChecksumPlugin);
    check(checksumInfo!=0);
    _TAutoClose_checksumHandle _autoClose_checksumHandle(strongChecksumPlugin,checksumInfo);

    std::vector<TByte> head;
    {//head
        std::vector<TByte>& buf=head;
        writeCStr(buf,kSyncUpdateTypeVersion);
        buf.push_back('&');
        writeCStr(buf,self->strongChecksumType);
        buf.push_back('&');
        if (self->compressType)
            writeCStr(buf,self->compressType);
        buf.push_back('\0');//c string end tag
        
        packUInt(buf,self->kStrongChecksumByteSize);
        packUInt(buf,self->kMatchBlockSize);
        packUInt(buf,self->samePairCount);
        packUInt(buf,self->newDataSize);
        packUInt(buf,self->newSyncDataSize);
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
        for (uint32_t i=0; i<kBlockCount; ++i){
            if ((curPair<self->samePairCount)
                &&(i==self->samePairList[curPair].curIndex)){ ++curPair; continue; }
            if (self->savedSizes[i]!=TNewDataSyncInfo_newDataBlockSize(self,i))
                packUInt(buf,self->savedSizes[i]);
            else
                packUInt(buf,(uint32_t)0);
        }
    }
    
    {//compress buf
        std::vector<TByte> cmbuf;
        if (compressPlugin){
            cmbuf.resize(compressPlugin->maxCompressedSize(buf.size()));
            size_t compressedSize=hdiff_compress_mem(compressPlugin,cmbuf.data(),cmbuf.data()+cmbuf.size(),
                                                     buf.data(),buf.data()+buf.size());
            check(compressedSize>0);
            if (compressedSize>=buf.size()) compressedSize=0; //not compressed
            cmbuf.resize(compressedSize);
        }
        
        //head +
        packUInt(head,buf.size());
        packUInt(head,cmbuf.size());
        if (cmbuf.size()>0){
            _clear(buf);
            buf.swap(cmbuf);
        }
    }
    {//newSyncInfoSize
        //head +
        self->newSyncInfoSize = head.size() + sizeof(self->newSyncInfoSize) + buf.size();
        self->newSyncInfoSize += (kBlockCount-self->samePairCount)
                                *(hpatch_StreamPos_t)(sizeof(roll_uint_t)+kPartStrongChecksumByteSize);
        self->newSyncInfoSize += kPartStrongChecksumByteSize;
        writeUInt(head,self->newSyncInfoSize);
    }

    //out head buf
    #define _outBuf(_buf)    if (!_buf.empty()){ \
            strongChecksumPlugin->append(checksumInfo,_buf.data(),_buf.data()+_buf.size()); \
            writeStream(out_stream,outPos,_buf);  _buf.clear(); }
    hpatch_StreamPos_t outPos=0;
    strongChecksumPlugin->begin(checksumInfo);
    _outBuf(head); _clear(head);
    _outBuf(buf);
    
    {//rollHashs
        uint32_t curPair=0;
        for (size_t i=0; i<kBlockCount; ++i){
            if ((curPair<self->samePairCount)
                &&(i==self->samePairList[curPair].curIndex)){ ++curPair; continue; }
            writeUInt(buf,self->rollHashs[i]);
            if (buf.size()>=hpatch_kFileIOBufBetterSize)
                _outBuf(buf);
        }
    }
    {//partStrongChecksums
        uint32_t curPair=0;
        for (size_t i=0; i<kBlockCount; ++i){
            if ((curPair<self->samePairCount)
                &&(i==self->samePairList[curPair].curIndex)){ ++curPair; continue; }
            writeData(buf,self->partChecksums+i*(size_t)kPartStrongChecksumByteSize,
                      kPartStrongChecksumByteSize);
            if (buf.size()>=hpatch_kFileIOBufBetterSize)
                _outBuf(buf);
        }
    }
    _outBuf(buf); _clear(buf);
    
    {// out infoPartChecksum
        std::vector<TByte> checksumBuf(self->kStrongChecksumByteSize);
        strongChecksumPlugin->end(checksumInfo,checksumBuf.data(),checksumBuf.data()+checksumBuf.size());
        toPartChecksum(self->infoPartChecksum,checksumBuf.data(),checksumBuf.size());
        writeStream(out_stream,outPos,self->infoPartChecksum,kPartStrongChecksumByteSize);
        assert(outPos==self->newSyncInfoSize);
    }
    return true;
}

class CNewDataSyncInfo :public TNewDataSyncInfo{
public:
    inline uint32_t blockCount()const{
        hpatch_StreamPos_t result=TNewDataSyncInfo_blockCount(this);
        check(result==(uint32_t)result);
        return (uint32_t)result; }
    inline CNewDataSyncInfo(hpatch_TChecksum*      strongChecksumPlugin,
                            const hdiff_TCompress* compressPlugin,
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
        
        const uint32_t kBlockCount=this->blockCount();
        this->samePairCount=0;
        this->_samePairList.resize(kBlockCount);
        this->samePairList=this->_samePairList.data();
        this->_rollHashs.resize(kBlockCount);
        this->rollHashs=this->_rollHashs.data();
        
        this->_infoPartChecksum.resize(kPartStrongChecksumByteSize,0);
        this->infoPartChecksum=this->_infoPartChecksum.data();
        hpatch_StreamPos_t _checksumsBufSize=kBlockCount*(hpatch_StreamPos_t)kPartStrongChecksumByteSize;
        check(_checksumsBufSize==(size_t)_checksumsBufSize);
        this->_partStrongChecksums.resize((size_t)_checksumsBufSize);
        this->partChecksums=this->_partStrongChecksums.data();
        if (compressPlugin){
            this->_savedSizes.resize(kBlockCount);
            this->savedSizes=this->_savedSizes.data();
        }
    }
    ~CNewDataSyncInfo(){}
    
    void setStrongChecksums(size_t index,const TByte* checksum,size_t checksumByteSize){
        assert(checksumByteSize==this->kStrongChecksumByteSize);
        toPartChecksum(this->partChecksums+index*(size_t)kPartStrongChecksumByteSize,
                       checksum,checksumByteSize);
    }
    void insetSamePair(uint32_t curIndex,uint32_t sameIndex){
        TSameNewDataPair& samePair=this->samePairList[this->samePairCount];
        samePair.curIndex=curIndex;
        samePair.sameIndex=sameIndex;
        ++this->samePairCount;
    }
private:
    std::string                 _compressType;
    std::string                 _strongChecksumType;
    std::vector<TByte>          _infoPartChecksum;
    std::vector<TSameNewDataPair> _samePairList;
    std::vector<uint32_t>       _savedSizes;
    std::vector<roll_uint_t>    _rollHashs;
    std::vector<TByte>          _partStrongChecksums;
    std::vector<TByte>          _newDataInsureStrongChecksums;
};

static void create_sync_data(const hpatch_TStreamInput*  newData,
                             CNewDataSyncInfo&           out_newSyncInfo,
                             const hpatch_TStreamOutput* out_newSyncData,
                             const hdiff_TCompress* compressPlugin,
                             hpatch_TChecksum*      strongChecksumPlugin,
                             uint32_t kMatchBlockSize){
    check(kMatchBlockSize>=kMatchBlockSize_min);
    if (compressPlugin) check(out_newSyncData!=0);
    
    const uint32_t kBlockCount=out_newSyncInfo.blockCount();
    std::vector<TByte> buf(kMatchBlockSize);
    std::vector<TByte> cmbuf(compressPlugin?compressPlugin->maxCompressedSize(kMatchBlockSize):0);
    const size_t checksumByteSize=strongChecksumPlugin->checksumByteSize();
    check((checksumByteSize==(uint32_t)checksumByteSize)
          &&(checksumByteSize>=kPartStrongChecksumByteSize)
          &&(checksumByteSize%kPartStrongChecksumByteSize==0));
    std::vector<TByte> checksumBlockData_buf(checksumByteSize);
    hpatch_checksumHandle checksumBlockData=strongChecksumPlugin->open(strongChecksumPlugin);
    check(checksumBlockData!=0);
    _TAutoClose_checksumHandle _autoClose_checksumHandle(strongChecksumPlugin,checksumBlockData);
    std::vector<TByte> checksumNewData_buf(checksumByteSize);
    hpatch_checksumHandle checksumNewData=strongChecksumPlugin->open(strongChecksumPlugin);
    check(checksumNewData!=0);
    _TAutoClose_checksumHandle _autoClose_checksumNewData(strongChecksumPlugin,checksumNewData);
    
    hpatch_StreamPos_t curReadPos=0;
    hpatch_StreamPos_t curOutPos=0;
    strongChecksumPlugin->begin(checksumNewData);
    for (uint32_t i=0; i<kBlockCount; ++i,curReadPos+=kMatchBlockSize) {
        //read data
        size_t dataLen=buf.size();
        if (i==kBlockCount-1) dataLen=newData->streamSize-curReadPos;
        check(newData->read(newData,curReadPos,buf.data(),buf.data()+dataLen));
        size_t backZeroLen=kMatchBlockSize-dataLen;
        if (backZeroLen>0)
            memset(buf.data()+dataLen,0,backZeroLen);
        //rool hash
        out_newSyncInfo.rollHashs[i]=roll_hash_start(buf.data(),kMatchBlockSize);
        //strong hash
        strongChecksumPlugin->begin(checksumBlockData);
        strongChecksumPlugin->append(checksumBlockData,buf.data(),buf.data()+kMatchBlockSize);
        strongChecksumPlugin->end(checksumBlockData,checksumBlockData_buf.data(),
                                  checksumBlockData_buf.data()+checksumByteSize);
        out_newSyncInfo.setStrongChecksums(i,checksumBlockData_buf.data(),checksumByteSize);
        
        //compress
        size_t compressedSize=0;
        if (compressPlugin){
            compressedSize=hdiff_compress_mem(compressPlugin,cmbuf.data(),cmbuf.data()+cmbuf.size(),
                                              buf.data(),buf.data()+dataLen);
            check(compressedSize>0);
            if (compressedSize+sizeof(uint32_t)>=dataLen)
                compressedSize=0; //not compressed
            //save compressed size
            check(compressedSize==(uint32_t)compressedSize);
            if (compressedSize>0)
                out_newSyncInfo.savedSizes[i]=(uint32_t)compressedSize;
            else
                out_newSyncInfo.savedSizes[i]=(uint32_t)dataLen;
        }
        //save data
        if (compressedSize>0){
            if (out_newSyncData)
                writeStream(out_newSyncData,curOutPos, cmbuf.data(),compressedSize);
            out_newSyncInfo.newSyncDataSize+=compressedSize;
        }else{
            if (out_newSyncData)
                writeStream(out_newSyncData,curOutPos, buf.data(),dataLen);
            out_newSyncInfo.newSyncDataSize+=dataLen;
        }
    }
    _clear(cmbuf);
    _clear(buf);
    matchNewDataInNew(&out_newSyncInfo);
}

void create_sync_data(const hpatch_TStreamInput*  newData,
                      const hpatch_TStreamOutput* out_newSyncInfo,
                      const hpatch_TStreamOutput* out_newSyncData,
                      const hdiff_TCompress* compressPlugin,
                      hpatch_TChecksum*      strongChecksumPlugin,
                      uint32_t kMatchBlockSize){
    CNewDataSyncInfo newSyncInfo(strongChecksumPlugin,compressPlugin,
                                 newData->streamSize,kMatchBlockSize);
    create_sync_data(newData,newSyncInfo,out_newSyncData,
                     compressPlugin,strongChecksumPlugin,kMatchBlockSize);
    check(TNewDataSyncInfo_saveTo(&newSyncInfo,out_newSyncInfo,
                                  strongChecksumPlugin,compressPlugin));
}

void create_sync_data(const char* newDataPath,
                      const char* out_newSyncInfoPath,
                      const char* out_newSyncDataPath,
                      const hdiff_TCompress* compressPlugin,
                      hpatch_TChecksum*      strongChecksumPlugin,
                      uint32_t kMatchBlockSize){
    hpatch_TFileStreamInput  newData;
    hpatch_TFileStreamOutput out_newSyncInfo;
    hpatch_TFileStreamOutput out_newSyncData;
    
    hpatch_TFileStreamInput_init(&newData);
    hpatch_TFileStreamOutput_init(&out_newSyncInfo);
    hpatch_TFileStreamOutput_init(&out_newSyncData);
    check(hpatch_TFileStreamInput_open(&newData,newDataPath));
    check(hpatch_TFileStreamOutput_open(&out_newSyncInfo,out_newSyncInfoPath,
                                        (hpatch_StreamPos_t)(-1)));
    if (out_newSyncDataPath)
        check(hpatch_TFileStreamOutput_open(&out_newSyncData,out_newSyncDataPath,
                                            (hpatch_StreamPos_t)(-1)));
    
    create_sync_data(&newData.base,&out_newSyncInfo.base,
                     (out_newSyncDataPath)?&out_newSyncData.base:0,
                     compressPlugin,strongChecksumPlugin,kMatchBlockSize);
    check(hpatch_TFileStreamOutput_close(&out_newSyncData));
    check(hpatch_TFileStreamOutput_close(&out_newSyncInfo));
    check(hpatch_TFileStreamInput_close(&newData));
}

void create_sync_data(const char* newDataPath,
                      const char* out_newSyncInfoPath,
                      hpatch_TChecksum*      strongChecksumPlugin,
                      uint32_t kMatchBlockSize){
    create_sync_data(newDataPath,out_newSyncInfoPath,0,0,strongChecksumPlugin,kMatchBlockSize);
}

void create_sync_data(const hpatch_TStreamInput*  newData,
                      const hpatch_TStreamOutput* out_newSyncInfo, //newSyncData same as newData
                      hpatch_TChecksum*      strongChecksumPlugin,
                      uint32_t kMatchBlockSize){
    create_sync_data(newData,out_newSyncInfo,0,0,strongChecksumPlugin,kMatchBlockSize);
}
