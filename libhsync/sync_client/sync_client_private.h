//  sync_client_private.h
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
#ifndef sync_client_private_h
#define sync_client_private_h
#include "sync_client_type_private.h"
#include "sync_client.h"
#include "match_in_old.h"
#include "match_in_types.h"
#include "../../libHDiffPatch/HDiff/private_diff/mem_buf.h"
#include "../../libHDiffPatch/HDiff/private_diff/limit_mem_diff/bloom_filter.h"
struct hpatch_TFileStreamOutput;
using namespace hdiff_private;
namespace sync_private{
    struct TSyncDiffData;

    TSyncClient_resultType
        _sync_patch(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,TSyncDiffData* diffData,
                    const hpatch_TStreamInput* oldStream,const TNewDataSyncInfo* newSyncInfo,
                    const hpatch_TStreamOutput* out_newStream,const hpatch_TStreamInput* newDataContinue,
                    const hpatch_TStreamOutput* out_diffStream,TSyncDiffType diffType,
                    const hpatch_TStreamInput* diffContinue,int threadNum);

    
    struct _TWriteDatas {
        const hpatch_TStreamOutput* out_newStream;
        const hpatch_TStreamInput*  newDataContinue;
        const hpatch_TStreamOutput* out_diffStream;
        hpatch_StreamPos_t          outDiffDataPos;
        const hpatch_TStreamInput*  oldStream;
        const TNewDataSyncInfo*     newSyncInfo;
        const TNeedSyncInfos*       needSyncInfo;
        const hpatch_StreamPos_t*   newBlockDataInOldPoss;
        TSyncDiffData*              continueDiffData;
        uint32_t                    needSyncBlockCount;
        bool                        isLocalPatch;
        hpatch_checksumHandle       checkChecksum;
        hsync_TDictDecompress*      decompressPlugin;
        IReadSyncDataListener*      syncDataListener;
    };
    
    struct _ISyncPatch_by{
        void  (*matchNewDataInOld)(hpatch_StreamPos_t* out_newBlockDataInOldPoss,const TNewDataSyncInfo* newSyncInfo,
                                   const hpatch_TStreamInput* oldStream,int threadNum);
        TSyncClient_resultType (*writeToNewOrDiff)(_TWriteDatas& wd,bool& isNeed_readSyncDataEnd);
        void  (*checkChecksumInit)(unsigned char* checkChecksumBuf,size_t checksumByteSize);
        void (*checkChecksumEndTo)(unsigned char* dst,unsigned char* checkChecksumBuf,
                                   hpatch_TChecksum* checkChecksumPlugin,hpatch_checksumHandle checkChecksum);
    };

    TSyncClient_resultType 
        _sync_patch_by(_ISyncPatch_by* sp_by,
                       ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,TSyncDiffData* diffData,
                       const hpatch_TStreamInput* oldStream,const TNewDataSyncInfo* newSyncInfo,
                       const hpatch_TStreamOutput* out_newStream,const hpatch_TStreamInput* newDataContinue,
                       const hpatch_TStreamOutput* out_diffStream,TSyncDiffType diffType,
                       const hpatch_TStreamInput* diffContinue,int threadNum);

    bool _open_continue_out(hpatch_BOOL& isOutContinue,const char* outFile,hpatch_TFileStreamOutput* out_stream,
                            hpatch_TStreamInput* continue_stream,hpatch_StreamPos_t maxContinueLength=hpatch_kNullStreamPos);

    struct _TMatchDatas{
        hpatch_StreamPos_t*         out_newBlockDataInOldPoss;
        const TNewDataSyncInfo*     newSyncInfo;
        const hpatch_TStreamInput*  oldStream;
        void*               filter;
        const uint32_t*     sorted_newIndexs;
        const uint32_t*     sorted_newIndexs_table;
        unsigned int        kTableHashShlBit;
        uint32_t            threadNum;
        void (*rollMatch)(_TMatchDatas& rd,hpatch_StreamPos_t oldRollBegin,hpatch_StreamPos_t oldRollEnd,void* _mt);
        const uint32_t* (*getSortedIndexs)(TAutoMem& _mem_sorted,const TNewDataSyncInfo* newSyncInfo,void* filter);
    };

    
    template<class TFilter_t> static hpatch_inline
    const uint32_t* _tm_getSortedIndexs(TAutoMem& _mem_sorted,const TNewDataSyncInfo* newSyncInfo,
                                        TFilter_t& filter){
        const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
        const uint32_t kMatchBlockCount=kBlockCount-newSyncInfo->samePairCount;
        const size_t savedRollHashByteSize=newSyncInfo->savedRollHashByteSize;
        const size_t needRollHashByteSize=savedRollHashByteSize*(newSyncInfo->isSeqMatch?2:1);
        typedef typename TFilter_t::value_type filter_value_t;
        _mem_sorted.realloc(kMatchBlockCount*(size_t)sizeof(uint32_t));
        uint32_t* sorted_newIndexs=(uint32_t*)_mem_sorted.data();

        const uint8_t* partRollHash=newSyncInfo->rollHashs;
        uint32_t curPair=0;
        uint32_t indexi=0;
        for (uint32_t i=0;i<kBlockCount;++i,partRollHash+=savedRollHashByteSize){
            if ((curPair<newSyncInfo->samePairCount)&&(i==newSyncInfo->samePairList[curPair].curIndex)){
                ++curPair;
            }else{
                sorted_newIndexs[indexi++]=i;
                filter.insert((filter_value_t)readRollHashBytes(partRollHash,needRollHashByteSize));
            }
        }
        assert(indexi==kMatchBlockCount);
        assert(curPair==newSyncInfo->samePairCount);

        TIndex_comp01 icomp01(newSyncInfo->rollHashs,savedRollHashByteSize,
                              newSyncInfo->partChecksums,newSyncInfo->savedStrongChecksumByteSize,newSyncInfo->isSeqMatch);
        std::sort(sorted_newIndexs,sorted_newIndexs+kMatchBlockCount,icomp01);
        return sorted_newIndexs;
    }

    static inline void _init_newBlockDataInOldPoss(hpatch_StreamPos_t* newBlockDataInOldPoss,uint32_t kBlockCount){
                            for (uint32_t i=0; i<kBlockCount; ++i) newBlockDataInOldPoss[i]=kBlockType_needSync; }
    static inline void _end_newBlockDataInOldPoss(hpatch_StreamPos_t* newBlockDataInOldPoss,uint32_t kBlockCount,
                                                  hpatch_StreamPos_t oldSize){
                            for (uint32_t i=0; i<kBlockCount; ++i){
                                hpatch_StreamPos_t& pos=newBlockDataInOldPoss[i];
                                pos=(pos<oldSize)?pos:kBlockType_needSync;
                            } }

#if (_IS_USED_MULTITHREAD)
    struct _TMatchDatas;
    namespace{
        struct TMt:public TMtByChannel{
            inline explicit TMt(struct _TMatchDatas& _matchDatas)
                :matchDatas(_matchDatas),workIndex(0){}
            CHLocker readLocker;
            CHLocker writeLocker;
        #if (_IS_USED_CPP_ATOMIC)
            std::atomic<size_t>  workIndex;
        #else
            CHLocker         workiLocker;
            volatile size_t  workIndex;
            inline bool getWorki(size_t worki){
                if (workIndex>worki) return false;
                CAutoLocker _auto_locker(workiLocker.locker);
                if (workIndex==worki){
                    ++workIndex;
                    return true;
                }else
                    return false;
            }
        #endif
            struct _TMatchDatas& matchDatas;
        };
    }

    void _rollMatch_mt(int threadIndex,void* workData);

#endif

    struct TStreamDataCache_base;
    bool _matchRange(hpatch_StreamPos_t* out_newBlockDataInOldPoss,const uint32_t* range_begin,const uint32_t* range_end,
                     TStreamDataCache_base& oldData,const TNewDataSyncInfo* newSyncInfo,hpatch_StreamPos_t kMinRevSameIndex,void* _mt);


    template<class TOldDataRoll_t,class TFilter_t,bool isSeqMatch,class TFunc_savedPartRollHash>
    static hpatch_inline
    void _tm_rollMatch(_TMatchDatas& rd,hpatch_StreamPos_t oldRollBegin,hpatch_StreamPos_t oldRollEnd,
                       TFunc_savedPartRollHash f_toSavedPartRollHash,void* _mt){
        const uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(rd.newSyncInfo);
        const hpatch_StreamPos_t kMinRevSameIndex=kBlockType_needSync-1-kBlockCount;
        const size_t savedRollHashByteSize=rd.newSyncInfo->savedRollHashByteSize;
        assert(isSeqMatch==(rd.newSyncInfo->isSeqMatch!=0));
        const size_t needRollHashByteSize=savedRollHashByteSize*(isSeqMatch?2:1);
        TIndex_comp0 icomp0(rd.newSyncInfo->rollHashs,savedRollHashByteSize);
        TIndex_comp0 icomp0_s(rd.newSyncInfo->rollHashs,savedRollHashByteSize,isSeqMatch);
        assert(oldRollBegin<oldRollEnd);
        TOldDataRoll_t  oldData(rd.oldStream,oldRollBegin,oldRollEnd,
                                rd.newSyncInfo->kSyncBlockSize,rd.newSyncInfo->strongChecksumPlugin
                            #if (_IS_USED_MULTITHREAD)
                                ,_mt?((TMt*)_mt)->readLocker.locker:0
                            #endif
                                );
        uint8_t part[sizeof(tm_roll_uint)]={0};
        const size_t savedRollHashBits=rd.newSyncInfo->savedRollHashBits;
        const TFilter_t& filter=*(TFilter_t*)rd.filter;
        typedef typename TFilter_t::value_type filter_value_t;
        tm_roll_uint digestFull_back=~oldData.hashValue(); //not same digest
        hpatch_StreamPos_t curOldPos=oldData.curStreamPos();
        hpatch_StreamPos_t _matchedPosBack=isSeqMatch?curOldPos:~(hpatch_StreamPos_t)0;
        while (true) {
            if (isSeqMatch) curOldPos=oldData.curStreamPos();
            tm_roll_uint digest=oldData.hashValue();
            if (digestFull_back!=digest){
                digestFull_back=digest;
                digest=f_toSavedPartRollHash(digest,savedRollHashBits);
                if ((!isSeqMatch)||(_matchedPosBack!=curOldPos)){
                    if (!filter.is_hit((filter_value_t)digest))
                        { if (oldData.roll()) continue; else break; }//finish
                }
            }else{
                if (oldData.roll()) continue; else break; //finish
            }
            
            const uint32_t* ti_pos0;
            const uint32_t* ti_pos1;
            if ((!isSeqMatch)||((savedRollHashBits<=rd.kTableHashShlBit)||(_matchedPosBack!=curOldPos))){
                const uint32_t* ti_pos=&rd.sorted_newIndexs_table[digest>>rd.kTableHashShlBit];
                ti_pos0=rd.sorted_newIndexs+ti_pos[0];
                ti_pos1=rd.sorted_newIndexs+ti_pos[1];
            }else{
                digest=digest>>savedRollHashBits<<savedRollHashBits; //remove next rollHash
                ti_pos0=rd.sorted_newIndexs+rd.sorted_newIndexs_table[digest>>rd.kTableHashShlBit];
                digest=digest|((1<<savedRollHashBits)-1);
                ti_pos1=rd.sorted_newIndexs+rd.sorted_newIndexs_table[(digest>>rd.kTableHashShlBit)+1];
            }
            writeRollHashBytes(part,digest,needRollHashByteSize);
            TIndex_comp0::TDigest digest_value(part);
            const TIndex_comp0& _icomp0=((!isSeqMatch)||(_matchedPosBack!=curOldPos))?icomp0_s:icomp0;
            std::pair<const uint32_t*,const uint32_t*>
            //range=std::equal_range(rd.sorted_newIndexs,rd.sorted_newIndexs+(kBlockCount-rd.newSyncInfo->samePairCount),digest_value,_icomp0);
            range=std::equal_range(ti_pos0,ti_pos1,digest_value,_icomp0);
            if (range.first==range.second)
                { if (oldData.roll()) continue; else break; }//finish

            bool isMatched=_matchRange(rd.out_newBlockDataInOldPoss,range.first,range.second,oldData,
                                       rd.newSyncInfo,kMinRevSameIndex,_mt);
            if (isMatched){
                if (!isSeqMatch) curOldPos=oldData.curStreamPos();
                _matchedPosBack=curOldPos+rd.newSyncInfo->kSyncBlockSize;
                if (kIsSkipMatchedBlock){
                    if (oldData.nextBlock()) { digestFull_back=~oldData.hashValue(); continue; } else break;
                }//else roll
            }//else roll
            if (oldData.roll()) continue; else break;
        }
    }

    void _matchNewDataInOld(_TMatchDatas& matchDatas,int threadNum);
    
    struct _IWriteToNewOrDiff_by{
        void (*checkChecksumAppendData)(unsigned char* checkChecksumBuf,uint32_t checksumIndex,
                                        hpatch_TChecksum* checkChecksumPlugin,hpatch_checksumHandle checkChecksum,
                                        const unsigned char* blockChecksum,const unsigned char* blockData,size_t blockDataSize);
    };
    TSyncClient_resultType _writeToNewOrDiff_by(_IWriteToNewOrDiff_by* wr_by,_TWriteDatas& wd,bool& isNeed_readSyncDataEnd);

}//namespace sync_private

#endif // sync_client_private_h
