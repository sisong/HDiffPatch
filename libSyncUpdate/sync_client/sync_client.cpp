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

static const size_t kMinCacheBufSize =256*(1<<10);

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


typedef uint32_t adler_uint_t;

struct TIndex_comp{
    inline TIndex_comp(const adler_uint_t* _blocks) :blocks(_blocks){ }
    template<class TIndex>
    inline bool operator()(TIndex x,TIndex y)const{
        return blocks[x]<blocks[y];
    }
    private:
    const adler_uint_t* blocks;
};

struct TDigest_comp{
    inline explicit TDigest_comp(const adler_uint_t* _blocks):blocks(_blocks){ }
    struct TDigest{
        adler_uint_t value;
        inline explicit TDigest(adler_uint_t _value):value(_value){}
    };
    template<class TIndex> inline
    bool operator()(const TIndex& x,const TDigest& y)const { return blocks[x]<y.value; }
    template<class TIndex> inline
    bool operator()(const TDigest& x,const TIndex& y)const { return x.value<blocks[y]; }
    template<class TIndex> inline
    bool operator()(const TIndex& x, const TIndex& y)const { return blocks[x]<blocks[y]; }
protected:
    const adler_uint_t* blocks;
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
            m_roolHash=fast_adler32_start(m_cur,kMatchBlockSize);
        }else{
            m_cur=m_cache.data_end();
        }
    }
    ~TOldDataCache(){
        if (m_checksumHandle)
            m_strongChecksumPlugin->close(m_strongChecksumPlugin,m_checksumHandle);
    }
    inline bool isEnd()const{ return m_cur==m_cache.data_end(); }
    inline uint32_t hashValue()const{ return m_roolHash; }
    inline void roll(){
        const TByte* curIn=m_cur+m_kMatchBlockSize;
        if (curIn!=m_cache.data_end()){
            m_roolHash=fast_adler32_roll(m_roolHash,m_kMatchBlockSize,*m_cur,*curIn);
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
    uint32_t                m_roolHash;
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


static const hpatch_StreamPos_t kNullPos =~(hpatch_StreamPos_t)0;

static int matchAllNewData(const hpatch_TStreamInput* oldStream,hpatch_StreamPos_t* out_newDataPoss,
                           hpatch_TChecksum* strongChecksumPlugin,const TNewDataSyncInfo* newSyncInfo){
    uint32_t kMatchBlockSize=newSyncInfo->kMatchBlockSize;
    uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);

    TAutoMem _mem(kBlockCount*sizeof(uint32_t));
    uint32_t* sorted_newIndexs=(uint32_t*)_mem.data();
    TBloomFilter<uint32_t> filter; filter.init(kBlockCount);
    for (uint32_t i=0; i<kBlockCount; ++i){
        out_newDataPoss[i]=kNullPos;
        sorted_newIndexs[i]=i;
        filter.insert(newSyncInfo->rollHashs[i]);
    }
    {
        TIndex_comp icomp(newSyncInfo->rollHashs);
        std::sort(sorted_newIndexs,sorted_newIndexs+kBlockCount,icomp);
    }

    TOldDataCache oldData(oldStream,kMatchBlockSize,strongChecksumPlugin);
    TDigest_comp dcomp(newSyncInfo->rollHashs);
    uint32_t _matchCount=0;
    for (;!oldData.isEnd();oldData.roll()) {
        uint32_t digest=oldData.hashValue();
        if (!filter.is_hit(digest)) continue;
        
        typename TDigest_comp::TDigest digest_value(digest);
        std::pair<const uint32_t*,const uint32_t*>
            range=std::equal_range(sorted_newIndexs,sorted_newIndexs+kBlockCount,digest_value,dcomp);
        if (range.first!=range.second){
            const TByte* oldPartStrongChecksum=0;
            do {
                uint32_t newBlockIndex=*range.first;
                if (out_newDataPoss[newBlockIndex]==kNullPos){
                    if (oldPartStrongChecksum==0)
                        oldPartStrongChecksum=oldData.calcPartStrongChecksum();
                    const TByte* newPairStrongChecksum = newSyncInfo->partStrongChecksums
                                                    + newBlockIndex*(size_t)kPartStrongChecksumByteSize;
                    if (0==memcmp(oldPartStrongChecksum,newPairStrongChecksum,kPartStrongChecksumByteSize)){
                        out_newDataPoss[newBlockIndex]=oldData.curOldPos();
                        _matchCount++;
                    }
                }
                ++range.first;
            }while (range.first!=range.second);
        }
    }
    _mem.clear();
    
    //try match last newBlock data
    if ((kBlockCount>0)&&(out_newDataPoss[kBlockCount-1]==kNullPos)){
        size_t lastNewNodeSize=(size_t)(newSyncInfo->newDataSize%kMatchBlockSize);
        if ((lastNewNodeSize>0)&&(oldStream->streamSize>=lastNewNodeSize)){
            uint32_t newBlockIndex=kBlockCount-1;
            const TByte* oldPartStrongChecksum = oldData.calcLastPartStrongChecksum(lastNewNodeSize);
            const TByte* newPairStrongChecksum = newSyncInfo->partStrongChecksums
                                                + newBlockIndex*(size_t)kPartStrongChecksumByteSize;
            if (0==memcmp(oldPartStrongChecksum,newPairStrongChecksum,kPartStrongChecksumByteSize)){
                out_newDataPoss[newBlockIndex]=oldStream->streamSize-lastNewNodeSize;
                _matchCount++;
            }
        }
    }
    
    uint32_t lostCount=kBlockCount-_matchCount;
    printf("\nlost: %d / %d =%.4f\n",lostCount,kBlockCount,(double)lostCount/kBlockCount);
    return kSyncClient_ok;
}

int sync_patch_by_info(const hpatch_TStreamInput* oldStream,const TNewDataSyncInfo* newSyncInfo,
                       const hpatch_TStreamOutput* out_newStream,ISyncPatchListener* listener){
    //todo: select checksum run
    
    uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    hpatch_TChecksum*      strongChecksumPlugin=&md5ChecksumPlugin;
    
    TAutoMem _mem(kBlockCount*sizeof(hpatch_StreamPos_t));
    hpatch_StreamPos_t* newDataPos=(hpatch_StreamPos_t*)_mem.data();
    
    matchAllNewData(oldStream,newDataPos,strongChecksumPlugin,newSyncInfo);
    //[0,newSyncDataSize)  need from listener
    //[newSyncDataSize,newSyncDataSize+oldDataSize) need copy from oldStream
    
    return kSyncClient_ok;
}

int sync_patch(const hpatch_TStreamInput* oldStream,const hpatch_TStreamInput* newSyncInfoStream,
               const hpatch_TStreamOutput* out_newStream,ISyncPatchListener* listener){
    int result=kSyncClient_ok;
    int _inClear=0;
    TNewDataSyncInfo newSyncInfo;
    TNewDataSyncInfo_init(&newSyncInfo);
    result=TNewDataSyncInfo_open(&newSyncInfo,newSyncInfoStream);
    check(result==kSyncClient_ok,result);
    result=sync_patch_by_info(oldStream,&newSyncInfo,out_newStream,listener);
clear:
    _inClear=1;
    TNewDataSyncInfo_close(&newSyncInfo);
    return result;
}

int sync_patch_by_file(const char* oldPath,const char* newSyncInfoPath,
                       const char* out_newPath,ISyncPatchListener* listener){
    //todo: error type
    int result=kSyncClient_ok;
    int _inClear=0;
    hpatch_TFileStreamInput  oldData;
    hpatch_TFileStreamInput  newSyncInfo;
    hpatch_TFileStreamOutput out_newData;
    
    hpatch_TFileStreamInput_init(&oldData);
    hpatch_TFileStreamInput_init(&newSyncInfo);
    hpatch_TFileStreamOutput_init(&out_newData);
    check(hpatch_TFileStreamInput_open(&oldData,oldPath),kSyncClient_TODO_Error);
    check(hpatch_TFileStreamInput_open(&newSyncInfo,newSyncInfoPath),kSyncClient_TODO_Error);
    check(hpatch_TFileStreamOutput_open(&out_newData,out_newPath,(hpatch_StreamPos_t)(-1)),
          kSyncClient_TODO_Error);
    
    result=sync_patch(&oldData.base,&newSyncInfo.base,&out_newData.base,listener);
clear:
    _inClear=1;
    check(hpatch_TFileStreamOutput_close(&out_newData),kSyncClient_TODO_Error);
    check(hpatch_TFileStreamInput_close(&newSyncInfo),kSyncClient_TODO_Error);
    check(hpatch_TFileStreamInput_close(&oldData),kSyncClient_TODO_Error);
    return result;
}
