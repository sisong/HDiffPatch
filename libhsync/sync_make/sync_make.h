//  sync_make.h
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
#ifndef hsync_make_h
#define hsync_make_h
#include "../../libHDiffPatch/HPatch/checksum_plugin.h"
#include "../../libHDiffPatch/HDiff/diff_types.h"
#include "../sync_client/sync_client_type.h"
#include "dict_compress_plugin.h"
#include "hsynz_plugin.h"

static const uint32_t kSyncBlockSize_default   = 1024*2;
static const uint32_t kSyncBlockSize_min       = 128; // >= kSyncBlockSize_min_limit
static const size_t   kSafeHashClashBit_default= 24;

//create out_hsyni
//  all clients need download out_hsyni, and dowload part of 
//    newData which is not found in client's oldData;
//  client's oldData can be different for different clients;
//  out_hsyni will be loaded into memory when client sync_patch;
//  out_hsyni's size becomes smaller when kSyncBlockSize increases,
//    but the part of newData's size that need download becomes larger;
void create_sync_data(const hpatch_TStreamInput*  newData,
                      const hpatch_TStreamOutput* out_hsyni,
                      hpatch_TChecksum*           strongChecksumPlugin,
                      hsync_TDictCompress* compressPlugin=0,
                      uint32_t kSyncBlockSize=kSyncBlockSize_default,
                      size_t kSafeHashClashBit=kSafeHashClashBit_default,
                      size_t threadNum=1);

// out_hsynz: out compressed newData by compressPlugin
//   client download compressed part of newData from out_hsynz;
void create_sync_data(const hpatch_TStreamInput*  newData,
                      const hpatch_TStreamOutput* out_hsyni,
                      const hpatch_TStreamOutput* out_hsynz,
                      hpatch_TChecksum*           strongChecksumPlugin,
                      hsync_TDictCompress*        compressPlugin,
                      hsync_THsynz* hsynzPlugin=0,
                      uint32_t kSyncBlockSize=kSyncBlockSize_default,
                      size_t kSafeHashClashBit=kSafeHashClashBit_default,
                      size_t threadNum=1);

#endif // hsync_make_h
