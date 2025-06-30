//sign_diff.h
//sign_diff
//Created by housisong on 2025-04-11.
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

#ifndef hsign_diff_h
#define hsign_diff_h
#include "_sign_diff_type.h"

//create a hdiff data by .hsyni:
//  newData: new data stream
//  oldSyncInfo: oldData's hsyni_file, is created by hsign_diff cmdline; oldData can't dir, can't compressed 
//  out_diff: output diff stream for hpatchz, i.e newData=hpatchz(oldData,out_diff); or call patch_single_stream() or patch_single_compressed_diff()
//  compressPlugin: for compress diff data, if null, not compress
//  throw std::runtime_error when I/O error,etc.
void create_hdiff_by_sign(const hpatch_TStreamInput* newData,const TOldDataSyncInfo* oldSyncInfo,
                          const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin=0,
                          size_t patchStepMemSize=kDefaultPatchStepMemSize,size_t threadNum=1);

#endif
