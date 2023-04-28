//  sync_make_hash_clash.h
//  sync_make
//  Created by housisong on 2019-09-17.
/*
 The MIT License (MIT)
 Copyright (c) 2019-2022 HouSisong
 
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
#ifndef hsync_make_hash_clash_h
#define hsync_make_hash_clash_h
#include "../sync_client/sync_client_type.h"
#include "../sync_client/sync_client_type_private.h"

namespace sync_private{
    const size_t _kMaxRollHashBits      = 8*sizeof(uint64_t);
    
    hpatch_inline static
    size_t _estimateCompareCountBit(hpatch_StreamPos_t newDataSize,uint32_t kSyncBlockSize){
        hpatch_StreamPos_t blockCount=getSyncBlockCount(newDataSize,kSyncBlockSize);
        const hpatch_StreamPos_t nbmul=newDataSize*blockCount;
        if ((blockCount==0)||(nbmul/blockCount)==newDataSize)
            return sync_private::upper_ilog2(nbmul);
        else
            return sync_private::upper_ilog2(newDataSize)+sync_private::upper_ilog2(blockCount);
    }
    
    hpatch_inline static
    size_t getNeedHashBits(size_t kSafeHashClashBit,hpatch_StreamPos_t newDataSize,uint32_t kSyncBlockSize){
        const size_t _kNeedMinHashBits=_kNeedMinRollHashBits+_kNeedMinStrongHashBits;
        size_t compareCountBit=_estimateCompareCountBit(newDataSize,kSyncBlockSize);
        size_t result=compareCountBit+kSafeHashClashBit;
        return (result>=_kNeedMinHashBits)?result:_kNeedMinHashBits;
    }
    hpatch_inline static
    size_t getSavedHashBits(size_t kSafeHashClashBit,hpatch_StreamPos_t newDataSize,uint32_t kSyncBlockSize,
                            size_t kStrongHashBits,size_t* out_partRollHashBits,size_t* out_partStrongHashBits){
        const size_t result=getNeedHashBits(kSafeHashClashBit,newDataSize,kSyncBlockSize);
        assert(kStrongHashBits>=kStrongChecksumByteSize_min*8);
        assert(result<=kStrongHashBits+_kMaxRollHashBits);
        size_t compareCountBit=_estimateCompareCountBit(newDataSize,kSyncBlockSize);
        size_t rollHashBits=compareCountBit;
        if (rollHashBits<_kNeedMinRollHashBits) rollHashBits=_kNeedMinRollHashBits;
        else if (rollHashBits>_kMaxRollHashBits) rollHashBits=_kMaxRollHashBits;
        assert(rollHashBits<=result);
        size_t strongHashBits=result-rollHashBits;
        if (strongHashBits<=_kNeedMinStrongHashBits){
            size_t moveBits=_kNeedMinStrongHashBits-strongHashBits;
            assert(rollHashBits>=moveBits+_kNeedMinRollHashBits);
            rollHashBits-=moveBits;
            strongHashBits+=moveBits;
        }else{
            size_t moveBits=(strongHashBits-_kNeedMinStrongHashBits+2)/3;
            moveBits=(moveBits<=_kNeedMinRollHashBits)?moveBits:_kNeedMinRollHashBits;
            if (rollHashBits+moveBits>=_kMaxRollHashBits)
                moveBits=_kMaxRollHashBits-rollHashBits;
            strongHashBits-=moveBits;
            rollHashBits+=moveBits;
        }
        if (strongHashBits>kStrongHashBits){
            strongHashBits=kStrongHashBits;
            rollHashBits=result-kStrongHashBits;
            assert((rollHashBits>=_kNeedMinRollHashBits)&&(rollHashBits<=_kMaxRollHashBits));
        }
        assert(result==rollHashBits+strongHashBits);
        *out_partRollHashBits=rollHashBits;
        *out_partStrongHashBits=strongHashBits;
        return result;
    }
}//namespace sync_private


hpatch_inline static //check strongChecksumBits is strong enough?
bool getStrongForHashClash(size_t kSafeHashClashBit,hpatch_StreamPos_t newDataSize,uint32_t kSyncBlockSize,
                           size_t strongChecksumBits){
    if (strongChecksumBits<kStrongChecksumByteSize_min*8)
        return false;
    size_t needHashBits=sync_private::getNeedHashBits(kSafeHashClashBit,newDataSize,kSyncBlockSize);
    return sync_private::_kMaxRollHashBits+strongChecksumBits>=needHashBits;
}

hpatch_inline static
hpatch_StreamPos_t estimatePatchMemSize(size_t kSafeHashClashBit,hpatch_StreamPos_t newDataSize,
                                        uint32_t kSyncBlockSize,bool isUsedCompress){
    hpatch_StreamPos_t blockCount=getSyncBlockCount(newDataSize,kSyncBlockSize);
    hpatch_StreamPos_t bet=24;
    bet+= _bitsToBytes(sync_private::getNeedHashBits(kSafeHashClashBit,newDataSize,kSyncBlockSize)+4);
    bet+=isUsedCompress?4:0;
    return bet*blockCount + 2*(hpatch_StreamPos_t)kSyncBlockSize;
}

#endif // hsync_make_hash_clash_h
