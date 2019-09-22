//  sync_client.cpp
//  sync_client
//
//  Created by housisong on 2019-09-18.
//  Copyright © 2019 sisong. All rights reserved.

#include "sync_client.h"
#include <algorithm> //sort, equal_range
#include "../../file_for_patch.h"
#include "../../libHDiffPatch/HDiff/private_diff/mem_buf.h"
#include "../../libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.h"
#include "../../libHDiffPatch/HDiff/private_diff/limit_mem_diff/bloom_filter.h"
#include "../../libHDiffPatch/HPatch/checksum_plugin.h"

#define  _ChecksumPlugin_md5
#include "../../checksum_plugin_demo.h"
using namespace hdiff_private;

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

int TNewDataSyncInfo_open_by_file(TNewDataSyncInfo* self,const char* newSyncInfoPath){
    hpatch_TFileStreamInput  newSyncInfo;
    hpatch_TFileStreamInput_init(&newSyncInfo);
    int rt;
    int result=kSyncClient_ok;
    int _inClear=0;
    check(hpatch_TFileStreamInput_open(&newSyncInfo,newSyncInfoPath), kSyncClient_newSyncInfoOpenError);

    rt=TNewDataSyncInfo_open(self,&newSyncInfo.base);
    check(rt==kSyncClient_ok,rt);
clear:
    _inClear=1;
    check(hpatch_TFileStreamInput_close(&newSyncInfo), kSyncClient_newSyncInfoCloseError);
    return result;
}



struct TIndex_comp{
    inline TIndex_comp(const roll_uint_t* _blocks) :blocks(_blocks){ }
    template<class TIndex>
    inline bool operator()(TIndex x,TIndex y)const{
        return blocks[x]<blocks[y];
    }
    private:
    const roll_uint_t* blocks;
};

struct TDigest_comp{
    inline explicit TDigest_comp(const roll_uint_t* _blocks):blocks(_blocks){ }
    struct TDigest{
        roll_uint_t value;
        inline explicit TDigest(roll_uint_t _value):value(_value){}
    };
    template<class TIndex> inline
    bool operator()(const TIndex& x,const TDigest& y)const { return blocks[x]<y.value; }
    template<class TIndex> inline
    bool operator()(const TDigest& x,const TIndex& y)const { return x.value<blocks[y]; }
    template<class TIndex> inline
    bool operator()(const TIndex& x, const TIndex& y)const { return blocks[x]<blocks[y]; }
protected:
    const roll_uint_t* blocks;
};


struct TOldDataCache {
    TOldDataCache(const hpatch_TStreamInput* oldStream,uint32_t kMatchBlockSize,
                  hpatch_TChecksum* strongChecksumPlugin):m_checksumHandle(0){
        //size_t cacheSize=kMatchBlockSize*2;
        //cacheSize=(cacheSize>=kMinCacheBufSize)?cacheSize:kMinCacheBufSize;
        m_kMatchBlockSize=kMatchBlockSize;
        m_strongChecksumPlugin=strongChecksumPlugin;
        m_checksumHandle=strongChecksumPlugin->open(strongChecksumPlugin);
        m_strongChecksum_buf.realloc((size_t)strongChecksumPlugin->checksumByteSize());
        m_cache.realloc(oldStream->streamSize);
        oldStream->read(oldStream,0,m_cache.data(),m_cache.data_end());
        if (oldStream->streamSize>=kMatchBlockSize){
            m_cur=m_cache.data();
            m_roolHash=roll_hash_start(m_cur,kMatchBlockSize);
        }else{
            m_cur=m_cache.data_end();
        }
    }
    ~TOldDataCache(){
        if (m_checksumHandle)
            m_strongChecksumPlugin->close(m_strongChecksumPlugin,m_checksumHandle);
    }
    inline bool isEnd()const{ return m_cur==m_cache.data_end(); }
    inline roll_uint_t hashValue()const{ return m_roolHash; }
    inline void roll(){
        const TByte* curIn=m_cur+m_kMatchBlockSize;
        if (curIn!=m_cache.data_end()){
            m_roolHash=roll_hash_roll(m_roolHash,m_kMatchBlockSize,*m_cur,*curIn);
            ++m_cur;
        }else{
            m_cur=m_cache.data_end();
        }
    }
    inline const TByte* calcPartStrongChecksum(){
        return _calcPartStrongChecksum(m_cur,m_kMatchBlockSize);
    }
    inline const TByte* calcLastPartStrongChecksum(size_t lastNewNodeSize){
        return _calcPartStrongChecksum(m_cache.data_end()-lastNewNodeSize,lastNewNodeSize);
    }
    inline hpatch_StreamPos_t curOldPos()const{
        return m_cur-m_cache.data();
    }
    TAutoMem                m_cache;
    TAutoMem                m_strongChecksum_buf;
    const TByte*            m_cur;
    roll_uint_t             m_roolHash;
    uint32_t                m_kMatchBlockSize;
    hpatch_TChecksum*       m_strongChecksumPlugin;
    hpatch_checksumHandle   m_checksumHandle;
    
    inline const TByte* _calcPartStrongChecksum(const TByte* buf,size_t bufSize){
        m_strongChecksumPlugin->begin(m_checksumHandle);
        m_strongChecksumPlugin->append(m_checksumHandle,buf,buf+bufSize);
        m_strongChecksumPlugin->end(m_checksumHandle,m_strongChecksum_buf.data(),
                                    m_strongChecksum_buf.data_end());
        toPartChecksum(m_strongChecksum_buf.data(),m_strongChecksum_buf.data(),m_strongChecksum_buf.size());
        return m_strongChecksum_buf.data();
    }
};

static const hpatch_StreamPos_t kBlockType_needSync =~(hpatch_StreamPos_t)0;

//cpp code used stdexcept
void matchNewDataInOld(const hpatch_TStreamInput* oldStream,hpatch_StreamPos_t* out_newDataPoss,
                       hpatch_TChecksum* strongChecksumPlugin,const TNewDataSyncInfo* newSyncInfo,
                       uint32_t* out_needSyncCount){
    uint32_t kMatchBlockSize=newSyncInfo->kMatchBlockSize;
    uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);

    TAutoMem _mem(kBlockCount*sizeof(uint32_t));
    uint32_t* sorted_newIndexs=(uint32_t*)_mem.data();
    TBloomFilter<roll_uint_t> filter; filter.init(kBlockCount);
    for (uint32_t i=0; i<kBlockCount; ++i){
        out_newDataPoss[i]=kBlockType_needSync;
        sorted_newIndexs[i]=i;
        filter.insert(newSyncInfo->rollHashs[i]);
    }
    {
        TIndex_comp icomp(newSyncInfo->rollHashs);
        std::sort(sorted_newIndexs,sorted_newIndexs+kBlockCount,icomp);
    }

    TOldDataCache oldData(oldStream,kMatchBlockSize,strongChecksumPlugin);
    TDigest_comp dcomp(newSyncInfo->rollHashs);
    uint32_t matchedCount=0;
    for (;!oldData.isEnd();oldData.roll()) {
        roll_uint_t digest=oldData.hashValue();
        if (!filter.is_hit(digest)) continue;
        
        typename TDigest_comp::TDigest digest_value(digest);
        std::pair<const uint32_t*,const uint32_t*>
            range=std::equal_range(sorted_newIndexs,sorted_newIndexs+kBlockCount,digest_value,dcomp);
        if (range.first!=range.second){
            const TByte* oldPartStrongChecksum=0;
            do {
                uint32_t newBlockIndex=*range.first;
                if (out_newDataPoss[newBlockIndex]==kBlockType_needSync){
                    if (oldPartStrongChecksum==0)
                        oldPartStrongChecksum=oldData.calcPartStrongChecksum();
                    const TByte* newPairStrongChecksum = newSyncInfo->partChecksums
                                                    + newBlockIndex*(size_t)kPartStrongChecksumByteSize;
                    if (0==memcmp(oldPartStrongChecksum,newPairStrongChecksum,kPartStrongChecksumByteSize)){
                        out_newDataPoss[newBlockIndex]=oldData.curOldPos();
                        matchedCount++;
                    }
                }
                ++range.first;
            }while (range.first!=range.second);
        }
    }
    _mem.clear();
    
    //try match last newBlock data
    if ((kBlockCount>0)&&(out_newDataPoss[kBlockCount-1]==kBlockType_needSync)){
        size_t lastNewNodeSize=(size_t)(newSyncInfo->newDataSize%kMatchBlockSize);
        if ((lastNewNodeSize>0)&&(oldStream->streamSize>=lastNewNodeSize)){
            uint32_t newBlockIndex=kBlockCount-1;
            const TByte* oldPartStrongChecksum = oldData.calcLastPartStrongChecksum(lastNewNodeSize);
            const TByte* newPairStrongChecksum = newSyncInfo->partChecksums
                                                + newBlockIndex*(size_t)kPartStrongChecksumByteSize;
            if (0==memcmp(oldPartStrongChecksum,newPairStrongChecksum,kPartStrongChecksumByteSize)){
                out_newDataPoss[newBlockIndex]=oldStream->streamSize-lastNewNodeSize;
                matchedCount++;
            }
        }
    }
    *out_needSyncCount=kBlockCount-matchedCount;
}

int sync_patch(const hpatch_TStreamOutput* out_newStream,
               const TNewDataSyncInfo*     newSyncInfo,
               const hpatch_TStreamInput*  oldStream, ISyncPatchListener* listener){
    //todo: select checksum\decompressPlugin
    //assert(listener!=0);

    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    const uint32_t kMatchBlockSize=newSyncInfo->kMatchBlockSize;
    hpatch_TChecksum* strongChecksumPlugin=&md5ChecksumPlugin;
    hpatch_checksumHandle checksumSync=0;
    hpatch_TDecompress* decompressPlugin=0;
    TByte* dataBuf=0;
    uint32_t needSyncCount=0;
    int result=kSyncClient_ok;
    int _inClear=0;
    //match in oldData
    hpatch_StreamPos_t* newDataPoss=(hpatch_StreamPos_t*)malloc(kBlockCount*sizeof(hpatch_StreamPos_t));
    check(newDataPoss!=0,kSyncClient_memError);
    checksumSync=strongChecksumPlugin->open(strongChecksumPlugin);
    check(checksumSync!=0,kSyncClient_strongChecksumOpenError);
    try{
        matchNewDataInOld(oldStream,newDataPoss,strongChecksumPlugin,newSyncInfo,&needSyncCount);
        printf("needSyncCount: %d / %d =%.4f\n",needSyncCount,kBlockCount,(double)needSyncCount/kBlockCount);
    }catch(...){
        result=kSyncClient_matchNewDataInOldError;
    }
    check(result==kSyncClient_ok,result);
    
    if ((listener)&&(listener->needSyncMsg)){//send msg: all need sync block
        hpatch_StreamPos_t posInNewSyncData=0;
        for (uint32_t i=0; i<kBlockCount; ++i) {
            uint32_t syncSize=TNewDataSyncInfo_syncBlockSize(newSyncInfo,i);
            if (newDataPoss[i]==kBlockType_needSync)
                listener->needSyncMsg(listener,needSyncCount,posInNewSyncData,syncSize);
            posInNewSyncData+=syncSize;
        }
    }
    
    {//write newData
        hpatch_StreamPos_t posInNewSyncData=0;
        hpatch_StreamPos_t outNewDataPos=0;
        TByte*             checksumSync_buf=0;
        
        size_t _memSize=kMatchBlockSize*(decompressPlugin?2:1)+newSyncInfo->kStrongChecksumByteSize;
        dataBuf=(TByte*)malloc(_memSize);
        check(dataBuf!=0,kSyncClient_memError);
        checksumSync_buf=dataBuf+_memSize-newSyncInfo->kStrongChecksumByteSize;
        for (uint32_t i=0; i<kBlockCount; ++i) {
            uint32_t syncSize=TNewDataSyncInfo_syncBlockSize(newSyncInfo,i);
            uint32_t newDataSize=TNewDataSyncInfo_newDataBlockSize(newSyncInfo,i);
            if (newDataPoss[i]==kBlockType_needSync){ //sync
                TByte* buf=decompressPlugin?(dataBuf+kMatchBlockSize):dataBuf;
                if ((out_newStream)||(listener)){
                    check(listener->readSyncData(listener,dataBuf,posInNewSyncData,syncSize),
                          kSyncClient_readSyncDataError);
                    if (decompressPlugin){
                        check(hpatch_deccompress_mem(decompressPlugin,buf,buf+syncSize,
                                                     dataBuf,dataBuf+newDataSize),kSyncClient_decompressError);
                    }
                    //checksum
                    strongChecksumPlugin->begin(checksumSync);
                    strongChecksumPlugin->append(checksumSync,dataBuf,dataBuf+newDataSize);
                    strongChecksumPlugin->end(checksumSync,checksumSync_buf,
                                              checksumSync_buf+newSyncInfo->kStrongChecksumByteSize);
                    toPartChecksum(checksumSync_buf,checksumSync_buf,newSyncInfo->kStrongChecksumByteSize);
                    check(0==memcmp(checksumSync_buf,
                                    newSyncInfo->partChecksums+i*(size_t)kPartStrongChecksumByteSize,
                                    kPartStrongChecksumByteSize),kSyncClient_checksumSyncDataError);
                }
            }else{//copy from old
                check(oldStream->read(oldStream,newDataPoss[i],dataBuf,dataBuf+newDataSize),
                      kSyncClient_readOldDataError);
            }
            if (out_newStream){//write
                check(out_newStream->write(out_newStream,outNewDataPos,dataBuf,
                                           dataBuf+newDataSize), kSyncClient_writeNewDataError);
            }
            outNewDataPos+=newDataSize;
            posInNewSyncData+=syncSize;
        }
        assert(outNewDataPos==newSyncInfo->newDataSize);
        assert(posInNewSyncData==newSyncInfo->newSyncDataSize);
    }

clear:
    _inClear=1;
    if (checksumSync) strongChecksumPlugin->close(strongChecksumPlugin,checksumSync);
    if (dataBuf) free(dataBuf);
    if (newDataPoss) free(newDataPoss);
    return result;
}

int sync_patch_by_file(const char* out_newPath,
                       const char* newSyncInfoPath,
                       const char* oldPath, ISyncPatchListener* listener){
    int result=kSyncClient_ok;
    int _inClear=0;
    TNewDataSyncInfo         newSyncInfo;
    hpatch_TFileStreamInput  oldData;
    hpatch_TFileStreamOutput out_newData;
    
    TNewDataSyncInfo_init(&newSyncInfo);
    hpatch_TFileStreamInput_init(&oldData);
    hpatch_TFileStreamOutput_init(&out_newData);
    result=TNewDataSyncInfo_open_by_file(&newSyncInfo,newSyncInfoPath);
    check(result==kSyncClient_ok,result);
    check(hpatch_TFileStreamInput_open(&oldData,oldPath),kSyncClient_oldFileOpenError);
    check(hpatch_TFileStreamOutput_open(&out_newData,out_newPath,(hpatch_StreamPos_t)(-1)),
          kSyncClient_newFileCreateError);
    
    result=sync_patch(&out_newData.base,&newSyncInfo,&oldData.base,listener);
clear:
    _inClear=1;
    check(hpatch_TFileStreamOutput_close(&out_newData),kSyncClient_newFileCloseError);
    check(hpatch_TFileStreamInput_close(&oldData),kSyncClient_oldFileCloseError);
    TNewDataSyncInfo_close(&newSyncInfo);
    return result;
}
