//  zsync_match_in_old.h
//  zsync_client for hsynz
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
#ifndef zsync_match_in_old_h
#define zsync_match_in_old_h
#include "zsync_client_type.h"
#include "../sync_client/match_in_old.h"
namespace sync_private{

void z_matchNewDataInOld(hpatch_StreamPos_t* out_newBlockDataInOldPoss,const TNewDataSyncInfo* newSyncInfo,
                         const hpatch_TStreamInput* oldStream,int threadNum);

} //namespace sync_private
#endif // zsync_match_in_old_h
