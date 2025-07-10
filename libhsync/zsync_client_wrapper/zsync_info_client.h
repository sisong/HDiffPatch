//  zsync_info_client.h
//  zsync_client for hsynz
//  Created by housisong on 2025-06-28.
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
#ifndef zsync_info_client_h
#define zsync_info_client_h
#include "zsync_client_type.h"

TSyncClient_resultType TNewDataZsyncInfo_open_by_file(TNewDataZsyncInfo* self,const char* newSyncInfoFile,
                                                      ISyncInfoListener* listener);
TSyncClient_resultType TNewDataZsyncInfo_open(TNewDataZsyncInfo* self,const hpatch_TStreamInput* newSyncInfo,
                                              ISyncInfoListener* listener);
void TNewDataZsyncInfo_close(TNewDataZsyncInfo* self);

TSyncClient_resultType  checkNewZsyncInfoType_by_file(const char* newSyncInfoFile);
TSyncClient_resultType  checkNewZsyncInfoType(const hpatch_TStreamInput* newSyncInfo);

#endif // zsync_info_client_h
