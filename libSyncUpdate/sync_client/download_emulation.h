//  download_emulation.h
//  sync_client
//  Created by housisong on 2019-09-23.
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
#ifndef download_emulation_h
#define download_emulation_h
#include "sync_client.h"
#ifdef __cplusplus
extern "C" {
#endif

//downloadEmulation for patch test:
//  when need to download part of newSyncData, emulation read it from local data;
bool downloadEmulation_open_by_file(ISyncPatchListener* out_emulation,const char* newSyncDataPath);
bool downloadEmulation_open(ISyncPatchListener* out_emulation,const hpatch_TStreamInput* newSyncData);
bool downloadEmulation_close(ISyncPatchListener* emulation);

bool cacheDownloadEmulation_open_by_file(ISyncPatchListener* out_emulation,const char* newSyncDataPath,
                                         const char* downloadCacheFile);
bool cacheDownloadEmulation_open(ISyncPatchListener* out_emulation,const hpatch_TStreamInput* newSyncData,
                                 const hpatch_TStreamOutput* downloadCacheBuf);
bool cacheDownloadEmulation_close(ISyncPatchListener* emulation);

#ifdef __cplusplus
}
#endif
        
#endif // download_emulation_h
