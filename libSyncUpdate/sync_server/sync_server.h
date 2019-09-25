//  sync_server.h
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
#ifndef sync_server_h
#define sync_server_h
#include "../sync_client/sync_client_type.h"
#include "../../libHDiffPatch/HPatch/checksum_plugin.h"
#include "../../libHDiffPatch/HDiff/diff_types.h"

static const uint32_t kMatchBlockSize_default = 1024*2;
static const uint32_t kMatchBlockSize_min     = 64;

//create sync data
//  all clients need download newSyncInfo, and dowload part of newData which is not found in client's oldData;
//  client's oldData can be different for different clients;
//  newSyncInfo will be loaded into memory when client sync_patch;
//  newSyncInfo's size becomes smaller when kMatchBlockSize increases,
//    but the part of newData's size that need download becomes larger;
//  note: you can compress part newData when downloading data by yourself;
void create_sync_data(const char* newDataPath,
                      const char* out_newSyncInfoPath,
                      hpatch_TChecksum*      strongChecksumPlugin,
                      uint32_t kMatchBlockSize=kMatchBlockSize_default);

void create_sync_data(const hpatch_TStreamInput*  newData,
                      const hpatch_TStreamOutput* out_newSyncInfo,
                      hpatch_TChecksum*      strongChecksumPlugin,
                      uint32_t kMatchBlockSize=kMatchBlockSize_default);

// out_newSyncData: out compressed newData by compressPlugin
//   client download compressed part of newData from out_newSyncData;
void create_sync_data(const char* newDataPath,
                      const char* out_newSyncInfoPath,
                      const char* out_newSyncDataPath,
                      const hdiff_TCompress* compressPlugin,
                      hpatch_TChecksum*      strongChecksumPlugin,
                      uint32_t kMatchBlockSize=kMatchBlockSize_default);

void create_sync_data(const hpatch_TStreamInput*  newData,
                      const hpatch_TStreamOutput* out_newSyncInfo,
                      const hpatch_TStreamOutput* out_newSyncData,
                      const hdiff_TCompress* compressPlugin,
                      hpatch_TChecksum*      strongChecksumPlugin,
                      uint32_t kMatchBlockSize=kMatchBlockSize_default);

#endif // sync_server_h
