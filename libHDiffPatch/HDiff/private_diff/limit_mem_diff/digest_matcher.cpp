//digest_macher.cpp
/*
 The MIT License (MIT)
 Copyright (c) 2012-2017 HouSisong
 
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

#include "digest_matcher.h"
#include "digest_match_private_types.h"
namespace hdiff_private{
static  const size_t kMinTrustMatchedLength=1024*16;
static  const size_t kMinMatchedLength = 16;
static  const size_t kMinReadSize=1024*4;    //for random first read speed
static  const size_t kMinBackupReadSize=256;
static  const size_t kBestMatchRange=1024*64;
static  const size_t kMaxLinkIndexFindCount=64;

typedef TRollHash_fadler64  TMatchRollHash;
#define hash_uint_t     TMatchRollHash::hash_uint_t
#define hash_start      TMatchRollHash::hash_start
#define hash_roll       TMatchRollHash::hash_roll
typedef tm_TNewStreamCache<TMatchRollHash>  TNewStreamCache;

inline static size_t getBackupSize(size_t kMatchBlockSize) {
    return (kMatchBlockSize>=kMinBackupReadSize)?kMatchBlockSize:kMinBackupReadSize; }

TDigestMatcher::~TDigestMatcher(){
}
    
size_t TDigestMatcher::getSearchThreadNum()const{
#if (_IS_USED_MULTITHREAD)
    const size_t threadNum=m_mtsets.threadNumForSearch;
    hpatch_StreamPos_t size=m_newData->streamSize;
    if ((threadNum>1)&&(m_oldData->streamSize>=m_kMatchBlockSize)
      &&(size>=kMinParallelSize)&&(size/2>=m_kMatchBlockSize)) {
        const hpatch_StreamPos_t maxThreanNum=size/(kMinParallelSize/2);
        return (threadNum<=maxThreanNum)?threadNum:(size_t)maxThreanNum;
    }else
#endif
    {
        return 1;
    }
}

TDigestMatcher::TDigestMatcher(const hpatch_TStreamInput* oldData,const hpatch_TStreamInput* newData,
                               size_t kMatchBlockSize,const hdiff_TMTSets_s& mtsets)
:m_oldData(oldData),m_newData(newData),m_isUseLargeSorted(true),m_mtsets(mtsets),
m_newCacheSize(0),m_oldCacheSize(0),m_oldMinCacheSize(0),m_backupCacheSize(0),m_kMatchBlockSize(0){
    kMatchBlockSize=limitMatchBlockSize(kMatchBlockSize,oldData->streamSize);
    m_kMatchBlockSize=kMatchBlockSize;
    if (oldData->streamSize<kMatchBlockSize) return;
    _out_diff_info("  match covers by block ...\n");
    
    hpatch_StreamPos_t _blockCount=getBlockCount(m_kMatchBlockSize,m_oldData->streamSize);
    m_isUseLargeSorted=(_blockCount>=((hpatch_StreamPos_t)1<<32));
    const size_t blockCount=(size_t)_blockCount;
    if (blockCount!=_blockCount)
        throw std::runtime_error("TDigestMatcher() oldData->streamSize/MatchBlockSize too big error!");
    m_blocks.resize(blockCount);
    if (m_isUseLargeSorted)
        m_sorted_larger.resize(blockCount);
    else
        m_sorted_limit.resize(blockCount);
    
    m_backupCacheSize=getBackupSize(m_kMatchBlockSize);
    m_newCacheSize=upperCount(m_kMatchBlockSize*2+m_backupCacheSize,kBestReadSize)*kBestReadSize;
    m_oldCacheSize=upperCount(m_kMatchBlockSize+m_backupCacheSize,kBestReadSize)*kBestReadSize;
    m_oldMinCacheSize=upperCount(m_kMatchBlockSize+m_backupCacheSize,kMinReadSize)*kMinReadSize;
    assert(m_oldMinCacheSize<=m_oldCacheSize);
    m_mem.realloc((m_newCacheSize+m_oldCacheSize)*getSearchThreadNum());
    
    _out_diff_info("    create blocks's match table ...\n");
    getDigests();
}

struct TIndex_comp{
    inline TIndex_comp(const hash_uint_t* _blocks,size_t _n,size_t _kMaxDeep)
        :blocks(_blocks),n(_n),kMaxDeep(_kMaxDeep){ }
    template<class TIndex>
    inline bool operator()(TIndex x,TIndex y)const {
        hash_uint_t xd=blocks[x];
        hash_uint_t yd=blocks[y];
        if (xd!=yd)
            return xd<yd;
        else
            return _cmp(x+1,y+1,n,kMaxDeep,blocks);
    }
private:
    const hash_uint_t* blocks;
    const size_t        n;
    const size_t        kMaxDeep;
    
    template<class TIndex>
    static bool _cmp(TIndex x,TIndex y,size_t n,size_t kMaxDeep,
                     const hash_uint_t* blocks) {
        if (x!=y) {} else{ return false; }
        size_t deep=n-((x>y)?x:y);
        deep=(deep<=kMaxDeep)?deep:kMaxDeep;
        for (size_t i=0;i<deep;++i){
            hash_uint_t xd=blocks[x++];
            hash_uint_t yd=blocks[y++];
            if (xd!=yd)
                return xd<yd;
        }
        return x>y;
    }
};

#define __sort_indexs(TIndex,indexs,comp,m_threadNum) sort_parallel<TIndex,TIndex_comp,1024*8,137> \
                            (indexs.data(),indexs.data()+indexs.size(),comp,m_threadNum)

void TDigestMatcher::getDigests(){
    if (m_blocks.empty()) return;
    
    getBlocksHash_parallel<TMatchRollHash>(m_oldData,m_blocks.data(),m_kMatchBlockSize,
                                           m_mem.data(),m_mem.size(),m_mtsets.threadNum);
    const size_t blockCount=m_blocks.size();
    m_filter.init(blockCount);
    filter_insert_parallel(m_filter,m_blocks.data(),m_blocks.data()+blockCount,m_mtsets.threadNum);

    for (size_t i=0;i<blockCount;++i) {
        if (m_isUseLargeSorted)
            m_sorted_larger[i]=i;
        else
            m_sorted_limit[i]=(uint32_t)i;
    }
    size_t kMaxCmpDeep= 1 + upperCount(kMinTrustMatchedLength,m_kMatchBlockSize);
    TIndex_comp comp(m_blocks.data(),m_blocks.size(),kMaxCmpDeep);
    if (m_isUseLargeSorted)
        __sort_indexs(size_t,m_sorted_larger,comp,m_mtsets.threadNum);
    else
        __sort_indexs(uint32_t,m_sorted_limit,comp,m_mtsets.threadNum);
}

struct TOldStreamCache:public TBlockStreamCache{
    TOldStreamCache(const hpatch_TStreamInput* _stream,unsigned char* _cache,
                    size_t _minCacheSize,size_t _maxCacheSize,
                    size_t _backupCacheSize,size_t _kMatchBlockSize,void* _locker)
    :TBlockStreamCache(_stream,_cache+_maxCacheSize-_minCacheSize,
                       _minCacheSize, _backupCacheSize,_kMatchBlockSize,_locker),
    minCacheSize(_minCacheSize),maxCacheSize(_maxCacheSize){ }
    
    inline bool resetPos(hpatch_StreamPos_t streamPos){
        if (!_is_hit_cache(backupCacheSize,streamPos,kMatchBlockSize))
            _doMinCacheSize();
        return TBlockStreamCache::resetPos(streamPos);
    }
    hpatch_StreamPos_t loop_backward_equal_length(TBlockStreamCache& y){
        if (!_loop_backward_cache(kMatchBlockSize,1)) return 0;
        if (!y._loop_backward_cache(kMatchBlockSize,1)) return 0;
        hpatch_StreamPos_t eq_len=0;
        while(true){
            const size_t xLen=dataLength();
            size_t len=y.dataLength();
            len=(len>=xLen)?xLen:len;
            const unsigned char* px=data();
            const unsigned char* py=y.data();
            for (size_t i=0;i<len;++i){
                if (px[i]!=py[i])
                    return eq_len+i;
            }
            eq_len+=len;
            if ((len==xLen)&(cacheSize<maxCacheSize))
                _doGrowCacheSize();
            if (!_loop_backward_cache(len,1)) break;
            if (!y._loop_backward_cache(len,1)) break;
        }
        return eq_len;
    }
private:
    const size_t minCacheSize;
    const size_t maxCacheSize;
    inline void _doMinCacheSize(){
        cache    +=cacheSize-minCacheSize;
        cacheSize=minCacheSize;
        cachePos =minCacheSize;
        m_readPos=(cachedLength()<=cacheSize)?m_readPos:m_readPosEnd-cacheSize;
    }
    inline void _doGrowCacheSize(){
        const size_t growSize=(cacheSize*2<=maxCacheSize)?cacheSize:(maxCacheSize-cacheSize);
        cache    -=growSize;
        cacheSize+=growSize;
        cachePos +=growSize;
    }
};


typedef tm_TDigest_comp<hash_uint_t>    TDigest_comp;

struct TDigest_comp_i:public TDigest_comp{
    inline TDigest_comp_i(const hash_uint_t* _blocks,size_t _n):TDigest_comp(_blocks),n(_n),i(0){ }
    template<class TIndex> inline
    bool operator()(const TIndex& x,const TDigest& y)const { return (x+i<n)?(blocks[x+i]<y.value):true; }
    template<class TIndex> inline
    bool operator()(const TDigest& x,const TIndex& y)const { return (y+i<n)?(x.value<blocks[y+i]):false; }
    template<class TIndex> inline
    bool operator()(const TIndex& x, const TIndex& y)const {
        if ((x+i<n)&(y+i<n)) return blocks[x+i]<blocks[y+i];
        if (x+i<n) return false;
        if (y+i<n) return true;
        return x>y;//n-x<n-y
    }
private:
    const size_t        n;
public:
    size_t              i;
};

    static hpatch_StreamPos_t getMatchLength(TOldStreamCache& oldStream,TNewStreamCache& newStream,
                                             hpatch_StreamPos_t* pOldPos,size_t kMatchBlockSize,
                                             const TCover& lastCover){
        if (oldStream.resetPos(*pOldPos)&&
            (0==memcmp(oldStream.data(),newStream.data(),kMatchBlockSize))){
            const hpatch_StreamPos_t newPos=newStream.pos();
            size_t feq_len=oldStream.forward_equal_length(newStream);
            if (newPos-feq_len<lastCover.newPos+lastCover.length)
                feq_len=(size_t)(newPos-(lastCover.newPos+lastCover.length));
            hpatch_StreamPos_t beq_len=oldStream.loop_backward_equal_length(newStream);
            *pOldPos=(*pOldPos)-feq_len;
            return feq_len+kMatchBlockSize+beq_len;
        }else{
            return 0;
        }
    }

template <class TIndex>
static const TIndex* getBestMatchi(const hash_uint_t* blocksBase,size_t blocksSize,
                                   const TIndex** _left,const TIndex** _right,
                                   TNewStreamCache& newStream,const TCover& lastCover,
                                   size_t* _digests_eq_n){
    const size_t kMatchBlockSize=newStream.kMatchBlockSize;
    size_t max_digests_n=upperCount(kMinTrustMatchedLength,kMatchBlockSize);
    size_t _data_max_digests_n=newStream.dataLength()/kMatchBlockSize;
    if (max_digests_n>_data_max_digests_n) max_digests_n=_data_max_digests_n;
    
    const TIndex* best=0;
    const TIndex*& left=*_left;
    const TIndex*& right=*_right;
    size_t& digests_eq_n=*_digests_eq_n;
    digests_eq_n=1;
    //Narrow down the [left best right) range to keep at most 2 matches (since signature matching doesn't guarantee equality, 2 should be sufficient?);
    if (right-left>1){
        //Find the position with longest signature match (which is most likely to be the longest actual match position);
        TDigest_comp_i comp_i(blocksBase,blocksSize);
        newStream.toBestDataLength();
        const unsigned char* bdata=newStream.data()+kMatchBlockSize;
        for (; (digests_eq_n<max_digests_n)&&(right-left>1);++digests_eq_n,bdata+=kMatchBlockSize){
            hash_uint_t digest=hash_start(bdata,kMatchBlockSize);
            typename TDigest_comp::TDigest digest_value(digest);
            comp_i.i=digests_eq_n;
            std::pair<const TIndex*,const TIndex*> i_range=
                std::equal_range(left,right,digest_value,comp_i);
            size_t rn=i_range.second-i_range.first;
            if (rn==0){
                break;
            }else if (rn==1){
                best=i_range.first;
                break;
            }else{
                left=i_range.first;
                right=i_range.second;
            }
        }
    }else{
        best=left;
    }
    //best==0 indicates there are >2 positions that are equally good, need to continue searching;
    
    //assert(newStream.pos()>lastCover.newPos);
    hpatch_StreamPos_t linkOldPos=newStream.pos()+lastCover.oldPos-lastCover.newPos;
    TIndex linkIndex=(TIndex)posToBlockIndex(linkOldPos,kMatchBlockSize,blocksSize);
    //Find a position near lastCover as a good default best value, beneficial for link or compression;
    if (best==0){
        TIndex_comp comp(blocksBase,blocksSize,max_digests_n);
        size_t findCount=(right-left)*2+1;
        if (findCount>kMaxLinkIndexFindCount) findCount=kMaxLinkIndexFindCount;
        for (TIndex inc=1;(inc<=findCount);++inc) { //Search around linkIndex;
            TIndex fi;  TIndex s=(inc>>1);
            if (inc&1){
                if (linkIndex<s) continue;
                fi=linkIndex-s;
            }else{
                if (linkIndex+s>=blocksSize) continue;
                fi=linkIndex+s;
            }
            std::pair<const TIndex*,const TIndex*> i_range=std::equal_range(left,right,fi,comp);
            if (i_range.first!=i_range.second){
                best=i_range.first+(i_range.second-i_range.first)/2;
                for (const TIndex* ci=best;ci<i_range.second; ++ci) {
                    if (*ci==fi) { best=ci; break;  } //Found;
                }
                break;
            }
        }
    }
    if(best==0){ //Continue searching;
        best=left+(right-left)/2;
        hpatch_StreamPos_t _best_distance=hpatch_kNullStreamPos;
        const TIndex* end=(left+kBestMatchRange>=right)?right:(left+kBestMatchRange);
        for (const TIndex* it=left;it<end; ++it) {
            hpatch_StreamPos_t oldIndex=(*it);
            hpatch_StreamPos_t distance=(oldIndex<linkIndex)?(linkIndex-oldIndex):(oldIndex-linkIndex);
            if (distance<_best_distance){ //Find the nearest;
                best=it;
                _best_distance=distance;
            }
        }
    }
    return best;
}

template <class TIndex>
static bool getBestMatch(const TIndex* left,const TIndex* right,const TIndex* best,
                         TOldStreamCache& oldStream,TNewStreamCache& newStream,
                         const TCover& lastCover,size_t digests_eq_n,TCover* out_curCover){
    assert(best!=0);
    const size_t kMatchBlockSize=newStream.kMatchBlockSize;
    const hpatch_StreamPos_t newPos=newStream.pos();
    bool isMatched=false;
    hpatch_StreamPos_t  bestLen=0;
    const size_t kMaxFindCount=5; //Distance 2 around;
    size_t findCount=(right-left)*2+1;
    if (findCount>kMaxFindCount) findCount=kMaxFindCount;
    for (size_t inc=1;(inc<=findCount);++inc) { //Search around best;
        const TIndex* fi;  size_t s=(inc>>1);
        if (inc&1){
            if (best<left+s) continue;
            fi=best-s;
        }else{
            if (best+s>=right) continue;
            fi=best+s;
        }
        if (inc>1)
            newStream.TBlockStreamCache::resetPos(newPos);
        
        hpatch_StreamPos_t oldPos=blockIndexToPos(*fi,kMatchBlockSize,oldStream.streamSize());
        hpatch_StreamPos_t matchedOldPos=oldPos;
        hpatch_StreamPos_t curEqLen=getMatchLength(oldStream,newStream,
                                                   &matchedOldPos,kMatchBlockSize,lastCover);
        if (curEqLen>bestLen){
            isMatched=true;
            bestLen=curEqLen;
            out_curCover->length=curEqLen;
            out_curCover->oldPos=matchedOldPos;
            out_curCover->newPos=newPos-(oldPos-matchedOldPos);
            if (curEqLen>=digests_eq_n*kMatchBlockSize)
                break;//matched maybe best
        }
    }
    return isMatched;
}
    
    static const size_t getOldPosCost(hpatch_StreamPos_t oldPos,const TCover& lastCover){
        hpatch_StreamPos_t oldPosEnd=lastCover.oldPos+lastCover.length;
        hpatch_StreamPos_t dis=(oldPos>=oldPosEnd)?(oldPos-oldPosEnd):(oldPosEnd-oldPos);
        return _getUIntCost(dis*2);
    }
    
    //Try collinearity;
    static void tryLink(const TCover& lastCover,TCover& matchCover,
                        TOldStreamCache& oldStream,TNewStreamCache& newStream){
        if (lastCover.length<=0) return;
        hpatch_StreamPos_t linkOldPos=matchCover.newPos+lastCover.oldPos-lastCover.newPos;
        if (linkOldPos==matchCover.oldPos) return;
        newStream.TBlockStreamCache::resetPos(matchCover.newPos);
        hpatch_StreamPos_t matchedOldPos=linkOldPos;
        hpatch_StreamPos_t curEqLen=getMatchLength(oldStream,newStream,&matchedOldPos,
                                                   newStream.kMatchBlockSize,lastCover);
        size_t unlinkCost=getOldPosCost(matchCover.oldPos,lastCover);
        size_t unlinkCost_link=getOldPosCost(matchedOldPos,lastCover);
        if ((curEqLen>=kMinMatchedLength)
            &&(curEqLen+unlinkCost>=matchCover.length+unlinkCost_link)){
            matchCover.length=curEqLen;
            matchCover.oldPos=matchedOldPos;
            matchCover.newPos=matchCover.newPos-(linkOldPos-matchedOldPos);
        }
    }

template <class TIndex>
static void tm_search_cover(const hash_uint_t* blocksBase,
                            const TIndex* iblocks,const TIndex* iblocks_end,
                            TOldStreamCache& oldStream,TNewStreamCache& newStream,
                            const TBloomFilter<hash_uint_t>& filter,
                            hpatch_TOutputCovers* out_covers,
                            hpatch_StreamPos_t _coverNewOffset,
                            void* _dataLocker) {
    const size_t blocksSize=iblocks_end-iblocks;
    TDigest_comp comp(blocksBase);
    TCover  lastCover={0,0,0};
    while (true) {
        if (!filter.is_hit(newStream.rollDigest())){
          _do_roll:
            if (newStream.roll()) continue; else break; //finish
        }
        std::pair<const TIndex*,const TIndex*>
          range=std::equal_range(iblocks,iblocks_end,typename TDigest_comp::TDigest(newStream.rollDigest()),comp);
        if (range.first==range.second) goto _do_roll;
        
        hpatch_StreamPos_t newPosNext=newStream.pos()+1;
        size_t digests_eq_n;
        const TIndex* besti=getBestMatchi(blocksBase,blocksSize,&range.first,&range.second,
                                          newStream,lastCover,&digests_eq_n);
        {
            TCover  curCover;
            if (getBestMatch(range.first,range.second,besti,oldStream,newStream,
                             lastCover,digests_eq_n,&curCover)){
                tryLink(lastCover,curCover,oldStream,newStream);
                if (curCover.length>=kMinMatchedLength){//matched
                    TCover _cover;
                    setCover(_cover,curCover.oldPos,curCover.newPos+_coverNewOffset,curCover.length);
                    {
    #if (_IS_USED_MULTITHREAD)
                        CAutoLocker _autoCoverLocker(_dataLocker);
    #endif
                        if (!out_covers->push_cover(out_covers,&_cover))
                            throw std::runtime_error("TDigestMatcher::search_cover() push_cover error!");
                    }
                    lastCover=curCover;
                    newPosNext=curCover.newPos+curCover.length;//next
                }
            }
        }
        //next
        if (newStream.resetPos(newPosNext)) continue; else break; //finish
    }
}

#define __search_cover(indexs)  \
            tm_search_cover(m_blocks.data(),indexs.data(),indexs.data()+indexs.size(), \
                            oldStream,newStream,m_filter,out_covers,newOffset,dataLocker)

void TDigestMatcher::_search_cover(const hpatch_TStreamInput* newData,hpatch_StreamPos_t newOffset,
                                   hpatch_TOutputCovers* out_covers,unsigned char* pmem,
                                   void* oldDataLocker,void* newDataLocker,void* dataLocker){
    TNewStreamCache newStream(newData,pmem,m_newCacheSize,m_backupCacheSize,
                              m_kMatchBlockSize,m_mtsets.newDataIsMTSafe?0:newDataLocker);
    TOldStreamCache oldStream(m_oldData,pmem+m_newCacheSize,m_oldMinCacheSize,m_oldCacheSize,m_backupCacheSize,
                              m_kMatchBlockSize,m_mtsets.oldDataIsMTSafe?0:oldDataLocker);
    if (m_isUseLargeSorted)
        __search_cover(m_sorted_larger);
    else
        __search_cover(m_sorted_limit);
}

#if (_IS_USED_MULTITHREAD)
# if (_IS_NO_ATOMIC_U64)
#   define uint_work size_t 
# else
#   define uint_work hpatch_StreamPos_t 
# endif
struct mt_data_t{
    CHLocker    oldDataLocker;
    CHLocker    newDataLocker;
    CHLocker    dataLocker;
    uint_work          workCount;
    volatile uint_work workIndex;
};
#endif

void TDigestMatcher::_search_cover_thread(hpatch_TOutputCovers* out_covers,
                                          unsigned char* pmem,void* mt_data){
#if (_IS_USED_MULTITHREAD)
    const size_t kPartPepeatSize=m_kMatchBlockSize-1;
    mt_data_t& mt=*(mt_data_t*)mt_data;
    const uint_work workCount=mt.workCount;
    const hpatch_StreamPos_t rollCount=m_newData->streamSize-(m_kMatchBlockSize-1);
    std::atomic<uint_work>& workIndex=*(std::atomic<uint_work>*)&mt.workIndex;
    while (true){
        uint_work curWorkIndex=workIndex++;
        if (curWorkIndex>=workCount) break;
        hpatch_StreamPos_t new_begin=rollCount*curWorkIndex/workCount;
        hpatch_StreamPos_t new_end=(curWorkIndex+1<workCount)?rollCount*(curWorkIndex+1)/workCount:rollCount;
        assert(new_end+kPartPepeatSize<=m_newData->streamSize);
        TStreamInputClip newClip;
        TStreamInputClip_init(&newClip,m_newData,new_begin,new_end+kPartPepeatSize);
        _search_cover(&newClip.base,new_begin,out_covers,pmem,
                      mt.oldDataLocker.locker,mt.newDataLocker.locker,mt.dataLocker.locker);
    }
#endif 
}

static inline void __search_cover_mt(TDigestMatcher* self,hpatch_TOutputCovers* out_covers,
                                     unsigned char* pmem,void* mt_data){
    self->_search_cover_thread(out_covers,pmem,mt_data);
}

void TDigestMatcher::search_cover(hpatch_TOutputCovers* out_covers){
    if (m_blocks.empty()) return;
    if (m_newData->streamSize<m_kMatchBlockSize) return;
    _out_diff_info("    search covers by match table ...\n");
#if (_IS_USED_MULTITHREAD)
    size_t threadNum=getSearchThreadNum();
    if (threadNum>1){
        const hpatch_StreamPos_t rollCount=m_newData->streamSize-(m_kMatchBlockSize-1);
        size_t bestStep=(kBestParallelSize/2>m_kMatchBlockSize)?kBestParallelSize:2*m_kMatchBlockSize;
        hpatch_StreamPos_t workCount=(rollCount+bestStep-1)/bestStep;
        workCount=(threadNum>workCount)?threadNum:workCount;
        mt_data_t mt_data;
        mt_data.workCount=(uint_work)workCount;
        if ((sizeof(mt_data.workCount)<sizeof(workCount))&&(mt_data.workCount!=workCount))
            throw std::runtime_error("TDigestMatcher::search_cover() multi-thread workCount error!");
        mt_data.workIndex=0;
        const size_t threadCount=threadNum-1;
        std::vector<std::thread> threads(threadCount);
        unsigned char* pmem=m_mem.data();
        for (size_t i=0;i<threadCount;i++,pmem+=(m_newCacheSize+m_oldCacheSize))
            threads[i]=std::thread(__search_cover_mt,this,out_covers,pmem,&mt_data);
        __search_cover_mt(this,out_covers,pmem,&mt_data);
        for (size_t i=0;i<threadCount;i++)
            threads[i].join();
        out_covers->collate_covers(out_covers);
    }else
#endif
    {
        _search_cover(m_newData,0,out_covers,m_mem.data());
    }
}

}//namespace hdiff_private
