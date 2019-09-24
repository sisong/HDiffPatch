//  sync_client_type.h
//  sync_client
//
//  Created by housisong on 2019-09-17.
//  Copyright © 2019 sisong. All rights reserved.
#ifndef sync_client_type_h
#define sync_client_type_h
#include "../../libHDiffPatch/HPatch/patch_types.h"
#include "../../libHDiffPatch/HPatch/checksum_plugin.h"
#ifdef __cplusplus
extern "C" {
#endif
    
    typedef struct TSameNewDataPair{
        uint32_t  curIndex;
        uint32_t  sameIndex; // sameIndex < curIndex;
    } TSameNewDataPair;
    
    #define kPartStrongChecksumByteSize       8
    
    typedef uint64_t roll_uint_t;
    #define roll_hash_start  fast_adler64_start
    #define roll_hash_roll   fast_adler64_roll
    
    
typedef struct TNewDataSyncInfo{
    const char*             compressType;
    const char*             strongChecksumType;
    uint32_t                kStrongChecksumByteSize;
    uint32_t                kMatchBlockSize;
    uint32_t                samePairCount;
    hpatch_StreamPos_t      newDataSize;
    hpatch_StreamPos_t      newSyncDataSize;
    hpatch_StreamPos_t      newSyncInfoSize;
    unsigned char*          infoPartChecksum; //this info data's strongChecksum
    TSameNewDataPair*       samePairList;
    uint32_t*               savedSizes;
    roll_uint_t*            rollHashs;
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
hpatch_StreamPos_t TNewDataSyncInfo_blockCount(const TNewDataSyncInfo* self) {
                                    return getBlockCount(self->newDataSize,self->kMatchBlockSize); }
    
hpatch_inline static
uint32_t TNewDataSyncInfo_newDataBlockSize(const TNewDataSyncInfo* self,uint32_t blockIndex){
    if (blockIndex+1<TNewDataSyncInfo_blockCount(self))
        return self->kMatchBlockSize;
    else
        return (uint32_t)(self->newDataSize%self->kMatchBlockSize);
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
