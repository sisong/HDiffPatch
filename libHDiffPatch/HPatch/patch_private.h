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

    
//byte rle type , ctrl code: high 2bit + packedLen(6bit+...)
typedef enum TByteRleType{
    kByteRleType_rle0  = 0,    //00 rle 0  , data code:0 byte
    kByteRleType_rle255= 1,    //01 rle 255, data code:0 byte
    kByteRleType_rle   = 2,    //10 rle x(1--254), data code:1 byte (save x)
    kByteRleType_unrle = 3     //11 n byte data, data code:n byte(save no rle data)
} TByteRleType;
    
static const int kByteRleType_bit=2;
    

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
    size_t          cacheBegin;
    size_t          cacheEnd;
} TStreamCacheClip;

hpatch_inline static
void _TStreamCacheClip_init(TStreamCacheClip* sclip,const hpatch_TStreamInput* srcStream,
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
#define _TStreamCacheClip_readPosOfSrcStream(sclip) ( \
            (sclip)->srcStream->streamSize - _TStreamCacheClip_streamSize(sclip) )
    
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
hpatch_BOOL _TStreamCacheClip_skipData(TStreamCacheClip* sclip,hpatch_StreamPos_t skipLongSize);
    
hpatch_inline static //error return 0
unsigned char* _TStreamCacheClip_readData(TStreamCacheClip* sclip,size_t readSize){
    unsigned char* result=_TStreamCacheClip_accessData(sclip,readSize);
    _TStreamCacheClip_skipData_noCheck(sclip,readSize);
    return result;
}

hpatch_BOOL _TStreamCacheClip_unpackUIntWithTag(TStreamCacheClip* sclip,
                                                hpatch_StreamPos_t* result,const int kTagBit);
    
hpatch_BOOL _TStreamCacheClip_readType_end(TStreamCacheClip* sclip,unsigned char endTag,
                                           char out_type[hpatch_kMaxPluginTypeLength+1]);
        
        
    typedef struct _TDecompressInputSteram{
        hpatch_TStreamInput         IInputSteram;
        hpatch_TDecompress*         decompressPlugin;
        hpatch_decompressHandle     decompressHandle;
    } _TDecompressInputSteram;
    
hpatch_BOOL getStreamClip(TStreamCacheClip* out_clip,_TDecompressInputSteram* out_stream,
                          hpatch_StreamPos_t dataSize,hpatch_StreamPos_t compressedSize,
                          const hpatch_TStreamInput* stream,hpatch_StreamPos_t* pCurStreamPos,
                          hpatch_TDecompress* decompressPlugin,unsigned char* aCache,size_t cacheSize);
    
#ifdef __cplusplus
}
#endif
#endif
