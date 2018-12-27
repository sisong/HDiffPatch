//  patch_private.h
//
/*
 The MIT License (MIT)
 Copyright (c) 2012-2018 HouSisong
 
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

#ifndef HPatch_patch_private_h
#define HPatch_patch_private_h

#include "patch_types.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct _THDiffzHead{
    hpatch_StreamPos_t coverCount;
    
    hpatch_StreamPos_t cover_buf_size;
    hpatch_StreamPos_t compress_cover_buf_size;
    hpatch_StreamPos_t rle_ctrlBuf_size;
    hpatch_StreamPos_t compress_rle_ctrlBuf_size;
    hpatch_StreamPos_t rle_codeBuf_size;
    hpatch_StreamPos_t compress_rle_codeBuf_size;
    hpatch_StreamPos_t newDataDiff_size;
    hpatch_StreamPos_t compress_newDataDiff_size;
    
    hpatch_StreamPos_t headEndPos;
    hpatch_StreamPos_t coverEndPos;
} _THDiffzHead;
    
hpatch_BOOL read_diffz_head(hpatch_compressedDiffInfo* out_diffInfo,_THDiffzHead* out_head,
                            const hpatch_TStreamInput* compressedDiff);

// Stream Clip cache
typedef struct TStreamCacheClip{
    hpatch_StreamPos_t       streamPos;
    hpatch_StreamPos_t       streamPos_end;
    const struct hpatch_TStreamInput*  srcStream;
    unsigned char*  cacheBuf;
    size_t          cacheBegin;
    size_t          cacheEnd;
} TStreamCacheClip;

hpatch_inline static
void _TStreamCacheClip_init(TStreamCacheClip* sclip,const struct hpatch_TStreamInput* srcStream,
                            hpatch_StreamPos_t streamPos,hpatch_StreamPos_t streamPos_end,
                            unsigned char* aCache,size_t cacheSize){
    sclip->streamPos=streamPos;
    sclip->streamPos_end=streamPos_end;
    sclip->srcStream=srcStream;
    sclip->cacheBuf=aCache;
    sclip->cacheBegin=cacheSize;
    sclip->cacheEnd=cacheSize;
}
    
#define _TStreamCacheClip_isFinish(sclip)     ( 0==_TStreamCacheClip_streamSize(sclip) )
#define _TStreamCacheClip_isCacheEmpty(sclip) ( (sclip)->cacheBegin==(sclip)->cacheEnd )
#define _TStreamCacheClip_cachedSize(sclip)   ( (size_t)((sclip)->cacheEnd-(sclip)->cacheBegin) )
#define _TStreamCacheClip_streamSize(sclip)   \
            (  (hpatch_StreamPos_t)((sclip)->streamPos_end-(sclip)->streamPos)  \
                + (hpatch_StreamPos_t)_TStreamCacheClip_cachedSize(sclip)  )
    
hpatch_BOOL _TStreamCacheClip_updateCache(TStreamCacheClip* sclip);
    
hpatch_inline static //error return 0
unsigned char* _TStreamCacheClip_accessData(TStreamCacheClip* sclip,size_t readSize){
    //assert(readSize<=sclip->cacheEnd);
    if (readSize>_TStreamCacheClip_cachedSize(sclip)){
        if (!_TStreamCacheClip_updateCache(sclip)) return 0;
        if (readSize>_TStreamCacheClip_cachedSize(sclip)) return 0;
    }
    return &sclip->cacheBuf[sclip->cacheBegin];
}

#define _TStreamCacheClip_skipData_noCheck(sclip,skipSize) ((sclip)->cacheBegin+=skipSize)
    
hpatch_inline static //error return 0
unsigned char* _TStreamCacheClip_readData(TStreamCacheClip* sclip,size_t readSize){
    unsigned char* result=_TStreamCacheClip_accessData(sclip,readSize);
    _TStreamCacheClip_skipData_noCheck(sclip,readSize);
    return result;
}

hpatch_BOOL _TStreamCacheClip_unpackUIntWithTag(TStreamCacheClip* sclip,
                                                hpatch_StreamPos_t* result,const int kTagBit);

#define _TStreamCacheClip_unpackUIntWithTagTo(puint,sclip,kTagBit) \
    { if (!_TStreamCacheClip_unpackUIntWithTag(sclip,puint,kTagBit)) return _hpatch_FALSE; }
#define _TStreamCacheClip_unpackUIntTo(puint,sclip) \
    _TStreamCacheClip_unpackUIntWithTagTo(puint,sclip,0)
    
#ifdef __cplusplus
}
#endif
#endif
