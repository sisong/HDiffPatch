//  zsync_info_make.h
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
#ifndef zsync_info_make_h
#define zsync_info_make_h
#include "../sync_make/sync_info_make.h"
#include "../zsync_client_wrapper/zsync_client_type.h"
#include <vector>
#include <string>

//check strongChecksumBits is strong enough?
bool z_getStrongForHashClash(size_t kSafeHashClashBit,hpatch_StreamPos_t newDataSize,
                             uint32_t kSyncBlockSize,size_t strongChecksumBits);

namespace sync_private{

void  TNewDataZsyncInfo_savedSizesToBits(TNewDataZsyncInfo* self);

void TNewDataZsyncInfo_saveTo(TNewDataZsyncInfo* self,const hpatch_TStreamOutput* out_stream,
                              const std::vector<std::string>& zsyncKeyValues);

class CNewDataZsyncInfo :public CNewDataSyncInfo{
public:
    CNewDataZsyncInfo(hpatch_TChecksum* fileChecksumPlugin,hpatch_TChecksum* strongChecksumPlugin,const hsync_TDictCompress* compressPlugin,
                      hpatch_StreamPos_t newDataSize,uint32_t syncBlockSize,size_t kSafeHashClashBit);
private:
};

}//namespace sync_private
#endif // zsync_info_make_h
