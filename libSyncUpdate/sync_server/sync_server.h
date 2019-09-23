//  sync_server.h
//  sync_server
//
//  Created by housisong on 2019-09-17.
//  Copyright © 2019 sisong. All rights reserved.
#ifndef sync_server_h
#define sync_server_h
#include "../sync_client/sync_client_type.h"
#include "../../libHDiffPatch/HPatch/checksum_plugin.h"
#include "../../libHDiffPatch/HDiff/diff_types.h"

static const uint32_t kMatchBlockSize_default = (1<<10)*4;

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
