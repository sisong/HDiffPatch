//sign_diff.cpp
//sign_diff
//Created by housisong on 2025-04-15.
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
 included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
*/
#include "sign_diff.h"
#include <stdexcept>
#include "../../libHDiffPatch/HDiff/private_diff/limit_mem_diff/covers.h"
#include "_match_in_old_sign.h"
using namespace hdiff_private;
using namespace sync_private;

static void get_match_covers_by_sign(const hpatch_TStreamInput* newData,const TOldDataSyncInfo* oldSyncInfo,
                                     TCoversBuf* out_covers,size_t threadNum){
    assert(out_covers->push_cover!=0);
    matchNewDataInOldSign(out_covers,newData,oldSyncInfo,(int)threadNum);
}


    void serialize_single_compressed_diff(const hpatch_TStreamInput* newStream,const hpatch_TStreamInput* oldStream,
                                          bool isZeroSubDiff,const TCovers& covers,const hpatch_TStreamOutput* out_diff,
                                          const hdiff_TCompress* compressPlugin,size_t patchStepMemSize);

void create_hdiff_by_sign(const hpatch_TStreamInput* newData,const TOldDataSyncInfo* oldSyncInfo,
                          const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                          size_t patchStepMemSize,size_t threadNum){
    if (oldSyncInfo->_decompressPlugin){
        if (oldSyncInfo->isDirSyncInfo)
            throw std::runtime_error("create_hdiff_by_sign() unsupport compressed old dir!");
        else
            printf("WARNING: This diff file format, requires uncompressed old file when patch!");
    }
    TCoversBuf covers(newData->streamSize,oldSyncInfo->newDataSize);
    get_match_covers_by_sign(newData,oldSyncInfo,&covers,threadNum);
    hpatch_TStreamInput oldData={0};
    oldData.streamSize=oldSyncInfo->newDataSize;
    serialize_single_compressed_diff(newData,&oldData,true,covers,
                                     out_diff,compressPlugin,patchStepMemSize);
}
