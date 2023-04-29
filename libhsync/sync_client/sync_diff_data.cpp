//  sync_diff_data.cpp
//  sync_client
//  Created by housisong on 2020-02-09.
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
#include "sync_diff_data.h"
#include <stdlib.h>
#include <vector>
#include <string>
#include <stdexcept>
#include "match_in_old.h"
#include "../../libHDiffPatch/HPatch/patch_private.h"
typedef unsigned char TByte;
namespace sync_private{

#define _pushV(vec,uv,stag,kTagBit) \
    do{ TByte _buf[hpatch_kMaxPackedUIntBytes]; \
        TByte* cur=_buf; \
        if (!hpatch_packUIntWithTag(&cur,_buf+sizeof(_buf),uv,stag,kTagBit)) \
            throw std::runtime_error("_packMatchedPoss() _pushV()"); \
        vec.insert(vec.end(),_buf,cur); \
    }while(0)

static const char* kSyncDiffType="HSynd23";
static const size_t kNeedSyncMaxRleValue=(1<<11)-1;

#define _pushNeedSync(){ \
    while (needSyncCount>0){            \
        hpatch_StreamPos_t v=(needSyncCount<=(kNeedSyncMaxRleValue+1))? \
                              needSyncCount:(kNeedSyncMaxRleValue+1);   \
        _pushV(out_possBuf,v-1,0,1);    \
        needSyncCount-=v;               \
    } }

static void _packMatchedPoss(const hpatch_StreamPos_t* newBlockDataInOldPoss,uint32_t kBlockCount,
                             uint32_t kBlockSize,std::vector<TByte>& out_possBuf){
    hpatch_StreamPos_t backv=0;
    hpatch_StreamPos_t needSyncCount=0;
    for (uint32_t i=0; i<kBlockCount; ++i) {
        hpatch_StreamPos_t v=newBlockDataInOldPoss[i];
        if (v!=kBlockType_needSync){
            _pushNeedSync();
            if (v>=backv)
                _pushV(out_possBuf,(hpatch_StreamPos_t)(v-backv),1,1);
            else
                _pushV(out_possBuf,(hpatch_StreamPos_t)(backv-v)+kNeedSyncMaxRleValue,0,1);
            backv=v+kBlockSize;
        }else{
            ++needSyncCount;
        }
    }
    _pushNeedSync();
}

hpatch_BOOL _saveSyncDiffData(const hpatch_StreamPos_t* newBlockDataInOldPoss,uint32_t kBlockCount,uint32_t kBlockSize,
                              hpatch_StreamPos_t oldDataSize,const TByte* newDataCheckChecksum,size_t checksumByteSize,
                              const hpatch_TStreamOutput* out_diffStream,TSyncDiffType diffType,hpatch_StreamPos_t* out_diffDataPos){
    assert(checksumByteSize==(uint32_t)checksumByteSize);
    try {
        std::vector<TByte> possBuf;
        if (diffType!=kSyncDiff_data)
            _packMatchedPoss(newBlockDataInOldPoss,kBlockCount,kBlockSize,possBuf);
        
        std::vector<TByte> headBuf;
        headBuf.insert(headBuf.end(),kSyncDiffType,kSyncDiffType+strlen(kSyncDiffType)+1); //with '\0'
        _pushV(headBuf,diffType,0,0);
        _pushV(headBuf,kBlockCount,0,0);
        _pushV(headBuf,kBlockSize,0,0);
        _pushV(headBuf,oldDataSize,0,0);
        _pushV(headBuf,checksumByteSize,0,0);
        _pushV(headBuf,possBuf.size(),0,0);
        hpatch_StreamPos_t curWritePos=0;
        if (!out_diffStream->write(out_diffStream,curWritePos,&headBuf[0],
                                   &headBuf[0]+headBuf.size())) return hpatch_FALSE;
        curWritePos+=headBuf.size();
        if (!out_diffStream->write(out_diffStream,curWritePos,newDataCheckChecksum,
                                   newDataCheckChecksum+checksumByteSize)) return hpatch_FALSE;
        curWritePos+=checksumByteSize;
        if (!possBuf.empty()){
            if (!out_diffStream->write(out_diffStream,curWritePos,&possBuf[0],
                                       &possBuf[0]+possBuf.size())) return hpatch_FALSE;
            curWritePos+=possBuf.size();
        }
        *out_diffDataPos=curWritePos;
        return hpatch_TRUE;
    } catch (...) {
        return hpatch_FALSE;
    }
}


    
TSyncDiffData::TSyncDiffData():in_diffStream(0){
    memset((IReadSyncDataListener*)this,0,sizeof(IReadSyncDataListener));
    memset(&this->localPoss,0,sizeof(this->localPoss));
}
TSyncDiffData::~TSyncDiffData(){
    if (this->localPoss.packedOldPoss)
        free((void*)this->localPoss.packedOldPoss);
}

static hpatch_BOOL _TSyncDiffData_readSyncData(IReadSyncDataListener* listener,uint32_t blockIndex,
                                               hpatch_StreamPos_t posInNewSyncData,hpatch_StreamPos_t posInNeedSyncData,
                                               unsigned char* out_syncDataBuf,uint32_t syncDataSize){
    TSyncDiffData* self=(TSyncDiffData*)listener->readSyncDataImport;
    if (self->diffDataPos0+posInNeedSyncData+syncDataSize>self->in_diffStream->streamSize)
        return hpatch_FALSE;
    hpatch_BOOL result=self->in_diffStream->read(self->in_diffStream,self->diffDataPos0+posInNeedSyncData,
                                                 out_syncDataBuf,out_syncDataBuf+syncDataSize);
    return result;
}
    
#define _loadV(dst,dstT,pclip) do{ \
    hpatch_StreamPos_t  v;  \
    if (!_TStreamCacheClip_unpackUIntWithTag(pclip,&v,0)) return hpatch_FALSE;  \
    if (v!=(dstT)v) return hpatch_FALSE;    \
    dst=(dstT)v;  }while(0)

static hpatch_BOOL _loadPoss(TSyncDiffLocalPoss* self,const hpatch_TStreamInput* in_diffStream,
                             TSyncDiffType* out_diffType,hpatch_StreamPos_t* out_diffDataPos){
    assert(self->packedOldPoss==0);
    size_t              packedOldPossSize;
    TStreamCacheClip    clip;
    TByte               _cache[hpatch_kStreamCacheSize];
    _TStreamCacheClip_init(&clip,in_diffStream,*out_diffDataPos,in_diffStream->streamSize,
                           _cache,hpatch_kStreamCacheSize);
    { // type
        char saved_type[hpatch_kMaxPluginTypeLength+1];
        if (!_TStreamCacheClip_readType_end(&clip,'\0',saved_type)) return hpatch_FALSE;
        if (0!=strcmp(saved_type,kSyncDiffType)) return hpatch_FALSE;//unsupport type
        _loadV(*out_diffType,TSyncDiffType,&clip);
        if ((*out_diffType)>_kSyncDiff_TYPE_MAX_) return hpatch_FALSE;//unsupport diff type
    }
    _loadV(self->kBlockCount,uint32_t,&clip);
    _loadV(self->kBlockSize,uint32_t,&clip);
    _loadV(self->oldDataSize,hpatch_StreamPos_t,&clip);
    _loadV(self->checksumByteSize,uint32_t,&clip);
    if (self->checksumByteSize>_kStrongChecksumByteSize_max_limit) return hpatch_FALSE;
    _loadV(packedOldPossSize,size_t,&clip);
    if (packedOldPossSize>=_TStreamCacheClip_leaveSize(&clip)) return hpatch_FALSE;
    {  // packedOldPoss data
        self->packedOldPoss=(TByte*)malloc(packedOldPossSize+self->checksumByteSize);
        if (!self->packedOldPoss) return hpatch_FALSE;
        self->packedOldPossEnd=self->packedOldPoss+packedOldPossSize;
        if (!_TStreamCacheClip_readDataTo(&clip,(TByte*)self->savedNewDataCheckChecksum,
                                          (TByte*)self->savedNewDataCheckChecksum+self->checksumByteSize)) return hpatch_FALSE;
        if (!_TStreamCacheClip_readDataTo(&clip,(TByte*)self->packedOldPoss,
                                          (TByte*)self->packedOldPossEnd)) return hpatch_FALSE;
    }
    *out_diffDataPos=_TStreamCacheClip_readPosOfSrcStream(&clip);
    return hpatch_TRUE;
}

static hpatch_BOOL _syncDiffLocalPoss_nextOldPos(TSyncDiffLocalPoss* self,hpatch_StreamPos_t* out_oldPos){
    if (self->packedNeedSyncCount>0){
        --self->packedNeedSyncCount;
        *out_oldPos=kBlockType_needSync;
        return hpatch_TRUE;
    }
    if (self->packedOldPoss<self->packedOldPossEnd){
        hpatch_StreamPos_t v;
        size_t tag=(*self->packedOldPoss)>>7;
        if (!hpatch_unpackUIntWithTag(&self->packedOldPoss,self->packedOldPossEnd,&v,1)) return hpatch_FALSE;
        if (tag==0){
            if (v<=kNeedSyncMaxRleValue){
                self->packedNeedSyncCount=(uint32_t)v;
                *out_oldPos=kBlockType_needSync;
            }else{
                if (self->backPos<(v-kNeedSyncMaxRleValue)) return hpatch_FALSE;
                v=self->backPos-(v-kNeedSyncMaxRleValue);
                *out_oldPos=v;
                self->backPos=v+self->kBlockSize;
            }
        }else{ //tag==1
            v+=self->backPos;
            *out_oldPos=v;
            self->backPos=v+self->kBlockSize;
        }
        return hpatch_TRUE;
    }else{
        return hpatch_FALSE;
    }
}
    
    static hpatch_inline hpatch_BOOL _syncDiffLocalPoss_isFinish(const TSyncDiffLocalPoss* self){
        return (self->packedNeedSyncCount==0)&&(self->packedOldPoss==self->packedOldPossEnd); }

hpatch_BOOL _TSyncDiffData_readOldPoss(TSyncDiffData* self,hpatch_StreamPos_t* out_newBlockDataInOldPoss,
                                       uint32_t kBlockCount,uint32_t kBlockSize,hpatch_StreamPos_t oldDataSize,
                                       const TByte* newDataCheckChecksum,size_t checksumByteSize){
    TSyncDiffLocalPoss localPoss=self->localPoss;
    //check
    if (localPoss.kBlockCount!=kBlockCount) return hpatch_FALSE;
    if (localPoss.kBlockSize!=kBlockSize) return hpatch_FALSE;
    if (localPoss.kBlockSize!=kBlockSize) return hpatch_FALSE;
    if (localPoss.oldDataSize!=oldDataSize) return hpatch_FALSE;
    if (localPoss.checksumByteSize!=checksumByteSize) return hpatch_FALSE;
    if (0!=memcmp(localPoss.savedNewDataCheckChecksum,newDataCheckChecksum,checksumByteSize))
        return hpatch_FALSE;
    if (self->diffType==kSyncDiff_data)
        return hpatch_TRUE;

    //read poss
    for (uint32_t i=0;i<kBlockCount; ++i){
        hpatch_StreamPos_t pos;
        if (!_syncDiffLocalPoss_nextOldPos(&localPoss,&pos)) return hpatch_FALSE;
        if ((pos==kBlockType_needSync)|(pos<oldDataSize))
            out_newBlockDataInOldPoss[i]=pos;
        else
            return hpatch_FALSE;
    }
    return 0!=_syncDiffLocalPoss_isFinish(&localPoss);
}

hpatch_BOOL _TSyncDiffData_load(TSyncDiffData* self,const hpatch_TStreamInput* in_diffStream){
    assert(self->readSyncDataImport==0);
    assert(self->localPoss.packedOldPoss==0);
    self->readSyncDataImport=self;
    self->readSyncData=_TSyncDiffData_readSyncData;
    self->in_diffStream=in_diffStream;
    self->diffDataPos0=0;
    if (!_loadPoss(&self->localPoss,in_diffStream,&self->diffType,&self->diffDataPos0))
        return hpatch_FALSE;
    return hpatch_TRUE;
}

} //namespace sync_private
