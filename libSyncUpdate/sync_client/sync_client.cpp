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

static const hpatch_StreamPos_t kBlockType_needSync =~(hpatch_StreamPos_t)0;

void matchNewDataInOld(const hpatch_TStreamInput* oldStream,hpatch_StreamPos_t* out_newDataPoss,
                       hpatch_TChecksum* strongChecksumPlugin,const TNewDataSyncInfo* newSyncInfo){
    uint32_t kMatchBlockSize=newSyncInfo->kMatchBlockSize;
    uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);

    TAutoMem _mem(kBlockCount*sizeof(uint32_t));
    uint32_t* sorted_newIndexs=(uint32_t*)_mem.data();
    TBloomFilter<uint32_t> filter; filter.init(kBlockCount);
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
                if (out_newDataPoss[newBlockIndex]==kBlockType_needSync){
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
    if ((kBlockCount>0)&&(out_newDataPoss[kBlockCount-1]==kBlockType_needSync)){
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
}

static int resyncNewData(const TNewDataSyncInfo* newSyncInfo,
                         const hpatch_TStreamOutput* out_newStream,
                         ISyncPatchListener* listener,
                         hpatch_TChecksum* strongChecksumPlugin,
                         hpatch_TDecompress* decompressPlugin,
                         hpatch_StreamPos_t* newDataPoss,
                         uint32_t curInsureBlockCount,hpatch_StreamPos_t curInsureOutNewDataPos){
    
    return -1;
}

int sync_patch(const hpatch_TStreamOutput* out_newStream,
               const TNewDataSyncInfo*     newSyncInfo,
               const hpatch_TStreamInput*  oldStream, ISyncPatchListener* listener){
    //todo: select checksum run

    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    const uint32_t kMatchBlockSize=newSyncInfo->kMatchBlockSize;
    const uint32_t checksumByteSize=newSyncInfo->kStrongChecksumByteSize;
    hpatch_TChecksum* strongChecksumPlugin=&md5ChecksumPlugin;
    hpatch_TDecompress* decompressPlugin=0;
    TByte* dataBuf=0;
    int result=kSyncClient_ok;
    int _inClear=0;
    //match in oldData
    hpatch_StreamPos_t* newDataPoss=(hpatch_StreamPos_t*)malloc(kBlockCount*sizeof(hpatch_StreamPos_t));
    check(newDataPoss!=0,kSyncClient_memError);
    try{
        matchNewDataInOld(oldStream,newDataPoss,strongChecksumPlugin,newSyncInfo);
    }catch(...){
        result=kSyncClient_matchNewDataInOldError;
    }
    check(result==kSyncClient_ok,result);
    
    {//send msg: all need sync block
        hpatch_StreamPos_t posInNewSyncData=0;
        for (uint32_t i=0; i<kBlockCount; ++i) {
            uint32_t syncDataSize=TNewDataSyncInfo_syncDataSize(newSyncInfo,i);
            if (newDataPoss[i]==kBlockType_needSync)
                listener->needSyncMsg(listener,posInNewSyncData,syncDataSize);
            posInNewSyncData+=syncDataSize;
        }
    }
    
    {//write newData
        const uint32_t     kInsureBlockCount=(uint32_t)TNewDataSyncInfo_insureBlockCount(newSyncInfo);
        hpatch_StreamPos_t posInNewSyncData=0;
        hpatch_StreamPos_t outNewDataPos=0;
        TByte*             checksumNewData_buf=0;
        uint32_t           curInsureBlockCount=0;
        hpatch_StreamPos_t curInsureOutNewDataPos=0;
        uint32_t kInsureStrongChecksumBlockSize=newSyncInfo->kInsureStrongChecksumBlockSize;
        hpatch_checksumHandle checksumNewData=strongChecksumPlugin->open(strongChecksumPlugin);
        check(checksumNewData!=0,kSyncClient_strongChecksumOpenError);
        
        dataBuf=(TByte*)malloc(kMatchBlockSize*2+checksumByteSize);
        check(dataBuf!=0,kSyncClient_memError);
        checksumNewData_buf=dataBuf+kMatchBlockSize*2;
        for (uint32_t i=0; i<kBlockCount; ++i) {
            uint32_t syncDataSize=TNewDataSyncInfo_syncDataSize(newSyncInfo,i);
            uint32_t newDataSize=TNewDataSyncInfo_newDataSize(newSyncInfo,i);
            if (newDataPoss[i]==kBlockType_needSync){ //sync
                TByte* buf=decompressPlugin?(dataBuf+kMatchBlockSize):dataBuf;
                check(listener->readSyncData(listener,dataBuf,posInNewSyncData,syncDataSize),
                      kSyncClient_readSyncDataError);
                if (decompressPlugin){
                    check(hpatch_deccompress_mem(decompressPlugin,buf,buf+syncDataSize,
                                                 dataBuf,dataBuf+newDataSize),kSyncClient_decompressError);
                }
            }else{//copy from old
                check(oldStream->read(oldStream,newDataPoss[i],dataBuf,dataBuf+newDataSize),
                      kSyncClient_readOldDataError);
            }
            //insureStrongChecksum
            strongChecksumPlugin->append(checksumNewData,dataBuf,dataBuf+newDataSize);
            if ((i+1)%kInsureStrongChecksumBlockSize==0){
                strongChecksumPlugin->end(checksumNewData,checksumNewData_buf,
                                          checksumNewData_buf+checksumByteSize);
                if (0!=memcmp(newSyncInfo->newDataInsureStrongChecksums+curInsureBlockCount*checksumByteSize,
                              checksumNewData_buf,checksumByteSize)){ //data error?
                  //  result=resyncNewData(curInsureBlockCount,curInsureOutNewDataPos);
                    check(result==kSyncClient_ok,result);
                }
                ++curInsureBlockCount;
                if (curInsureBlockCount<kInsureBlockCount){
                    strongChecksumPlugin->begin(checksumNewData);
                    curInsureOutNewDataPos=outNewDataPos;
                }
            }
            if (out_newStream){//write
                check(out_newStream->write(out_newStream,outNewDataPos,dataBuf,
                                           dataBuf+newDataSize), kSyncClient_writeNewDataError);
            }
            outNewDataPos+=newDataSize;
            posInNewSyncData+=syncDataSize;
        }//end for
        if (curInsureBlockCount<kInsureBlockCount){//check last block
            strongChecksumPlugin->end(checksumNewData,checksumNewData_buf,
                                      checksumNewData_buf+checksumByteSize);
            if (0!=memcmp(newSyncInfo->newDataInsureStrongChecksums+curInsureBlockCount*checksumByteSize,
                          checksumNewData_buf,checksumByteSize)){ //data error?
               // result=resyncNewData(curInsureBlockCount,curInsureOutNewDataPos);
                check(result==kSyncClient_ok,result);
            }
            ++curInsureBlockCount;
        }
        assert(outNewDataPos==newSyncInfo->newDataSize);
        assert(posInNewSyncData==newSyncInfo->newSyncDataSize);
        assert(curInsureBlockCount==kInsureBlockCount);
    }
    
clear:
    _inClear=1;
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
