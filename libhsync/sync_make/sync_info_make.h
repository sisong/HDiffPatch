//  sync_info_make.h
//  sync_make
//  Created by housisong on 2019-09-17.
/*
 The MIT License (MIT)
 Copyright (c) 2019-2023 HouSisong
 
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
#include "hsynz_plugin.h"

namespace sync_private{

void TNewDataSyncInfo_saveTo(TNewDataSyncInfo* self,const hpatch_TStreamOutput* out_stream,
                             hsync_TDictCompress* compressPlugin);
#if (_IS_NEED_DIR_DIFF_PATCH)
void TNewDataSyncInfo_dirWithHead_saveTo(TNewDataSyncInfo_dir* self,std::vector<hpatch_byte>& out_buf);
#endif

class CNewDataSyncInfo :public TNewDataSyncInfo{
public:
    inline uint32_t blockCount()const{
        hpatch_StreamPos_t result=getSyncBlockCount(this->newDataSize,this->kSyncBlockSize);
        checkv(result==(uint32_t)result);
        return (uint32_t)result; }
    CNewDataSyncInfo(hpatch_TChecksum* strongChecksumPlugin,const hsync_TDictCompress* compressPlugin,
                     hpatch_StreamPos_t newDataSize,uint32_t syncBlockSize,size_t kSafeHashClashBit);
    ~CNewDataSyncInfo(){}
private:
    std::string                 _compressType;
    std::string                 _strongChecksumType;
    hdiff_private::TAutoMem     _mem;
};

void getHsynzPluginDefault(hsync_THsynz* out_hsynzPlugin);

}//namespace sync_private
#endif // hsync_info_make_h
