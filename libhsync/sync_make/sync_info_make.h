//  sync_info_make.h
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
#ifndef hsync_info_make_h
#define hsync_info_make_h
#include <string>
#include "../../libHDiffPatch/HPatch/checksum_plugin.h"
#include "../../libHDiffPatch/HDiff/diff_types.h"
#include "../../dirDiffPatch/dir_diff/dir_diff_tools.h"
#include "../sync_client/sync_client_type.h"
#include "dict_compress_plugin.h"

namespace sync_private{
    
void TNewDataSyncInfo_saveTo(TNewDataSyncInfo* self,const hpatch_TStreamOutput* out_stream,
                             const hsync_TDictCompress* compressPlugin,hsync_dictCompressHandle dictHandle,size_t dictSize);

class CNewDataSyncInfo :public TNewDataSyncInfo{
public:
    inline uint32_t blockCount()const{
        hpatch_StreamPos_t result=getSyncBlockCount(this->newDataSize,this->kMatchBlockSize);
        checkv(result==(uint32_t)result);
        return (uint32_t)result; }
    CNewDataSyncInfo(hpatch_TChecksum* strongChecksumPlugin,const hsync_TDictCompress* compressPlugin,
                     hpatch_StreamPos_t newDataSize,uint32_t kMatchBlockSize,size_t kSafeHashClashBit);
    ~CNewDataSyncInfo(){}
private:
    std::string                 _compressType;
    std::string                 _strongChecksumType;
    hdiff_private::TAutoMem     _mem;
};

}//namespace sync_private
#endif // hsync_info_make_h
