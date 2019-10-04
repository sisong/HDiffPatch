//  download_emulation.cpp
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
#include "download_emulation.h"
#include "../../file_for_patch.h"
#include <map>

struct TDownloadEmulation {
    const hpatch_TStreamInput* emulation_newSyncData;
    hpatch_TFileStreamInput    newSyncFile;
};

static bool _readSyncData(ISyncPatchListener* listener,hpatch_StreamPos_t posInNewSyncData,
                          uint32_t syncDataSize,TSyncDataType samePosInNewSyncData,
                          unsigned char* out_syncDataBuf){
//warning: "Read newSyncData from emulation data;" \
    " In the actual project, these data need downloaded from server."
    TDownloadEmulation* self=(TDownloadEmulation*)listener->import;
    return hpatch_FALSE!=self->emulation_newSyncData->read(self->emulation_newSyncData,posInNewSyncData,
                                                           out_syncDataBuf,out_syncDataBuf+syncDataSize);
}

static void downloadEmulation_open_by(TDownloadEmulation* self,ISyncPatchListener* out_emulation,
                                      const hpatch_TStreamInput* newSyncData){
    memset(out_emulation,0,sizeof(*out_emulation));
    self->emulation_newSyncData=newSyncData;
    out_emulation->import=self;
    out_emulation->isCanCacheRepeatSyncData=false;
    out_emulation->readSyncData=_readSyncData;
}

bool downloadEmulation_open(ISyncPatchListener* out_emulation,const hpatch_TStreamInput* newSyncData){
    assert(out_emulation->import==0);
    TDownloadEmulation* self=(TDownloadEmulation*)malloc(sizeof(TDownloadEmulation));
    if (self==0) return false;
    memset(self,0,sizeof(*self));
    downloadEmulation_open_by(self,out_emulation,newSyncData);
    return true;
}

bool downloadEmulation_open_by_file(ISyncPatchListener* out_emulation,const char* newSyncDataPath){
    assert(out_emulation->import==0);
    TDownloadEmulation* self=(TDownloadEmulation*)malloc(sizeof(TDownloadEmulation));
    if (self==0) return false;
    memset(self,0,sizeof(*self));
    if (!hpatch_TFileStreamInput_open(&self->newSyncFile,newSyncDataPath)){
        free(self);
        return false;
    }
    downloadEmulation_open_by(self,out_emulation,&self->newSyncFile.base);
    return true;
}

bool downloadEmulation_close(ISyncPatchListener* emulation){
    if (emulation==0) return true;
    TDownloadEmulation* self=(TDownloadEmulation*)emulation->import;
    memset(emulation,0,sizeof(*emulation));
    if (self==0) return true;
    bool result=(0!=hpatch_TFileStreamInput_close(&self->newSyncFile));
    free(self);
    return result;
}


//cache demo
struct TCacheDownloadEmulation {
    const hpatch_TStreamInput*  emulation_newSyncData;
    const hpatch_TStreamOutput* downloadCacheBuf;
    hpatch_TFileStreamInput     newSyncFile;
    hpatch_TFileStreamOutput    downloadCacheFile;
    uint32_t                    needCacheSyncCount;
    uint32_t                    curCachedSyncCount;
    hpatch_StreamPos_t          curCachedSize;
    typedef std::map<hpatch_StreamPos_t,hpatch_StreamPos_t> TMap;
    TMap*                       cachePosMap;
};

static bool _doCache(TCacheDownloadEmulation* self,hpatch_StreamPos_t posInNewSyncData,
                     uint32_t syncDataSize,const unsigned char* syncDataBuf){
    assert(self->curCachedSyncCount<self->needCacheSyncCount);
    try {
        if (self->cachePosMap==0)
            self->cachePosMap=new TCacheDownloadEmulation::TMap();
        //assert(self->cachePosMap->find(posInNewSyncData)==self->cachePosMap->end());
        self->cachePosMap->insert(TCacheDownloadEmulation::TMap::value_type(posInNewSyncData,self->curCachedSize));
    } catch (...) { return false; }
    if (!self->downloadCacheBuf->write(self->downloadCacheBuf,self->curCachedSize,
                                       syncDataBuf,syncDataBuf+syncDataSize)) return false;
    self->curCachedSize+=syncDataSize;
    ++self->curCachedSyncCount;
    return true;
}


static bool _readCache(const TCacheDownloadEmulation* self,hpatch_StreamPos_t samePosInNewSyncData,
                     uint32_t syncDataSize,unsigned char* out_syncDataBuf){
    TCacheDownloadEmulation::TMap::const_iterator it=self->cachePosMap->find(samePosInNewSyncData);
    if (it==self->cachePosMap->end()) return false;
    hpatch_StreamPos_t savedPos=it->second;
    if (!self->downloadCacheBuf->read_writed(self->downloadCacheBuf,savedPos,
               out_syncDataBuf,out_syncDataBuf+syncDataSize)) return false;
    return true;
}

static bool _cache_readSyncData(ISyncPatchListener* listener,hpatch_StreamPos_t posInNewSyncData,
                                uint32_t syncDataSize,TSyncDataType samePosInNewSyncData,
                                unsigned char* out_syncDataBuf){
    TCacheDownloadEmulation* self=(TCacheDownloadEmulation*)listener->import;
    if (samePosInNewSyncData<posInNewSyncData) //can read from cache
        return _readCache(self,samePosInNewSyncData,syncDataSize,out_syncDataBuf);
    //download
    bool result=hpatch_FALSE!=self->emulation_newSyncData->read(self->emulation_newSyncData,posInNewSyncData,
                                                                out_syncDataBuf,out_syncDataBuf+syncDataSize);
    if (result&&(samePosInNewSyncData==posInNewSyncData))//cache
        result=_doCache(self,posInNewSyncData,syncDataSize,out_syncDataBuf);
    return result;
}

static void _cache_needSyncMsg(ISyncPatchListener* listener,uint32_t needSyncCount,uint32_t needCacheSyncCount){
    TCacheDownloadEmulation* self=(TCacheDownloadEmulation*)listener->import;
    self->needCacheSyncCount=needCacheSyncCount;
}

static void cacheDownloadEmulation_open_by(TCacheDownloadEmulation* self,ISyncPatchListener* out_emulation,
                                           const hpatch_TStreamInput* newSyncData,
                                           const hpatch_TStreamOutput* downloadCacheBuf){
    memset(out_emulation,0,sizeof(*out_emulation));
    self->emulation_newSyncData=newSyncData;
    self->downloadCacheBuf=downloadCacheBuf;
    out_emulation->import=self;
    out_emulation->isCanCacheRepeatSyncData=true;
    out_emulation->readSyncData=_cache_readSyncData;
    out_emulation->needSyncMsg=_cache_needSyncMsg;
}

bool cacheDownloadEmulation_open(ISyncPatchListener* out_emulation,const hpatch_TStreamInput* newSyncData,
                                 const hpatch_TStreamOutput* downloadCacheBuf){
    assert(out_emulation->import==0);
    if (downloadCacheBuf->read_writed==0) return false; //must can read
    TCacheDownloadEmulation* self=(TCacheDownloadEmulation*)malloc(sizeof(TCacheDownloadEmulation));
    if (self==0) return false;
    memset(self,0,sizeof(*self));
    cacheDownloadEmulation_open_by(self,out_emulation,newSyncData,downloadCacheBuf);
    return true;
}

bool cacheDownloadEmulation_open_by_file(ISyncPatchListener* out_emulation,const char* newSyncDataPath,
                                         const char* downloadCacheFile){
    assert(out_emulation->import==0);
    TCacheDownloadEmulation* self=(TCacheDownloadEmulation*)malloc(sizeof(TCacheDownloadEmulation));
    if (self==0) return false;
    memset(self,0,sizeof(*self));
    if (!hpatch_TFileStreamInput_open(&self->newSyncFile,newSyncDataPath)){
        free(self);
        return false;
    }
    if (!hpatch_TFileStreamOutput_open(&self->downloadCacheFile,downloadCacheFile,-1)){
        hpatch_TFileStreamInput_close(&self->newSyncFile);
        free(self);
        return false;
    }
    hpatch_TFileStreamOutput_setRandomOut(&self->downloadCacheFile,hpatch_TRUE);
    cacheDownloadEmulation_open_by(self,out_emulation,&self->newSyncFile.base,&self->downloadCacheFile.base);
    return true;
}

bool cacheDownloadEmulation_close(ISyncPatchListener* emulation){
    if (emulation==0) return true;
    TCacheDownloadEmulation* self=(TCacheDownloadEmulation*)emulation->import;
    memset(emulation,0,sizeof(*emulation));
    if (self==0) return true;
    if (self->cachePosMap) delete self->cachePosMap;
    bool result=(0!=hpatch_TFileStreamInput_close(&self->newSyncFile));
    if (!hpatch_TFileStreamOutput_close(&self->downloadCacheFile))
        result=false;
    free(self);
    return result;
}

