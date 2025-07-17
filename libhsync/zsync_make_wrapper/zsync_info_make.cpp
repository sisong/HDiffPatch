//  zsync_info_make.cpp
//  zsync_make for hsynz
//  Created by housisong on 2025-07-16.
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
#include "zsync_info_make.h"
#include "../sync_make/sync_make_hash_clash.h"
#include "../sync_client/sync_info_client.h" // TNewDataSyncInfo_dir_saveHeadTo
using namespace hdiff_private;
namespace sync_private{


void TNewDataZsyncInfo_saveTo(TNewDataZsyncInfo* self,const hpatch_TStreamOutput* out_stream,
                              hsync_TDictCompress* compressPlugin){
    //todo:
}


static size_t z_getSavedHashBits(size_t kSafeHashClashBit,hpatch_StreamPos_t newDataSize,uint32_t kSyncBlockSize,size_t kStrongHashBits,
                                 size_t* out_partRollHashBits,size_t* out_partStrongHashBits,uint8_t* out_isSeqMatch){
    //todo:
    return -1;
}

static bool z_isNeedSamePair(){
    return false;
}

CNewDataZsyncInfo::CNewDataZsyncInfo(hpatch_TChecksum* _fileChecksumPlugin,hpatch_TChecksum* _strongChecksumPlugin,const hsync_TDictCompress* compressPlugin,
                                     hpatch_StreamPos_t newDataSize,uint32_t syncBlockSize,size_t kSafeHashClashBit){
    _ISyncInfo_by si_by={0};
    si_by.getSavedHashBits=z_getSavedHashBits;
    si_by.isNeedSamePair=z_isNeedSamePair;
    _init_by(&si_by,_fileChecksumPlugin,_strongChecksumPlugin,compressPlugin,
             newDataSize,syncBlockSize,kSafeHashClashBit);
    this->isNotCChecksumNewMTParallel=hpatch_TRUE; //unsupport parallel checksum, must check by order
}

}//namespace sync_private

