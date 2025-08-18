//  zsync_make_wrapper.h
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
#ifndef zsync_make_wrapper_h
#define zsync_make_wrapper_h
#include "../sync_make/sync_make.h"
#include <vector>
#include <string>

//create out_zsync_info
//  all clients need download out_zsync_info, and dowload part of 
//    newData which is not found in client's oldData;
//  client's oldData can be different for different clients;
//  out_zsync_info will be loaded into memory when client zsync_patch;
//  out_zsync_info's size becomes smaller when kSyncBlockSize increases,
//    but the part of newData's size that need download becomes larger;
void create_zsync_data(const hpatch_TStreamInput*  newData,
                       const hpatch_TStreamOutput* out_zsync_info,          // .zsync file format
                       hpatch_TChecksum*           fileChecksumPlugin,      // sha1
                       hpatch_TChecksum*           strongChecksumPlugin,    // md4
                       const std::vector<std::string>& zsyncKeyValues,      // save directly to out_zsync_info
                       uint32_t kSyncBlockSize=kSyncBlockSize_default,
                       size_t kSafeHashClashBit=kSafeHashClashBit_default,
                       size_t threadNum=1);

//create out_zsync_info & out_zsync_gz
// out_zsync_gz: out compressed newData by compressPlugin
//   client download compressed part of newData from out_zsync_gz;
void create_zsync_data(const hpatch_TStreamInput* newData,
                       const hpatch_TStreamOutput* out_zsync_info,          // .zsync file format
                       const hpatch_TStreamOutput* out_zsync_gz,            // .gz file format
                       hpatch_TChecksum*           fileChecksumPlugin,      // sha1
                       hpatch_TChecksum*           strongChecksumPlugin,    // md4
                       hsync_TDictCompress*        compressPlugin,          // gzipDictCompressPlugin or lgzipDictCompressPlugin
                       hsync_THsynz*               hsynzPlugin,             // hsync_THsynz_gzip? for create .gz
                       const std::vector<std::string>& zsyncKeyValues,      // save directly to out_zsync_info
                       uint32_t kSyncBlockSize=kSyncBlockSize_default,
                       size_t kSafeHashClashBit=kSafeHashClashBit_default,
                       size_t threadNum=1);

#endif // zsync_make_wrapper_h
