//  match_in_old.h
//  sync_client
//
//  Created by housisong on 2019-09-22.
//  Copyright © 2019 sisong. All rights reserved.
#ifndef match_in_old_h
#define match_in_old_h
#include "sync_client_type.h"
#include "../../libHDiffPatch/HPatch/checksum_plugin.h"

static const hpatch_StreamPos_t kBlockType_needSync =~(hpatch_StreamPos_t)0;

//used stdexcept
void matchNewDataInOld(hpatch_StreamPos_t* out_newDataPoss,uint32_t* out_needSyncCount,
                       hpatch_StreamPos_t* out_needSyncSize,const TNewDataSyncInfo* newSyncInfo,
                       const hpatch_TStreamInput* oldStream,hpatch_TChecksum* strongChecksumPlugin);

#endif // match_in_old_h
