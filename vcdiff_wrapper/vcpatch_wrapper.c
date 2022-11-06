// vcpatch_wrapper.c
// HDiffPatch
/*
 The MIT License (MIT)
 Copyright (c) 2022 HouSisong
 
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
#include "vcpatch_wrapper.h"
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

static const unsigned char kVcDiffType[3]={('V'|(1<<7)),('C'|(1<<7)),('D'|(1<<7))};
#define     kVcDiffDefaultVersion   0
#define     kVcDiffGoogleVersion    'S'
#define     kVcDiffMinHeadLen       (sizeof(kVcDiffType)+1+1)

#define VCD_SOURCE  (1<<0)
#define VCD_TARGET  (1<<1)
#define VCD_ADLER32 (1<<2)

#define _clip_unpackUInt64(_clip,_result) { \
    if (!_TStreamCacheClip_unpackUIntWithTag(_clip,_result,0)) \
        return _hpatch_FALSE; \
}
#define _clip_readUInt8(_clip,_result) { \
    const unsigned char* buf=_TStreamCacheClip_readData(_clip,1); \
    if (buf!=0) *(_result)=*buf; \
    else return _hpatch_FALSE; \
}

hpatch_BOOL getVcDiffInfo(hpatch_VcDiffInfo* out_diffinfo,const hpatch_TStreamInput* diffStream){
    TStreamCacheClip  diffClip;
    unsigned char _cache[hpatch_kStreamCacheSize];
    unsigned char Hdr_Indicator;
    if (diffStream->streamSize<kVcDiffMinHeadLen)
        return _hpatch_FALSE;
    assert(kVcDiffMinHeadLen<=hpatch_kStreamCacheSize);
    _TStreamCacheClip_init(&diffClip,diffStream,0,diffStream->streamSize,_cache,sizeof(_cache));

    {//head
        const unsigned char* buf=_TStreamCacheClip_readData(&diffClip,kVcDiffMinHeadLen);
        if (buf==0)
            return _hpatch_FALSE;
        if (0!=memcmp(buf,kVcDiffType,sizeof(kVcDiffType)))
            return _hpatch_FALSE; //not VCDIFF
        buf+=sizeof(kVcDiffType);
        memset(out_diffinfo,0,sizeof(*out_diffinfo));
        switch (*buf++){ // version
            case kVcDiffDefaultVersion: out_diffinfo->isGoogleVersion=hpatch_FALSE; break;
            case kVcDiffGoogleVersion: out_diffinfo->isGoogleVersion=hpatch_TRUE; break;
            default:
                return _hpatch_FALSE; //unsupport version
        }
        Hdr_Indicator=*buf++; // Hdr_Indicator
    }

    if (Hdr_Indicator&(1<<0)){ // VCD_DECOMPRESS
        _clip_readUInt8(&diffClip,&out_diffinfo->compressorID);
        if (out_diffinfo->compressorID!=kVcDiff_compressorID_lzma)
            return _hpatch_FALSE; //unsupport compressor ID
    }
    if (Hdr_Indicator&(1<<1)) // VCD_CODETABLE
        return _hpatch_FALSE; //unsupport code table
    if (Hdr_Indicator&(1<<2)) // VCD_APPHEADER
        _clip_unpackUInt64(&diffClip,&out_diffinfo->appHeadDataLen);
    out_diffinfo->appHeadDataOffset=_TStreamCacheClip_readPosOfSrcStream(&diffClip);
    out_diffinfo->windowOffset=out_diffinfo->appHeadDataOffset+out_diffinfo->appHeadDataLen;
#ifdef __RUN_MEM_SAFE_CHECK
    {
        const hpatch_StreamPos_t maxPos=_TStreamCacheClip_leaveSize(&diffClip);
        if ( (out_diffinfo->appHeadDataLen>maxPos)|(out_diffinfo->windowOffset>maxPos) )
            return _hpatch_FALSE; //data size error
    }
#endif
    return hpatch_TRUE;
}

hpatch_BOOL getVcDiffInfo_mem(hpatch_VcDiffInfo* out_diffinfo,const unsigned char* diffData,const unsigned char* diffData_end){
    hpatch_TStreamInput diffStream;
    mem_as_hStreamInput(&diffStream,diffData,diffData_end);
    return getVcDiffInfo(out_diffinfo,&diffStream);
}


hpatch_BOOL _vcpatch_delta(_TOutStreamCache* outCache,hpatch_StreamPos_t targetLen,
                           const hpatch_TStreamInput* srcData,hpatch_StreamPos_t srcPos,hpatch_StreamPos_t srcPosEnd,
                           TStreamCacheClip* data,TStreamCacheClip* inst,TStreamCacheClip* addr,
                           unsigned char* temp_cache,hpatch_size_t cache_size){
    //todo:

    return hpatch_TRUE;
}


static const hpatch_uint64_t _kUnknowMaxSize=~(hpatch_uint64_t)0;
#define _clear_return(exitValue) {  result=exitValue; goto clear; }

hpatch_BOOL _vcpatch_window(_TOutStreamCache* outCache,const hpatch_TStreamInput* oldData,
                            const hpatch_TStreamInput* compressedDiff,hpatch_TDecompress* decompressPlugin,
                            hpatch_StreamPos_t windowOffset,unsigned char* tempCaches,hpatch_size_t cache_size){
    unsigned char _cache[hpatch_kStreamCacheSize];
    //window loop
    while (windowOffset<compressedDiff->streamSize){
        TStreamCacheClip   diffClip;
        hpatch_StreamPos_t srcPos=0;
        hpatch_StreamPos_t srcPosEnd=0;
        hpatch_StreamPos_t targetLen;
        hpatch_StreamPos_t deltaLen;
        hpatch_StreamPos_t dataLen;
        hpatch_StreamPos_t instLen;
        hpatch_StreamPos_t addrLen;
        hpatch_StreamPos_t curDiffOffset;
        const hpatch_TStreamInput* srcData;
        unsigned char* temp_cache=tempCaches;
        hpatch_BOOL   isHaveAdler32;
        unsigned char Delta_Indicator;
        _TStreamCacheClip_init(&diffClip,compressedDiff,windowOffset,compressedDiff->streamSize,
                               _cache,sizeof(_cache));
        {
            unsigned char Win_Indicator;
            _clip_readUInt8(&diffClip,&Win_Indicator);
            isHaveAdler32=(0!=(Win_Indicator&VCD_ADLER32));
            if (isHaveAdler32) Win_Indicator-=VCD_ADLER32;
            switch (Win_Indicator){
                case 0         :{ srcData=0; } break;
                case VCD_SOURCE:{ srcData=oldData; } break;
                case VCD_TARGET:{
                    if (!_TOutStreamCache_flush(outCache))
                        return _hpatch_FALSE;
                    srcData=(const hpatch_TStreamInput*)outCache->dstStream;
                    if (srcData->read==0)
                        return _hpatch_FALSE; //unsupport, target can't read  
                } break;
                default:
                    return _hpatch_FALSE; //error data or unsupport
            }
            if (srcData){
                hpatch_StreamPos_t srcLen;
                _clip_unpackUInt64(&diffClip,&srcLen);
                _clip_unpackUInt64(&diffClip,&srcPos);
                srcPosEnd=srcPos+srcLen;
#ifdef __RUN_MEM_SAFE_CHECK
                {
                    hpatch_StreamPos_t streamSafeEnd;
                    if ((srcPosEnd<srcPos)|(srcPosEnd<srcLen))
                        return _hpatch_FALSE; //error data
                    streamSafeEnd=(srcData==oldData)?srcData->streamSize:outCache->writeToPos;
                    if (srcPosEnd>streamSafeEnd)
                        return _hpatch_FALSE; //error data
                }
#endif
            }
        }
        _clip_unpackUInt64(&diffClip,&deltaLen);
        {
#ifdef __RUN_MEM_SAFE_CHECK
            hpatch_StreamPos_t deltaHeadSize=_TStreamCacheClip_readPosOfSrcStream(&diffClip);
            if (deltaLen>_TStreamCacheClip_leaveSize(&diffClip))
                return _hpatch_FALSE; //error data
#endif
            _clip_unpackUInt64(&diffClip,&targetLen);
            _clip_readUInt8(&diffClip,&Delta_Indicator);
            if (decompressPlugin==0) Delta_Indicator=0;
            _clip_unpackUInt64(&diffClip,&dataLen);
            _clip_unpackUInt64(&diffClip,&instLen);
            _clip_unpackUInt64(&diffClip,&addrLen);
            if (isHaveAdler32){
                if (!_TStreamCacheClip_skipData(&diffClip,4)) //now not checksum
                    return _hpatch_FALSE; //error data or no data
            }
            curDiffOffset=_TStreamCacheClip_readPosOfSrcStream(&diffClip);
#ifdef __RUN_MEM_SAFE_CHECK
            deltaHeadSize=curDiffOffset-deltaHeadSize;
            if (deltaLen!=deltaHeadSize+dataLen+instLen+addrLen)
                return _hpatch_FALSE; //error data
#endif
            
        }
        windowOffset=curDiffOffset+dataLen+instLen+addrLen;

        {
            hpatch_BOOL result=hpatch_TRUE;
            hpatch_size_t i;
            TStreamCacheClip   dataClip;
            TStreamCacheClip   instClip;
            TStreamCacheClip   addrClip;
            _TDecompressInputStream decompressers[3];
            for (i=0;i<sizeof(decompressers)/sizeof(_TDecompressInputStream);++i)
                decompressers[i].decompressHandle=0;

            #define _getStreamClip(clip,index,len) \
                if (Delta_Indicator&(1<<index)){    \
                    if (!getStreamClip(clip,&decompressers[index],_kUnknowMaxSize,len,compressedDiff,&curDiffOffset, \
                                       decompressPlugin,temp_cache,cache_size)) _clear_return(_hpatch_FALSE); \
                }else{ \
                    if (!getStreamClip(clip,0,len,0,compressedDiff,&curDiffOffset, \
                                       0,temp_cache,cache_size)) _clear_return(_hpatch_FALSE); \
                } \
                temp_cache+=cache_size;
            _getStreamClip(&dataClip,0,dataLen);
            _getStreamClip(&instClip,1,instLen);
            _getStreamClip(&addrClip,2,addrLen);
            assert(curDiffOffset==windowOffset);
            
            result=_vcpatch_delta(outCache,targetLen,srcData,srcPos,srcPosEnd,
                                  &dataClip,&instClip,&addrClip,temp_cache,cache_size);

        clear:
            for (i=0;i<sizeof(decompressers)/sizeof(_TDecompressInputStream);++i) {
                if (decompressers[i].decompressHandle){
                    if (!decompressPlugin->close(decompressPlugin,decompressers[i].decompressHandle))
                        result=_hpatch_FALSE;
                    decompressers[i].decompressHandle=0;
                }
            }
            if (!result)
                return _hpatch_FALSE;
        }
    }

    if (!_TOutStreamCache_flush(outCache))
        return _hpatch_FALSE;
    if (_TOutStreamCache_isFinish(outCache))
        return hpatch_TRUE;
    else
        return _hpatch_FALSE;
}


#define _kCacheBsDecCount (1+3+1)

hpatch_BOOL vcpatch_with_cache(const hpatch_TStreamOutput* out_newData,
                               const hpatch_TStreamInput*  oldData,
                               const hpatch_TStreamInput*  compressedDiff,
                               hpatch_TDecompress* decompressPlugin,
                               unsigned char* temp_cache,unsigned char* temp_cache_end){
    hpatch_VcDiffInfo diffInfo;
    hpatch_BOOL  result=hpatch_TRUE;
    const hpatch_size_t cacheSize=(temp_cache_end-temp_cache)/_kCacheBsDecCount;
    if (cacheSize<hpatch_kMaxPackedUIntBytes) return _hpatch_FALSE;
    assert(out_newData!=0);
    assert(out_newData->write!=0);
    assert(oldData!=0);
    assert(oldData->read!=0);
    assert(compressedDiff!=0);
    assert(compressedDiff->read!=0);
    if (!getVcDiffInfo(&diffInfo,compressedDiff)) 
        return _hpatch_FALSE;

    if (diffInfo.compressorID){
        if (decompressPlugin==0)
            return _hpatch_FALSE;
    }else{
        decompressPlugin=0;
    }

    {
        _TOutStreamCache outCache;
        _TOutStreamCache_init(&outCache,out_newData,temp_cache,cacheSize);
        temp_cache+=cacheSize;
        result=_vcpatch_window(&outCache,oldData,compressedDiff,decompressPlugin,
                               diffInfo.windowOffset,temp_cache,cacheSize);
    }

    return result;
}