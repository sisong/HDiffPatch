//  zsync_make_wrapper.cpp
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
#include "zsync_make_wrapper.h"
#include "zsync_info_make.h"
#include "../zsync_client_wrapper/zsync_client_type_private.h"
#include "../sync_make/sync_make_private.h"

using namespace hdiff_private;
using namespace sync_private;

namespace sync_private{

static tm_roll_uint z_roll_hash_start(const adler_data_t* pdata,size_t n){
    return z_roll_hash32_start(pdata,n);
}

static
void _private_create_zsync_data(TNewDataSyncInfo* newSyncInfo, const hpatch_TStreamInput*  newData,
                                const hpatch_TStreamOutput* out_hsyni, const hpatch_TStreamOutput* out_hsynz,
                                hsync_TDictCompress* compressPlugin, hsync_THsynz* hsynzPlugin,
                                const std::vector<std::string>& zsyncKeyValues,size_t threadNum){
    _ICreateSync_by cs_by={0};
    cs_by.roll_hash_start=z_roll_hash_start;
    cs_by.toSavedPartRollHash=z_toSavedPartRollHash;
    cs_by.checkChecksumInit=z_checkChecksumInit;
    cs_by.checkChecksumAppendData=z_checkChecksumAppendData;
    cs_by.checkChecksumEndTo=z_checkChecksumEndTo;
    _create_sync_data_by(&cs_by,newSyncInfo,newData,out_hsynz,compressPlugin,hsynzPlugin,threadNum);
    if (newSyncInfo->savedSizes)
        TNewDataZsyncInfo_savedSizesToBits(newSyncInfo);
    TNewDataZsyncInfo_saveTo(newSyncInfo,out_hsyni,zsyncKeyValues);//save to out_hsyni
}

}//namespace sync_private

void create_zsync_data(const hpatch_TStreamInput* newData,
                       const hpatch_TStreamOutput* out_zsync_info,const hpatch_TStreamOutput* out_zsync_gz,
                       hpatch_TChecksum* fileChecksumPlugin,hpatch_TChecksum* strongChecksumPlugin,
                       hsync_TDictCompress* compressPlugin,hsync_THsynz* hsynzPlugin,const std::vector<std::string>& zsyncKeyValues,
                       uint32_t kSyncBlockSize,size_t kSafeHashClashBit,size_t threadNum){
    CNewDataZsyncInfo newSyncInfo(fileChecksumPlugin,strongChecksumPlugin,compressPlugin,
                                  newData->streamSize,kSyncBlockSize,kSafeHashClashBit);
    _private_create_zsync_data(&newSyncInfo,newData,out_zsync_info,out_zsync_gz,compressPlugin,hsynzPlugin,
                               zsyncKeyValues,threadNum);
}

void create_zsync_data(const hpatch_TStreamInput*  newData,const hpatch_TStreamOutput* out_zsync_info,
                       hpatch_TChecksum* fileChecksumPlugin,hpatch_TChecksum* strongChecksumPlugin,
                       hsync_TDictCompress*  compressPlugin,const std::vector<std::string>& zsyncKeyValues,
                       uint32_t kSyncBlockSize,size_t kSafeHashClashBit,size_t threadNum){
    create_zsync_data(newData,out_zsync_info,0,fileChecksumPlugin,strongChecksumPlugin,compressPlugin,0,
                      zsyncKeyValues,kSyncBlockSize,kSafeHashClashBit,threadNum);
}
