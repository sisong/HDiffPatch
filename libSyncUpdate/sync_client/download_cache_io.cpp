//  download_cache_io.cpp
//  sync_client
//  Created by housisong on 2020-01-05.
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
#include "download_cache_io.h"
#include "sync_client.h"
#include "../../file_for_patch.h"

    static bool _deleteMemoryCacheIO(const TDownloadCacheIO* memIO){
        TDownloadCacheIO* self=(TDownloadCacheIO*)memIO;
        if (self!=0)
            free(self);
        return true;
    }
const TDownloadCacheIO* createMemoryCacheIO(hpatch_StreamPos_t needCacheSyncSize){
    //mem:TDownloadCacheIO + hpatch_TStreamOutput + needCacheSyncSize
    const size_t nodeSize=sizeof(TDownloadCacheIO)+sizeof(hpatch_TStreamOutput);
    hpatch_StreamPos_t _memSize=nodeSize+needCacheSyncSize;
    size_t memSize=(size_t)_memSize;
    if (memSize!=_memSize) return 0; //error
    TByte* mem=(TByte*)malloc(memSize);
    if (mem==0) return 0; //error
    TDownloadCacheIO* self=(TDownloadCacheIO*)mem;
    memset(self,0,nodeSize);
    self->streamIO=(hpatch_TStreamOutput*)(mem+sizeof(TDownloadCacheIO));
    self->deleteCacheIO=_deleteMemoryCacheIO;
    mem_as_hStreamOutput(self->streamIO,mem+nodeSize,mem+memSize);
    return self;
}

    static bool _deleteTempFileCacheIO(const TDownloadCacheIO* fileIO){
        TDownloadCacheIO* self=(TDownloadCacheIO*)fileIO;
        if (self==0) return true;
        char* mem=(char*)self;
        hpatch_TFileStreamOutput* fileStream=(hpatch_TFileStreamOutput*)(mem+sizeof(TDownloadCacheIO));
        hpatch_BOOL ret=hpatch_TFileStreamOutput_close(fileStream);
        const size_t nodeSize=sizeof(TDownloadCacheIO)+sizeof(hpatch_TFileStreamOutput);
        const char* tempCacheFileName=mem+nodeSize;
        if (!hpatch_removeFile(tempCacheFileName))
            ret=hpatch_FALSE;
        free(self);
        return (ret!=hpatch_FALSE);
    }
const TDownloadCacheIO* createTempFileCacheIO(const char* tempCacheFileName,hpatch_StreamPos_t needCacheSyncSize){
    //mem:TDownloadCacheIO + hpatch_TFileStreamOutput + tempCacheFileName + needCacheSyncSize
    assert(tempCacheFileName!=0);
    size_t fNameSize=strlen(tempCacheFileName);
    const size_t nodeSize=sizeof(TDownloadCacheIO)+sizeof(hpatch_TFileStreamOutput);
    size_t memSize=nodeSize+fNameSize+1; //with '\0'
    TByte* mem=(TByte*)malloc(memSize);
    if (mem==0) return 0; //error
    TDownloadCacheIO* self=(TDownloadCacheIO*)malloc(memSize);
    memset(self,0,nodeSize);
    memcpy(mem+nodeSize,tempCacheFileName,fNameSize+1);
    hpatch_TFileStreamOutput* fileStream=(hpatch_TFileStreamOutput*)(mem+sizeof(TDownloadCacheIO));
    self->streamIO=&fileStream->base;
    self->deleteCacheIO=_deleteTempFileCacheIO;
    if (!hpatch_TFileStreamOutput_open(fileStream,tempCacheFileName,needCacheSyncSize)){
        free(self);
        return 0; //error
    }
    return self;
}
