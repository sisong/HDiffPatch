//digest_match_private_types.h
//
/*
 The MIT License (MIT)
 Copyright (c) 2012-2025 HouSisong
 
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

#ifndef digest_match_private_types_h
#define digest_match_private_types_h
#include "bloom_filter.h"
#include "adler_roll.h"
#include "../mem_buf.h"
#include <stdexcept>  //std::runtime_error
#include <algorithm>  //std::sort,std::equal_range
#include "../compress_detect.h" //_getUIntCost
#include "../../../../libParallel/parallel_channel.h"
#include "../qsort_parallel.h"
#if (_IS_USED_MULTITHREAD)
#include <atomic>
#endif
namespace hdiff_private{

static  const size_t kMatchBlockSize_min=4;//sizeof(hpatch_uint32_t);
static  const size_t kBestReadSize=1024*256; //for sequence read
static  const size_t kMinParallelSize=1024*1024*2;
static  const size_t kBestParallelSize=1024*1024*8;

template <class T,class Tn>
inline static T upperCount(T all_size,Tn node_size){ return (all_size+node_size-1)/node_size; }

inline static hpatch_StreamPos_t getBlockCount(size_t kMatchBlockSize,
                                               hpatch_StreamPos_t streamSize){
    return upperCount(streamSize,kMatchBlockSize);
}

inline static size_t limitMatchBlockSize(size_t kMatchBlockSize,hpatch_StreamPos_t oldStreamSize){
    hpatch_StreamPos_t maxBetterBlockSize=((oldStreamSize+63)/64+63)/64*64;
    kMatchBlockSize=(kMatchBlockSize<=maxBetterBlockSize)?kMatchBlockSize:(size_t)maxBetterBlockSize;
    return (kMatchBlockSize>=kMatchBlockSize_min)?kMatchBlockSize:kMatchBlockSize_min;
}

inline static hpatch_StreamPos_t blockIndexToPos(hpatch_StreamPos_t index,size_t kMatchBlockSize,hpatch_StreamPos_t streamSize){
    hpatch_StreamPos_t pos=index*kMatchBlockSize;
    return (pos+kMatchBlockSize<=streamSize)?pos:streamSize-kMatchBlockSize;
}
inline static hpatch_StreamPos_t posToBlockIndex(hpatch_StreamPos_t pos,size_t kMatchBlockSize,hpatch_StreamPos_t blockCount){
    hpatch_StreamPos_t result=((pos+(kMatchBlockSize-1))/kMatchBlockSize);
    return (result<blockCount)?result:blockCount-1;
}

template<bool isMT,class v_uint_t> static inline
void _filter_insert(TBloomFilter<v_uint_t>* filter,const v_uint_t* begin,const v_uint_t* end){
    while (begin!=end){
        v_uint_t h=(*begin++);
#if (_IS_USED_MULTITHREAD)
        if (isMT)
            filter->insert_MT(h);
        else
#endif
            filter->insert(h);
    }
}

template<class v_uint_t> inline static
void filter_insert_parallel(TBloomFilter<v_uint_t>& filter,const v_uint_t* begin,
                            const v_uint_t* end,size_t threadNum){
#if (_IS_USED_MULTITHREAD)
    const size_t kInsertMinParallelSize=4096;
    const size_t size=end-begin;
    if ((threadNum>1)&&(size>=kInsertMinParallelSize)) {
        const size_t maxThreanNum=size/(kInsertMinParallelSize/2);
        threadNum=(threadNum<=maxThreanNum)?threadNum:maxThreanNum;

        const size_t step=size/threadNum;
        const size_t threadCount=threadNum-1;
        std::vector<std::thread> threads(threadCount);
        for (size_t i=0;i<threadCount;i++,begin+=step)
            threads[i]=std::thread(_filter_insert<true,v_uint_t>,&filter,begin,begin+step);
        _filter_insert<true>(&filter,begin,end);
        for (size_t i=0;i<threadCount;i++)
            threads[i].join();
    }else
#endif
    {
        _filter_insert<false>(&filter,begin,end);
    }
}

struct TStreamCache{
    TStreamCache(const hpatch_TStreamInput* _stream,unsigned char* _cache,size_t _cacheSize,void* _locker)
    :stream(_stream),m_readPos(0),m_readPosEnd(0),m_locker(_locker),
        cache(_cache),cacheSize(_cacheSize),cachePos(_cacheSize){ }
    inline hpatch_StreamPos_t streamSize()const{ return stream->streamSize; }
    inline hpatch_StreamPos_t pos()const { return m_readPosEnd-dataLength(); }
    hpatch_force_inline const size_t   dataLength()const{ return (size_t)(cacheSize-cachePos); }
    hpatch_force_inline const unsigned char* data()const { return cache+cachePos; }
    inline bool resetPos(size_t kBackupCacheSize,hpatch_StreamPos_t streamPos,size_t kMinCacheDataSize){
        if (_is_hit_cache(kBackupCacheSize,streamPos,kMinCacheDataSize)){
            cachePos=cacheSize-(size_t)(m_readPosEnd-streamPos);
            return true; //hit cache
        }
        return _resetPos_continue(kBackupCacheSize,streamPos,kMinCacheDataSize);
    }
protected:
    inline void readStream(hpatch_StreamPos_t pos,hpatch_byte* dst,size_t n){
        if (((n)>0)&&(!(stream)->read(stream,pos,dst,dst+n)))
            throw std::runtime_error("TStreamCache::_resetPos_continue() stream->read() error!"); }
    inline bool _is_hit_cache(size_t kBackupCacheSize,hpatch_StreamPos_t streamPos,size_t kMinCacheDataSize){
        return (streamPos+kMinCacheDataSize<=m_readPosEnd)&&(streamPos>=m_readPos+kBackupCacheSize);
    }
    inline unsigned char* cachedData(){ return cache+cacheSize-cachedLength(); }
    inline const size_t   cachedLength()const{ return (size_t)(m_readPosEnd-m_readPos); }
    bool _resetPos_continue(size_t kBackupCacheSize,hpatch_StreamPos_t streamPos,size_t kMinCacheDataSize){
        //stream:[...     |                |                   |                  |                      |      ...]
        //             readPos (streamPos-kBackupCacheSize) streamPos (streamPos+kMinCacheDataSize) (readPosEnd)
        //cache:     [    |                                    |                  |                      ]
        //           0                                   (init cachePos)                              cacheSize
        hpatch_StreamPos_t streamSize=stream->streamSize;
        if (streamPos+kMinCacheDataSize>streamSize) return false;
        hpatch_StreamPos_t readPos=(streamPos>=kBackupCacheSize)?(streamPos-kBackupCacheSize):0;
        size_t readLen=((streamSize-readPos)>=cacheSize)?cacheSize:(size_t)(streamSize-readPos);
#if (_IS_USED_MULTITHREAD)
        if (m_locker){
            CAutoLocker _autoLocker(m_locker);
            _resetPos_continue_read(readPos,readLen);
        }else
#endif
        {
            _resetPos_continue_read(readPos,readLen);
        }
        m_readPos=readPos;
        m_readPosEnd=readPos+readLen;
        cachePos=cacheSize-(size_t)(m_readPosEnd-streamPos);
        return true;
    }
private:
    void _resetPos_continue_read(hpatch_StreamPos_t readPos,size_t readLen){
        unsigned char* dst=cache+cacheSize-readLen;
        if ((m_readPosEnd>readPos)&&(m_readPos<=readPos)){
            size_t moveLen=(size_t)(m_readPosEnd-readPos);
            memmove(dst,cache+cacheSize-moveLen,moveLen);
            readStream(readPos+moveLen,dst+moveLen,readLen-moveLen);
        }else if ((m_readPos<readPos+readLen)&&(m_readPosEnd>=readPos+readLen)){
            size_t moveLen=(size_t)(readPos+readLen-m_readPos);
            memmove(dst+readLen-moveLen,cachedData(),moveLen);
            readStream(readPos,dst,readLen-moveLen);
        }else{
            readStream(readPos,dst,readLen);
        }
    }
    const hpatch_TStreamInput* stream;
protected:
    hpatch_StreamPos_t         m_readPos;
    hpatch_StreamPos_t         m_readPosEnd;
    void*                      m_locker;
    unsigned char*             cache;
    size_t                     cacheSize;
    size_t                     cachePos;
};

struct TBlockStreamCache:public TStreamCache{
    TBlockStreamCache(const hpatch_TStreamInput* _stream,unsigned char* _cache,size_t _cacheSize,
                      size_t _backupCacheSize, size_t _kMatchBlockSize,void* _locker)
    :TStreamCache(_stream,_cache,_cacheSize,_locker),
    backupCacheSize(_backupCacheSize),kMatchBlockSize(_kMatchBlockSize){
        assert(cacheSize>=(backupCacheSize+kMatchBlockSize)); }
    inline bool resetPos(hpatch_StreamPos_t streamPos){
        return TStreamCache::resetPos(backupCacheSize,streamPos,kMatchBlockSize);
    }
public:
    inline bool _loop_backward_cache(size_t skipLen,size_t needDataSize){
        return TStreamCache::resetPos(0,pos()+skipLen,needDataSize);
    }
    size_t forward_equal_length(const TBlockStreamCache& y)const{
        size_t y_len=(size_t)(y.pos()-y.m_readPos);
        size_t len=(size_t)(pos()-m_readPos);
        len=(len<y_len)?len:y_len;
        const unsigned char* px=data();
        const unsigned char* py=y.data();
        size_t i=0;
        for (;i<len;++i){
            if (*(--px) != *(--py)) break;
        }
        return i;
    }
public:
    const size_t    backupCacheSize;
    const size_t    kMatchBlockSize;
};

struct TRollHash_fadler64{
    typedef uint64_t    hash_uint_t;
    hpatch_force_inline static 
    hash_uint_t hash_start(const hpatch_byte* pdata,size_t n) { return fast_adler64_start(pdata,n); }
    hpatch_force_inline static
    hash_uint_t hash_roll(hash_uint_t hash,uint64_t blockSize,hpatch_byte out_data,hpatch_byte in_data) { 
                                                    return fast_adler64_roll(hash,blockSize,out_data,in_data); }
};

struct TRollHash_fadler32{
    typedef uint32_t    hash_uint_t;
    hpatch_force_inline static 
    hash_uint_t hash_start(const hpatch_byte* pdata,size_t n) { return fast_adler32_start(pdata,n); }
    hpatch_force_inline static
    hash_uint_t hash_roll(hash_uint_t hash,size_t blockSize,hpatch_byte out_data,hpatch_byte in_data) { 
                                                    return fast_adler32_roll(hash,blockSize,out_data,in_data); }
};

    struct TBlocksHash_mt{
        const hpatch_TStreamInput* streamData;
        void*       out_blocks;
        size_t      kMatchBlockSize;
        size_t      workCount;
        volatile hpatch_StreamPos_t curReadedPos;
        void*       locker;
    };

template <class TRollHash>
void _getBlocksHash_thread(TBlocksHash_mt* mt,hpatch_byte* pmem,size_t memStep){
    const size_t kMatchBlockSize=mt->kMatchBlockSize;
    const hpatch_StreamPos_t streamSize=mt->streamData->streamSize;
    const hpatch_StreamPos_t blockCount=getBlockCount(kMatchBlockSize,streamSize);
    typename TRollHash::hash_uint_t* out_blocks=(typename TRollHash::hash_uint_t*)mt->out_blocks;
    while (true){
        hpatch_StreamPos_t readPos;
        size_t readLen=memStep;
        {//read stream
        #if (_IS_USED_MULTITHREAD)
            CAutoLocker _autoLocker(mt->locker);
        #endif
            readPos=mt->curReadedPos;
            if (readPos+readLen>streamSize){
                if (readPos>=streamSize) break;
                if (streamSize>=kMatchBlockSize+readPos){
                    readLen=(size_t)(streamSize-readPos);
                }else{
                    readLen=kMatchBlockSize;
                    readPos=streamSize-kMatchBlockSize;
                }
            }
            mt->curReadedPos=readPos+readLen;
            if (!mt->streamData->read(mt->streamData,readPos,pmem,pmem+readLen))
                throw std::runtime_error("_getBlocksHash_thread() streamData->read error!");
        }
        assert(readLen>=kMatchBlockSize);
        size_t blocki=(size_t)posToBlockIndex(readPos,kMatchBlockSize,blockCount);
        const hpatch_byte* pdataEnd=pmem+readLen;
        for (const hpatch_byte* pdata=pmem;pdata<pdataEnd;pdata+=kMatchBlockSize){
            if (pdata+kMatchBlockSize>pdataEnd)
                pdata=pdataEnd-kMatchBlockSize;
            out_blocks[blocki++]=TRollHash::hash_start(pdata,kMatchBlockSize);
        }
    }
}

template <class TRollHash>
void getBlocksHash_parallel(const hpatch_TStreamInput* streamData,typename TRollHash::hash_uint_t* out_blocks,
                            size_t kMatchBlockSize,hpatch_byte* pmem,size_t memSize,size_t threadNum){
    assert(streamData->streamSize>=kMatchBlockSize);
#if (_IS_USED_MULTITHREAD)
    if (threadNum<1)
#endif
        threadNum=1;
    size_t memStep=(memSize/threadNum)/kMatchBlockSize*kMatchBlockSize;
    assert(memStep>0);
    hpatch_StreamPos_t workCount=upperCount(streamData->streamSize,memStep);
    if (threadNum>workCount){
        threadNum=(size_t)workCount;
        memStep=(memSize/threadNum)/kMatchBlockSize*kMatchBlockSize;
    }
    TBlocksHash_mt mt_data={streamData,out_blocks,kMatchBlockSize,(size_t)workCount,0,0};
    if ((sizeof(mt_data.workCount)<sizeof(workCount))&&(mt_data.workCount!=workCount))
        throw std::runtime_error("getBlocksHash_parallel() multi-thread workCount error!");
#if (_IS_USED_MULTITHREAD)
    if (threadNum>1){
        CHLocker _locker;
        mt_data.locker=_locker.locker;
        const size_t threadCount=threadNum-1;
        std::vector<std::thread> threads(threadCount);
        for (size_t i=0;i<threadCount;i++,pmem+=memStep)
            threads[i]=std::thread(_getBlocksHash_thread<TRollHash>,&mt_data,pmem,memStep);
        _getBlocksHash_thread<TRollHash>(&mt_data,pmem,memStep);
        for (size_t i=0;i<threadCount;i++)
            threads[i].join();
    }else
#endif
    {
        _getBlocksHash_thread<TRollHash>(&mt_data,pmem,memStep);
    }
 }


template <class hash_uint_t>
struct tm_TDigest_comp{
    inline explicit tm_TDigest_comp(const hash_uint_t* _blocks):blocks(_blocks){ }
    struct TDigest{
        hash_uint_t value;
        hpatch_force_inline explicit TDigest(hash_uint_t _value):value(_value){}
    };
    template<class TIndex> hpatch_force_inline
    bool operator()(const TIndex x,const TDigest& y)const { return blocks[x]<y.value; }
    template<class TIndex> hpatch_force_inline
    bool operator()(const TDigest& x,const TIndex y)const { return x.value<blocks[y]; }
    template<class TIndex> hpatch_force_inline
    bool operator()(const TIndex x, const TIndex y)const { return blocks[x]<blocks[y]; }
protected:
    const hash_uint_t* blocks;
};

template <class TRollHash>
struct tm_TNewStreamCache:public TBlockStreamCache{
    typedef typename TRollHash::hash_uint_t hash_uint_t;
    tm_TNewStreamCache(const hpatch_TStreamInput* _stream,unsigned char* _cache,size_t _cacheSize,
                       size_t _backupCacheSize,size_t _kMatchBlockSize,void* _locker)
    :TBlockStreamCache(_stream,_cache,_cacheSize,_backupCacheSize,_kMatchBlockSize,_locker){
        resetPos(0);
    }
    void toBestDataLength(){
        if (dataLength()*2>=cacheSize) return;
        bool result=_resetPos_continue(backupCacheSize,pos(),kMatchBlockSize);
        assert(result);
    }
    
    inline bool resetPos(hpatch_StreamPos_t streamPos){
        if (!TBlockStreamCache::resetPos(streamPos)) return false;
        roll_digest=TRollHash::hash_start(data(),kMatchBlockSize);
        return true;
    }
    hpatch_force_inline bool roll(){
        //warning: after running _loop_backward_cache(),cache roll logic is failure
        if (dataLength()>kMatchBlockSize){
            const unsigned char* cur_datas=data();
            roll_digest=TRollHash::hash_roll(roll_digest,kMatchBlockSize,cur_datas[0],cur_datas[kMatchBlockSize]);
            ++cachePos;
            return true;
        }else{
            return _resetPos_and_roll(); 
        }
    }
    hpatch_force_inline hash_uint_t rollDigest()const{ return roll_digest; }
private:
    hash_uint_t     roll_digest;
    bool _resetPos_and_roll(){
        if (!TBlockStreamCache::resetPos(pos()+1)) return false;
        --cachePos;
        return roll();
    }
};

}//namespace hdiff_private
#endif
