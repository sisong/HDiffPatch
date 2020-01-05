//  sync_info_server.h
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
#ifndef sync_info_server_h
#define sync_info_server_h
#include <string>
#include "../sync_client/sync_client_type.h"
#include "../../libHDiffPatch/HPatch/checksum_plugin.h"
#include "../../libHDiffPatch/HDiff/diff_types.h"
#include "../../dirDiffPatch/dir_diff/dir_diff_tools.h"

namespace sync_private{
    
bool TNewDataSyncInfo_saveTo(TNewDataSyncInfo* self,const hpatch_TStreamOutput* out_stream,
                             hpatch_TChecksum* strongChecksumPlugin,const hdiff_TCompress* compressPlugin);

class CNewDataSyncInfo :public TNewDataSyncInfo{
public:
    inline uint32_t blockCount()const{
        hpatch_StreamPos_t result=TNewDataSyncInfo_blockCount(this);
        checkv(result==(uint32_t)result);
        return (uint32_t)result; }
    CNewDataSyncInfo(hpatch_TChecksum* strongChecksumPlugin,const hdiff_TCompress* compressPlugin,
                     hpatch_StreamPos_t newDataSize,uint32_t kMatchBlockSize);
    ~CNewDataSyncInfo(){}
private:
    std::string                 _compressType;
    std::string                 _strongChecksumType;
    hdiff_private::TAutoMem     _mem;
};
    
}//namespace sync_private
#endif // sync_info_server_h
