//  match_in_new.cpp
//  sync_make
//  Created by housisong on 2019-09-25.
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
#include "match_in_new.h"
#include <algorithm> //sort, equal_range lower_bound
#include "../sync_client/sync_client_type_private.h"
#include "../../libHDiffPatch/HDiff/private_diff/mem_buf.h"

using namespace hdiff_private;
namespace sync_private{

typedef uint64_t tm_roll_uint;

struct TIndex_comp{
    inline explicit TIndex_comp(const uint8_t* _hashs,size_t _byteSize)
    :hashs(_hashs),byteSize(_byteSize){ }
    typedef uint32_t TIndex;
    struct TDigest{
        const uint8_t*  hashs;
        TIndex          index;
        inline explicit TDigest(const uint8_t* _hashs,TIndex _index)
        :hashs(_hashs),index(_index){}
    };
    inline bool operator()(const TIndex x,const TDigest& y)const { //for equal_range
        int cmp=_cmp(hashs+x*byteSize,y.hashs+y.index*byteSize,byteSize);
        if (cmp!=0)
            return cmp<0; //value
        else
            return y.index<=x; //for: assert(newBlockIndex<i);
    }
    bool operator()(const TDigest& x,const TIndex y)const { //for equal_range
        int cmp=_cmp(x.hashs+x.index*byteSize,hashs+y*byteSize,byteSize);
        if (cmp!=0)
            return cmp<0; //value
        else
            return x.index<=y; //for: assert(newBlockIndex<i);
    }
    
    inline bool operator()(const TIndex x, const TIndex y)const {//for sort
        int cmp=_cmp(hashs+x*byteSize,hashs+y*byteSize,byteSize);
        if (cmp!=0)
            return cmp<0; //value sort
        else
            return x>y; //index sort
    }
protected:
    const uint8_t*  hashs;
    size_t          byteSize;
    inline static int _cmp(const uint8_t* px,const uint8_t* py,size_t byteSize){
        const uint8_t* px_end=px+byteSize;
        for (;px!=px_end;++px,++py){
            int sub=(int)(*px)-(*py);
            if (sub!=0) return sub; //value sort
        }
        return 0;
    }
};

void matchNewDataInNew(TNewDataSyncInfo* newSyncInfo){
    uint32_t kBlockCount=(uint32_t)TNewDataSyncInfo_blockCount(newSyncInfo);
    const unsigned char* partChecksums=newSyncInfo->partChecksums;
    TSameNewBlockPair* samePairList=newSyncInfo->samePairList;

    TAutoMem _mem(kBlockCount*(size_t)sizeof(uint32_t));
    uint32_t* sorted_newIndexs=(uint32_t*)_mem.data();
    for (uint32_t i=0; i<kBlockCount; ++i){
        sorted_newIndexs[i]=i;
    }
    const size_t savedRollHashByteSize=newSyncInfo->savedRollHashByteSize;
    const size_t savedRollHashBits=newSyncInfo->savedRollHashBits;
    TIndex_comp icomp(newSyncInfo->rollHashs,savedRollHashByteSize);
    std::sort(sorted_newIndexs,sorted_newIndexs+kBlockCount,icomp);
    
    //optimize for std::equal_range
    unsigned int kTableBit =getBetterCacheBlockTableBit(kBlockCount);
    if (kTableBit>savedRollHashBits) kTableBit=(unsigned int)savedRollHashBits;
    const unsigned int kTableHashShlBit=(unsigned int)(savedRollHashBits-kTableBit);
    TAutoMem _mem_table((size_t)sizeof(uint32_t)*((1<<kTableBit)+1));
    uint32_t* sorted_newIndexs_table=(uint32_t*)_mem_table.data();
    {
        uint32_t* pos=sorted_newIndexs;
        uint8_t part[sizeof(tm_roll_uint)]={0};
        for (uint32_t i=0; i<((uint32_t)1<<kTableBit); ++i) {
            tm_roll_uint digest=((tm_roll_uint)i)<<kTableHashShlBit;
            writeRollHashBytes(part,digest,savedRollHashByteSize);
            TIndex_comp::TDigest digest_value(part,0);
            pos=std::lower_bound(pos,sorted_newIndexs+kBlockCount,digest_value,icomp);
            sorted_newIndexs_table[i]=(uint32_t)(pos-sorted_newIndexs);
        }
        sorted_newIndexs_table[((size_t)1<<kTableBit)]=kBlockCount;
    }

    TIndex_comp dcomp(newSyncInfo->rollHashs,savedRollHashByteSize);
    uint32_t matchedCount=0;
    const unsigned char* curChecksum=partChecksums;
    for (uint32_t i=0; i<kBlockCount; ++i,curChecksum+=newSyncInfo->savedStrongChecksumByteSize){
        TIndex_comp::TDigest digest_value(newSyncInfo->rollHashs,i);
        tm_roll_uint digest=readRollHashBytes(newSyncInfo->rollHashs+i*savedRollHashByteSize,
                                              savedRollHashByteSize);
        const uint32_t* ti_pos=&sorted_newIndexs_table[digest>>kTableHashShlBit];
        std::pair<const uint32_t*,const uint32_t*>
            //range=std::equal_range(sorted_newIndexs,sorted_newIndexs+kBlockCount,digest_value,dcomp);
            range=std::equal_range(sorted_newIndexs+ti_pos[0],
                                   sorted_newIndexs+ti_pos[1],digest_value,dcomp);
        for (;range.first!=range.second; ++range.first) {
            uint32_t newBlockIndex=*range.first;
            assert(newBlockIndex<i);
            const unsigned char* newChecksum=partChecksums+newBlockIndex*newSyncInfo->savedStrongChecksumByteSize;
            if (0==memcmp(newChecksum,curChecksum,newSyncInfo->savedStrongChecksumByteSize)){
                samePairList[matchedCount].curIndex=i;
                samePairList[matchedCount].sameIndex=newBlockIndex;
                ++matchedCount;
                break;
            }
        }
    }
    //printf("matchedCount: %d\n",matchedCount);
    newSyncInfo->samePairCount=matchedCount;
}

}//namespace sync_private

