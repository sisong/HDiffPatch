//  sync_client.cpp
//  sync_client
//
//  Created by housisong on 2019-09-18.
//  Copyright © 2019 sisong. All rights reserved.

#include "sync_client.h"
#include <map>
#include "../../file_for_patch.h"
#include "../../libHDiffPatch/HDiff/private_diff/mem_buf.h"

#define check(v,errorCode) \
    do{ if (!(v)) { if (result==kSyncClient_ok) result=errorCode; \
                    if (!_inClear) goto clear; } }while(0)

int TNewDataSyncInfo_open(TNewDataSyncInfo* self,const hpatch_TStreamInput* newSyncInfo){
    assert(false);
    return -1;
}

void TNewDataSyncInfo_close(TNewDataSyncInfo* self){
    //todo:
    assert(false);
    TNewDataSyncInfo_init(self);
}

int TNewDataSyncInfo_openByFile(TNewDataSyncInfo* self,const char* newSyncInfoPath){
    hpatch_TFileStreamInput  newSyncInfo;
    hpatch_TFileStreamInput_init(&newSyncInfo);
    int rt;
    int result=kSyncClient_ok;
    int _inClear=0;
    check(hpatch_TFileStreamInput_open(&newSyncInfo,newSyncInfoPath), kSyncClient_infoFileOpenError);

    rt=TNewDataSyncInfo_open(self,&newSyncInfo.base);
    check(rt==kSyncClient_ok,rt);
clear:
    _inClear=1;
    check(hpatch_TFileStreamInput_close(&newSyncInfo), kSyncClient_infoFileCloseError);
    return result;
}


struct TOldDataCache {
    TOldDataCache(const hpatch_TStreamInput* oldStream,uint32_t kBlockSize){
        
    }
};


static const hpatch_StreamPos_t KNullPos = ~(hpatch_StreamPos_t)0;
static const uint32_t KNullIndex = ~(uint32_t)0;

int sync_patch(const hpatch_TStreamInput*  oldStream,
               const hpatch_TStreamOutput* out_newStream,
               const TNewDataSyncInfo* newInfo,ISyncPatchListener* listener){
    uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newInfo);
    uint32_t kBlockSize=newInfo->kMatchBlockSize;
    hdiff_private::TAutoMem _mem(kBlockCount*sizeof(hpatch_StreamPos_t));
    hpatch_StreamPos_t* newDataPos=(hpatch_StreamPos_t*)_mem.data();
    const hpatch_StreamPos_t newSyncDataSize=newInfo->newSyncDataSize;
    const hpatch_StreamPos_t oldDataSize=oldStream->streamSize;
    const hpatch_StreamPos_t newDataSize=newInfo->newDataSize;
    //[0,newSyncDataSize)  need from listener
    //[newSyncDataSize,newSyncDataSize+oldDataSize) need copy from oldStream

    std::map<uint64_t,uint32_t>  rollHashs;
    uint32_t curPair=0;
    for (uint32_t i=0; i<kBlockCount; ++i){
        newDataPos[i]=KNullPos;
        if ((curPair<newInfo->samePairCount)
            &&(i==newInfo->samePairList[curPair].curIndex)){ ++curPair; continue; }
        rollHashs[((uint64_t*)newInfo->rollHashs)[i]]=i;
    }
    
    hdiff_private::TAutoMem oldbuf(kBlockSize*2);
    
    return -1;
}
