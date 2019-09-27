//  sync_client_type.h
//  sync_client
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
#ifndef sync_client_type_h
#define sync_client_type_h
#include "../../libHDiffPatch/HPatch/patch_types.h"
#include "../../libHDiffPatch/HPatch/checksum_plugin.h"
#include "../../libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.h"
#ifdef __cplusplus
inline static uint64_t roll_hash_start(uint64_t*,const adler_data_t* pdata,size_t n){
                                       return fast_adler64_start(pdata,n); }
inline static uint32_t roll_hash_start(uint32_t*,const adler_data_t* pdata,size_t n){
                                       return fast_adler32_start(pdata,n); }
inline static uint64_t roll_hash_roll(uint64_t adler,size_t blockSize,
                                      adler_data_t out_data,adler_data_t in_data){
                                        return fast_adler64_roll(adler,blockSize,out_data,in_data); }
inline static uint32_t roll_hash_roll(uint32_t adler,size_t blockSize,
                                      adler_data_t out_data,adler_data_t in_data){
                                        return fast_adler32_roll(adler,blockSize,out_data,in_data); }
#endif

#ifdef __cplusplus
extern "C" {
#endif
    
    typedef struct TSameNewDataPair{
        uint32_t  curIndex;
        uint32_t  sameIndex; // sameIndex < curIndex;
    } TSameNewDataPair;
    
    #define kPartStrongChecksumByteSize       8
    
typedef struct TNewDataSyncInfo{
    const char*             compressType;
    const char*             strongChecksumType;
    uint32_t                kStrongChecksumByteSize;
    uint32_t                kMatchBlockSize;
    uint32_t                samePairCount;
    uint8_t                 is32Bit_rollHash;
    hpatch_StreamPos_t      newDataSize;
    hpatch_StreamPos_t      newSyncDataSize;
    hpatch_StreamPos_t      newSyncInfoSize;
    unsigned char*          infoPartChecksum; //this info data's strongChecksum
    TSameNewDataPair*       samePairList;
    uint32_t*               savedSizes;
    void*                   rollHashs;
    unsigned char*          partChecksums;
    
    void*                   _import;
    hpatch_TChecksum*       _strongChecksumPlugin;
    hpatch_TDecompress*     _decompressPlugin;
} TNewDataSyncInfo;

hpatch_inline static void
TNewDataSyncInfo_init(TNewDataSyncInfo* self) { memset(self,0,sizeof(*self)); }

hpatch_inline static
hpatch_StreamPos_t getBlockCount(hpatch_StreamPos_t newDataSize,uint32_t kMatchBlockSize){
                                    return (newDataSize+(kMatchBlockSize-1))/kMatchBlockSize; }

    hpatch_inline static
    unsigned int upper_ilog2(long double v){
        unsigned int bit=0;
        long double p=1;
        while (p<v){ ++bit; p*=2; }
        return bit;  }
    
    const int kAllowMaxHashClashBit = -48; // (1/2^48)
    static int estimateHashClashBit(hpatch_StreamPos_t newDataSize,
                                uint32_t kMatchBlockSize,bool isUse32bitRollHash=false){
        //clash=oldDataSize*(newDataSize/kMatchBlockSize)/2^64/2^64
        long double blockCount=(long double)getBlockCount(newDataSize,kMatchBlockSize);
        int cmpHashCountBit=upper_ilog2(newDataSize*blockCount);
        return cmpHashCountBit-(isUse32bitRollHash?32:64)-kPartStrongChecksumByteSize*8;
    }
hpatch_inline static
bool isCanUse32bitRollHash(hpatch_StreamPos_t newDataSize,uint32_t kMatchBlockSize){
    const bool isUse32bitRollHash=true;
    int clashBit=estimateHashClashBit(newDataSize,kMatchBlockSize,isUse32bitRollHash);
    return clashBit<=kAllowMaxHashClashBit;
}

hpatch_inline static
hpatch_StreamPos_t estimatePatchMemSize(hpatch_StreamPos_t newDataSize,
                                        uint32_t kMatchBlockSize,bool isUsedCompress){
    hpatch_StreamPos_t blockCount=getBlockCount(newDataSize,kMatchBlockSize);
    bool  isUse32bitRollHash=isCanUse32bitRollHash(newDataSize,kMatchBlockSize);
    hpatch_StreamPos_t bet=40;
    if (isUse32bitRollHash) bet-=4;
    if (isUsedCompress)     bet+=4;
    return bet*blockCount + 2*(hpatch_StreamPos_t)kMatchBlockSize;
}

hpatch_inline static
hpatch_StreamPos_t TNewDataSyncInfo_blockCount(const TNewDataSyncInfo* self) {
                                    return getBlockCount(self->newDataSize,self->kMatchBlockSize); }
    
hpatch_inline static
uint32_t TNewDataSyncInfo_newDataBlockSize(const TNewDataSyncInfo* self,uint32_t blockIndex){
    uint32_t blockCount=(uint32_t)TNewDataSyncInfo_blockCount(self);
    if (blockIndex+1!=blockCount)
        return self->kMatchBlockSize;
    else
        return (uint32_t)(self->newDataSize-self->kMatchBlockSize*blockIndex);
}
hpatch_inline static
uint32_t TNewDataSyncInfo_syncBlockSize(const TNewDataSyncInfo* self,uint32_t blockIndex){
    if (self->savedSizes)
        return self->savedSizes[blockIndex];
    else
        return TNewDataSyncInfo_newDataBlockSize(self,blockIndex);
}


hpatch_inline static
void toPartChecksum(unsigned char* out_partChecksum,
                    const unsigned char* checksum,size_t checksumByteSize){
    assert((checksumByteSize>=kPartStrongChecksumByteSize)
           &&(checksumByteSize%kPartStrongChecksumByteSize==0));
    assert(sizeof(hpatch_uint64_t)==kPartStrongChecksumByteSize);
    const unsigned char* checksum_end=checksum+checksumByteSize;
    hpatch_uint64_t v; memcpy(&v,checksum,kPartStrongChecksumByteSize);
    checksum+=kPartStrongChecksumByteSize;
    while (checksum!=checksum_end) {
        hpatch_uint64_t c; memcpy(&c,checksum,kPartStrongChecksumByteSize);
        checksum+=kPartStrongChecksumByteSize;
        v^=c;
    }
    memcpy(out_partChecksum,&v,kPartStrongChecksumByteSize);
}
    

#ifdef __cplusplus
}
#endif
#endif //sync_client_type_h
