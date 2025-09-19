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

#include "patch.h"

#ifdef __cplusplus
extern "C" {
#endif

    
//byte rle type , ctrl code: high 2bit + packedLen(6bit+...)
typedef enum TByteRleType{
    kByteRleType_rle0  = 0,    //00 rle 0  , data code:0 byte
    kByteRleType_rle255= 1,    //01 rle 255, data code:0 byte
    kByteRleType_rle   = 2,    //10 rle x(1--254), data code:1 byte (save x)
    kByteRleType_unrle = 3     //11 n byte data, data code:n byte(save no rle data)
} TByteRleType;
    
static const hpatch_uint kByteRleType_bit=2;
    

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
    
    hpatch_StreamPos_t typesEndPos;
    hpatch_StreamPos_t compressSizeBeginPos;
    hpatch_StreamPos_t headEndPos;
    hpatch_StreamPos_t coverEndPos;
} _THDiffzHead;
    
hpatch_BOOL read_diffz_head(hpatch_compressedDiffInfo* out_diffInfo,_THDiffzHead* out_head,
                            const hpatch_TStreamInput* compressedDiff);

// Stream Clip cache
typedef struct TStreamCacheClip{
    hpatch_StreamPos_t          streamPos;
    hpatch_StreamPos_t          streamPos_end;
    const hpatch_TStreamInput*  srcStream;
    unsigned char*  cacheBuf;
    hpatch_size_t   cacheBegin;
    hpatch_size_t   cacheEnd;
} TStreamCacheClip;

hpatch_inline static
void _TStreamCacheClip_init(TStreamCacheClip* sclip,const hpatch_TStreamInput* srcStream,
                            hpatch_StreamPos_t streamPos,hpatch_StreamPos_t streamPos_end,
                            unsigned char* aCache,hpatch_size_t cacheSize){
    assert((streamPos<=streamPos_end)&&(streamPos_end<=(srcStream?srcStream->streamSize:0)));
    sclip->streamPos=streamPos;
    sclip->streamPos_end=streamPos_end;
    sclip->srcStream=srcStream;
    sclip->cacheBuf=aCache;
    sclip->cacheBegin=cacheSize;
    sclip->cacheEnd=cacheSize;
}
    
#define _TStreamCacheClip_isFinish(sclip)     ( 0==_TStreamCacheClip_leaveSize(sclip) )
#define _TStreamCacheClip_isCacheEmpty(sclip) ( (sclip)->cacheBegin==(sclip)->cacheEnd )
#define _TStreamCacheClip_cachedSize(sclip)   ( (hpatch_size_t)((sclip)->cacheEnd-(sclip)->cacheBegin) )
#define _TStreamCacheClip_leaveSize(sclip)   \
            (  (hpatch_StreamPos_t)((sclip)->streamPos_end-(sclip)->streamPos)  \
                + (hpatch_StreamPos_t)_TStreamCacheClip_cachedSize(sclip)  )
#define _TStreamCacheClip_readPosOfSrcStream(sclip) ( \
            (sclip)->streamPos - _TStreamCacheClip_cachedSize(sclip) )
    
hpatch_BOOL _TStreamCacheClip_updateCache(TStreamCacheClip* sclip);
    
hpatch_inline static //error return 0
unsigned char* _TStreamCacheClip_accessData(TStreamCacheClip* sclip,hpatch_size_t readSize){
    //assert(readSize<=sclip->cacheEnd);
    if (readSize>_TStreamCacheClip_cachedSize(sclip)){
        if (!_TStreamCacheClip_updateCache(sclip)) return 0;
        if (readSize>_TStreamCacheClip_cachedSize(sclip)) return 0;
    }
    return &sclip->cacheBuf[sclip->cacheBegin];
}

#define _TStreamCacheClip_skipData_noCheck(sclip,skipSize) ((sclip)->cacheBegin+=skipSize)
hpatch_BOOL _TStreamCacheClip_skipData(TStreamCacheClip* sclip,hpatch_StreamPos_t skipLongSize);
    
hpatch_inline static //error return 0
unsigned char* _TStreamCacheClip_readData(TStreamCacheClip* sclip,hpatch_size_t readSize){
    unsigned char* result=_TStreamCacheClip_accessData(sclip,readSize);
    _TStreamCacheClip_skipData_noCheck(sclip,readSize);
    return result;
}

hpatch_BOOL _TStreamCacheClip_readDataTo(TStreamCacheClip* sclip,
                                         unsigned char* out_buf,unsigned char* bufEnd);
hpatch_BOOL _TStreamCacheClip_addDataTo(TStreamCacheClip* self,unsigned char* dst,hpatch_size_t addLen);

hpatch_BOOL _TStreamCacheClip_unpackUIntWithTag(TStreamCacheClip* sclip,
                                                hpatch_StreamPos_t* result,const hpatch_uint kTagBit);

hpatch_BOOL _TStreamCacheClip_readStr_end(TStreamCacheClip* sclip,hpatch_byte endTag,
                                          char* out_str,size_t strBufLen);
hpatch_inline static
hpatch_BOOL _TStreamCacheClip_readType_end(TStreamCacheClip* sclip,hpatch_byte endTag,
                                           char out_type[hpatch_kMaxPluginTypeLength+1]){
                return _TStreamCacheClip_readStr_end(sclip,endTag,out_type,hpatch_kMaxPluginTypeLength+1); }

// Stream Clip cache
typedef struct {
    hpatch_StreamPos_t           writeToPos;
    const hpatch_TStreamOutput*  dstStream;
    unsigned char*  cacheBuf;
    hpatch_size_t   cacheCur;
    hpatch_size_t   cacheEnd;
} _TOutStreamCache;

static hpatch_inline void _TOutStreamCache_init(_TOutStreamCache* self,const hpatch_TStreamOutput*  dstStream,
                                                unsigned char* aCache,hpatch_size_t aCacheSize){
    self->writeToPos=0;
    self->cacheCur=0;
    self->dstStream=dstStream;
    self->cacheBuf=aCache;
    self->cacheEnd=aCacheSize;
}
hpatch_inline static
void _TOutStreamCache_resetCache(_TOutStreamCache* self,unsigned char* aCache,hpatch_size_t aCacheSize){
    assert(0==self->cacheCur);
    self->cacheBuf=aCache;
    self->cacheEnd=aCacheSize;
}

static hpatch_inline hpatch_StreamPos_t _TOutStreamCache_leaveSize(const _TOutStreamCache* self){
    return self->dstStream->streamSize-self->writeToPos;
}
static hpatch_inline hpatch_BOOL _TOutStreamCache_isFinish(const _TOutStreamCache* self){
    return self->writeToPos==self->dstStream->streamSize;
}
static hpatch_inline hpatch_size_t _TOutStreamCache_cachedDataSize(const _TOutStreamCache* self){
    return self->cacheCur;
}
hpatch_BOOL _TOutStreamCache_flush(_TOutStreamCache* self);
hpatch_BOOL _TOutStreamCache_write(_TOutStreamCache* self,const unsigned char* data,hpatch_size_t dataSize);
hpatch_BOOL _TOutStreamCache_fill(_TOutStreamCache* self,hpatch_byte fillValue,hpatch_StreamPos_t fillLength);

hpatch_BOOL _TOutStreamCache_copyFromClip(_TOutStreamCache* self,TStreamCacheClip* src,hpatch_StreamPos_t copyLength);
hpatch_BOOL _TOutStreamCache_copyFromStream(_TOutStreamCache* self,const hpatch_TStreamInput* src,
                                            hpatch_StreamPos_t srcPos,hpatch_StreamPos_t copyLength);
hpatch_BOOL _TOutStreamCache_copyFromSelf(_TOutStreamCache* self,hpatch_StreamPos_t aheadLength,hpatch_StreamPos_t copyLength);


    typedef struct _TDecompressInputStream{
        hpatch_TStreamInput         IInputStream;
        hpatch_TDecompress*         decompressPlugin;
        hpatch_decompressHandle     decompressHandle;
    } _TDecompressInputStream;
    
hpatch_BOOL getStreamClip(TStreamCacheClip* out_clip,_TDecompressInputStream* out_stream,
                          hpatch_StreamPos_t dataSize,hpatch_StreamPos_t compressedSize,
                          const hpatch_TStreamInput* stream,hpatch_StreamPos_t* pCurStreamPos,
                          hpatch_TDecompress* decompressPlugin,unsigned char* aCache,hpatch_size_t cacheSize);
    

#define _TDiffToSingleStream_kBufSize hpatch_kStreamCacheSize
typedef struct {
    hpatch_TStreamInput         base;
    const hpatch_TStreamInput*  diffStream;
    hpatch_StreamPos_t          readedSize;
    hpatch_size_t               cachedBufBegin;
    hpatch_BOOL                 isInSingleStream;
    unsigned char               buf[_TDiffToSingleStream_kBufSize];
} TDiffToSingleStream;

void TDiffToSingleStream_init(TDiffToSingleStream* self,const hpatch_TStreamInput* diffStream);
hpatch_inline static 
void TDiffToSingleStream_setInSingleStream(TDiffToSingleStream* self,hpatch_StreamPos_t singleStreamPos){
    self->isInSingleStream=hpatch_TRUE; }
    
hpatch_inline static
void TDiffToSingleStream_resetStream(TDiffToSingleStream* self,const hpatch_TStreamInput* diffStream){
    self->diffStream=diffStream; }

static hpatch_force_inline 
hpatch_StreamPos_t _patch_cache_all_old_needSize(hpatch_StreamPos_t oldDataSize,hpatch_size_t kMinTempCacheSize){
                                                    return oldDataSize+kMinTempCacheSize+sizeof(hpatch_TStreamInput)+sizeof(hpatch_StreamPos_t); }
#if (_IS_NEED_CACHE_OLD_ALL)
static hpatch_force_inline 
hpatch_BOOL _patch_is_can_cache_all_old(hpatch_StreamPos_t oldDataSize,hpatch_size_t kMinTempCacheSize,hpatch_size_t tempCacheSize){
                                            return tempCacheSize>=_patch_cache_all_old_needSize(oldDataSize,kMinTempCacheSize); }
hpatch_BOOL _patch_cache_all_old(const hpatch_TStreamInput** poldData,size_t kMinTempCacheSize,
                                 hpatch_byte** ptemp_cache,hpatch_byte** ptemp_cache_end,hpatch_BOOL* out_isReadError);// try cache all oldData
#else
static hpatch_force_inline 
hpatch_BOOL _patch_is_can_cache_all_old(hpatch_StreamPos_t oldDataSize,hpatch_size_t kMinTempCacheSize,hpatch_size_t tempCacheSize){
                                            return hpatch_FALSE; }
static hpatch_force_inline 
hpatch_BOOL _patch_cache_all_old(const hpatch_TStreamInput** poldData,size_t kMinTempCacheSize,
                                 hpatch_byte** ptemp_cache,hpatch_byte** ptemp_cache_end,hpatch_BOOL* out_isReadError){
                                            *out_isReadError=hpatch_FALSE; return hpatch_FALSE; }
#endif

#if (_IS_NEED_CACHE_OLD_BY_COVERS)

hpatch_size_t _patch_step_cache_old_canUsedSize(hpatch_size_t stepCoversMemSize,hpatch_size_t kMinTempCacheSize,hpatch_size_t tempCacheSize);

// try cache part of oldData, used by patch_single_stream_diff()
hpatch_BOOL _patch_step_cache_old(const hpatch_TStreamInput** poldData,hpatch_StreamPos_t newDataSize,size_t stepCoversMemSize,
                                  size_t kMinTempCacheSize,hpatch_byte** ptemp_cache,hpatch_byte** ptemp_cache_end);
hpatch_BOOL _patch_step_cache_old_onStepCovers(const hpatch_TStreamInput* self,const unsigned char* covers_cache,const unsigned char* covers_cacheEnd);
#endif // _IS_NEED_CACHE_OLD_BY_COVERS

#ifdef __cplusplus
}
#endif
#endif
