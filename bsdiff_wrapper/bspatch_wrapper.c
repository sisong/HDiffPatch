// bspatch_wrapper.c
// HDiffPatch
/*
 The MIT License (MIT)
 Copyright (c) 2021 HouSisong
 
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
#include "bspatch_wrapper.h"
#include "../libHDiffPatch/HPatch/patch_types.h"
#include "../libHDiffPatch/HPatch/patch_private.h"
#include <string.h>
#define _hpatch_FALSE   hpatch_FALSE
//hpatch_uint __debug_check_false_x=0; //for debug
//#define _hpatch_FALSE (1/__debug_check_false_x)

#ifndef _IS_RUN_MEM_SAFE_CHECK
#   define _IS_RUN_MEM_SAFE_CHECK  1
#endif

#if (_IS_RUN_MEM_SAFE_CHECK)
//__RUN_MEM_SAFE_CHECK用来启动内存访问越界检查,用以防御可能被意外或故意损坏的数据.
#   define __RUN_MEM_SAFE_CHECK
#endif

static const char* kBsDiffVersionType = "BSDIFF40";
#define            kBsDiffVersionTypeLen 8  // ==strlen(kBsDiffVersionType);
#define            kBsDiffHeadLen       (kBsDiffVersionTypeLen+3*8)

static const char* kEsBsDiffVersionType = "ENDSLEY/BSDIFF43";
#define            kEsBsDiffVersionTypeLen 16  // ==strlen(kEsBsDiffVersionType);
#define            kEsBsDiffHeadLen     (kEsBsDiffVersionTypeLen+8)


static hpatch_inline hpatch_uint32_t _readUInt32(const unsigned char* buf){
    return buf[0] | (((hpatch_uint32_t)buf[1])<<8)  |
           (((hpatch_uint32_t)buf[2])<<16)  | (((hpatch_uint32_t)buf[3])<<24) ;
}
static hpatch_inline hpatch_uint64_t readUInt64(const unsigned char* buf){
    return _readUInt32(buf) | (((hpatch_uint64_t)_readUInt32(buf+4))<<32);
}

#define _clip_readUInt64(_clip,_result) { \
    const unsigned char* buf=_TStreamCacheClip_readData(_clip,8); \
    if (buf!=0) *(_result)=readUInt64(buf); \
    else return _hpatch_FALSE; \
}

hpatch_BOOL getBsDiffInfo(hpatch_BsDiffInfo* out_diffinfo,const hpatch_TStreamInput* diffStream){
    unsigned char _buf[kBsDiffHeadLen];
    unsigned char* buf=&_buf[0];
    if (diffStream->streamSize<kBsDiffHeadLen)
        return _hpatch_FALSE;
    if (!diffStream->read(diffStream,0,buf,buf+kBsDiffHeadLen)) // must bz2 compressed size>=8
        return _hpatch_FALSE;
    if (0==memcmp(buf,kBsDiffVersionType,kBsDiffVersionTypeLen)){
        out_diffinfo->isEndsleyBsdiff=hpatch_FALSE;
        out_diffinfo->headSize=kBsDiffHeadLen;
    }else if (0==memcmp(buf,kEsBsDiffVersionType,kEsBsDiffVersionTypeLen)){
        out_diffinfo->isEndsleyBsdiff=hpatch_TRUE;
        out_diffinfo->headSize=kEsBsDiffHeadLen;
    }else{
        return _hpatch_FALSE;
    }
    if (out_diffinfo->isEndsleyBsdiff){
        buf+=kEsBsDiffVersionTypeLen;
        out_diffinfo->ctrlDataSize=0;
        out_diffinfo->subDataSize =0;
        out_diffinfo->newDataSize =readUInt64(buf);
    }else{
        buf+=kBsDiffVersionTypeLen;
        out_diffinfo->ctrlDataSize=readUInt64(buf);
        out_diffinfo->subDataSize =readUInt64(buf+8);
        out_diffinfo->newDataSize =readUInt64(buf+8*2);
    }
    return (out_diffinfo->ctrlDataSize<diffStream->streamSize)&&
           (out_diffinfo->subDataSize<diffStream->streamSize)&&
           (out_diffinfo->headSize+out_diffinfo->ctrlDataSize+out_diffinfo->subDataSize<diffStream->streamSize);
}

hpatch_BOOL getBsDiffInfo_mem(hpatch_BsDiffInfo* out_diffinfo,const unsigned char* diffData,const unsigned char* diffData_end){
    hpatch_TStreamInput diffStream;
    mem_as_hStreamInput(&diffStream,diffData,diffData_end);
    return getBsDiffInfo(out_diffinfo,&diffStream);
}

hpatch_BOOL getIsBsDiff(const hpatch_TStreamInput* diffData,hpatch_BOOL* out_isSingleCompressedDiff){
    hpatch_BsDiffInfo diffinfo;
    hpatch_BOOL result=getBsDiffInfo(&diffinfo,diffData);
    if (result&&out_isSingleCompressedDiff) *out_isSingleCompressedDiff=diffinfo.isEndsleyBsdiff;
    return result;
}

hpatch_BOOL getIsBsDiff_mem(const unsigned char* diffData,const unsigned char* diffData_end,hpatch_BOOL* out_isSingleCompressedDiff){
    hpatch_TStreamInput diffStream;
    mem_as_hStreamInput(&diffStream,diffData,diffData_end);
    return getIsBsDiff(&diffStream,out_isSingleCompressedDiff);
}

static hpatch_BOOL _patch_add_old_with_sub(_TOutStreamCache* outCache,TStreamCacheClip* subClip,
                                           const hpatch_TStreamInput* old,hpatch_uint64_t oldPos,
                                           hpatch_uint64_t addLength,unsigned char* aCache,hpatch_size_t aCacheSize){
    while (addLength>0){
        hpatch_size_t decodeStep=aCacheSize;
        if (decodeStep>addLength)
            decodeStep=(hpatch_size_t)addLength;
        if (!old->read(old,oldPos,aCache,aCache+decodeStep)) return _hpatch_FALSE;
        if (!_TStreamCacheClip_addDataTo(subClip,aCache,decodeStep)) return _hpatch_FALSE;
        if (!_TOutStreamCache_write(outCache,aCache,decodeStep)) return _hpatch_FALSE;
        oldPos+=decodeStep;
        addLength-=decodeStep;
    }
    return hpatch_TRUE;
}

hpatch_BOOL bspatchByClip(_TOutStreamCache* outCache,const hpatch_TStreamInput* oldData,
                          TStreamCacheClip* ctrlClip,TStreamCacheClip* subClip,TStreamCacheClip* newDataDiffClip,
                          unsigned char* temp_cache,hpatch_size_t cache_size){
    const hpatch_uint64_t newDataSize=_TOutStreamCache_leaveSize(outCache);
#ifdef __RUN_MEM_SAFE_CHECK
    const hpatch_uint64_t oldDataSize=oldData->streamSize;
#endif
    hpatch_uint64_t     newPosBack=0;
    hpatch_uint64_t     oldPosBack=0;
    assert(cache_size>=8);
    
    while (newPosBack<newDataSize){
        hpatch_uint64_t coverLen;
        hpatch_uint64_t skipNewLen;
        hpatch_uint64_t skipOldLen;
        {//read ctrl
            _clip_readUInt64(ctrlClip,&coverLen);
            _clip_readUInt64(ctrlClip,&skipNewLen);
            _clip_readUInt64(ctrlClip,&skipOldLen);
            if ((skipOldLen>>63)!=0)
                skipOldLen=-(skipOldLen&((((hpatch_uint64_t)1)<<63)-1));
        }
#ifdef __RUN_MEM_SAFE_CHECK
        if (coverLen>(hpatch_uint64_t)(newDataSize-newPosBack)) return _hpatch_FALSE;
        if (oldPosBack>oldDataSize) return _hpatch_FALSE;
        if (coverLen>(hpatch_uint64_t)(oldDataSize-oldPosBack)) return _hpatch_FALSE;
#endif
        if (!_patch_add_old_with_sub(outCache,subClip,oldData,oldPosBack,coverLen,
                                     temp_cache,cache_size)) return _hpatch_FALSE;
        oldPosBack+=coverLen+skipOldLen;
        newPosBack+=coverLen;
        if (skipNewLen){
#ifdef __RUN_MEM_SAFE_CHECK
            if (skipNewLen>(hpatch_uint64_t)(newDataSize-newPosBack)) return _hpatch_FALSE;
#endif
            if (!_TOutStreamCache_copyFromClip(outCache,newDataDiffClip,skipNewLen)) return _hpatch_FALSE;
            newPosBack+=skipNewLen;
        }
    }

    if (!_TOutStreamCache_flush(outCache))
        return _hpatch_FALSE;
    if (_TOutStreamCache_isFinish(outCache)
        && (newPosBack==newDataSize) )
        return hpatch_TRUE;
    else
        return _hpatch_FALSE;
}


#define _kCacheBsDecCount 5

static const hpatch_uint64_t _kUnknowMaxSize=~(hpatch_uint64_t)0;
#define _clear_return(exitValue) {  result=exitValue; goto clear; }

hpatch_BOOL bspatch_with_cache(const hpatch_TStreamOutput* out_newData,
                               const hpatch_TStreamInput*  oldData,
                               const hpatch_TStreamInput*  compressedDiff,
                               hpatch_TDecompress* decompressPlugin,
                               unsigned char* temp_cache,unsigned char* temp_cache_end){
    hpatch_BsDiffInfo diffInfo;
    TStreamCacheClip  ctrlClip;
    TStreamCacheClip  subClip;
    TStreamCacheClip* _ctrlClip=&ctrlClip;
    TStreamCacheClip* _subClip=&subClip;
    TStreamCacheClip  newDataDiffClip;
    _TDecompressInputStream decompressers[3];
    hpatch_size_t           i;
    hpatch_BOOL  result=hpatch_TRUE;
    hpatch_StreamPos_t  diffPos0;
    hpatch_TStreamInput  _oldDataCache;
    hpatch_size_t cacheSize;
    assert(decompressPlugin!=0);
    assert(out_newData!=0);
    assert(out_newData->write!=0);
    assert(oldData!=0);
    assert(oldData->read!=0);
    assert(compressedDiff!=0);
    assert(compressedDiff->read!=0);
    if (!getBsDiffInfo(&diffInfo,compressedDiff)) 
        return _hpatch_FALSE;
    if (out_newData->streamSize!=diffInfo.newDataSize)
        return _hpatch_FALSE;
    for (i=0;i<sizeof(decompressers)/sizeof(_TDecompressInputStream);++i)
        decompressers[i].decompressHandle=0;

    cacheSize=(temp_cache_end-temp_cache);
    if (cacheSize>=(oldData->streamSize+_kCacheBsDecCount*hpatch_kStreamCacheSize)){//can cache old?
        cacheSize=(hpatch_size_t)oldData->streamSize;
        if (!oldData->read(oldData,0,temp_cache,temp_cache+cacheSize))
            return _hpatch_FALSE;
        mem_as_hStreamInput(&_oldDataCache,temp_cache,temp_cache+cacheSize);
        oldData=&_oldDataCache;
        temp_cache+=cacheSize;
        cacheSize=(temp_cache_end-temp_cache);
    }
    cacheSize=cacheSize/(diffInfo.isEndsleyBsdiff?_kCacheBsDecCount-2:_kCacheBsDecCount);
    if (cacheSize<8) return _hpatch_FALSE;

    diffPos0=diffInfo.headSize;
    if (diffInfo.isEndsleyBsdiff){
        if (!getStreamClip(&newDataDiffClip,&decompressers[0],
                        _kUnknowMaxSize,compressedDiff->streamSize-diffPos0,compressedDiff,&diffPos0,
                        decompressPlugin,temp_cache,cacheSize)) _clear_return(_hpatch_FALSE);
        temp_cache+=cacheSize;
        _ctrlClip=&newDataDiffClip;
        _subClip=&newDataDiffClip;
    }else{
        if (!getStreamClip(&ctrlClip,&decompressers[0],
                        _kUnknowMaxSize,diffInfo.ctrlDataSize,compressedDiff,&diffPos0,
                        decompressPlugin,temp_cache,cacheSize)) _clear_return(_hpatch_FALSE);
        temp_cache+=cacheSize;
        if (!getStreamClip(&subClip,&decompressers[1],
                        _kUnknowMaxSize,diffInfo.subDataSize,compressedDiff,&diffPos0,
                        decompressPlugin,temp_cache,cacheSize)) _clear_return(_hpatch_FALSE);
        temp_cache+=cacheSize;
        if (!getStreamClip(&newDataDiffClip,&decompressers[2],
                        _kUnknowMaxSize,compressedDiff->streamSize-diffPos0,compressedDiff,&diffPos0,
                        decompressPlugin,temp_cache,cacheSize)) _clear_return(_hpatch_FALSE);
        temp_cache+=cacheSize;
    }
    assert(diffPos0==compressedDiff->streamSize);

    {
        _TOutStreamCache outCache;
        _TOutStreamCache_init(&outCache,out_newData,temp_cache,cacheSize);
        temp_cache+=cacheSize;
        result=bspatchByClip(&outCache,oldData,_ctrlClip,_subClip,&newDataDiffClip,
                            temp_cache,cacheSize);
    }

clear:
    for (i=0;i<sizeof(decompressers)/sizeof(_TDecompressInputStream);++i) {
        if (decompressers[i].decompressHandle){
            if (!decompressPlugin->close(decompressPlugin,decompressers[i].decompressHandle))
                result=_hpatch_FALSE;
            decompressers[i].decompressHandle=0;
        }
    }
    return result;
}