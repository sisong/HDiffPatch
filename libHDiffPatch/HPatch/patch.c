//patch.c
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
#include "patch.h"
#if (_IS_NEED_CACHE_OLD_BY_COVERS)
#   include <stdlib.h> //qsort
#endif
#include "patch_private.h"

#ifndef  _IS_NEED_MIN_CODE_SIZE
#   define _IS_NEED_MIN_CODE_SIZE  0   //default used fast code
#endif

#ifndef _IS_RUN_MEM_SAFE_CHECK
#   define _IS_RUN_MEM_SAFE_CHECK  1
#endif

#if (_IS_RUN_MEM_SAFE_CHECK)
//__RUN_MEM_SAFE_CHECK用来启动内存访问越界检查,用以防御可能被意外或故意损坏的数据.
#   define __RUN_MEM_SAFE_CHECK
#endif

#ifdef __RUN_MEM_SAFE_CHECK
#   define  _SAFE_CHECK_DO(code)    do{ if (!(code)) return _hpatch_FALSE; }while(0)
#else
#   define  _SAFE_CHECK_DO(code)    do{ code; }while(0)
#endif

#define _hpatch_FALSE   hpatch_FALSE
//hpatch_uint __debug_check_false_x=0; //for debug
//#define _hpatch_FALSE (1/__debug_check_false_x)

typedef unsigned char TByte;


//变长正整数编码方案(x bit额外类型标志位,x<=7),从高位开始输出1--n byte:
// x0*  7-x bit
// x1* 0*  7+7-x bit
// x1* 1* 0*  7+7+7-x bit
// x1* 1* 1* 0*  7+7+7+7-x bit
// x1* 1* 1* 1* 0*  7+7+7+7+7-x bit
// ......
hpatch_BOOL hpatch_packUIntWithTag(TByte** out_code,TByte* out_code_end,
                                   hpatch_StreamPos_t uValue,hpatch_uint highTag,
                                   const hpatch_uint kTagBit){//写入整数并前进指针.
    TByte*          pcode=*out_code;
    const hpatch_StreamPos_t kMaxValueWithTag=((hpatch_StreamPos_t)1<<(7-kTagBit))-1;
    TByte           codeBuf[hpatch_kMaxPackedUIntBytes];
    TByte*          codeEnd=codeBuf;
#ifdef __RUN_MEM_SAFE_CHECK
    //static const hpatch_uint kPackMaxTagBit=7;
    //assert((0<=kTagBit)&&(kTagBit<=kPackMaxTagBit));
    //assert((highTag>>kTagBit)==0);
#endif
    while (uValue>kMaxValueWithTag) {
        *codeEnd=uValue&((1<<7)-1); ++codeEnd;
        uValue>>=7;
    }
#ifdef __RUN_MEM_SAFE_CHECK
    if ((out_code_end-pcode)<(1+(codeEnd-codeBuf))) return _hpatch_FALSE;
#endif
    *pcode=(TByte)( (TByte)uValue | (highTag<<(8-kTagBit))
                   | (((codeBuf!=codeEnd)?1:0)<<(7-kTagBit))  );
    ++pcode;
    while (codeBuf!=codeEnd) {
        --codeEnd;
        *pcode=(*codeEnd) | (((codeBuf!=codeEnd)?1:0)<<7);
        ++pcode;
    }
    *out_code=pcode;
    return hpatch_TRUE;
}

hpatch_uint hpatch_packUIntWithTag_size(hpatch_StreamPos_t uValue,const hpatch_uint kTagBit){
    const hpatch_StreamPos_t kMaxValueWithTag=((hpatch_StreamPos_t)1<<(7-kTagBit))-1;
    hpatch_uint size=0;
    while (uValue>kMaxValueWithTag) {
        ++size;
        uValue>>=7;
    }
    ++size;
    return size;
}

hpatch_BOOL hpatch_unpackUIntWithTag(const TByte** src_code,const TByte* src_code_end,
                                     hpatch_StreamPos_t* result,const hpatch_uint kTagBit){//读出整数并前进指针.
#ifdef __RUN_MEM_SAFE_CHECK
    //const hpatch_uint kPackMaxTagBit=7;
#endif
    hpatch_StreamPos_t  value;
    TByte               code;
    const TByte*        pcode=*src_code;
    
#ifdef __RUN_MEM_SAFE_CHECK
    //assert(kTagBit<=kPackMaxTagBit);
    if (src_code_end<=pcode) return _hpatch_FALSE;
#endif
    code=*pcode; ++pcode;
    value=code&((1<<(7-kTagBit))-1);
    if ((code&(1<<(7-kTagBit)))!=0){
        do {
#ifdef __RUN_MEM_SAFE_CHECK
            if ((value>>(sizeof(value)*8-7))!=0) return _hpatch_FALSE;//cannot save 7bit
            if (src_code_end==pcode) return _hpatch_FALSE;
#endif
            code=*pcode; ++pcode;
            value=(value<<7) | (code&((1<<7)-1));
        } while ((code&(1<<7))!=0);
    }
    (*src_code)=pcode;
    *result=value;
    return hpatch_TRUE;
}


    static hpatch_BOOL _read_mem_stream(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                        unsigned char* out_data,unsigned char* out_data_end){
        const unsigned char* src=(const unsigned char*)stream->streamImport;
        hpatch_size_t readLen=out_data_end-out_data;
#ifdef __RUN_MEM_SAFE_CHECK
        if (readFromPos>stream->streamSize) return _hpatch_FALSE;
        if (readLen>(hpatch_StreamPos_t)(stream->streamSize-readFromPos)) return _hpatch_FALSE;
#endif
        memcpy(out_data,src+readFromPos,readLen);
        return hpatch_TRUE;
    }
void mem_as_hStreamInput(hpatch_TStreamInput* out_stream,
                         const unsigned char* mem,const unsigned char* mem_end){
    out_stream->streamImport=(void*)mem;
    out_stream->streamSize=mem_end-mem;
    out_stream->read=_read_mem_stream;
}

    static hpatch_BOOL _write_mem_stream(const hpatch_TStreamOutput* stream,hpatch_StreamPos_t writeToPos,
                                         const unsigned char* data,const unsigned char* data_end){
        unsigned char* out_dst=(unsigned char*)stream->streamImport;
        hpatch_size_t writeLen=data_end-data;
#ifdef __RUN_MEM_SAFE_CHECK
        if (writeToPos>stream->streamSize) return _hpatch_FALSE;
        if (writeLen>(hpatch_StreamPos_t)(stream->streamSize-writeToPos)) return _hpatch_FALSE;
#endif
        memcpy(out_dst+writeToPos,data,writeLen);
        return hpatch_TRUE;
    }
    typedef hpatch_BOOL (*_read_mem_stream_t)(const hpatch_TStreamOutput* stream,hpatch_StreamPos_t readFromPos,
                                              unsigned char* out_data,unsigned char* out_data_end);
void mem_as_hStreamOutput(hpatch_TStreamOutput* out_stream,
                          unsigned char* mem,unsigned char* mem_end){
    out_stream->streamImport=mem;
    out_stream->streamSize=mem_end-mem;
    out_stream->read_writed=(_read_mem_stream_t)_read_mem_stream;
    out_stream->write=_write_mem_stream;
}

hpatch_BOOL hpatch_deccompress_mem(hpatch_TDecompress* decompressPlugin,
                                   const unsigned char* code,const unsigned char* code_end,
                                   unsigned char* out_data,unsigned char* out_data_end){
    hpatch_decompressHandle dec=0;
    hpatch_BOOL result,colose_rt;
    hpatch_TStreamInput  codeStream;
    mem_as_hStreamInput(&codeStream,code,code_end);
    dec=decompressPlugin->open(decompressPlugin,(out_data_end-out_data),
                               &codeStream,0,codeStream.streamSize);
    if (dec==0) return _hpatch_FALSE;
    result=decompressPlugin->decompress_part(dec,out_data,out_data_end);
    colose_rt=decompressPlugin->close(decompressPlugin,dec);
    assert(colose_rt);
    return result;
}


////////
//patch by memory

static const hpatch_uint kSignTagBit=1;

static hpatch_BOOL _bytesRle_load(TByte* out_data,TByte* out_dataEnd,
                                  const TByte* rle_code,const TByte* rle_code_end);
static void addData(TByte* dst,const TByte* src,hpatch_size_t length);
hpatch_inline
static hpatch_BOOL _unpackUIntWithTag(const TByte** src_code,const TByte* src_code_end,
                                      hpatch_size_t* result,const hpatch_uint kTagBit){
    if (sizeof(hpatch_size_t)==sizeof(hpatch_StreamPos_t)){
        return hpatch_unpackUIntWithTag(src_code,src_code_end,(hpatch_StreamPos_t*)result,kTagBit);
    }else{
        hpatch_StreamPos_t u64=0;
        hpatch_BOOL rt=hpatch_unpackUIntWithTag(src_code,src_code_end,&u64,kTagBit);
        hpatch_size_t u=(hpatch_size_t)u64;
        *result=u;
#ifdef __RUN_MEM_SAFE_CHECK
        return rt&(u==u64);
#else
        return rt;
#endif
    }
}

#define unpackUIntWithTagTo(puint,src_code,src_code_end,kTagBit) \
        _SAFE_CHECK_DO(_unpackUIntWithTag(src_code,src_code_end,puint,kTagBit))
#define unpackUIntTo(puint,src_code,src_code_end) \
        unpackUIntWithTagTo(puint,src_code,src_code_end,0)

hpatch_BOOL patch(TByte* out_newData,TByte* out_newData_end,
                  const TByte* oldData,const TByte* oldData_end,
                  const TByte* serializedDiff,const TByte* serializedDiff_end){
    const TByte *code_lengths, *code_lengths_end,
                *code_inc_newPos, *code_inc_newPos_end,
                *code_inc_oldPos, *code_inc_oldPos_end,
                *code_newDataDiff, *code_newDataDiff_end;
    hpatch_size_t  coverCount;

    assert(out_newData<=out_newData_end);
    assert(oldData<=oldData_end);
    assert(serializedDiff<=serializedDiff_end);
    unpackUIntTo(&coverCount,&serializedDiff, serializedDiff_end);
    {   //head
        hpatch_size_t lengthSize,inc_newPosSize,inc_oldPosSize,newDataDiffSize;
        unpackUIntTo(&lengthSize,&serializedDiff, serializedDiff_end);
        unpackUIntTo(&inc_newPosSize,&serializedDiff, serializedDiff_end);
        unpackUIntTo(&inc_oldPosSize,&serializedDiff, serializedDiff_end);
        unpackUIntTo(&newDataDiffSize,&serializedDiff, serializedDiff_end);
#ifdef __RUN_MEM_SAFE_CHECK
        if (lengthSize>(hpatch_size_t)(serializedDiff_end-serializedDiff)) return _hpatch_FALSE;
#endif
        code_lengths=serializedDiff;     serializedDiff+=lengthSize;
        code_lengths_end=serializedDiff;
#ifdef __RUN_MEM_SAFE_CHECK
        if (inc_newPosSize>(hpatch_size_t)(serializedDiff_end-serializedDiff)) return _hpatch_FALSE;
#endif
        code_inc_newPos=serializedDiff; serializedDiff+=inc_newPosSize;
        code_inc_newPos_end=serializedDiff;
#ifdef __RUN_MEM_SAFE_CHECK
        if (inc_oldPosSize>(hpatch_size_t)(serializedDiff_end-serializedDiff)) return _hpatch_FALSE;
#endif
        code_inc_oldPos=serializedDiff; serializedDiff+=inc_oldPosSize;
        code_inc_oldPos_end=serializedDiff;
#ifdef __RUN_MEM_SAFE_CHECK
        if (newDataDiffSize>(hpatch_size_t)(serializedDiff_end-serializedDiff)) return _hpatch_FALSE;
#endif
        code_newDataDiff=serializedDiff; serializedDiff+=newDataDiffSize;
        code_newDataDiff_end=serializedDiff;
    }

    //decode rle ; rle data begin==cur serializedDiff;
    _SAFE_CHECK_DO(_bytesRle_load(out_newData, out_newData_end, serializedDiff, serializedDiff_end));

    {   //patch
        const hpatch_size_t newDataSize=(hpatch_size_t)(out_newData_end-out_newData);
        hpatch_size_t oldPosBack=0;
        hpatch_size_t newPosBack=0;
        hpatch_size_t i;
        for (i=0; i<coverCount; ++i){
            hpatch_size_t copyLength,addLength, oldPos,inc_oldPos,inc_oldPos_sign;
            unpackUIntTo(&copyLength,&code_inc_newPos, code_inc_newPos_end);
            unpackUIntTo(&addLength,&code_lengths, code_lengths_end);
#ifdef __RUN_MEM_SAFE_CHECK
            if (code_inc_oldPos>=code_inc_oldPos_end) return _hpatch_FALSE;
#endif
            inc_oldPos_sign=(*code_inc_oldPos)>>(8-kSignTagBit);
            unpackUIntWithTagTo(&inc_oldPos,&code_inc_oldPos, code_inc_oldPos_end, kSignTagBit);
            if (inc_oldPos_sign==0)
                oldPos=oldPosBack+inc_oldPos;
            else
                oldPos=oldPosBack-inc_oldPos;
            if (copyLength>0){
#ifdef __RUN_MEM_SAFE_CHECK
                if (copyLength>(hpatch_size_t)(newDataSize-newPosBack)) return _hpatch_FALSE;
                if (copyLength>(hpatch_size_t)(code_newDataDiff_end-code_newDataDiff)) return _hpatch_FALSE;
#endif
                memcpy(out_newData+newPosBack,code_newDataDiff,copyLength);
                code_newDataDiff+=copyLength;
                newPosBack+=copyLength;
            }
#ifdef __RUN_MEM_SAFE_CHECK
            if ( (addLength>(hpatch_size_t)(newDataSize-newPosBack)) ) return _hpatch_FALSE;
            if ( (oldPos>(hpatch_size_t)(oldData_end-oldData)) ||
                 (addLength>(hpatch_size_t)(oldData_end-oldData-oldPos)) ) return _hpatch_FALSE;
#endif
            addData(out_newData+newPosBack,oldData+oldPos,addLength);
            oldPosBack=oldPos;
            newPosBack+=addLength;
        }

        if (newPosBack<newDataSize){
            hpatch_size_t copyLength=newDataSize-newPosBack;
#ifdef __RUN_MEM_SAFE_CHECK
            if (copyLength>(hpatch_size_t)(code_newDataDiff_end-code_newDataDiff)) return _hpatch_FALSE;
#endif
            memcpy(out_newData+newPosBack,code_newDataDiff,copyLength);
            code_newDataDiff+=copyLength;
            //newPosBack=newDataSize;
        }
    }

    if (  (code_lengths==code_lengths_end)
        &&(code_inc_newPos==code_inc_newPos_end)
        &&(code_inc_oldPos==code_inc_oldPos_end)
        &&(code_newDataDiff==code_newDataDiff_end))
        return hpatch_TRUE;
    else
        return _hpatch_FALSE;
}

#if (_IS_NEED_MIN_CODE_SIZE)
hpatch_inline static void addData(TByte* dst,const TByte* src,hpatch_size_t length){
    while (length--) { *dst++ += *src++; }
}
#else
static void addData(TByte* dst,const TByte* src,hpatch_size_t length){
    hpatch_size_t length_fast,i;
    
    length_fast=length&(~(hpatch_size_t)7);
    for (i=0;i<length_fast;i+=8){
        dst[i  ]+=src[i  ];
        dst[i+1]+=src[i+1];
        dst[i+2]+=src[i+2];
        dst[i+3]+=src[i+3];
        dst[i+4]+=src[i+4];
        dst[i+5]+=src[i+5];
        dst[i+6]+=src[i+6];
        dst[i+7]+=src[i+7];
    }
    for (;i<length;++i)
        dst[i]+=src[i];
}
#endif


static hpatch_BOOL _bytesRle_load(TByte* out_data,TByte* out_dataEnd,
                                  const TByte* rle_code,const TByte* rle_code_end){
    const TByte*    ctrlBuf,*ctrlBuf_end;
    hpatch_size_t ctrlSize;
    unpackUIntTo(&ctrlSize,&rle_code,rle_code_end);
#ifdef __RUN_MEM_SAFE_CHECK
    if (ctrlSize>(hpatch_size_t)(rle_code_end-rle_code)) return _hpatch_FALSE;
#endif
    ctrlBuf=rle_code;
    rle_code+=ctrlSize;
    ctrlBuf_end=rle_code;
    while (ctrlBuf_end-ctrlBuf>0){
        enum TByteRleType type=(enum TByteRleType)((*ctrlBuf)>>(8-kByteRleType_bit));
        hpatch_size_t length;
        unpackUIntWithTagTo(&length,&ctrlBuf,ctrlBuf_end,kByteRleType_bit);
#ifdef __RUN_MEM_SAFE_CHECK
        if (length>=(hpatch_size_t)(out_dataEnd-out_data)) return _hpatch_FALSE;
#endif
        ++length;
        switch (type){
            case kByteRleType_rle0:{
                memset(out_data,0,length);
                out_data+=length;
            }break;
            case kByteRleType_rle255:{
                memset(out_data,255,length);
                out_data+=length;
            }break;
            case kByteRleType_rle:{
#ifdef __RUN_MEM_SAFE_CHECK
                if (1>(hpatch_size_t)(rle_code_end-rle_code)) return _hpatch_FALSE;
#endif
                memset(out_data,*rle_code,length);
                ++rle_code;
                out_data+=length;
            }break;
            case kByteRleType_unrle:{
#ifdef __RUN_MEM_SAFE_CHECK
                if (length>(hpatch_size_t)(rle_code_end-rle_code)) return _hpatch_FALSE;
#endif
                memcpy(out_data,rle_code,length);
                rle_code+=length;
                out_data+=length;
            }break;
        }
    }
    
    if (  (ctrlBuf==ctrlBuf_end)
        &&(rle_code==rle_code_end)
        &&(out_data==out_dataEnd))
        return hpatch_TRUE;
    else
        return _hpatch_FALSE;
}

//----------------------
//patch by stream

    static hpatch_BOOL _TStreamInputClip_read(const hpatch_TStreamInput* stream,
                                              hpatch_StreamPos_t readFromPos,
                                              unsigned char* out_data,unsigned char* out_data_end){
        TStreamInputClip* self=(TStreamInputClip*)stream->streamImport;
#ifdef __RUN_MEM_SAFE_CHECK
        if (readFromPos+(out_data_end-out_data)>self->base.streamSize) return _hpatch_FALSE;
#endif
        return self->srcStream->read(self->srcStream,readFromPos+self->clipBeginPos,out_data,out_data_end);
    }
void TStreamInputClip_init(TStreamInputClip* self,const hpatch_TStreamInput*  srcStream,
                           hpatch_StreamPos_t clipBeginPos,hpatch_StreamPos_t clipEndPos){
    assert(self!=0);
    assert(srcStream!=0);
    assert(clipBeginPos<=clipEndPos);
    assert(clipEndPos<=srcStream->streamSize);
    self->srcStream=srcStream;
    self->clipBeginPos=clipBeginPos;
    self->base.streamImport=self;
    self->base.streamSize=clipEndPos-clipBeginPos;
    self->base.read=_TStreamInputClip_read;
}

    static hpatch_BOOL _TStreamOutputClip_write(const hpatch_TStreamOutput* stream,
                                                hpatch_StreamPos_t writePos,
                                                const unsigned char* data,const unsigned char* data_end){
    TStreamOutputClip* self=(TStreamOutputClip*)stream->streamImport;
#ifdef __RUN_MEM_SAFE_CHECK
    if (writePos+(data_end-data)>self->base.streamSize) return _hpatch_FALSE;
#endif
    return self->srcStream->write(self->srcStream,writePos+self->clipBeginPos,data,data_end);
}

void TStreamOutputClip_init(TStreamOutputClip* self,const hpatch_TStreamOutput*  srcStream,
                            hpatch_StreamPos_t clipBeginPos,hpatch_StreamPos_t clipEndPos){
    assert(self!=0);
    assert(srcStream!=0);
    assert(clipBeginPos<=clipEndPos);
    assert(clipEndPos<=srcStream->streamSize);
    self->srcStream=srcStream;
    self->clipBeginPos=clipBeginPos;
    self->base.streamImport=self;
    self->base.streamSize=clipEndPos-clipBeginPos;
    ((TStreamInputClip*)self)->base.read=_TStreamInputClip_read;
    self->base.write=_TStreamOutputClip_write;
}



//assert(hpatch_kStreamCacheSize>=hpatch_kMaxPluginTypeLength+1);
struct __private_hpatch_check_kMaxCompressTypeLength {
    char _[(hpatch_kStreamCacheSize>=(hpatch_kMaxPluginTypeLength+1))?1:-1];};

hpatch_BOOL _TStreamCacheClip_readType_end(TStreamCacheClip* sclip,TByte endTag,
                                           char out_type[hpatch_kMaxPluginTypeLength+1]){
    const TByte* type_begin;
    hpatch_size_t i;
    hpatch_size_t readLen=hpatch_kMaxPluginTypeLength+1;
    if (readLen>_TStreamCacheClip_streamSize(sclip))
        readLen=(hpatch_size_t)_TStreamCacheClip_streamSize(sclip);
    type_begin=_TStreamCacheClip_accessData(sclip,readLen);
    if (type_begin==0) return _hpatch_FALSE;//not found
    for (i=0; i<readLen; ++i) {
        if (type_begin[i]!=endTag)
            continue;
        else{
            memcpy(out_type,type_begin,i);  out_type[i]='\0';
            _TStreamCacheClip_skipData_noCheck(sclip,i+1);
            return hpatch_TRUE;
        }
    }
    return _hpatch_FALSE;//not found
}

hpatch_BOOL _TStreamCacheClip_updateCache(TStreamCacheClip* sclip){
    TByte* buf0=&sclip->cacheBuf[0];
    const hpatch_StreamPos_t streamSize=sclip->streamPos_end-sclip->streamPos;
    hpatch_size_t readSize=sclip->cacheBegin;
    if (readSize>streamSize)
        readSize=(hpatch_size_t)streamSize;
    if (readSize==0) return hpatch_TRUE;
    if (!_TStreamCacheClip_isCacheEmpty(sclip)){
        memmove(buf0+(hpatch_size_t)(sclip->cacheBegin-readSize),
                buf0+sclip->cacheBegin,_TStreamCacheClip_cachedSize(sclip));
    }
    if (!sclip->srcStream->read(sclip->srcStream,sclip->streamPos,
                                buf0+(sclip->cacheEnd-readSize),buf0+sclip->cacheEnd))
        return _hpatch_FALSE;//read error
    sclip->cacheBegin-=readSize;
    sclip->streamPos+=readSize;
    return hpatch_TRUE;
}

hpatch_BOOL _TStreamCacheClip_skipData(TStreamCacheClip* sclip,hpatch_StreamPos_t skipLongSize){
    while (skipLongSize>0) {
        hpatch_size_t len=sclip->cacheEnd;
        if (len>skipLongSize)
            len=(hpatch_size_t)skipLongSize;
        if (_TStreamCacheClip_accessData(sclip,len)){
            _TStreamCacheClip_skipData_noCheck(sclip,len);
            skipLongSize-=len;
        }else{
            return _hpatch_FALSE;
        }
    }
    return hpatch_TRUE;
}

//assert(hpatch_kStreamCacheSize>=hpatch_kMaxPackedUIntBytes);
struct __private_hpatch_check_hpatch_kMaxPackedUIntBytes {
    char _[(hpatch_kStreamCacheSize>=hpatch_kMaxPackedUIntBytes)?1:-1]; };

hpatch_BOOL _TStreamCacheClip_unpackUIntWithTag(TStreamCacheClip* sclip,hpatch_StreamPos_t* result,const hpatch_uint kTagBit){
    TByte* curCode,*codeBegin;
    hpatch_size_t readSize=hpatch_kMaxPackedUIntBytes;
    const hpatch_StreamPos_t dataSize=_TStreamCacheClip_streamSize(sclip);
    if (readSize>dataSize)
        readSize=(hpatch_size_t)dataSize;
    codeBegin=_TStreamCacheClip_accessData(sclip,readSize);
    if (codeBegin==0) return _hpatch_FALSE;
    curCode=codeBegin;
    _SAFE_CHECK_DO(hpatch_unpackUIntWithTag((const TByte**)&curCode,codeBegin+readSize,result,kTagBit));
    _TStreamCacheClip_skipData_noCheck(sclip,(hpatch_size_t)(curCode-codeBegin));
    return hpatch_TRUE;
}

hpatch_BOOL _TStreamCacheClip_readUInt(TStreamCacheClip* sclip,hpatch_StreamPos_t* result,hpatch_size_t uintSize){
    // assert(uintSize<=sizeof(hpatch_StreamPos_t));
    const TByte* buf=_TStreamCacheClip_readData(sclip,uintSize);
    hpatch_StreamPos_t v=0;
    if (buf!=0){
        switch (uintSize) {
            case 8: v|=((hpatch_StreamPos_t)buf[7])<<(7*8);
            case 7: v|=((hpatch_StreamPos_t)buf[6])<<(6*8);
            case 6: v|=((hpatch_StreamPos_t)buf[5])<<(5*8);
            case 5: v|=((hpatch_StreamPos_t)buf[4])<<(4*8);
            case 4: v|=((hpatch_StreamPos_t)buf[3])<<(3*8);
            case 3: v|=((hpatch_StreamPos_t)buf[2])<<(2*8);
            case 2: v|=((hpatch_StreamPos_t)buf[1])<<(1*8);
            case 1: v|=((hpatch_StreamPos_t)buf[0]);
            case 0: { *result=v; return hpatch_TRUE; }
            default: return _hpatch_FALSE;
        }
    }else{
        return _hpatch_FALSE;
    }
}

hpatch_BOOL _TStreamCacheClip_readDataTo(TStreamCacheClip* sclip,TByte* out_buf,TByte* bufEnd){
    hpatch_size_t readLen=_TStreamCacheClip_cachedSize(sclip);
    hpatch_size_t outLen=bufEnd-out_buf;
    if (readLen>=outLen)
        readLen=outLen;
    memcpy(out_buf,&sclip->cacheBuf[sclip->cacheBegin],readLen);
    sclip->cacheBegin+=readLen;
    out_buf+=readLen;
    outLen-=readLen;
    if (outLen){
        if (!sclip->srcStream->read(sclip->srcStream,sclip->streamPos,
                                    out_buf,bufEnd)) return _hpatch_FALSE;
        sclip->streamPos+=outLen;
    }
    return hpatch_TRUE;
}

    static hpatch_BOOL _decompress_read(const hpatch_TStreamInput* stream,
                                        const hpatch_StreamPos_t  readFromPos,
                                        TByte* out_data,TByte* out_data_end){
        _TDecompressInputStream* self=(_TDecompressInputStream*)stream->streamImport;
        return self->decompressPlugin->decompress_part(self->decompressHandle,out_data,out_data_end);
    }
hpatch_BOOL getStreamClip(TStreamCacheClip* out_clip,_TDecompressInputStream* out_stream,
                          hpatch_StreamPos_t dataSize,hpatch_StreamPos_t compressedSize,
                          const hpatch_TStreamInput* stream,hpatch_StreamPos_t* pCurStreamPos,
                          hpatch_TDecompress* decompressPlugin,TByte* aCache,hpatch_size_t cacheSize){
    hpatch_StreamPos_t curStreamPos=*pCurStreamPos;
    if (compressedSize==0){
#ifdef __RUN_MEM_SAFE_CHECK
        if ((curStreamPos+dataSize)<curStreamPos) return _hpatch_FALSE;
        if ((curStreamPos+dataSize)>stream->streamSize) return _hpatch_FALSE;
#endif
        if (out_clip)
            _TStreamCacheClip_init(out_clip,stream,curStreamPos,curStreamPos+dataSize,aCache,cacheSize);
        curStreamPos+=dataSize;
    }else{
#ifdef __RUN_MEM_SAFE_CHECK
        if ((curStreamPos+compressedSize)<curStreamPos) return _hpatch_FALSE;
        if ((curStreamPos+compressedSize)>stream->streamSize) return _hpatch_FALSE;
#endif
        if (out_clip){
            out_stream->IInputStream.streamImport=out_stream;
            out_stream->IInputStream.streamSize=dataSize;
            out_stream->IInputStream.read=_decompress_read;
            out_stream->decompressPlugin=decompressPlugin;
            out_stream->decompressHandle=decompressPlugin->open(decompressPlugin,dataSize,stream,
                                                                curStreamPos,curStreamPos+compressedSize);
            if (!out_stream->decompressHandle) return _hpatch_FALSE;
            _TStreamCacheClip_init(out_clip,&out_stream->IInputStream,0,
                                   out_stream->IInputStream.streamSize,aCache,cacheSize);
        }
        curStreamPos+=compressedSize;
    }
    *pCurStreamPos=curStreamPos;
    return hpatch_TRUE;
}

///////

// Stream Clip cache
typedef struct {
    hpatch_StreamPos_t           writeToPos;
    const hpatch_TStreamOutput*  dstStream;
    unsigned char*  cacheBuf;
    hpatch_size_t   cacheCur;
    hpatch_size_t   cacheEnd;
} TOutStreamCache;

static hpatch_inline void TOutStreamCache_init(TOutStreamCache* self,const hpatch_TStreamOutput*  dstStream,
                                               TByte* aCache,hpatch_size_t aCacheSize){
    self->writeToPos=0;
    self->dstStream=dstStream;
    self->cacheBuf=aCache;
    self->cacheCur=0;
    self->cacheEnd=aCacheSize;
}
static hpatch_inline hpatch_BOOL TOutStreamCache_isFinish(const TOutStreamCache* self){
    return self->writeToPos==self->dstStream->streamSize;
}

static hpatch_inline hpatch_BOOL _TOutStreamCache_write(TOutStreamCache* self,const TByte* data,hpatch_size_t dataSize){
    if (!self->dstStream->write(self->dstStream,self->writeToPos,data,data+dataSize))
        return _hpatch_FALSE;
    self->writeToPos+=dataSize;
    return hpatch_TRUE;
}

static hpatch_BOOL TOutStreamCache_flush(TOutStreamCache* self){
    hpatch_size_t curSize=self->cacheCur;
    if (curSize>0){
        if (!_TOutStreamCache_write(self,self->cacheBuf,curSize))
            return _hpatch_FALSE;
        self->cacheCur=0;
    }
    return hpatch_TRUE;
}

static hpatch_BOOL TOutStreamCache_write(TOutStreamCache* self,const TByte* data,hpatch_size_t dataSize){
    while (dataSize>0) {
        hpatch_size_t copyLen;
        hpatch_size_t curSize=self->cacheCur;
        if ((dataSize>=self->cacheEnd)&&(curSize==0)){
            return _TOutStreamCache_write(self,data,dataSize);
        }
        copyLen=self->cacheEnd-curSize;
        copyLen=(copyLen<=dataSize)?copyLen:dataSize;
        memcpy(self->cacheBuf+curSize,data,copyLen);
        self->cacheCur=curSize+copyLen;
        data+=copyLen;
        dataSize-=copyLen;
        if (self->cacheCur==self->cacheEnd){
            if (!TOutStreamCache_flush(self)) return _hpatch_FALSE;
        }
    }
    return hpatch_TRUE;
}


static  hpatch_BOOL _patch_copy_diff_by_outCache(TOutStreamCache* outCache,TStreamCacheClip* diff,hpatch_StreamPos_t copyLength){
    while (copyLength>0){
        const TByte* data;
        hpatch_size_t decodeStep=diff->cacheEnd;
        if (decodeStep>copyLength)
            decodeStep=(hpatch_size_t)copyLength;
        data=_TStreamCacheClip_readData(diff,decodeStep);
        if (data==0) return _hpatch_FALSE;
        if (!TOutStreamCache_write(outCache,data,decodeStep))
            return _hpatch_FALSE;
        copyLength-=decodeStep;
    }
    return hpatch_TRUE;
}


typedef struct _TBytesRle_load_stream{
    hpatch_StreamPos_t  memCopyLength;
    hpatch_StreamPos_t  memSetLength;
    TByte               memSetValue;
    TStreamCacheClip    ctrlClip;
    TStreamCacheClip    rleCodeClip;
} _TBytesRle_load_stream;

hpatch_inline
static void _TBytesRle_load_stream_init(_TBytesRle_load_stream* loader){
    loader->memSetLength=0;
    loader->memSetValue=0;//nil;
    loader->memCopyLength=0;
    _TStreamCacheClip_init(&loader->ctrlClip,0,0,0,0,0);
    _TStreamCacheClip_init(&loader->rleCodeClip,0,0,0,0,0);
}

#if (_IS_NEED_MIN_CODE_SIZE)
hpatch_inline static void memSet_add(TByte* dst,const TByte src,hpatch_size_t length){
    while (length--) { *dst++ += src; }
}
#else
static void memSet_add(TByte* dst,const TByte src,hpatch_size_t length){
    hpatch_size_t length_fast,i;

    length_fast=length&(~(hpatch_size_t)7);
    for (i=0;i<length_fast;i+=8){
        dst[i  ]+=src;
        dst[i+1]+=src;
        dst[i+2]+=src;
        dst[i+3]+=src;
        dst[i+4]+=src;
        dst[i+5]+=src;
        dst[i+6]+=src;
        dst[i+7]+=src;
    }
    for (;i<length;++i)
        dst[i]+=src;
}
#endif

static hpatch_BOOL _TBytesRle_load_stream_mem_add(_TBytesRle_load_stream* loader,
                                                  hpatch_size_t* _decodeSize,TByte** _out_data){
    hpatch_size_t  decodeSize=*_decodeSize;
    TByte* out_data=*_out_data;
    TStreamCacheClip* rleCodeClip=&loader->rleCodeClip;
    
    hpatch_StreamPos_t memSetLength=loader->memSetLength;
    if (memSetLength!=0){
        hpatch_size_t memSetStep=((memSetLength<=decodeSize)?(hpatch_size_t)memSetLength:decodeSize);
        const TByte byteSetValue=loader->memSetValue;
        if (out_data!=0){
            if (byteSetValue!=0)
                memSet_add(out_data,byteSetValue,memSetStep);
            out_data+=memSetStep;
        }
        decodeSize-=memSetStep;
        loader->memSetLength=memSetLength-memSetStep;
    }
    while ((loader->memCopyLength>0)&&(decodeSize>0)) {
        TByte* rleData;
        hpatch_size_t decodeStep=rleCodeClip->cacheEnd;
        if (decodeStep>loader->memCopyLength)
            decodeStep=(hpatch_size_t)loader->memCopyLength;
        if (decodeStep>decodeSize)
            decodeStep=decodeSize;
        rleData=_TStreamCacheClip_readData(rleCodeClip,decodeStep);
        if (rleData==0) return _hpatch_FALSE;
        if (out_data){
            addData(out_data,rleData,decodeStep);
            out_data+=decodeStep;
        }
        decodeSize-=decodeStep;
        loader->memCopyLength-=decodeStep;
    }
    *_decodeSize=decodeSize;
    *_out_data=out_data;
    return hpatch_TRUE;
}

hpatch_inline
static hpatch_BOOL _TBytesRle_load_stream_isFinish(const _TBytesRle_load_stream* loader){
    return(loader->memSetLength==0)
        &&(loader->memCopyLength==0)
        &&(_TStreamCacheClip_isFinish(&loader->rleCodeClip))
        &&(_TStreamCacheClip_isFinish(&loader->ctrlClip));
}



#define _clip_unpackUIntWithTagTo(puint,sclip,kTagBit) \
    { if (!_TStreamCacheClip_unpackUIntWithTag(sclip,puint,kTagBit)) return _hpatch_FALSE; }
#define _clip_unpackUIntTo(puint,sclip) _clip_unpackUIntWithTagTo(puint,sclip,0)

static hpatch_BOOL _TBytesRle_load_stream_decode_add(_TBytesRle_load_stream* loader,
                                                     TByte* out_data,hpatch_size_t decodeSize){
    if (!_TBytesRle_load_stream_mem_add(loader,&decodeSize,&out_data))
        return _hpatch_FALSE;

    while ((decodeSize>0)&&(!_TStreamCacheClip_isFinish(&loader->ctrlClip))){
        enum TByteRleType type;
        hpatch_StreamPos_t length;
        const TByte* pType=_TStreamCacheClip_accessData(&loader->ctrlClip,1);
        if (pType==0) return _hpatch_FALSE;
        type=(enum TByteRleType)((*pType)>>(8-kByteRleType_bit));
        _clip_unpackUIntWithTagTo(&length,&loader->ctrlClip,kByteRleType_bit);
        ++length;
        switch (type){
            case kByteRleType_rle0:{
                loader->memSetLength=length;
                loader->memSetValue=0;
            }break;
            case kByteRleType_rle255:{
                loader->memSetLength=length;
                loader->memSetValue=255;
            }break;
            case kByteRleType_rle:{
                const TByte* pSetValue=_TStreamCacheClip_readData(&loader->rleCodeClip,1);
                if (pSetValue==0) return _hpatch_FALSE;
                loader->memSetValue=*pSetValue;
                loader->memSetLength=length;
            }break;
            case kByteRleType_unrle:{
                loader->memCopyLength=length;
            }break;
        }
        if (!_TBytesRle_load_stream_mem_add(loader,&decodeSize,&out_data)) return _hpatch_FALSE;
    }

    if (decodeSize==0)
        return hpatch_TRUE;
    else
        return _hpatch_FALSE;
}

#define _TBytesRle_load_stream_decode_skip(loader,decodeSize) \
        _TBytesRle_load_stream_decode_add(loader,0,decodeSize)

static  hpatch_BOOL _patch_add_old_with_rle(TOutStreamCache* outCache,_TBytesRle_load_stream* rle_loader,
                                            const hpatch_TStreamInput* old,hpatch_StreamPos_t oldPos,
                                            hpatch_StreamPos_t addLength,TByte* aCache,hpatch_size_t aCacheSize){
    while (addLength>0){
        hpatch_size_t decodeStep=aCacheSize;
        if (decodeStep>addLength)
            decodeStep=(hpatch_size_t)addLength;
        if (!old->read(old,oldPos,aCache,aCache+decodeStep)) return _hpatch_FALSE;
        if (!_TBytesRle_load_stream_decode_add(rle_loader,aCache,decodeStep)) return _hpatch_FALSE;
        if (!TOutStreamCache_write(outCache,aCache,decodeStep)) return _hpatch_FALSE;
        oldPos+=decodeStep;
        addLength-=decodeStep;
    }
    return hpatch_TRUE;
}

typedef struct _TCovers{
    hpatch_TCovers      ICovers;
    hpatch_StreamPos_t  coverCount;
    hpatch_StreamPos_t  oldPosBack;
    hpatch_StreamPos_t  newPosBack;
    TStreamCacheClip*   code_inc_oldPosClip;
    TStreamCacheClip*   code_inc_newPosClip;
    TStreamCacheClip*   code_lengthsClip;
    hpatch_BOOL         isOldPosBackNeedAddLength;
} _TCovers;

static hpatch_StreamPos_t _covers_leaveCoverCount(const hpatch_TCovers* covers){
    const _TCovers* self=(const _TCovers*)covers;
    return self->coverCount;
}
static hpatch_BOOL _covers_close_nil(hpatch_TCovers* covers){
    //empty
    return hpatch_TRUE;
}

static  hpatch_BOOL _covers_read_cover(hpatch_TCovers* covers,hpatch_TCover* out_cover){
    _TCovers* self=(_TCovers*)covers;
    hpatch_StreamPos_t oldPosBack=self->oldPosBack;
    hpatch_StreamPos_t newPosBack=self->newPosBack;
    hpatch_StreamPos_t coverCount=self->coverCount;
    if (coverCount>0)
        self->coverCount=coverCount-1;
    else
        return _hpatch_FALSE;
    
    {
        hpatch_StreamPos_t copyLength,coverLength, oldPos,inc_oldPos;
        TByte inc_oldPos_sign;
        const TByte* pSign=_TStreamCacheClip_accessData(self->code_inc_oldPosClip,1);
        if (pSign)
            inc_oldPos_sign=(*pSign)>>(8-kSignTagBit);
        else
            return _hpatch_FALSE;
        _clip_unpackUIntWithTagTo(&inc_oldPos,self->code_inc_oldPosClip,kSignTagBit);
        oldPos=(inc_oldPos_sign==0)?(oldPosBack+inc_oldPos):(oldPosBack-inc_oldPos);
        _clip_unpackUIntTo(&copyLength,self->code_inc_newPosClip);
        _clip_unpackUIntTo(&coverLength,self->code_lengthsClip);
        newPosBack+=copyLength;
        oldPosBack=oldPos;
        oldPosBack+=(self->isOldPosBackNeedAddLength)?coverLength:0;
        
        out_cover->oldPos=oldPos;
        out_cover->newPos=newPosBack;
        out_cover->length=coverLength;
        newPosBack+=coverLength;
    }
    self->oldPosBack=oldPosBack;
    self->newPosBack=newPosBack;
    return hpatch_TRUE;
}

static  hpatch_BOOL _covers_is_finish(const struct hpatch_TCovers* covers){
    _TCovers* self=(_TCovers*)covers;
    return _TStreamCacheClip_isFinish(self->code_lengthsClip)
        && _TStreamCacheClip_isFinish(self->code_inc_newPosClip)
        && _TStreamCacheClip_isFinish(self->code_inc_oldPosClip);
}


static void _covers_init(_TCovers* covers,hpatch_StreamPos_t coverCount,
                         TStreamCacheClip* code_inc_oldPosClip,
                         TStreamCacheClip* code_inc_newPosClip,
                         TStreamCacheClip* code_lengthsClip,
                         hpatch_BOOL  isOldPosBackNeedAddLength){
    covers->ICovers.leave_cover_count=_covers_leaveCoverCount;
    covers->ICovers.read_cover=_covers_read_cover;
    covers->ICovers.is_finish=_covers_is_finish;
    covers->ICovers.close=_covers_close_nil;
    covers->coverCount=coverCount;
    covers->newPosBack=0;
    covers->oldPosBack=0;
    covers->code_inc_oldPosClip=code_inc_oldPosClip;
    covers->code_inc_newPosClip=code_inc_newPosClip;
    covers->code_lengthsClip=code_lengthsClip;
    covers->isOldPosBackNeedAddLength=isOldPosBackNeedAddLength;
}

static hpatch_BOOL _rle_decode_skip(struct _TBytesRle_load_stream* rle_loader,hpatch_StreamPos_t copyLength){
    while (copyLength>0) {
        hpatch_size_t len=(~(hpatch_size_t)0);
        if (len>copyLength)
            len=(hpatch_size_t)copyLength;
        if (!_TBytesRle_load_stream_decode_skip(rle_loader,len)) return _hpatch_FALSE;
        copyLength-=len;
    }
    return hpatch_TRUE;
}

static hpatch_BOOL patchByClip(const hpatch_TStreamOutput* out_newData,
                               const hpatch_TStreamInput*  oldData,
                               hpatch_TCovers* covers,
                               TStreamCacheClip* code_newDataDiffClip,
                               struct _TBytesRle_load_stream* rle_loader,
                               TByte* temp_cache,hpatch_size_t cache_size){
    const hpatch_StreamPos_t newDataSize=out_newData->streamSize;
    const hpatch_StreamPos_t oldDataSize=oldData->streamSize;
    hpatch_StreamPos_t coverCount=covers->leave_cover_count(covers);
    TOutStreamCache          outCache;
    hpatch_StreamPos_t newPosBack=0;
    assert(cache_size>=hpatch_kMaxPackedUIntBytes);
    TOutStreamCache_init(&outCache,out_newData,temp_cache+cache_size,cache_size);
    
    while (coverCount--){
        hpatch_TCover cover;
        if(!covers->read_cover(covers,&cover)) return _hpatch_FALSE;
#ifdef __RUN_MEM_SAFE_CHECK
        if (cover.newPos>newDataSize) return _hpatch_FALSE;
        if (cover.length>(hpatch_StreamPos_t)(newDataSize-cover.newPos)) return _hpatch_FALSE;
        if (cover.oldPos>oldDataSize) return _hpatch_FALSE;
        if (cover.length>(hpatch_StreamPos_t)(oldDataSize-cover.oldPos)) return _hpatch_FALSE;
        if (cover.newPos<newPosBack) return _hpatch_FALSE;
#endif
        if (newPosBack<cover.newPos){
            hpatch_StreamPos_t copyLength=cover.newPos-newPosBack;
            if (!_patch_copy_diff_by_outCache(&outCache,code_newDataDiffClip,copyLength)) return _hpatch_FALSE;
            if (!_rle_decode_skip(rle_loader,copyLength)) return _hpatch_FALSE;
        }
        if (!_patch_add_old_with_rle(&outCache,rle_loader,oldData,cover.oldPos,cover.length,
                                     temp_cache,cache_size)) return _hpatch_FALSE;
        newPosBack=cover.newPos+cover.length;
    }
    
    if (newPosBack<newDataSize){
        hpatch_StreamPos_t copyLength=newDataSize-newPosBack;
        if (!_patch_copy_diff_by_outCache(&outCache,code_newDataDiffClip,copyLength)) return _hpatch_FALSE;
        if (!_rle_decode_skip(rle_loader,copyLength)) return _hpatch_FALSE;
        newPosBack=newDataSize;
    }
    if (!TOutStreamCache_flush(&outCache))
        return _hpatch_FALSE;
    if (   _TBytesRle_load_stream_isFinish(rle_loader)
        && covers->is_finish(covers)
        && TOutStreamCache_isFinish(&outCache)
        && _TStreamCacheClip_isFinish(code_newDataDiffClip)
        && (newPosBack==newDataSize) )
        return hpatch_TRUE;
    else
        return _hpatch_FALSE;
}


#define _kCachePatCount 8

#define _cache_alloc(dst,dst_type,memSize,temp_cache,temp_cache_end){   \
    if ((hpatch_size_t)(temp_cache_end-temp_cache) <   \
        sizeof(hpatch_StreamPos_t)+(memSize))  return hpatch_FALSE;      \
    (dst)=(dst_type*)_hpatch_align_upper(temp_cache,sizeof(hpatch_StreamPos_t));\
    temp_cache=(TByte*)(dst)+(hpatch_size_t)(memSize); \
}

typedef struct _TPackedCovers{
    _TCovers            base;
    TStreamCacheClip    code_inc_oldPosClip;
    TStreamCacheClip    code_inc_newPosClip;
    TStreamCacheClip    code_lengthsClip;
} _TPackedCovers;

typedef struct _THDiffHead{
    hpatch_StreamPos_t coverCount;
    hpatch_StreamPos_t lengthSize;
    hpatch_StreamPos_t inc_newPosSize;
    hpatch_StreamPos_t inc_oldPosSize;
    hpatch_StreamPos_t newDataDiffSize;
    hpatch_StreamPos_t headEndPos;
    hpatch_StreamPos_t coverEndPos;
} _THDiffHead;

static hpatch_BOOL read_diff_head(_THDiffHead* out_diffHead,
                                  const hpatch_TStreamInput* serializedDiff){
    hpatch_StreamPos_t       diffPos0;
    const hpatch_StreamPos_t diffPos_end=serializedDiff->streamSize;
    TByte       temp_cache[hpatch_kStreamCacheSize];
    TStreamCacheClip  diffHeadClip;
    _TStreamCacheClip_init(&diffHeadClip,serializedDiff,0,diffPos_end,temp_cache,hpatch_kStreamCacheSize);
    _clip_unpackUIntTo(&out_diffHead->coverCount,&diffHeadClip);
    _clip_unpackUIntTo(&out_diffHead->lengthSize,&diffHeadClip);
    _clip_unpackUIntTo(&out_diffHead->inc_newPosSize,&diffHeadClip);
    _clip_unpackUIntTo(&out_diffHead->inc_oldPosSize,&diffHeadClip);
    _clip_unpackUIntTo(&out_diffHead->newDataDiffSize,&diffHeadClip);
    diffPos0=(hpatch_StreamPos_t)(_TStreamCacheClip_readPosOfSrcStream(&diffHeadClip));
    out_diffHead->headEndPos=diffPos0;
#ifdef __RUN_MEM_SAFE_CHECK
    if (out_diffHead->lengthSize>(hpatch_StreamPos_t)(diffPos_end-diffPos0)) return _hpatch_FALSE;
#endif
    diffPos0+=out_diffHead->lengthSize;
#ifdef __RUN_MEM_SAFE_CHECK
    if (out_diffHead->inc_newPosSize>(hpatch_StreamPos_t)(diffPos_end-diffPos0)) return _hpatch_FALSE;
#endif
    diffPos0+=out_diffHead->inc_newPosSize;
#ifdef __RUN_MEM_SAFE_CHECK
    if (out_diffHead->inc_oldPosSize>(hpatch_StreamPos_t)(diffPos_end-diffPos0)) return _hpatch_FALSE;
#endif
    diffPos0+=out_diffHead->inc_oldPosSize;
    out_diffHead->coverEndPos=diffPos0;
#ifdef __RUN_MEM_SAFE_CHECK
    if (out_diffHead->newDataDiffSize>(hpatch_StreamPos_t)(diffPos_end-diffPos0)) return _hpatch_FALSE;
#endif
    return hpatch_TRUE;
}

static hpatch_BOOL _packedCovers_open(_TPackedCovers** out_self,
                                      _THDiffHead* out_diffHead,
                                      const hpatch_TStreamInput* serializedDiff,
                                      TByte* temp_cache,TByte* temp_cache_end){
    hpatch_size_t   cacheSize;
    _TPackedCovers* self=0;
    _cache_alloc(self,_TPackedCovers,sizeof(_TPackedCovers),temp_cache,temp_cache_end);
    cacheSize=(temp_cache_end-temp_cache)/3;
    {
        hpatch_StreamPos_t       diffPos0;
        if (!read_diff_head(out_diffHead,serializedDiff)) return _hpatch_FALSE;
        diffPos0=out_diffHead->headEndPos;
        _TStreamCacheClip_init(&self->code_lengthsClip,serializedDiff,diffPos0,
                               diffPos0+out_diffHead->lengthSize,temp_cache,cacheSize);
        diffPos0+=out_diffHead->lengthSize;
        _TStreamCacheClip_init(&self->code_inc_newPosClip,serializedDiff,diffPos0,
                               diffPos0+out_diffHead->inc_newPosSize,temp_cache+cacheSize*1,cacheSize);
        diffPos0+=out_diffHead->inc_newPosSize;
        _TStreamCacheClip_init(&self->code_inc_oldPosClip,serializedDiff,diffPos0,
                               diffPos0+out_diffHead->inc_oldPosSize,temp_cache+cacheSize*2,cacheSize);
    }
    
     _covers_init(&self->base,out_diffHead->coverCount,&self->code_inc_oldPosClip,
                  &self->code_inc_newPosClip,&self->code_lengthsClip,hpatch_FALSE);
    *out_self=self;
    return hpatch_TRUE;
}

static hpatch_BOOL _patch_stream_with_cache(const hpatch_TStreamOutput* out_newData,
                                            const hpatch_TStreamInput*  oldData,
                                            const hpatch_TStreamInput*  serializedDiff,
                                            hpatch_TCovers*  cached_covers,
                                            TByte*   temp_cache,TByte* temp_cache_end){
    struct _THDiffHead              diffHead;
    TStreamCacheClip                code_newDataDiffClip;
    struct _TBytesRle_load_stream   rle_loader;
    hpatch_TCovers*                 pcovers=0;
    hpatch_StreamPos_t       diffPos0;
    const hpatch_StreamPos_t diffPos_end=serializedDiff->streamSize;
    const hpatch_size_t cacheSize=(temp_cache_end-temp_cache)/(cached_covers?(_kCachePatCount-3):_kCachePatCount);

    assert(out_newData!=0);
    assert(out_newData->write!=0);
    assert(oldData!=0);
    assert(oldData->read!=0);
    assert(serializedDiff!=0);
    assert(serializedDiff->read!=0);
    
    //covers
    if (cached_covers==0){
        struct _TPackedCovers* packedCovers;
        if (!_packedCovers_open(&packedCovers,&diffHead,serializedDiff,temp_cache+cacheSize*(_kCachePatCount-3),
                                temp_cache_end)) return _hpatch_FALSE;
            pcovers=&packedCovers->base.ICovers; //not need close before return
    }else{
        pcovers=cached_covers;
        if (!read_diff_head(&diffHead,serializedDiff)) return _hpatch_FALSE;
    }
    //newDataDiff
    diffPos0=diffHead.coverEndPos;
    _TStreamCacheClip_init(&code_newDataDiffClip,serializedDiff,diffPos0,
                           diffPos0+diffHead.newDataDiffSize,temp_cache+cacheSize*0,cacheSize);
    diffPos0+=diffHead.newDataDiffSize;
        
    {//rle
        hpatch_StreamPos_t rleCtrlSize;
        hpatch_StreamPos_t rlePos0;
        TStreamCacheClip*  rleHeadClip=&rle_loader.ctrlClip;//rename, share address
#ifdef __RUN_MEM_SAFE_CHECK
        if (cacheSize<hpatch_kMaxPackedUIntBytes) return _hpatch_FALSE;
#endif
        _TStreamCacheClip_init(rleHeadClip,serializedDiff,diffPos0,diffPos_end,
                               temp_cache+cacheSize*1,hpatch_kMaxPackedUIntBytes);
        _clip_unpackUIntTo(&rleCtrlSize,rleHeadClip);
        rlePos0=(hpatch_StreamPos_t)(_TStreamCacheClip_readPosOfSrcStream(rleHeadClip));
#ifdef __RUN_MEM_SAFE_CHECK
        if (rleCtrlSize>(hpatch_StreamPos_t)(diffPos_end-rlePos0)) return _hpatch_FALSE;
#endif
        _TBytesRle_load_stream_init(&rle_loader);
        _TStreamCacheClip_init(&rle_loader.ctrlClip,serializedDiff,rlePos0,rlePos0+rleCtrlSize,
                               temp_cache+cacheSize*1,cacheSize);
        _TStreamCacheClip_init(&rle_loader.rleCodeClip,serializedDiff,rlePos0+rleCtrlSize,diffPos_end,
                               temp_cache+cacheSize*2,cacheSize);
    }
    
    return patchByClip(out_newData,oldData,pcovers,&code_newDataDiffClip,
                       &rle_loader,temp_cache+cacheSize*3,cacheSize);
}


hpatch_BOOL read_diffz_head(hpatch_compressedDiffInfo* out_diffInfo,_THDiffzHead* out_head,
                            const hpatch_TStreamInput* compressedDiff){
    TStreamCacheClip  _diffHeadClip;
    TStreamCacheClip* diffHeadClip=&_diffHeadClip;
    TByte             temp_cache[hpatch_kStreamCacheSize];
    _TStreamCacheClip_init(&_diffHeadClip,compressedDiff,0,compressedDiff->streamSize,
                           temp_cache,hpatch_kStreamCacheSize);
    {//type
        const char* kVersionType="HDIFF13";
        char* tempType=out_diffInfo->compressType;
        if (!_TStreamCacheClip_readType_end(diffHeadClip,'&',tempType)) return _hpatch_FALSE;
        if (0!=strcmp(tempType,kVersionType)) return _hpatch_FALSE;
    }
    {//read compressType
        if (!_TStreamCacheClip_readType_end(diffHeadClip,'\0',
                                            out_diffInfo->compressType)) return _hpatch_FALSE;
        out_head->typesEndPos=_TStreamCacheClip_readPosOfSrcStream(diffHeadClip);
    }
    _clip_unpackUIntTo(&out_diffInfo->newDataSize,diffHeadClip);
    _clip_unpackUIntTo(&out_diffInfo->oldDataSize,diffHeadClip);
    _clip_unpackUIntTo(&out_head->coverCount,diffHeadClip);
    out_head->compressSizeBeginPos=_TStreamCacheClip_readPosOfSrcStream(diffHeadClip);
    _clip_unpackUIntTo(&out_head->cover_buf_size,diffHeadClip);
    _clip_unpackUIntTo(&out_head->compress_cover_buf_size,diffHeadClip);
    _clip_unpackUIntTo(&out_head->rle_ctrlBuf_size,diffHeadClip);
    _clip_unpackUIntTo(&out_head->compress_rle_ctrlBuf_size,diffHeadClip);
    _clip_unpackUIntTo(&out_head->rle_codeBuf_size,diffHeadClip);
    _clip_unpackUIntTo(&out_head->compress_rle_codeBuf_size,diffHeadClip);
    _clip_unpackUIntTo(&out_head->newDataDiff_size,diffHeadClip);
    _clip_unpackUIntTo(&out_head->compress_newDataDiff_size,diffHeadClip);
    out_head->headEndPos=_TStreamCacheClip_readPosOfSrcStream(diffHeadClip);
    
    out_diffInfo->compressedCount=((out_head->compress_cover_buf_size)?1:0)
                                 +((out_head->compress_rle_ctrlBuf_size)?1:0)
                                 +((out_head->compress_rle_codeBuf_size)?1:0)
                                 +((out_head->compress_newDataDiff_size)?1:0);
    if (out_head->compress_cover_buf_size>0)
        out_head->coverEndPos=out_head->headEndPos+out_head->compress_cover_buf_size;
    else
        out_head->coverEndPos=out_head->headEndPos+out_head->cover_buf_size;
    return hpatch_TRUE;
}

hpatch_BOOL getCompressedDiffInfo(hpatch_compressedDiffInfo* out_diffInfo,
                                  const hpatch_TStreamInput* compressedDiff){
    _THDiffzHead head;
    assert(out_diffInfo!=0);
    assert(compressedDiff!=0);
    assert(compressedDiff->read!=0);
    return read_diffz_head(out_diffInfo,&head,compressedDiff);
}

#define _clear_return(exitValue) {  result=exitValue; goto clear; }

#define _kCacheDecCount 6

static
hpatch_BOOL _patch_decompress_cache(const hpatch_TStreamOutput*  out_newData,
                                    hpatch_TStreamInput*         once_in_newData,
                                    const hpatch_TStreamInput*   oldData,
                                    const hpatch_TStreamInput*   compressedDiff,
                                    hpatch_TDecompress*          decompressPlugin,
                                    hpatch_TCovers*              cached_covers,
                                    TByte* temp_cache, TByte* temp_cache_end){
    TStreamCacheClip              coverClip;
    TStreamCacheClip              code_newDataDiffClip;
    struct _TBytesRle_load_stream   rle_loader;
    _THDiffzHead                head;
    hpatch_compressedDiffInfo   diffInfo;
    _TDecompressInputStream     decompressers[4];
    hpatch_uint                 i;
    hpatch_StreamPos_t          coverCount;
    hpatch_BOOL  result=hpatch_TRUE;
    hpatch_StreamPos_t        diffPos0=0;
    const hpatch_StreamPos_t  diffPos_end=compressedDiff->streamSize;
    const hpatch_size_t cacheSize=(temp_cache_end-temp_cache)/(cached_covers?(_kCacheDecCount-1):_kCacheDecCount);
    if (cacheSize<=hpatch_kMaxPluginTypeLength) return _hpatch_FALSE;
    assert(out_newData!=0);
    assert(out_newData->write!=0);
    assert(oldData!=0);
    assert(oldData->read!=0);
    assert(compressedDiff!=0);
    assert(compressedDiff->read!=0);
    {//head
        if (!read_diffz_head(&diffInfo,&head,compressedDiff)) return _hpatch_FALSE;
        if ((diffInfo.oldDataSize!=oldData->streamSize)
            ||(diffInfo.newDataSize!=out_newData->streamSize)) return _hpatch_FALSE;
            
        if ((decompressPlugin==0)&&(diffInfo.compressedCount!=0)) return _hpatch_FALSE;
        if ((decompressPlugin)&&(diffInfo.compressedCount>0))
            if (!decompressPlugin->is_can_open(diffInfo.compressType)) return _hpatch_FALSE;
        diffPos0=head.headEndPos;
    }
    
    for (i=0;i<sizeof(decompressers)/sizeof(_TDecompressInputStream);++i)
        decompressers[i].decompressHandle=0;
    _TBytesRle_load_stream_init(&rle_loader);
    
    if (cached_covers){
        diffPos0=head.coverEndPos;
    }else{
        if (!getStreamClip(&coverClip,&decompressers[0],
                           head.cover_buf_size,head.compress_cover_buf_size,compressedDiff,&diffPos0,
                           decompressPlugin,temp_cache+cacheSize*(_kCacheDecCount-1),cacheSize)) _clear_return(_hpatch_FALSE);
    }
    if (!getStreamClip(&rle_loader.ctrlClip,&decompressers[1],
                       head.rle_ctrlBuf_size,head.compress_rle_ctrlBuf_size,compressedDiff,&diffPos0,
                       decompressPlugin,temp_cache+cacheSize*0,cacheSize)) _clear_return(_hpatch_FALSE);
    if (!getStreamClip(&rle_loader.rleCodeClip,&decompressers[2],
                       head.rle_codeBuf_size,head.compress_rle_codeBuf_size,compressedDiff,&diffPos0,
                       decompressPlugin,temp_cache+cacheSize*1,cacheSize)) _clear_return(_hpatch_FALSE);
    if (!getStreamClip(&code_newDataDiffClip,&decompressers[3],
                       head.newDataDiff_size,head.compress_newDataDiff_size,compressedDiff,&diffPos0,
                       decompressPlugin,temp_cache+cacheSize*2,cacheSize)) _clear_return(_hpatch_FALSE);
#ifdef __RUN_MEM_SAFE_CHECK
    if (diffPos0!=diffPos_end) _clear_return(_hpatch_FALSE);
#endif
    
    coverCount=head.coverCount;
    {
        _TCovers covers;
        hpatch_TCovers* pcovers=0;
        if (cached_covers){
            pcovers=cached_covers;
        }else{
            _covers_init(&covers,coverCount,&coverClip,&coverClip,&coverClip,hpatch_TRUE);
            pcovers=&covers.ICovers;  //not need close before return
        }
        result=patchByClip(out_newData,oldData,pcovers,&code_newDataDiffClip,&rle_loader,
                           temp_cache+cacheSize*3,cacheSize);
        //if ((pcovers!=cached_covers)&&(!pcovers->close(pcovers))) result=_hpatch_FALSE;
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


hpatch_inline static hpatch_BOOL _cache_load_all(const hpatch_TStreamInput* data,
                                                 TByte* cache,TByte* cache_end){
    assert((hpatch_size_t)(cache_end-cache)==data->streamSize);
    return data->read(data,0,cache,cache_end);
}

typedef struct _TCompressedCovers{
    _TCovers                base;
    TStreamCacheClip        coverClip;
    _TDecompressInputStream decompresser;
} _TCompressedCovers;

static hpatch_BOOL _compressedCovers_close(hpatch_TCovers* covers){
    hpatch_BOOL result=hpatch_TRUE;
    _TCompressedCovers* self=(_TCompressedCovers*)covers;
    if (self){
        if (self->decompresser.decompressHandle){
            result=self->decompresser.decompressPlugin->close(self->decompresser.decompressPlugin,
                                                              self->decompresser.decompressHandle);
            self->decompresser.decompressHandle=0;
        }
    }
    return result;
}

static hpatch_BOOL _compressedCovers_open(_TCompressedCovers** out_self,
                                          hpatch_compressedDiffInfo* out_diffInfo,
                                          const hpatch_TStreamInput* compressedDiff,
                                          hpatch_TDecompress* decompressPlugin,
                                          TByte* temp_cache,TByte* temp_cache_end){
    _THDiffzHead    head;
    hpatch_StreamPos_t  diffPos0=0;
    _TCompressedCovers* self=0;
    _cache_alloc(self,_TCompressedCovers,sizeof(_TCompressedCovers),temp_cache,temp_cache_end);
    if (!read_diffz_head(out_diffInfo,&head,compressedDiff)) return _hpatch_FALSE;
    diffPos0=head.headEndPos;
    if (head.compress_cover_buf_size>0){
        if (decompressPlugin==0) return _hpatch_FALSE;
        if (!decompressPlugin->is_can_open(out_diffInfo->compressType)) return _hpatch_FALSE;
    }
    
    _covers_init(&self->base,head.coverCount,&self->coverClip,
                 &self->coverClip,&self->coverClip,hpatch_TRUE);
    self->base.ICovers.close=_compressedCovers_close;
    memset(&self->decompresser,0, sizeof(self->decompresser));
    if (!getStreamClip(&self->coverClip,&self->decompresser,
                       head.cover_buf_size,head.compress_cover_buf_size,
                       compressedDiff,&diffPos0,decompressPlugin,
                       temp_cache,temp_cache_end-temp_cache)) {
        return _hpatch_FALSE;
    };
    *out_self=self;
    return hpatch_TRUE;
}

#if (_IS_NEED_CACHE_OLD_BY_COVERS)

typedef struct _TArrayCovers{
    hpatch_TCovers  ICovers;
    void*           pCCovers;
    hpatch_size_t   coverCount;
    hpatch_size_t   cur_index;
    hpatch_BOOL     is32;
} _TArrayCovers;


typedef struct hpatch_TCCover32{
    hpatch_uint32_t oldPos;
    hpatch_uint32_t newPos;
    hpatch_uint32_t length;
    hpatch_uint32_t cachePos; //todo:放到临时内存中,用完释放?逻辑会比较复杂;
} hpatch_TCCover32;

typedef struct hpatch_TCCover64{
    hpatch_StreamPos_t oldPos;
    hpatch_StreamPos_t newPos;
    hpatch_StreamPos_t length;
    hpatch_StreamPos_t cachePos;
} hpatch_TCCover64;

#define _arrayCovers_get(self,i,item) (((self)->is32)? \
    ((const hpatch_uint32_t*)(self)->pCCovers)[(i)*4+(item)]:\
    ((const hpatch_StreamPos_t*)(self)->pCCovers)[(i)*4+(item)])
#define _arrayCovers_get_oldPos(self,i)   _arrayCovers_get(self,i,0)
#define _arrayCovers_get_len(self,i)      _arrayCovers_get(self,i,2)
#define _arrayCovers_get_cachePos(self,i) _arrayCovers_get(self,i,3)

#define _arrayCovers_set(self,i,item,v) { if ((self)->is32){ \
    ((hpatch_uint32_t*)(self)->pCCovers)[(i)*4+(item)]=(hpatch_uint32_t)(v); }else{ \
    ((hpatch_StreamPos_t*)(self)->pCCovers)[(i)*4+(item)]=(v); } }
#define _arrayCovers_set_cachePos(self,i,v) _arrayCovers_set(self,i,3,v)

hpatch_inline static hpatch_StreamPos_t arrayCovers_memSize(hpatch_StreamPos_t coverCount,hpatch_BOOL is32){
    return coverCount*(is32?sizeof(hpatch_TCCover32):sizeof(hpatch_TCCover64));
}

static hpatch_BOOL _arrayCovers_is_finish(const hpatch_TCovers* covers){
    const _TArrayCovers* self=(const _TArrayCovers*)covers;
    return (self->coverCount==self->cur_index);
}
static hpatch_StreamPos_t _arrayCovers_leaveCoverCount(const hpatch_TCovers* covers){
    const _TArrayCovers* self=(const _TArrayCovers*)covers;
    return self->coverCount-self->cur_index;
}
static hpatch_BOOL _arrayCovers_read_cover(struct hpatch_TCovers* covers,hpatch_TCover* out_cover){
    _TArrayCovers* self=(_TArrayCovers*)covers;
    hpatch_size_t i=self->cur_index;
    if (i<self->coverCount){
        if (self->is32){
            const hpatch_TCCover32* pCover=((const hpatch_TCCover32*)self->pCCovers)+i;
            out_cover->oldPos=pCover->oldPos;
            out_cover->newPos=pCover->newPos;
            out_cover->length=pCover->length;
        }else{
            const hpatch_TCCover64* pCover=((const hpatch_TCCover64*)self->pCCovers)+i;
            out_cover->oldPos=pCover->oldPos;
            out_cover->newPos=pCover->newPos;
            out_cover->length=pCover->length;
        }
        self->cur_index=i+1;
        return hpatch_TRUE;
    }else{
        return _hpatch_FALSE;
    }
}

static hpatch_BOOL _arrayCovers_load(_TArrayCovers** out_self,hpatch_TCovers* src_covers,
                                     hpatch_BOOL isUsedCover32,hpatch_BOOL* out_isReadError,
                                     TByte** ptemp_cache,TByte* temp_cache_end){
    TByte* temp_cache=*ptemp_cache;
    hpatch_StreamPos_t _coverCount=src_covers->leave_cover_count(src_covers);
    hpatch_StreamPos_t memSize=arrayCovers_memSize(_coverCount,isUsedCover32);
    hpatch_size_t i;
    void*  pCovers;
    _TArrayCovers* self=0;
    hpatch_size_t coverCount=(hpatch_size_t)_coverCount;
    
    *out_isReadError=hpatch_FALSE;
    if (coverCount!=_coverCount) return hpatch_FALSE;
    
    _cache_alloc(self,_TArrayCovers,sizeof(_TArrayCovers),temp_cache,temp_cache_end);
    _cache_alloc(pCovers,void,memSize,temp_cache,temp_cache_end);
    if (isUsedCover32){
        hpatch_TCCover32* pdst=(hpatch_TCCover32*)pCovers;
        for (i=0;i<coverCount;++i,++pdst) {
            hpatch_TCover cover;
            if (!src_covers->read_cover(src_covers,&cover))
                { *out_isReadError=hpatch_TRUE; return _hpatch_FALSE; }
            pdst->oldPos=(hpatch_uint32_t)cover.oldPos;
            pdst->newPos=(hpatch_uint32_t)cover.newPos;
            pdst->length=(hpatch_uint32_t)cover.length;
        }
    }else{
        hpatch_TCCover64* pdst=(hpatch_TCCover64*)pCovers;
        for (i=0;i<coverCount;++i,++pdst) {
            if (!src_covers->read_cover(src_covers,(hpatch_TCover*)pdst))
                { *out_isReadError=hpatch_TRUE; return _hpatch_FALSE; }
        }
    }
    if (!src_covers->is_finish(src_covers))
        { *out_isReadError=hpatch_TRUE; return _hpatch_FALSE; }

    self->pCCovers=pCovers;
    self->is32=isUsedCover32;
    self->coverCount=coverCount;
    self->cur_index=0;
    self->ICovers.close=_covers_close_nil;
    self->ICovers.is_finish=_arrayCovers_is_finish;
    self->ICovers.leave_cover_count=_arrayCovers_leaveCoverCount;
    self->ICovers.read_cover=_arrayCovers_read_cover;
    *out_self=self;
    *ptemp_cache=temp_cache;
    return hpatch_TRUE;
}

#define _arrayCovers_comp(_uint_t,_x,_y,item){ \
    _uint_t x=((const _uint_t*)_x)[item]; \
    _uint_t y=((const _uint_t*)_y)[item]; \
    return (x<y)?(-1):((x>y)?1:0); \
}
static hpatch_int _arrayCovers_comp_by_old_32(const void* _x, const void *_y){
    _arrayCovers_comp(hpatch_uint32_t,_x,_y,0);
}
static hpatch_int _arrayCovers_comp_by_old(const void* _x, const void *_y){
    _arrayCovers_comp(hpatch_StreamPos_t,_x,_y,0);
}
static hpatch_int _arrayCovers_comp_by_new_32(const void* _x, const void *_y){
    _arrayCovers_comp(hpatch_uint32_t,_x,_y,1);
}
static hpatch_int _arrayCovers_comp_by_new(const void* _x, const void *_y){
    _arrayCovers_comp(hpatch_StreamPos_t,_x,_y,1);
}
static hpatch_int _arrayCovers_comp_by_len_32(const void* _x, const void *_y){
    _arrayCovers_comp(hpatch_uint32_t,_x,_y,2);
}
static hpatch_int _arrayCovers_comp_by_len(const void* _x, const void *_y){
    _arrayCovers_comp(hpatch_StreamPos_t,_x,_y,2);
}

static void _arrayCovers_sort_by_old(_TArrayCovers* self){
    if (self->is32)
        qsort(self->pCCovers,self->coverCount,sizeof(hpatch_TCCover32),_arrayCovers_comp_by_old_32);
    else
        qsort(self->pCCovers,self->coverCount,sizeof(hpatch_TCCover64),_arrayCovers_comp_by_old);
}
static void _arrayCovers_sort_by_new(_TArrayCovers* self){
    if (self->is32)
        qsort(self->pCCovers,self->coverCount,sizeof(hpatch_TCCover32),_arrayCovers_comp_by_new_32);
    else
        qsort(self->pCCovers,self->coverCount,sizeof(hpatch_TCCover64),_arrayCovers_comp_by_new);
}
static void _arrayCovers_sort_by_len(_TArrayCovers* self){
    if (self->is32)
        qsort(self->pCCovers,self->coverCount,sizeof(hpatch_TCCover32),_arrayCovers_comp_by_len_32);
    else
        qsort(self->pCCovers,self->coverCount,sizeof(hpatch_TCCover64),_arrayCovers_comp_by_len);
}

static hpatch_size_t _getMaxCachedLen(const _TArrayCovers* src_covers,
                                      TByte* temp_cache,TByte* temp_cache_end,TByte* cache_buf_end){
    const hpatch_size_t kMaxCachedLen  =~((hpatch_size_t)0);//允许缓存的最长单个数据长度;
    hpatch_StreamPos_t mlen=0;
    hpatch_StreamPos_t sum=0;
    const hpatch_size_t coverCount=src_covers->coverCount;
    hpatch_size_t i;
    _TArrayCovers cur_covers=*src_covers;
    hpatch_size_t cacheSize=temp_cache_end-temp_cache;
    hpatch_StreamPos_t memSize=arrayCovers_memSize(src_covers->coverCount,src_covers->is32);
    _cache_alloc(cur_covers.pCCovers,void,memSize,temp_cache,temp_cache_end); //fail return 0
    memcpy(cur_covers.pCCovers,src_covers->pCCovers,(hpatch_size_t)memSize);
    _arrayCovers_sort_by_len(&cur_covers);
    
    for (i=0; i<coverCount;++i) {
        mlen=_arrayCovers_get_len(&cur_covers,i);
        sum+=mlen;
        if (sum<=cacheSize){
            continue;
        }else{
            --mlen;
            break;
        }
    }
    if (mlen>kMaxCachedLen)
        mlen=kMaxCachedLen;
    return (hpatch_size_t)mlen;
}

static hpatch_size_t _set_cache_pos(_TArrayCovers* covers,hpatch_size_t maxCachedLen,
                                    hpatch_StreamPos_t* poldPosBegin,hpatch_StreamPos_t* poldPosEnd){
    const hpatch_size_t coverCount=covers->coverCount;
    const hpatch_size_t kMinCacheCoverCount=coverCount/8+1; //控制最小缓存数量,否则缓存的意义太小;
    hpatch_StreamPos_t oldPosBegin=~(hpatch_StreamPos_t)0;
    hpatch_StreamPos_t oldPosEnd=0;
    hpatch_size_t cacheCoverCount=0;
    hpatch_size_t sum=0;//result
    hpatch_size_t i;
    for (i=0; i<coverCount;++i) {
        hpatch_StreamPos_t clen=_arrayCovers_get_len(covers,i);
        if (clen<=maxCachedLen){
            hpatch_StreamPos_t oldPos;
            _arrayCovers_set_cachePos(covers,i,sum);
            sum+=(hpatch_size_t)clen;
            ++cacheCoverCount;
            
            oldPos=_arrayCovers_get_oldPos(covers,i);
            if (oldPos<oldPosBegin) oldPosBegin=oldPos;
            if (oldPos+clen>oldPosEnd) oldPosEnd=oldPos+clen;
        }
    }
    if (cacheCoverCount<kMinCacheCoverCount)
        return 0;//fail
    *poldPosBegin=oldPosBegin;
    *poldPosEnd=oldPosEnd;
    return sum;
}

//一个比较简单的缓存策略:
//  1. 根据缓冲区大小限制，选择出最短的一批覆盖线来缓存;
//  2. 顺序访问一次oldData文件，填充这些缓存;
//  3. 顺序访问时跳过中间过大的对缓存无用的区域;

static hpatch_BOOL _cache_old_load(const hpatch_TStreamInput*oldData,
                                   hpatch_StreamPos_t oldPos,hpatch_StreamPos_t oldPosAllEnd,
                                   _TArrayCovers* arrayCovers,hpatch_size_t maxCachedLen,hpatch_size_t sumCacheLen,
                                   TByte* old_cache,TByte* old_cache_end,TByte* cache_buf_end){
    const hpatch_size_t kMinSpaceLen   =(1<<(20+2));//跳过seekTime*speed长度的空间(SSD可以更小)时间上划得来,否则就顺序访问;
    const hpatch_size_t kAccessPageSize=(1<<(10+2));//页面对齐访问;
    hpatch_BOOL result=hpatch_TRUE;
    hpatch_size_t cur_i=0,i;
    const hpatch_size_t coverCount=arrayCovers->coverCount;
    TByte* cache_buf=old_cache_end;
    assert((hpatch_size_t)(old_cache_end-old_cache)>=sumCacheLen);
    
    if ((hpatch_size_t)(cache_buf_end-cache_buf)>=kAccessPageSize*2){
        cache_buf=(TByte*)_hpatch_align_upper(cache_buf,kAccessPageSize);
        if ((hpatch_size_t)(cache_buf_end-cache_buf)>=(kMinSpaceLen>>1))
            cache_buf_end=cache_buf+(kMinSpaceLen>>1);
        else
            cache_buf_end=(TByte*)_hpatch_align_lower(cache_buf_end,kAccessPageSize);
    }
    oldPos=_hpatch_align_type_lower(hpatch_StreamPos_t,oldPos,kAccessPageSize);
    if (oldPos<kMinSpaceLen) oldPos=0;
    
    _arrayCovers_sort_by_old(arrayCovers);
    while ((oldPos<oldPosAllEnd)&(cur_i<coverCount)) {
        hpatch_StreamPos_t oldPosEnd;
        hpatch_size_t readLen=(cache_buf_end-cache_buf);
        if (readLen>(oldPosAllEnd-oldPos)) readLen=(hpatch_size_t)(oldPosAllEnd-oldPos);
        if (!oldData->read(oldData,oldPos,cache_buf,
                           cache_buf+readLen)) { result=_hpatch_FALSE; break; } //error
        oldPosEnd=oldPos+readLen;
        for (i=cur_i;i<coverCount;++i){
            hpatch_StreamPos_t ioldPos,ioldPosEnd;
            hpatch_StreamPos_t ilen=_arrayCovers_get_len(arrayCovers,i);
            if (ilen>maxCachedLen){//覆盖线比较长不需要缓存,下一个覆盖线;
                if (i==cur_i)
                    ++cur_i;
                continue;
            }
            ioldPos=_arrayCovers_get_oldPos(arrayCovers,i);
            ioldPosEnd=ioldPos+ilen;
            if (ioldPosEnd>oldPos){
                //        [oldPos                  oldPosEnd]
                //                           ioldPosEnd]----or----]
                if (ioldPos<oldPosEnd){//有交集,需要cache
                //  [----or----[ioldPos      ioldPosEnd]----or----]
                    hpatch_StreamPos_t from;
                    hpatch_size_t      copyLen;
                    hpatch_StreamPos_t dstPos=_arrayCovers_get_cachePos(arrayCovers,i);
                    //assert(dstPos<=(hpatch_size_t)(old_cache_end-old_cache));
                    if (ioldPos>=oldPos){
                //             [ioldPos      ioldPosEnd]----or----]
                        from=ioldPos;
                    }else{
                //  [ioldPos                 ioldPosEnd]----or----]
                        from=oldPos;
                        dstPos+=(oldPos-ioldPos);
                    }
                    copyLen=(hpatch_size_t)(((ioldPosEnd<=oldPosEnd)?ioldPosEnd:oldPosEnd)-from);
                    //assert(dstPos+copyLen<=(hpatch_size_t)(old_cache_end-old_cache));
                    //assert(sumCacheLen>=copyLen);
                    memcpy(old_cache+(hpatch_size_t)dstPos,cache_buf+(from-oldPos),copyLen);
                    sumCacheLen-=copyLen;
                    if ((i==cur_i)&(oldPosEnd>=ioldPosEnd))
                        ++cur_i;
                }else{//后面覆盖线暂时都不会与当前数据有交集了,下一块数据;
                //  [oldPos     oldPosEnd]
                //                        [ioldPos      ioldPosEnd]
                    if ((i==cur_i)&&(ioldPos-oldPosEnd>=kMinSpaceLen))
                        oldPosEnd=_hpatch_align_type_lower(hpatch_StreamPos_t,ioldPos,kAccessPageSize);
                    break;
                }
            }else{//当前覆盖线已经落后于当前数据,下一个覆盖线;
                //                        [oldPos     oldPosEnd]
                // [ioldPos    ioldPosEnd]
                if (i==cur_i)
                    ++cur_i;
            }
        }
        oldPos=oldPosEnd;
    }
    _arrayCovers_sort_by_new(arrayCovers);
    assert(sumCacheLen==0);
    return result;
}

typedef struct _cache_old_TStreamInput{
    _TArrayCovers       arrayCovers;
    hpatch_BOOL         isInHitCache;
    hpatch_size_t       maxCachedLen;
    hpatch_StreamPos_t  readFromPos;
    hpatch_StreamPos_t  readFromPosEnd;
    const TByte*        caches;
    const TByte*        cachesEnd;
    const hpatch_TStreamInput* oldData;
} _cache_old_TStreamInput;

static hpatch_BOOL _cache_old_StreamInput_read(const hpatch_TStreamInput* stream,
                                               hpatch_StreamPos_t readFromPos,
                                               unsigned char* out_data,unsigned char* out_data_end){
    _cache_old_TStreamInput* self=(_cache_old_TStreamInput*)stream->streamImport;
    hpatch_StreamPos_t dataLen=(hpatch_size_t)(self->readFromPosEnd-self->readFromPos);
    hpatch_size_t readLen;
    if (dataLen==0){//next cover
        hpatch_StreamPos_t oldPos;
        hpatch_size_t i=self->arrayCovers.cur_index++;
        if (i>=self->arrayCovers.coverCount) return _hpatch_FALSE;//error;
        oldPos=_arrayCovers_get_oldPos(&self->arrayCovers,i);
        dataLen=_arrayCovers_get_len(&self->arrayCovers,i);
        self->isInHitCache=(dataLen<=self->maxCachedLen);
        self->readFromPos=oldPos;
        self->readFromPosEnd=oldPos+dataLen;
    }
    readLen=out_data_end-out_data;
    if ((readLen>dataLen)||(self->readFromPos!=readFromPos)) return _hpatch_FALSE; //error
    self->readFromPos=readFromPos+readLen;
    if (self->isInHitCache){
        assert(readLen<=(hpatch_size_t)(self->cachesEnd-self->caches));
        memcpy(out_data,self->caches,readLen);
        self->caches+=readLen;
        return hpatch_TRUE;
    }else{
        return self->oldData->read(self->oldData,readFromPos,out_data,out_data_end);
    }
}

static hpatch_BOOL _cache_old(hpatch_TStreamInput** out_cachedOld,const hpatch_TStreamInput* oldData,
                              _TArrayCovers* arrayCovers,hpatch_BOOL* out_isReadError,
                              TByte* temp_cache,TByte** ptemp_cache_end,TByte* cache_buf_end){
    _cache_old_TStreamInput* self;
    TByte* temp_cache_end=*ptemp_cache_end;
    hpatch_StreamPos_t oldPosBegin;
    hpatch_StreamPos_t oldPosEnd;
    hpatch_size_t      sumCacheLen;
    hpatch_size_t      maxCachedLen;
    *out_isReadError=hpatch_FALSE;
    _cache_alloc(*out_cachedOld,hpatch_TStreamInput,sizeof(hpatch_TStreamInput),
                 temp_cache,temp_cache_end);
    _cache_alloc(self,_cache_old_TStreamInput,sizeof(_cache_old_TStreamInput),
                 temp_cache,temp_cache_end);
    
    maxCachedLen=_getMaxCachedLen(arrayCovers,temp_cache,temp_cache_end,cache_buf_end);
    if (maxCachedLen==0) return hpatch_FALSE;
    sumCacheLen=_set_cache_pos(arrayCovers,maxCachedLen,&oldPosBegin,&oldPosEnd);
    if (sumCacheLen==0) return hpatch_FALSE;
    temp_cache_end=temp_cache+sumCacheLen;
    
    if (!_cache_old_load(oldData,oldPosBegin,oldPosEnd,arrayCovers,maxCachedLen,sumCacheLen,
                         temp_cache,temp_cache_end,cache_buf_end))
        { *out_isReadError=hpatch_TRUE; return _hpatch_FALSE; }
    
    {//out
        self->arrayCovers=*arrayCovers;
        self->arrayCovers.cur_index=0;
        self->isInHitCache=hpatch_FALSE;
        self->maxCachedLen=maxCachedLen;
        self->caches=temp_cache;
        self->cachesEnd=temp_cache_end;
        self->readFromPos=0;
        self->readFromPosEnd=0;
        self->oldData=oldData;
        (*out_cachedOld)->streamImport=self;
        (*out_cachedOld)->streamSize=oldData->streamSize;
        (*out_cachedOld)->read=_cache_old_StreamInput_read;
        *ptemp_cache_end=temp_cache_end;
    }
    return hpatch_TRUE;
}

#endif //_IS_NEED_CACHE_OLD_BY_COVERS


static hpatch_BOOL _patch_cache(hpatch_TCovers** out_covers,
                                const hpatch_TStreamInput** poldData,hpatch_StreamPos_t newDataSize,
                                const hpatch_TStreamInput*  diffData,hpatch_BOOL isCompressedDiff,
                                hpatch_TDecompress* decompressPlugin,size_t kCacheCount,
                                TByte** ptemp_cache,TByte** ptemp_cache_end,hpatch_BOOL* out_isReadError){
    const hpatch_TStreamInput* oldData=*poldData;
    const hpatch_size_t kMinCacheSize=hpatch_kStreamCacheSize*kCacheCount;
#if (_IS_NEED_CACHE_OLD_BY_COVERS)
    const hpatch_size_t kBestACacheSize=hpatch_kFileIOBufBetterSize;   //内存足够时比较好的hpatch_kStreamCacheSize值;
    const hpatch_size_t _minActiveSize=(1<<20)*8;
    const hpatch_StreamPos_t _betterActiveSize=kBestACacheSize*kCacheCount*2+oldData->streamSize/8;
    const hpatch_size_t kActiveCacheOldMemorySize = //尝试激活CacheOld功能的内存下限;
                (_betterActiveSize>_minActiveSize)?_minActiveSize:(hpatch_size_t)_betterActiveSize;
#endif //_IS_NEED_CACHE_OLD_BY_COVERS
    TByte* temp_cache=*ptemp_cache;
    TByte* temp_cache_end=*ptemp_cache_end;
    *out_isReadError=hpatch_FALSE;
    if ((hpatch_size_t)(temp_cache_end-temp_cache)>=oldData->streamSize+kMinCacheSize
        +sizeof(hpatch_TStreamInput)+sizeof(hpatch_StreamPos_t)){//load all oldData
        hpatch_TStreamInput* replace_oldData=0;
        _cache_alloc(replace_oldData,hpatch_TStreamInput,sizeof(hpatch_TStreamInput),
                     temp_cache,temp_cache_end);
        if (!_cache_load_all(oldData,temp_cache_end-oldData->streamSize,
                             temp_cache_end)){ *out_isReadError=hpatch_TRUE; return _hpatch_FALSE; }
        
        mem_as_hStreamInput(replace_oldData,temp_cache_end-oldData->streamSize,temp_cache_end);
        temp_cache_end-=oldData->streamSize;
        // [          patch cache            |       oldData cache     ]
        // [ (cacheSize-oldData->streamSize) |  (oldData->streamSize)  ]
        *out_covers=0;
        *poldData=replace_oldData;
        *ptemp_cache=temp_cache;
        *ptemp_cache_end=temp_cache_end;
        return hpatch_TRUE;
    }
#if (_IS_NEED_CACHE_OLD_BY_COVERS)
    else if ((hpatch_size_t)(temp_cache_end-temp_cache)>=kActiveCacheOldMemorySize) {
        hpatch_BOOL         isUsedCover32;
        TByte*              temp_cache_end_back=temp_cache_end;
        _TArrayCovers*      arrayCovers=0;
        assert((hpatch_size_t)(temp_cache_end-temp_cache)>kBestACacheSize*kCacheCount);
        assert(kBestACacheSize>sizeof(_TCompressedCovers)+sizeof(_TPackedCovers));
        if (isCompressedDiff){
            hpatch_compressedDiffInfo diffInfo;
            _TCompressedCovers* compressedCovers=0;
            if (!_compressedCovers_open(&compressedCovers,&diffInfo,diffData,decompressPlugin,
                                        temp_cache_end-kBestACacheSize-sizeof(_TCompressedCovers),temp_cache_end))
                { *out_isReadError=hpatch_TRUE; return _hpatch_FALSE; }
            if ((oldData->streamSize!=diffInfo.oldDataSize)||(newDataSize!=diffInfo.newDataSize))
                { *out_isReadError=hpatch_TRUE; return _hpatch_FALSE; }
            temp_cache_end-=kBestACacheSize+sizeof(_TCompressedCovers);
            // [                       ...                                 |   compressedCovers cache   ]
            // [           (cacheSize-kBestACacheSize)                     |      (kBestACacheSize)     ]
            *out_covers=&compressedCovers->base.ICovers;
            isUsedCover32=(diffInfo.oldDataSize|diffInfo.newDataSize)<((hpatch_StreamPos_t)1<<32);
        }else{
            _TPackedCovers* packedCovers=0;
            _THDiffHead     diffHead;
            hpatch_StreamPos_t oldDataSize=oldData->streamSize;
            if (!_packedCovers_open(&packedCovers,&diffHead,diffData,
                                    temp_cache_end-kBestACacheSize*3-sizeof(_TPackedCovers),temp_cache_end))
                { *out_isReadError=hpatch_TRUE; return _hpatch_FALSE; }
            temp_cache_end-=kBestACacheSize*3+sizeof(_TPackedCovers);
            // [                       ...                                 |     packedCovers cache     ]
            // [          (cacheSize-kBestACacheSize*3)                    |    (kBestACacheSize*3)     ]
            *out_covers=&packedCovers->base.ICovers;
            isUsedCover32=(oldDataSize|newDataSize)<((hpatch_StreamPos_t)1<<32);
        }
        
        if (!_arrayCovers_load(&arrayCovers,*out_covers,isUsedCover32,
                               out_isReadError,&temp_cache,temp_cache_end-kBestACacheSize)){
            if (*out_isReadError) return _hpatch_FALSE;
            // [                    patch cache                            |       *edCovers cache      ]
            // [           (cacheSize-kBestACacheSize*?)                   |     (kBestACacheSize*?)    ]
            *ptemp_cache=temp_cache;
            *ptemp_cache_end=temp_cache_end;
            return hpatch_FALSE;
        }else{
            // [         arrayCovers cache         |                         ...                        ]
            // [((new temp_cache)-(old temp_cache))|          (cacheSize-(arrayCovers cache size))      ]
            TByte* old_cache_end;
            hpatch_TStreamInput* replace_oldData=0;
            assert(!(*out_isReadError));
            if (!((*out_covers)->close(*out_covers))) return _hpatch_FALSE;
            *out_covers=&arrayCovers->ICovers;
            temp_cache_end=temp_cache_end_back; //free compressedCovers or packedCovers memory
            old_cache_end=temp_cache_end-kBestACacheSize*kCacheCount;
            // [       arrayCovers cache           |        ...        |      patch reserve cache       ]
            // [                                   |        ...        | (kBestACacheSize*kCacheCount) ]
            if (((hpatch_size_t)(temp_cache_end-temp_cache)<=kBestACacheSize*kCacheCount)
                ||(!_cache_old(&replace_oldData,oldData,arrayCovers,out_isReadError,
                               temp_cache,&old_cache_end,temp_cache_end))){
                if (*out_isReadError) return _hpatch_FALSE;
            // [         arrayCovers cache         |                   patch cache                      ]
                *ptemp_cache=temp_cache;
                *ptemp_cache_end=temp_cache_end;
                return hpatch_FALSE;
            }else{
            // [         arrayCovers cache         | oldData cache |             patch cache            ]
            // [                                   |               |(temp_cache_end-(new old_cache_end))]
                assert(!(*out_isReadError));
                assert((hpatch_size_t)(temp_cache_end-old_cache_end)>=kBestACacheSize*kCacheCount);
                temp_cache=old_cache_end;
                
                *poldData=replace_oldData;
                *ptemp_cache=temp_cache;
                *ptemp_cache_end=temp_cache_end;
                return hpatch_TRUE;
            }
        }
    }
#endif//_IS_NEED_CACHE_OLD_BY_COVERS
    return hpatch_FALSE;//not cache oldData
}

hpatch_BOOL patch_stream_with_cache(const struct hpatch_TStreamOutput* out_newData,
                                    const struct hpatch_TStreamInput*  oldData,
                                    const struct hpatch_TStreamInput*  serializedDiff,
                                    TByte*   temp_cache,TByte* temp_cache_end){
    hpatch_BOOL     result;
    hpatch_TCovers* covers=0;//not need close before return
    hpatch_BOOL    isReadError=hpatch_FALSE;
    _patch_cache(&covers,&oldData,out_newData->streamSize,serializedDiff,hpatch_FALSE,0,
                _kCachePatCount,&temp_cache,&temp_cache_end,&isReadError);
    if (isReadError) return _hpatch_FALSE;
    result=_patch_stream_with_cache(out_newData,oldData,serializedDiff,covers,
                                    temp_cache,temp_cache_end);
    //if ((covers!=0)&&(!covers->close(covers))) result=_hpatch_FALSE;
    return result;
}

hpatch_BOOL patch_stream(const hpatch_TStreamOutput* out_newData,
                         const hpatch_TStreamInput*  oldData,
                         const hpatch_TStreamInput*  serializedDiff){
    TByte temp_cache[hpatch_kStreamCacheSize*_kCachePatCount];
    return _patch_stream_with_cache(out_newData,oldData,serializedDiff,0,
                                    temp_cache,temp_cache+sizeof(temp_cache)/sizeof(TByte));
}

hpatch_BOOL patch_decompress_with_cache(const hpatch_TStreamOutput* out_newData,
                                        const hpatch_TStreamInput*  oldData,
                                        const hpatch_TStreamInput*  compressedDiff,
                                        hpatch_TDecompress* decompressPlugin,
                                        TByte* temp_cache,TByte* temp_cache_end){
    hpatch_BOOL     result;
    hpatch_TCovers* covers=0; //need close before return
    hpatch_BOOL    isReadError=hpatch_FALSE;
    _patch_cache(&covers,&oldData,out_newData->streamSize,compressedDiff,hpatch_TRUE,
                 decompressPlugin,_kCacheDecCount,&temp_cache,&temp_cache_end,&isReadError);
    if (isReadError) return _hpatch_FALSE;
    result=_patch_decompress_cache(out_newData,0,oldData,compressedDiff,decompressPlugin,
                                   covers,temp_cache,temp_cache_end);
    if ((covers!=0)&&(!covers->close(covers))) result=_hpatch_FALSE;
    return result;
}

hpatch_BOOL patch_decompress(const hpatch_TStreamOutput* out_newData,
                             const hpatch_TStreamInput*  oldData,
                             const hpatch_TStreamInput*  compressedDiff,
                             hpatch_TDecompress* decompressPlugin){
    TByte temp_cache[hpatch_kStreamCacheSize*_kCacheDecCount];
    return _patch_decompress_cache(out_newData,0,oldData,compressedDiff,decompressPlugin,
                                   0,temp_cache,temp_cache+sizeof(temp_cache)/sizeof(TByte));
}

hpatch_BOOL hpatch_coverList_open_serializedDiff(hpatch_TCoverList* out_coverList,
                                                 const hpatch_TStreamInput*  serializedDiff){
    TByte* temp_cache;
    TByte* temp_cache_end;
    _TPackedCovers* packedCovers=0;
    _THDiffHead     diffHead;
    assert((out_coverList!=0)&&(out_coverList->ICovers==0));
    temp_cache=out_coverList->_buf;
    temp_cache_end=temp_cache+sizeof(out_coverList->_buf);
    if (!_packedCovers_open(&packedCovers,&diffHead,serializedDiff,
                            temp_cache,temp_cache_end))
        return _hpatch_FALSE;
    out_coverList->ICovers=&packedCovers->base.ICovers;
    return hpatch_TRUE;
}

hpatch_BOOL hpatch_coverList_open_compressedDiff(hpatch_TCoverList* out_coverList,
                                                 const hpatch_TStreamInput*  compressedDiff,
                                                 hpatch_TDecompress*         decompressPlugin){
    TByte* temp_cache;
    TByte* temp_cache_end;
    _TCompressedCovers* compressedCovers=0;
    hpatch_compressedDiffInfo diffInfo;
    assert((out_coverList!=0)&&(out_coverList->ICovers==0));
    temp_cache=out_coverList->_buf;
    temp_cache_end=temp_cache+sizeof(out_coverList->_buf);
    if (!_compressedCovers_open(&compressedCovers,&diffInfo,compressedDiff,decompressPlugin,
                                temp_cache,temp_cache_end))
        return _hpatch_FALSE;
    out_coverList->ICovers=&compressedCovers->base.ICovers;
    return hpatch_TRUE;
}

//

#define     _kCacheSgCount  3

hpatch_BOOL patch_single_compressed_diff(const hpatch_TStreamOutput* out_newData,
                                         const hpatch_TStreamInput*  oldData,
                                         const hpatch_TStreamInput*  singleCompressedDiff,
                                         hpatch_StreamPos_t          diffData_pos,
                                         hpatch_StreamPos_t          uncompressedSize,
                                         hpatch_TDecompress*         decompressPlugin,
                                         hpatch_StreamPos_t coverCount,hpatch_size_t stepMemSize,
                                         unsigned char* temp_cache,unsigned char* temp_cache_end){
    hpatch_BOOL result;
    hpatch_TUncompresser_t uncompressedStream;
    memset(&uncompressedStream,0,sizeof(uncompressedStream));
    if (decompressPlugin){
        if (!compressed_stream_as_uncompressed(&uncompressedStream,uncompressedSize,decompressPlugin,singleCompressedDiff,
                                               diffData_pos,singleCompressedDiff->streamSize)) return _hpatch_FALSE;
        singleCompressedDiff=&uncompressedStream.base;
        diffData_pos=0;
    }
    result=patch_single_stream_diff(out_newData,oldData,singleCompressedDiff,diffData_pos,
                                    coverCount,stepMemSize,temp_cache,temp_cache_end);
    if (decompressPlugin)
        close_compressed_stream_as_uncompressed(&uncompressedStream);
    return result;
}

hpatch_BOOL getSingleCompressedDiffInfo(hpatch_singleCompressedDiffInfo* out_diffInfo,
                                        const hpatch_TStreamInput* singleCompressedDiff,
                                        hpatch_StreamPos_t         diffInfo_pos){
    TStreamCacheClip  _diffHeadClip;
    TStreamCacheClip* diffHeadClip=&_diffHeadClip;
    TByte             temp_cache[hpatch_kStreamCacheSize];
    _TStreamCacheClip_init(&_diffHeadClip,singleCompressedDiff,diffInfo_pos,singleCompressedDiff->streamSize,
                           temp_cache,hpatch_kStreamCacheSize);
    {//type
        const char* kVersionType="HDIFFSF20";
        char* tempType=out_diffInfo->compressType;
        if (!_TStreamCacheClip_readType_end(diffHeadClip,'&',tempType)) return _hpatch_FALSE;
        if (0!=strcmp(tempType,kVersionType)) return _hpatch_FALSE;
    }
    {//read compressType
        if (!_TStreamCacheClip_readType_end(diffHeadClip,'\0',
                                            out_diffInfo->compressType)) return _hpatch_FALSE;
    }
    _clip_unpackUIntTo(&out_diffInfo->newDataSize,diffHeadClip);
    _clip_unpackUIntTo(&out_diffInfo->oldDataSize,diffHeadClip);
    _clip_unpackUIntTo(&out_diffInfo->coverCount,diffHeadClip);
    _clip_unpackUIntTo(&out_diffInfo->stepMemSize,diffHeadClip);
    _clip_unpackUIntTo(&out_diffInfo->uncompressedSize,diffHeadClip);
    _clip_unpackUIntTo(&out_diffInfo->compressedSize,diffHeadClip);
    out_diffInfo->diffDataPos=_TStreamCacheClip_readPosOfSrcStream(diffHeadClip)-diffInfo_pos;
    if (out_diffInfo->stepMemSize>(size_t)((~(size_t)0)-hpatch_kStreamCacheSize*_kCacheSgCount)) 
        return _hpatch_FALSE;
    return hpatch_TRUE;
}

static hpatch_BOOL _TUncompresser_read(const struct hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                       unsigned char* out_data,unsigned char* out_data_end){
    hpatch_TUncompresser_t* self=(hpatch_TUncompresser_t*)stream->streamImport;
    return self->_decompressPlugin->decompress_part(self->_decompressHandle,out_data,out_data_end);
}

hpatch_BOOL compressed_stream_as_uncompressed(hpatch_TUncompresser_t* uncompressedStream,hpatch_StreamPos_t uncompressedSize,
                                              hpatch_TDecompress* decompressPlugin,const hpatch_TStreamInput* compressedStream,
                                              hpatch_StreamPos_t compressed_pos,hpatch_StreamPos_t compressed_end){
    hpatch_TUncompresser_t* self=uncompressedStream;
    assert(decompressPlugin!=0);
    assert(self->_decompressHandle==0);
    self->_decompressHandle=decompressPlugin->open(decompressPlugin,uncompressedSize,compressedStream,
                                                   compressed_pos,compressed_end);
    if (self->_decompressHandle==0) return _hpatch_FALSE;
    self->_decompressPlugin=decompressPlugin;
    
    self->base.streamImport=self;
    self->base.streamSize=uncompressedSize;
    self->base.read=_TUncompresser_read;
    return hpatch_TRUE;
}

void close_compressed_stream_as_uncompressed(hpatch_TUncompresser_t* uncompressedStream){
    hpatch_TUncompresser_t* self=uncompressedStream;
    if (self==0) return;
    if (self->_decompressHandle==0) return;
    self->_decompressPlugin->close(self->_decompressPlugin,self->_decompressHandle);
    self->_decompressHandle=0;
}

typedef struct{
    const unsigned char* code;
    const unsigned char* code_end;
    hpatch_size_t   len0;
    hpatch_size_t   lenv;
    hpatch_BOOL     isNeedDecode0;
} rle0_decoder_t;

static void _rle0_decoder_init(rle0_decoder_t* self,const unsigned char* code,const unsigned char* code_end){
    self->code=code;
    self->code_end=code_end;
    self->len0=0;
    self->lenv=0;
    self->isNeedDecode0=hpatch_TRUE;
}

static hpatch_BOOL _rle0_decoder_add(rle0_decoder_t* self,TByte* out_data,hpatch_size_t decodeSize){
    if (self->len0){
    _0_process:
        if (self->len0>=decodeSize){
            self->len0-=decodeSize;
            return hpatch_TRUE;
        }else{
            decodeSize-=self->len0;
            out_data+=self->len0;
            self->len0=0;
            goto _decode_v_process;
        }
    }
    
    if (self->lenv){
    _v_process:
        if (self->lenv>=decodeSize){
            addData(out_data,self->code,decodeSize);
            self->code+=decodeSize;
            self->lenv-=decodeSize;
            return hpatch_TRUE;
        }else{
            addData(out_data,self->code,self->lenv);
            out_data+=self->lenv;
            decodeSize-=self->lenv;
            self->code+=self->lenv;
            self->lenv=0;
            goto _decode_0_process;
        }
    }
    
    assert(decodeSize>0);
    if (self->isNeedDecode0){
        hpatch_StreamPos_t len0;
    _decode_0_process:
        self->isNeedDecode0=hpatch_FALSE;
        if (!hpatch_unpackUInt(&self->code,self->code_end,&len0)) return _hpatch_FALSE;
        if (len0!=(hpatch_size_t)len0) return _hpatch_FALSE;
        self->len0=(hpatch_size_t)len0;
        goto _0_process;
    }else{
        hpatch_StreamPos_t lenv;
    _decode_v_process:
        self->isNeedDecode0=hpatch_TRUE;
        if (!hpatch_unpackUInt(&self->code,self->code_end,&lenv)) return _hpatch_FALSE;
        if (lenv>(size_t)(self->code_end-self->code)) return _hpatch_FALSE;
        self->lenv=(hpatch_size_t)lenv;
        goto _v_process;
    }
}


static  hpatch_BOOL _patch_add_old_with_rle0(TOutStreamCache* outCache,rle0_decoder_t* rle0_decoder,
                                             const hpatch_TStreamInput* old,hpatch_StreamPos_t oldPos,
                                             hpatch_StreamPos_t addLength,TByte* aCache,hpatch_size_t aCacheSize){
    while (addLength>0){
        hpatch_size_t decodeStep=aCacheSize;
        if (decodeStep>addLength)
            decodeStep=(hpatch_size_t)addLength;
        if (!old->read(old,oldPos,aCache,aCache+decodeStep)) return _hpatch_FALSE;
        if (!_rle0_decoder_add(rle0_decoder,aCache,decodeStep)) return _hpatch_FALSE;
        if (!TOutStreamCache_write(outCache,aCache,decodeStep)) return _hpatch_FALSE;
        oldPos+=decodeStep;
        addLength-=decodeStep;
    }
    return hpatch_TRUE;
}

hpatch_BOOL patch_single_stream_diff(const hpatch_TStreamOutput*  out_newData,
                                     const hpatch_TStreamInput*   oldData,
                                     const hpatch_TStreamInput*   uncompressedDiffData,
                                     hpatch_StreamPos_t           diffData_pos,
                                     hpatch_StreamPos_t coverCount,hpatch_size_t stepMemSize,
                                     unsigned char* temp_cache,unsigned char* temp_cache_end){
    unsigned char*      step_cache=temp_cache;
    hpatch_StreamPos_t  lastOldEnd=0;
    hpatch_StreamPos_t  lastNewEnd=0;
    hpatch_size_t       cache_size;
    TStreamCacheClip    inClip;
    TOutStreamCache     outCache;
    if ((size_t)(temp_cache_end-temp_cache)<stepMemSize+hpatch_kStreamCacheSize*_kCacheSgCount) return _hpatch_FALSE;
    temp_cache+=stepMemSize;
    cache_size=(temp_cache_end-temp_cache)/_kCacheSgCount;
    _TStreamCacheClip_init(&inClip,uncompressedDiffData,diffData_pos,uncompressedDiffData->streamSize,
                           temp_cache,cache_size);
    temp_cache+=cache_size;
    TOutStreamCache_init(&outCache,out_newData,temp_cache+cache_size,cache_size);
    while (coverCount) {//step loop
        const unsigned char* covers_cache     = step_cache;
        const unsigned char* covers_cache_end;
        rle0_decoder_t       rle0_decoder;
        {//read step info
            hpatch_StreamPos_t bufCover_size;
            hpatch_StreamPos_t bufRle_size;
            _clip_unpackUIntTo(&bufCover_size,&inClip);
            _clip_unpackUIntTo(&bufRle_size,&inClip);
            if (bufCover_size+bufRle_size>stepMemSize) return _hpatch_FALSE;
            if (!_TStreamCacheClip_readDataTo(&inClip,step_cache,step_cache+bufCover_size+bufRle_size)) return _hpatch_FALSE;
            covers_cache_end=covers_cache+bufCover_size;
            _rle0_decoder_init(&rle0_decoder,covers_cache_end,covers_cache_end+bufRle_size);
        }
        while (covers_cache!=covers_cache_end) {// cover loop
            hpatch_StreamPos_t oldPos;
            hpatch_StreamPos_t newPos;
            hpatch_StreamPos_t coverLen;
            {//read cover
                hpatch_BOOL inc_oldPos_sign;
                if (covers_cache==covers_cache_end) return _hpatch_FALSE;
                inc_oldPos_sign=(*covers_cache)>>7;
                if (!hpatch_unpackUIntWithTag(&covers_cache,covers_cache_end,&oldPos,1)) return _hpatch_FALSE;
                if (inc_oldPos_sign==0)
                    oldPos+=lastOldEnd;
                else
                    oldPos=lastOldEnd-oldPos;
                if (!hpatch_unpackUInt(&covers_cache,covers_cache_end,&newPos)) return _hpatch_FALSE;
                newPos+=lastNewEnd;
                if (!hpatch_unpackUInt(&covers_cache,covers_cache_end,&coverLen)) return _hpatch_FALSE;
            }
            
            if (newPos>lastNewEnd){
                if (!_patch_copy_diff_by_outCache(&outCache,&inClip,newPos-lastNewEnd)) return _hpatch_FALSE;
            }
            
            --coverCount;
            if (coverLen){
                #ifdef __RUN_MEM_SAFE_CHECK
                    if (oldPos>oldData->streamSize) return _hpatch_FALSE;
                    if (coverLen>(hpatch_StreamPos_t)(oldData->streamSize-oldPos)) return _hpatch_FALSE;
                #endif
                if (!_patch_add_old_with_rle0(&outCache,&rle0_decoder,oldData,oldPos,coverLen,
                                              temp_cache,cache_size)) return _hpatch_FALSE;
                lastOldEnd=oldPos+coverLen;
                lastNewEnd=newPos+coverLen;
            }else{
                if (coverCount!=0) return _hpatch_FALSE;
            }
        }
    }
    
    if (!TOutStreamCache_flush(&outCache))
        return _hpatch_FALSE;
    if (_TStreamCacheClip_isFinish(&inClip)&TOutStreamCache_isFinish(&outCache)&(coverCount==0))
        return hpatch_TRUE;
    else
        return _hpatch_FALSE;
}


static hpatch_BOOL _TDiffToSingleStream_read(const struct hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                           unsigned char* out_data,unsigned char* out_data_end){
    //[                                                         |readedSize                  ]
    //     [     |cachedBufBegin   _TDiffToSingleStream_kBufSize]
    //                                 readFromPos[out_data       out_data_end]
    TDiffToSingleStream* self=(TDiffToSingleStream*)stream->streamImport;
    hpatch_StreamPos_t readedSize=self->readedSize;
    while (1){
        size_t rLen=out_data_end-out_data;
        if (readFromPos==readedSize){
            hpatch_BOOL result=self->diffStream->read(self->diffStream,readedSize,out_data,out_data_end);
            self->readedSize=readedSize+rLen;
            if ((self->isInSingleStream)||(rLen>_TDiffToSingleStream_kBufSize)){
                self->cachedBufBegin=_TDiffToSingleStream_kBufSize;
            }else{
                //cache
                if (rLen>=_TDiffToSingleStream_kBufSize){
                    memcpy(self->buf,out_data_end-_TDiffToSingleStream_kBufSize,_TDiffToSingleStream_kBufSize);
                    self->cachedBufBegin = 0;
                }else{
                    size_t new_cachedBufBegin;
                    if (self->cachedBufBegin>=rLen){
                        new_cachedBufBegin=self->cachedBufBegin-rLen;
                        memmove(self->buf+new_cachedBufBegin,self->buf+self->cachedBufBegin,_TDiffToSingleStream_kBufSize-self->cachedBufBegin);
                    }else{
                        new_cachedBufBegin=0;
                        memmove(self->buf,self->buf+rLen,_TDiffToSingleStream_kBufSize-rLen);
                    }
                    memcpy(self->buf+(_TDiffToSingleStream_kBufSize-rLen),out_data,rLen);
                    self->cachedBufBegin=new_cachedBufBegin;
                }
            }
            return result;
        }else{
            size_t cachedSize=_TDiffToSingleStream_kBufSize-self->cachedBufBegin;
            size_t bufSize=(size_t)(readedSize-readFromPos);
            if ((readFromPos<readedSize)&(bufSize<=cachedSize)){
                if (rLen>bufSize)
                    rLen=bufSize;
                memcpy(out_data,self->buf+(_TDiffToSingleStream_kBufSize-bufSize),rLen);
                out_data+=rLen;
                readFromPos+=rLen;
                if (out_data==out_data_end) 
                    return hpatch_TRUE;
                else
                    continue;
            }else{
                return _hpatch_FALSE;
            }
        }
    }
}
void TDiffToSingleStream_init(TDiffToSingleStream* self,const hpatch_TStreamInput* diffStream){
    self->base.streamImport=self;
    self->base.streamSize=diffStream->streamSize;
    self->base.read=_TDiffToSingleStream_read;
    self->base._private_reserved=0;
    self->diffStream=diffStream;
    self->readedSize=0;
    self->cachedBufBegin=_TDiffToSingleStream_kBufSize;
    self->isInSingleStream=hpatch_FALSE;
}

hpatch_BOOL patch_single_stream_by(sspatch_listener_t* listener,
                                   const hpatch_TStreamOutput* __out_newData,
                                   const hpatch_TStreamInput*  oldData,
                                   const hpatch_TStreamInput*  singleCompressedDiff, 
                                   hpatch_StreamPos_t  diffInfo_pos){
    hpatch_BOOL result;
    hpatch_TDecompress*     decompressPlugin=0;
    unsigned char*          temp_cache=0;
    unsigned char*          temp_cacheEnd=0;
    hpatch_singleCompressedDiffInfo diffInfo;
    hpatch_TStreamOutput    _out_newData=*__out_newData;
    hpatch_TStreamOutput*   out_newData=&_out_newData;
    TDiffToSingleStream     _toSStream;
    assert((listener)&&(listener->onDiffInfo));
    TDiffToSingleStream_init(&_toSStream,singleCompressedDiff);
    singleCompressedDiff=&_toSStream.base;

    if (!getSingleCompressedDiffInfo(&diffInfo,singleCompressedDiff,diffInfo_pos))
        return _hpatch_FALSE;
    if (diffInfo.newDataSize>out_newData->streamSize)
        return _hpatch_FALSE;
    out_newData->streamSize=diffInfo.newDataSize;
    if (diffInfo.oldDataSize!=oldData->streamSize)
        return _hpatch_FALSE;
    if ((diffInfo.compressedSize?diffInfo.compressedSize:diffInfo.uncompressedSize)
        !=(singleCompressedDiff->streamSize-diffInfo.diffDataPos))
        return _hpatch_FALSE;

    if (!listener->onDiffInfo(listener,&diffInfo,&decompressPlugin,&temp_cache,&temp_cacheEnd))
        return _hpatch_FALSE;
    if ((temp_cache==0)||(temp_cache>=temp_cacheEnd))
        return _hpatch_FALSE;
    result=(diffInfo.compressType[0]=='\0')?(decompressPlugin==0):(decompressPlugin!=0);
    result=result&&patch_single_compressed_diff(out_newData,oldData,singleCompressedDiff,
                                                diffInfo.diffDataPos,diffInfo.uncompressedSize,
                                                decompressPlugin,diffInfo.coverCount,
                                                (size_t)diffInfo.stepMemSize,temp_cache,temp_cacheEnd);
    if (listener->onPatchFinish)
        listener->onPatchFinish(listener,temp_cache,temp_cacheEnd);
    return result;
}
