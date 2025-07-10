//  zsync_client_type_private.h
//  zsync_client for hsynz
//  Created by housisong on 2025-07-04.
/*
 The MIT License (MIT)
 Copyright (c) 2015 HouSisong
 
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
#ifndef zsync_client_type_private_h
#define zsync_client_type_private_h
#include "../sync_client/sync_client_type_private.h"

namespace sync_private{

inline static hpatch_uint32_t z_roll_hash32_start(const adler_data_t* pdata,size_t n){
    hpatch_uint32_t radler=0; hpatch_uint32_t sum=0;
    for (;n>0;n--){
        radler+=*pdata++;
        sum+=radler;
    }
    return (sum<<16) | (radler&0xFFFF);
}
    
inline static hpatch_uint32_t z_roll_hash32_roll(hpatch_uint32_t adler,size_t blockSize,
                                                 adler_data_t out_data,adler_data_t in_data){
    hpatch_uint32_t sum=(adler>>16)-((hpatch_uint32_t)blockSize)*out_data;
    hpatch_uint32_t radler=adler+in_data-out_data;
    return ((sum+radler)<<16) | (radler&0xFFFF);
}


struct zsync_checkChecksum_t{
    volatile hpatch_uint32_t checkBlockIndex;
    volatile hpatch_byte     state; //0: not run, 1: checking, 2: end
};

static hpatch_inline
size_t z_checkChecksumBufByteSize(size_t kStrongChecksumByteSize){
        return 2*kStrongChecksumByteSize+sizeof(zsync_checkChecksum_t); }

void z_checkChecksumInit(unsigned char* checkChecksumBuf,size_t kStrongChecksumByteSize);


//must call by checksumIndex order
void z_checkChecksumAppendData(unsigned char* checkChecksumBuf,uint32_t checksumIndex,
                               hpatch_TChecksum* checkChecksumPlugin,hpatch_checksumHandle checkChecksum,
                               const unsigned char* blockChecksum,const unsigned char* blockData,size_t blockDataSize);

void z_checkChecksumEndTo(unsigned char* dst,unsigned char* checkChecksumBuf,
                          hpatch_TChecksum* checkChecksumPlugin,hpatch_checksumHandle checkChecksum);

static hpatch_inline
hpatch_uint64_t z_toSavedPartRollHash(hpatch_uint64_t _rollHash,size_t savedRollHashBits){
    //byte order: 0123 -> 2301
    hpatch_uint32_t rollHash=(hpatch_uint32_t)_rollHash;
    rollHash= (rollHash<<16) | (rollHash>>16);
    return rollHash<<(32-savedRollHashBits)>>(32-savedRollHashBits);
}

} //namespace sync_private
#endif //zsync_client_type_private_h
