//  match_in_old.h
//  sync_client
//  Created by housisong on 2019-09-22.
/*
 The MIT License (MIT)
 Copyright (c) 2019-2020 HouSisong
 
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
#ifndef match_in_old_h
#define match_in_old_h
#include "sync_client_type.h"
namespace sync_private{

struct TSyncDiffLocalPoss;
static const hpatch_StreamPos_t kBlockType_needSync =~(hpatch_StreamPos_t)0; //download, default

//matchNewDataInOld()
//throw std::runtime_error on error
//result value in out_newBlockDataInOldPoss:
// value==kBlockType_needSync : this block need download from server;
// 0<=value<oldDataSize : this block read from oldStream,starting at value in oldStream;
void matchNewDataInOld(hpatch_StreamPos_t* out_newBlockDataInOldPoss,const TNewDataSyncInfo* newSyncInfo,
                       const hpatch_TStreamInput* oldStream,hpatch_TChecksum* strongChecksumPlugin,int threadNum);

} //namespace sync_private
#endif // match_in_old_h
