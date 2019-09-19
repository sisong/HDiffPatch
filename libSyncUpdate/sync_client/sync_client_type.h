//  sync_client_type.h
//  sync_client
//
//  Created by housisong on 2019-09-17.
//  Copyright © 2019 sisong. All rights reserved.
#ifndef sync_client_type_h
#define sync_client_type_h
#include "../../libHDiffPatch/HPatch/patch_types.h"
#ifdef __cplusplus
extern "C" {
#endif
    
    typedef struct TSameNewDataPair{
        uint32_t  curIndex;
        uint32_t  sameIndex; // sameIndex < curIndex;
    } TSameNewDataPair;
    
    #define kPartStrongChecksumByteSize       8
    #define kInsureStrongChecksumBlockSize  128
    
typedef struct TNewDataSyncInfo{
    const char*             compressType;
    const char*             strongChecksumType;
    unsigned char           kStrongChecksumByteSize;
    uint32_t                kMatchBlockSize;
    uint32_t                samePairCount;
    hpatch_StreamPos_t      newDataSize;
    hpatch_StreamPos_t      newSyncDataSize;
    unsigned char*          newData_strongChecksum;
    unsigned char*          info_strongChecksum; //this info data's strongChecksum
    TSameNewDataPair*       samePairList;
    uint32_t*               compressedSizes;
    uint32_t*               rollHashs;
    unsigned char*          partStrongChecksums;
    unsigned char*          insureStrongChecksums;
    
    void*                   _import;
} TNewDataSyncInfo;

hpatch_inline static
void TNewDataSyncInfo_init(TNewDataSyncInfo* self) { memset(self,0,sizeof(*self)); }


hpatch_inline static hpatch_StreamPos_t
    getBlockCount(hpatch_StreamPos_t newDataSize,uint32_t kMatchBlockSize){
        return (newDataSize+(kMatchBlockSize-1))/kMatchBlockSize; }
hpatch_inline static hpatch_StreamPos_t
    TNewDataSyncInfo_blockCount(const TNewDataSyncInfo* self) {
        return getBlockCount(self->newDataSize,self->kMatchBlockSize); }
hpatch_inline static hpatch_StreamPos_t
    TNewDataSyncInfo_insureBlockCount(const TNewDataSyncInfo* self) {
        return getBlockCount(TNewDataSyncInfo_blockCount(self),kInsureStrongChecksumBlockSize); }

static void toPartChecksum(unsigned char* out_partChecksum,
                           const unsigned char* checksum,size_t checksumByteSize){
    assert((checksumByteSize>kPartStrongChecksumByteSize)
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
