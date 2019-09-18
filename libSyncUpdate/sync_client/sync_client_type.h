//  sync_client_type.h
//  sync_client
//
//  Created by housisong on 2019-09-17.
//  Copyright © 2019 sisong. All rights reserved.
#ifndef sync_client_type_h
#define sync_client_type_h
#include "../../ApkDiffPatch/HDiffPatch/libHDiffPatch/HPatch/patch_types.h"
#ifdef __cplusplus
extern "C" {
#endif
    
    typedef enum TRoolHashType{
        kRollHash_fadler32=0,
        kRollHash_fadler64
    } TRoolHashType;
    typedef struct TSameNewDataPair{
        uint32_t  curIndex;
        uint32_t  sameIndex; // sameIndex < curIndex;
    } TSameNewDataPair;
    
typedef struct TNewDataSyncInfo{
    const char*             compressType;
    const char*             strongChecksumType;
    unsigned char           kStrongChecksumByteSize;
    unsigned char           kStrongChecksumsSavedByteSize; // kStrongChecksumsSavedByteSize<=kStrongChecksumByteSize
    TRoolHashType           kRollHashType;
    uint32_t                kMatchBlockSize;
    uint32_t                samePairCount;
    hpatch_StreamPos_t      newDataSize;
    hpatch_StreamPos_t      newSyncDataSize;
    unsigned char*          newData_strongChecksum;
    unsigned char*          info_strongChecksum; //this info data's strongChecksum
    TSameNewDataPair*       samePairList;
    uint32_t*               compressedSizes;
    void*                   rollHashs; // uint32_t* or uint64_t*
    unsigned char*          strongChecksums;
    
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

#ifdef __cplusplus
}
#endif
#endif //sync_client_type_h
