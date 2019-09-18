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

static const uint32_t kMatchBlockSize_default = (1<<10);

void create_sync_data(const hpatch_TStreamInput*  newData,
                      const hpatch_TStreamOutput* out_newSyncInfo,
                      const hpatch_TStreamOutput* out_newSyncData,
                      hpatch_TChecksum*      strongChecksumPlugin,
                      const hdiff_TCompress* compressPlugin=0,
                      uint32_t kMatchBlockSize=kMatchBlockSize_default);

void create_sync_data(const char* newDataPath,
                      const char* out_newSyncInfoPath,
                      const char* out_newSyncDataPath,
                      hpatch_TChecksum*      strongChecksumPlugin,
                      const hdiff_TCompress* compressPlugin=0,
                      uint32_t kMatchBlockSize=kMatchBlockSize_default);

#endif // sync_server_h
