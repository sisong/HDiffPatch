//  sync_patch_hash_clash.h
//  sync_server
//  Created by housisong on 2019-09-17.
/*
 The MIT License (MIT)
 Copyright (c) 2019-2019 HouSisong
 
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
#ifndef sync_patch_hash_clash_h
#define sync_patch_hash_clash_h
#include "../sync_client/sync_client_type.h"

const int kAllowMaxHashClashBit = -48; // (1/2^48)

hpatch_inline static
int estimateHashClashBit(hpatch_StreamPos_t newDataSize,uint32_t kMatchBlockSize,
                         bool isUse32bitRollHash=false){
    //clash=oldDataSize*(newDataSize/kMatchBlockSize)/2^64/2^64
    long double blockCount=(long double)getSyncBlockCount(newDataSize,kMatchBlockSize);
    int cmpHashCountBit=upper_ilog2(newDataSize*blockCount);
    return cmpHashCountBit-(isUse32bitRollHash?32:64)-kPartStrongChecksumByteSize*8;
}

namespace sync_private{
    hpatch_inline static
    bool isCanUse32bitRollHash(hpatch_StreamPos_t newDataSize,uint32_t kMatchBlockSize){
        const bool isUse32bitRollHash=true;
        int clashBit=estimateHashClashBit(newDataSize,kMatchBlockSize,isUse32bitRollHash);
        return clashBit<=kAllowMaxHashClashBit;
    }
    
}//namespace sync_private

hpatch_inline static
hpatch_StreamPos_t estimatePatchMemSize(hpatch_StreamPos_t newDataSize,
                                        uint32_t kMatchBlockSize,bool isUsedCompress){
    hpatch_StreamPos_t blockCount=getSyncBlockCount(newDataSize,kMatchBlockSize);
    bool  isUse32bitRollHash=sync_private::isCanUse32bitRollHash(newDataSize,kMatchBlockSize);
    hpatch_StreamPos_t bet=40;
    if (isUse32bitRollHash) bet-=4;
    if (isUsedCompress)     bet+=4;
    return bet*blockCount + 2*(hpatch_StreamPos_t)kMatchBlockSize;
}

#endif // sync_patch_hash_clash_h
