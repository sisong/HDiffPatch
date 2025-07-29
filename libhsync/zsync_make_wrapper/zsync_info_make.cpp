//  zsync_info_make.cpp
//  zsync_make for hsynz
//  Created by housisong on 2025-07-16.
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
#include "zsync_info_make.h"
#include "../sync_make/sync_make_hash_clash.h"
#include "../sync_client/sync_info_client.h" // TNewDataSyncInfo_dir_saveHeadTo
#include "../../_atosize.h"
#include "../../_hextobytes.h"
#include <algorithm> //std::min,std::max
using namespace hdiff_private;

static const size_t _kAdler32Bytes=4;
static const size_t _kAdler32BadLostBits=3;
bool z_getStrongForHashClash(size_t kSafeHashClashBit,hpatch_StreamPos_t newDataSize,
                             uint32_t kSyncBlockSize,size_t strongChecksumBits){
    return _getStrongForHashClash(kSafeHashClashBit,newDataSize,kSyncBlockSize,
                                  strongChecksumBits,(_kAdler32Bytes*8-_kAdler32BadLostBits)*2);
}

static void pushUIntAsStr(std::vector<TByte>& out_code,hpatch_uint64_t value){
    char temp_buf[_u64_to_a_kMaxLen];
    pushCStr(out_code,u64_to_enough_a(value,temp_buf));
}

static void pushBytesAsHex(std::vector<TByte>& out_code,const hpatch_byte* bytes,size_t blen){
    const size_t sizebck=out_code.size();
    out_code.resize(sizebck+blen*2);
    bytes_to_hexs(bytes,blen,(char*)out_code.data()+sizebck);
}

namespace sync_private{

void TNewDataZsyncInfo_savedSizesToBits(TNewDataZsyncInfo* self){
    if ((self->savedSizes==0)||(self->isSavedBitsSizes)) return;
    const uint32_t kBlockCount=(uint32_t)getSyncBlockCount(self->newDataSize,self->kSyncBlockSize);
    for (size_t i=0;i<kBlockCount;++i){
        assert(self->savedSizes[i]>0);
        check(self->savedSizes[i]<(1<<(32-3-3)),"block size is too large for zsync");
        self->savedSizes[i]<<=3;
    }
    self->isSavedBitsSizes=hpatch_TRUE;
}


#define _flushV(_v)  if (!_v.empty()){ \
                        writeStream(out_stream,outPos,_v.data(),_v.data()+_v.size()); _v.clear(); }

const size_t _kBestFlushSize=hpatch_kFileIOBufBetterSize*2-16;

    static void pack_zmap2_data(std::vector<TByte>& buf,hpatch_uint32_t bitsSize,uint32_t outBytes){
        assert(bitsSize==(uint16_t)bitsSize);
        check(bitsSize==(uint16_t)bitsSize,"block size is too large for zsync");
        pushByte(buf,(hpatch_byte)(bitsSize>>8));
        pushByte(buf,(hpatch_byte)bitsSize);
        pushByte(buf,(hpatch_byte)(outBytes>>8));
        pushByte(buf,(hpatch_byte)outBytes);
    }
    static size_t  getZmBlocks(const savedBitsInfo_t* savedBitsInfos,uint32_t kBlockCount){
        return kBlockCount+1;
    }
static void saveSavedBitsInfos(const hpatch_TStreamOutput* out_stream,hpatch_StreamPos_t& outPos,
                               hpatch_StreamPos_t gzOffset0,hpatch_StreamPos_t gzFileSize,hpatch_StreamPos_t newDataSize,
                               const savedBitsInfo_t* savedBitsInfos,uint32_t kBlockCount,
                               uint32_t kBlockSize,std::vector<TByte>& buf){
    check(kBlockSize==(kBlockSize&((1<<15)-1)),"block size is too large for zsync");
    hpatch_StreamPos_t curBitsPos=gzOffset0*8;
    pack_zmap2_data(buf,(hpatch_uint32_t)curBitsPos,0);
    for (size_t i=0; i<kBlockCount; ++i){
        const hpatch_uint32_t bitsSize=savedBitsInfos[i].bitsSize;
        assert((curBitsPos&7)==savedBitsInfos[i].skipBitsInFirstCodeByte);
        pack_zmap2_data(buf,bitsSize,(i+1<kBlockCount)?kBlockSize:(hpatch_uint32_t)(newDataSize-kBlockSize*(hpatch_StreamPos_t)i));
        curBitsPos+=bitsSize;
        if (buf.size()>=_kBestFlushSize)
            _flushV(buf);
    }
    check(curBitsPos+(8<<3)==(gzFileSize<<3),".gz size or compressed block size error");//8 bytes is gzip file foot size
}

static void save2PartHash(const hpatch_TStreamOutput* out_stream,hpatch_StreamPos_t& outPos,
                          const hpatch_byte* rollHashs,size_t rollBytes,
                          const hpatch_byte* checksums,size_t checksumBytes,
                          uint32_t kBlockCount,std::vector<TByte>& buf){
    for (size_t i=0; i<kBlockCount; ++i,rollHashs+=rollBytes,checksums+=checksumBytes){
        pushBack(buf,rollHashs,rollBytes);
        pushBack(buf,checksums,checksumBytes);
        if (buf.size()>=_kBestFlushSize)
            _flushV(buf);
    }
}

static void saveKeyValues(const hpatch_TStreamOutput* out_stream,hpatch_StreamPos_t& outPos,
                          const std::vector<std::string>& zsyncKeyValues,std::vector<TByte>& buf){
    for (size_t i=0;i<zsyncKeyValues.size();i+=2){
        const std::string& key=zsyncKeyValues[i];
        pushBack(buf,(const hpatch_byte*)key.data(),key.size());
        pushByte(buf,':'); pushByte(buf,' ');
        if (i+1<zsyncKeyValues.size()){
            const std::string& value=zsyncKeyValues[i+1];
            pushBack(buf,(const hpatch_byte*)value.data(),value.size());
        }
        pushByte(buf,'\n');
    }
}


void TNewDataZsyncInfo_saveTo(TNewDataZsyncInfo* self,const hpatch_TStreamOutput* out_stream,
                              const std::vector<std::string>& zsyncKeyValues){
    const uint32_t kBlockCount=(uint32_t)getSyncBlockCount(self->newDataSize,self->kSyncBlockSize);
    hpatch_StreamPos_t outPos=0;
    std::vector<TByte> buf;
    pushCStr(buf,"zsync: 0.6.3\n");
    pushCStr(buf,"Blocksize: ");    pushUIntAsStr(buf,self->kSyncBlockSize);        pushByte(buf,'\n');
    pushCStr(buf,"Length: ");       pushUIntAsStr(buf,self->newDataSize);           pushByte(buf,'\n');
    pushCStr(buf,"Hash-Lengths: "); pushUIntAsStr(buf,self->isSeqMatch?2:1);        pushByte(buf,',');
        pushUIntAsStr(buf,self->savedRollHashByteSize);         pushByte(buf,','); 
        pushUIntAsStr(buf,self->savedStrongChecksumByteSize);   pushByte(buf,'\n');
    pushCStr(buf,"SHA-1: "); pushBytesAsHex(buf,self->savedNewDataCheckChecksum,
                                            self->fileChecksumPlugin->checksumByteSize()); pushByte(buf,'\n');
    saveKeyValues(out_stream,outPos,zsyncKeyValues,buf);

    if (self->isSavedBitsSizes){
        pushCStr(buf,"Z-Map2: "); 
        hpatch_uint64_t _zmBlocks=getZmBlocks(self->savedBitsInfos,kBlockCount);
        pushUIntAsStr(buf,_zmBlocks); pushByte(buf,'\n');
        saveSavedBitsInfos(out_stream,outPos,self->newSyncDataOffsert,self->newSyncDataSize,self->newDataSize,
                           self->savedBitsInfos,kBlockCount,self->kSyncBlockSize,buf);
    }
    pushByte(buf,'\n'); //end of key-value

    save2PartHash(out_stream,outPos,self->rollHashs,self->savedRollHashByteSize,
                  self->partChecksums,self->savedStrongChecksumByteSize,kBlockCount,buf);
    _flushV(buf);
}


static size_t z_getSavedHashBits(size_t kSafeHashClashBit,hpatch_StreamPos_t newDataSize,uint32_t kSyncBlockSize,size_t kStrongHashBits,
                                 size_t* out_partRollHashBits,size_t* out_partStrongHashBits,uint8_t* out_isSeqMatch){
    const size_t _kMinStrongHashBytes=4;
    const size_t kStrongHashBytes=kStrongHashBits/8;
    const uint32_t kBlockCount=(uint32_t)getSyncBlockCount(newDataSize,kSyncBlockSize);
    
    //calc for zsync compatible
    const int compareCountBit=(int)_estimateCompareCountBit(newDataSize,kBlockCount)-9;
    int rollHashBytes=(compareCountBit+7)/8;
    size_t seqMatch=1;
    if (rollHashBytes>_kAdler32Bytes){
        rollHashBytes=_kAdler32Bytes;
        seqMatch=2;
    }
    rollHashBytes=std::max(rollHashBytes,2);

    const size_t safeBit0=(kSafeHashClashBit+_estimateCompareCountBit(newDataSize,kSyncBlockSize))/seqMatch;
    const size_t safeBit1=kSafeHashClashBit+sync_private::upper_ilog2(kBlockCount);
    size_t safeBytes=(std::max(safeBit0,safeBit1)+7)/8;
    safeBytes=std::min(std::max(safeBytes,_kMinStrongHashBytes),kStrongHashBytes);
    
    *out_isSeqMatch=(seqMatch>1);
    *out_partRollHashBits=(size_t)rollHashBytes*8;
    *out_partStrongHashBits=safeBytes*8;
    return (*out_partRollHashBits)+(*out_partStrongHashBits);
}

static bool z_isNeedSamePair(){
    return false;
}

CNewDataZsyncInfo::CNewDataZsyncInfo(hpatch_TChecksum* _fileChecksumPlugin,hpatch_TChecksum* _strongChecksumPlugin,const hsync_TDictCompress* compressPlugin,
                                     hpatch_StreamPos_t newDataSize,uint32_t syncBlockSize,size_t kSafeHashClashBit){
    _ISyncInfo_by si_by={0};
    si_by.getSavedHashBits=z_getSavedHashBits;
    si_by.isNeedSamePair=z_isNeedSamePair;
    _init_by(&si_by,_fileChecksumPlugin,_strongChecksumPlugin,compressPlugin,
             newDataSize,syncBlockSize,kSafeHashClashBit);
    this->isNotCChecksumNewMTParallel=hpatch_TRUE; //unsupport parallel checksum, must check by order
}

}//namespace sync_private

