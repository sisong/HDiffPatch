//match_in_old_sign.cpp
//sign_diff
//Created by housisong on 2025-04-16.
/*
 The MIT License (MIT)
 Copyright (c) 2025 HouSisong
 
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
#include "_match_in_old_sign.h"
#include "../sync_client/match_in_types.h"
namespace sync_private{

    static  const size_t kMinMatchedLength = 16;

#if (_IS_USED_MULTITHREAD)
    struct _TMatchSDatas;
    namespace{
        struct TMt:public TMtByChannel{
            inline explicit TMt(struct _TMatchSDatas& _matchDatas)
                :matchDatas(_matchDatas),workIndex(0){}
            CHLocker readLocker;
            CHLocker writeLocker;
            std::atomic<size_t>  workIndex;
            struct _TMatchSDatas& matchDatas;
        };
    }
#endif //_IS_USED_MULTITHREAD

static bool matchRange(hpatch_TOutputCovers* out_covers,const uint32_t* range_begin,const uint32_t* range_end,
                       TStreamDataCache_base& newData,const TNewDataSyncInfo* oldSyncInfo,uint32_t& oldBlockIndex_bck,void* _mt){
    const TByte* newPartStrongChecksum=0;
    const size_t outPartChecksumSize=_bitsToBytes(oldSyncInfo->savedStrongChecksumBits);
    int hitOutLimit=kMatchHitOutLimit;
    do {
        uint32_t oldBlockIndex=*range_begin;
        if (newPartStrongChecksum==0)
            newPartStrongChecksum=newData.calcPartStrongChecksum(oldSyncInfo->savedStrongChecksumBits);
        const TByte* oldPairStrongChecksum=oldSyncInfo->partChecksums+oldBlockIndex*outPartChecksumSize;
        if (0==memcmp(newPartStrongChecksum,oldPairStrongChecksum,outPartChecksumSize)){
            const hpatch_uint32_t nextIndex=oldBlockIndex_bck+1;
            if (nextIndex!=oldBlockIndex){ //try continuous
                const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(oldSyncInfo);
                const size_t kRollHashByteSize=oldSyncInfo->savedRollHashByteSize;
                if ((nextIndex<kBlockCount)
                  &&(0==memcmp(newPartStrongChecksum,oldSyncInfo->partChecksums+nextIndex*outPartChecksumSize,outPartChecksumSize))
                  &&(0==memcmp(oldSyncInfo->rollHashs+oldBlockIndex*kRollHashByteSize,oldSyncInfo->rollHashs+nextIndex*kRollHashByteSize,kRollHashByteSize))){
                    oldBlockIndex=nextIndex;
                }
            }
            oldBlockIndex_bck=oldBlockIndex;
            return true;
        }else{
            if ((--hitOutLimit)<=0) break;
        }
    }while ((++range_begin)!=range_end);
    return false;
}

    inline static bool _tryCombine(hpatch_TCover& lastCover,const hpatch_TCover& cover){
        if ((lastCover.oldPos+lastCover.length==cover.oldPos)
          &&(lastCover.newPos+lastCover.length==cover.newPos)){
            lastCover.length+=cover.length;
            return true;
        }else
            return false;
    }
    
    inline static void _outACover(hpatch_TOutputCovers* out_covers,hpatch_TCover& cover,void *_mt){
        if (cover.length>=kMinMatchedLength){
    #if (_IS_USED_MULTITHREAD)
            TMt* mt=(TMt*)_mt;
            CAutoLocker _autoLocker(mt?mt->writeLocker.locker:0);
    #endif
            out_covers->push_cover(out_covers,&cover);
        }
    }

    struct _TMatchSDatas{
        hpatch_TOutputCovers*       out_covers;
        const TOldDataSyncInfo*     oldSyncInfo;
        const hpatch_TStreamInput*  newStream;
        hpatch_TChecksum*           strongChecksumPlugin;
        const void*         filter;
        const uint32_t*     sorted_oldIndexs;
        const uint32_t*     sorted_oldIndexs_table;
        size_t              kMatchBlockCount;
        unsigned int        kTableHashShlBit;
        uint32_t            threadNum;
    };

static void _rollMatch(_TMatchSDatas& rd,hpatch_StreamPos_t newRollBegin,
                       hpatch_StreamPos_t newRollEnd,void* _mt=0){
    const hpatch_uint32_t kSyncBlockSize=rd.oldSyncInfo->kSyncBlockSize;
    const hpatch_StreamPos_t oldSize=rd.oldSyncInfo->newDataSize;
    const hpatch_StreamPos_t newSize=rd.newStream->streamSize;
    if (newSize<kSyncBlockSize) return;
    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(rd.oldSyncInfo);
    TIndex_comp0 icomp0(rd.oldSyncInfo->rollHashs,rd.oldSyncInfo->savedRollHashByteSize);
    TStreamDataRoll newData(rd.newStream,newRollBegin,newRollEnd,kSyncBlockSize,rd.strongChecksumPlugin
                        #if (_IS_USED_MULTITHREAD)
                            ,_mt?((TMt*)_mt)->readLocker.locker:0
                        #endif
                            );
    uint8_t part[sizeof(tm_roll_uint)]={0};
    const size_t savedRollHashBits=rd.oldSyncInfo->savedRollHashBits;
    const TBloomFilter<tm_roll_uint>& filter=*(TBloomFilter<tm_roll_uint>*)rd.filter;
    tm_roll_uint digestFull_back=~newData.hashValue(); //not same digest
    uint32_t oldBlockIndex_bck=-1;
    hpatch_TCover cover_prev={0};
    while (true) {
        tm_roll_uint digest=newData.hashValue();
        if (digestFull_back!=digest){
            digestFull_back=digest;
            digest=toSavedPartRollHash(digest,savedRollHashBits);
            if (!filter.is_hit(digest))
                { if (newData.roll()) continue; else break; }//finish
        }else{
            if (newData.roll()) continue; else break; //finish
        }
        
        const uint32_t* ti_pos=&rd.sorted_oldIndexs_table[digest>>rd.kTableHashShlBit];
        writeRollHashBytes(part,digest,rd.oldSyncInfo->savedRollHashByteSize);
        TIndex_comp0::TDigest digest_value(part);
        std::pair<const uint32_t*,const uint32_t*>
        //range=std::equal_range(rd.sorted_oldIndexs,rd.sorted_oldIndexs+rd.kMatchBlockCount,digest_value,icomp0);
        range=std::equal_range(rd.sorted_oldIndexs+ti_pos[0],
                               rd.sorted_oldIndexs+ti_pos[1],digest_value,icomp0);
        if (range.first==range.second)
            { if (newData.roll()) continue; else break; }//finish

        bool isMatched=matchRange(rd.out_covers,range.first,range.second,
                                  newData,rd.oldSyncInfo,oldBlockIndex_bck,_mt);
        if (isMatched){
            hpatch_TCover cover;
            cover.oldPos=oldBlockIndex_bck*(hpatch_StreamPos_t)kSyncBlockSize;
            cover.newPos=newData.curStreamPos();
            cover.length=kSyncBlockSize;
            assert(cover.oldPos<oldSize);
            assert(cover.newPos<newSize);
            if (cover.oldPos+cover.length>oldSize) cover.length=oldSize-cover.oldPos;
            if (cover.newPos+cover.length>newSize) cover.length=newSize-cover.newPos;
            if (!_tryCombine(cover_prev,cover)){
                _outACover(rd.out_covers,cover_prev,_mt);
                cover_prev=cover;
            }
            if (newData.nextBlock()){ digestFull_back=~newData.hashValue(); continue; } else break;
        }//else roll
        if (newData.roll()) continue; else break;
    }
    _outACover(rd.out_covers,cover_prev,_mt);
}


#if (_IS_USED_MULTITHREAD)
static void _rollMatch_mt(int threadIndex,void* workData){
    TMt& mt=*(TMt*)workData;
    TMtByChannel::TAutoThreadEnd __auto_thread_end(mt);

    hpatch_StreamPos_t clipSize=kBestMTClipSize;
    hpatch_StreamPos_t _minClipSize=(hpatch_StreamPos_t)(mt.matchDatas.oldSyncInfo->kSyncBlockSize)*4;
    if (clipSize<_minClipSize) clipSize=_minClipSize;
    uint32_t threadNum=mt.matchDatas.threadNum;
    hpatch_StreamPos_t newSize=mt.matchDatas.newStream->streamSize;
    hpatch_StreamPos_t _maxClipSize=(newSize+threadNum-1)/threadNum;
    if (clipSize>_maxClipSize) clipSize=_maxClipSize;
    size_t workCount=(size_t)((newSize+clipSize-1)/clipSize);
    assert(workCount*clipSize>=newSize);
    try{
        std::atomic<size_t>& workIndex=*(std::atomic<size_t>*)&mt.workIndex;
        while (true){
            size_t curWorkIndex=workIndex++;
            if (curWorkIndex>=workCount) break;
            hpatch_StreamPos_t newPosBegin=curWorkIndex*clipSize;
            hpatch_StreamPos_t newPosEnd = newPosBegin+clipSize;
            if (newPosEnd>newSize) newPosEnd=newSize;
            _rollMatch(mt.matchDatas,newPosBegin,newPosEnd,&mt);
        }
    }catch(...){
        mt.on_error();
    }
}
#endif

static void _matchNewDataInOldSign(_TMatchSDatas& matchDatas,int threadNum){
    const TOldDataSyncInfo* oldSyncInfo=matchDatas.oldSyncInfo;
    const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(oldSyncInfo);
    const uint32_t kMatchBlockCount=kBlockCount-oldSyncInfo->samePairCount;
    
    TAutoMem _mem_sorted;
    TBloomFilter<tm_roll_uint> filter;
    const uint32_t* sorted_oldIndexs=getSortedIndexs(_mem_sorted,oldSyncInfo,filter);
    
    TAutoMem _mem_table;
    unsigned int kTableHashShlBit=0;
    const uint32_t* sorted_oldIndexs_table=getSortedIndexsTable(_mem_table,oldSyncInfo,sorted_oldIndexs,&kTableHashShlBit);

    matchDatas.filter=&filter;
    matchDatas.sorted_oldIndexs=sorted_oldIndexs;
    matchDatas.kMatchBlockCount=kMatchBlockCount;
    matchDatas.sorted_oldIndexs_table=sorted_oldIndexs_table;
    matchDatas.kTableHashShlBit=kTableHashShlBit;
#if (_IS_USED_MULTITHREAD)
    const hpatch_StreamPos_t _bestWorkCount=matchDatas.newStream->streamSize/kBestMTClipSize;
    threadNum=(_bestWorkCount<=threadNum)?(int)_bestWorkCount:threadNum;
    if (threadNum>1){
        matchDatas.threadNum=threadNum;
        TMt mt(matchDatas);
        checkv(mt.start_threads(threadNum,_rollMatch_mt,&mt,true));
        mt.wait_all_thread_end();
        checkv(!mt.is_on_error());
    }else
#endif
    {
        _rollMatch(matchDatas,0,matchDatas.newStream->streamSize);
    }
    matchDatas.out_covers->collate_covers(matchDatas.out_covers);
}


void matchNewDataInOldSign(hpatch_TOutputCovers* out_covers,const hpatch_TStreamInput* newStream,
                           const TOldDataSyncInfo* oldSyncInfo,int threadNum){
    checkv(oldSyncInfo->_strongChecksumPlugin!=0);
    _TMatchSDatas matchDatas; memset(&matchDatas,0,sizeof(matchDatas));
    matchDatas.out_covers=out_covers;
    matchDatas.oldSyncInfo=oldSyncInfo;
    matchDatas.newStream=newStream;
    matchDatas.strongChecksumPlugin=oldSyncInfo->_strongChecksumPlugin;
    _matchNewDataInOldSign(matchDatas,threadNum);
}

} //namespace sync_private
