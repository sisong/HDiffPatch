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
#include <string.h> //memcpy memset size_t
#if (_IS_NEED_CACHE_OLD_BY_COVERS)
#   include <stdlib.h> //qsort
#endif
#include "patch_private.h"

//__RUN_MEM_SAFE_CHECK用来启动内存访问越界检查,用以防御可能被意外或故意损坏的数据.
#define __RUN_MEM_SAFE_CHECK

#define _hpatch_FALSE   hpatch_FALSE
//int __debug_check_false_x=0; //for debug
//#define _hpatch_FALSE (1/__debug_check_false_x)

static const int kSignTagBit=1;
typedef unsigned char TByte;
#define TUInt size_t

    static hpatch_BOOL _read_mem_stream(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                        unsigned char* out_data,unsigned char* out_data_end){
        const unsigned char* src=(const unsigned char*)stream->streamImport;
        if (readFromPos+(out_data_end-out_data)<=stream->streamSize){
            memcpy(out_data,src+readFromPos,out_data_end-out_data);
            return hpatch_TRUE;
        }else{
            return _hpatch_FALSE;
        }
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
        if (writeToPos+(data_end-data)<=stream->streamSize){
            memcpy(out_dst+writeToPos,data,data_end-data);
            return hpatch_TRUE;
        }else{
            return _hpatch_FALSE;
        }
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

static hpatch_BOOL _TStreamInputClip_read(const hpatch_TStreamInput* stream,
                                          hpatch_StreamPos_t readFromPos,
                                          unsigned char* out_data,unsigned char* out_data_end){
    TStreamInputClip* self=(TStreamInputClip*)stream->streamImport;
    if (readFromPos+(out_data_end-out_data)<=self->base.streamSize)
        return self->srcStream->read(self->srcStream,readFromPos+self->clipBeginPos,out_data,out_data_end);
    else
        return _hpatch_FALSE;
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


static hpatch_BOOL _bytesRle_load(TByte* out_data,TByte* out_dataEnd,
                                  const TByte* rle_code,const TByte* rle_code_end);
static void addData(TByte* dst,const TByte* src,size_t length);
hpatch_inline
static hpatch_BOOL _unpackUIntWithTag(const TByte** src_code,const TByte* src_code_end,
                                      TUInt* result,const unsigned int kTagBit){
    if (sizeof(TUInt)==sizeof(hpatch_StreamPos_t)){
        return hpatch_unpackUIntWithTag(src_code,src_code_end,(hpatch_StreamPos_t*)result,kTagBit);
    }else{
        hpatch_StreamPos_t u64=0;
        hpatch_BOOL rt=hpatch_unpackUIntWithTag(src_code,src_code_end,&u64,kTagBit);
        TUInt u=(TUInt)u64;
        *result=u;
#ifdef __RUN_MEM_SAFE_CHECK
        return rt&(u==u64);
#else
        return rt;
#endif
    }
}

#define unpackUIntWithTagTo(puint,src_code,src_code_end,kTagBit) \
        { if (!_unpackUIntWithTag(src_code,src_code_end,puint,kTagBit)) return _hpatch_FALSE; }
#define unpackUIntTo(puint,src_code,src_code_end) \
        unpackUIntWithTagTo(puint,src_code,src_code_end,0)

hpatch_BOOL patch(TByte* out_newData,TByte* out_newData_end,
                  const TByte* oldData,const TByte* oldData_end,
                  const TByte* serializedDiff,const TByte* serializedDiff_end){
    const TByte *code_lengths, *code_lengths_end,
                *code_inc_newPos, *code_inc_newPos_end,
                *code_inc_oldPos, *code_inc_oldPos_end,
                *code_newDataDiff, *code_newDataDiff_end;
    TUInt       coverCount;

    assert(out_newData<=out_newData_end);
    assert(oldData<=oldData_end);
    assert(serializedDiff<=serializedDiff_end);
    unpackUIntTo(&coverCount,&serializedDiff, serializedDiff_end);
    {   //head
        TUInt lengthSize,inc_newPosSize,inc_oldPosSize,newDataDiffSize;
        unpackUIntTo(&lengthSize,&serializedDiff, serializedDiff_end);
        unpackUIntTo(&inc_newPosSize,&serializedDiff, serializedDiff_end);
        unpackUIntTo(&inc_oldPosSize,&serializedDiff, serializedDiff_end);
        unpackUIntTo(&newDataDiffSize,&serializedDiff, serializedDiff_end);
#ifdef __RUN_MEM_SAFE_CHECK
        if (lengthSize>(TUInt)(serializedDiff_end-serializedDiff)) return _hpatch_FALSE;
#endif
        code_lengths=serializedDiff;     serializedDiff+=lengthSize;
        code_lengths_end=serializedDiff;
#ifdef __RUN_MEM_SAFE_CHECK
        if (inc_newPosSize>(TUInt)(serializedDiff_end-serializedDiff)) return _hpatch_FALSE;
#endif
        code_inc_newPos=serializedDiff; serializedDiff+=inc_newPosSize;
        code_inc_newPos_end=serializedDiff;
#ifdef __RUN_MEM_SAFE_CHECK
        if (inc_oldPosSize>(TUInt)(serializedDiff_end-serializedDiff)) return _hpatch_FALSE;
#endif
        code_inc_oldPos=serializedDiff; serializedDiff+=inc_oldPosSize;
        code_inc_oldPos_end=serializedDiff;
#ifdef __RUN_MEM_SAFE_CHECK
        if (newDataDiffSize>(TUInt)(serializedDiff_end-serializedDiff)) return _hpatch_FALSE;
#endif
        code_newDataDiff=serializedDiff; serializedDiff+=newDataDiffSize;
        code_newDataDiff_end=serializedDiff;
    }

    //decode rle ; rle data begin==cur serializedDiff;
    if (!_bytesRle_load(out_newData, out_newData_end, serializedDiff, serializedDiff_end))
        return _hpatch_FALSE;

    {   //patch
        const TUInt newDataSize=(TUInt)(out_newData_end-out_newData);
        TUInt oldPosBack=0;
        TUInt newPosBack=0;
        TUInt i;
        for (i=0; i<coverCount; ++i){
            TUInt copyLength,addLength, oldPos,inc_oldPos,inc_oldPos_sign;
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
                if (copyLength>(TUInt)(newDataSize-newPosBack)) return _hpatch_FALSE;
                if (copyLength>(TUInt)(code_newDataDiff_end-code_newDataDiff)) return _hpatch_FALSE;
#endif
                memcpy(out_newData+newPosBack,code_newDataDiff,copyLength);
                code_newDataDiff+=copyLength;
                newPosBack+=copyLength;
            }
#ifdef __RUN_MEM_SAFE_CHECK
            if ( (addLength>(TUInt)(newDataSize-newPosBack)) ) return _hpatch_FALSE;
            if ( (oldPos>(TUInt)(oldData_end-oldData)) ||
                 (addLength>(TUInt)(oldData_end-oldData-oldPos)) ) return _hpatch_FALSE;
#endif
            addData(out_newData+newPosBack,oldData+oldPos,addLength);
            oldPosBack=oldPos;
            newPosBack+=addLength;
        }

        if (newPosBack<newDataSize){
            TUInt copyLength=newDataSize-newPosBack;
#ifdef __RUN_MEM_SAFE_CHECK
            if (copyLength>(TUInt)(code_newDataDiff_end-code_newDataDiff)) return _hpatch_FALSE;
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

//变长正整数编码方案(x bit额外类型标志位,x<=7),从高位开始输出1--n byte:
// x0*  7-x bit
// x1* 0*  7+7-x bit
// x1* 1* 0*  7+7+7-x bit
// x1* 1* 1* 0*  7+7+7+7-x bit
// x1* 1* 1* 1* 0*  7+7+7+7+7-x bit
// ......
hpatch_BOOL hpatch_packUIntWithTag(TByte** out_code,TByte* out_code_end,
                                   hpatch_StreamPos_t uValue,unsigned int highTag,
                                   const unsigned int kTagBit){//写入整数并前进指针.
    TByte*          pcode=*out_code;
    const unsigned int kMaxValueWithTag=(1<<(7-kTagBit))-1;
    TByte           codeBuf[hpatch_kMaxPackedUIntBytes];
    TByte*          codeEnd=codeBuf;
#ifdef __RUN_MEM_SAFE_CHECK
    //static const int kPackMaxTagBit=7;
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

unsigned int hpatch_packUIntWithTag_size(hpatch_StreamPos_t uValue,const unsigned int kTagBit){
    const unsigned int kMaxValueWithTag=(1<<(7-kTagBit))-1;
    unsigned int size=0;
    while (uValue>kMaxValueWithTag) {
        ++size;
        uValue>>=7;
    }
    ++size;
    return size;
}

hpatch_BOOL hpatch_unpackUIntWithTag(const TByte** src_code,const TByte* src_code_end,
                                     hpatch_StreamPos_t* result,const unsigned int kTagBit){//读出整数并前进指针.
#ifdef __RUN_MEM_SAFE_CHECK
    //const unsigned int kPackMaxTagBit=7;
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

static void addData(TByte* dst,const TByte* src,size_t length){
    TUInt length_fast,i;

    length_fast=length&(~(TUInt)7);
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

static hpatch_BOOL _bytesRle_load(TByte* out_data,TByte* out_dataEnd,
                                  const TByte* rle_code,const TByte* rle_code_end){
    const TByte*    ctrlBuf,*ctrlBuf_end;
    TUInt ctrlSize;
    unpackUIntTo(&ctrlSize,&rle_code,rle_code_end);
#ifdef __RUN_MEM_SAFE_CHECK
    if (ctrlSize>(TUInt)(rle_code_end-rle_code)) return _hpatch_FALSE;
#endif
    ctrlBuf=rle_code;
    rle_code+=ctrlSize;
    ctrlBuf_end=rle_code;
    while (ctrlBuf_end-ctrlBuf>0){
        enum TByteRleType type=(enum TByteRleType)((*ctrlBuf)>>(8-kByteRleType_bit));
        TUInt length;
        unpackUIntWithTagTo(&length,&ctrlBuf,ctrlBuf_end,kByteRleType_bit);
#ifdef __RUN_MEM_SAFE_CHECK
        if (length>=(TUInt)(out_dataEnd-out_data)) return _hpatch_FALSE;
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
                if (1>(TUInt)(rle_code_end-rle_code)) return _hpatch_FALSE;
#endif
                memset(out_data,*rle_code,length);
                ++rle_code;
                out_data+=length;
            }break;
            case kByteRleType_unrle:{
#ifdef __RUN_MEM_SAFE_CHECK
                if (length>(TUInt)(rle_code_end-rle_code)) return _hpatch_FALSE;
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
#undef  TUInt
#define TUInt hpatch_StreamPos_t

hpatch_BOOL _TStreamCacheClip_updateCache(TStreamCacheClip* sclip){
    TByte* buf0=&sclip->cacheBuf[0];
    const TUInt streamSize=(TUInt)(sclip->streamPos_end-sclip->streamPos);
    size_t readSize=sclip->cacheBegin;
    if (readSize>streamSize)
        readSize=(size_t)streamSize;
    if (readSize==0) return hpatch_TRUE;
    if (!_TStreamCacheClip_isCacheEmpty(sclip)){
        memmove(buf0+(size_t)(sclip->cacheBegin-readSize),
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
        size_t len=sclip->cacheEnd;
        if (len>skipLongSize)
            len=(size_t)skipLongSize;
        if (_TStreamCacheClip_accessData(sclip,len)){
            _TStreamCacheClip_skipData_noCheck(sclip,len);
            skipLongSize-=len;
        }else{
            return hpatch_FALSE;
        }
    }
    return hpatch_TRUE;
}

//assert(hpatch_kStreamCacheSize>=hpatch_kMaxPackedUIntBytes);
struct __private_hpatch_check_hpatch_kMaxPackedUIntBytes {
    char _[(hpatch_kStreamCacheSize>=hpatch_kMaxPackedUIntBytes)?1:-1]; };

hpatch_BOOL _TStreamCacheClip_unpackUIntWithTag(TStreamCacheClip* sclip,TUInt* result,const int kTagBit){
    TByte* curCode,*codeBegin;
    size_t readSize=hpatch_kMaxPackedUIntBytes;
    const TUInt dataSize=_TStreamCacheClip_streamSize(sclip);
    if (readSize>dataSize)
        readSize=(size_t)dataSize;
    codeBegin=_TStreamCacheClip_accessData(sclip,readSize);
    if (codeBegin==0) return _hpatch_FALSE;
    curCode=codeBegin;
    if (!hpatch_unpackUIntWithTag((const TByte**)&curCode,codeBegin+readSize,result,kTagBit))
        return _hpatch_FALSE;
    _TStreamCacheClip_skipData_noCheck(sclip,(size_t)(curCode-codeBegin));
    return hpatch_TRUE;
}

#define _TStreamCacheClip_unpackUIntWithTagTo(puint,sclip,kTagBit) \
    { if (!_TStreamCacheClip_unpackUIntWithTag(sclip,puint,kTagBit)) return _hpatch_FALSE; }
#define _TStreamCacheClip_unpackUIntTo(puint,sclip) \
    _TStreamCacheClip_unpackUIntWithTagTo(puint,sclip,0)


typedef struct _TBytesRle_load_stream{
    TUInt               memCopyLength;
    TUInt               memSetLength;
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


static void memSet_add(TByte* dst,const TByte src,size_t length){
    size_t length_fast,i;

    length_fast=length&(~(size_t)7);
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

static hpatch_BOOL _TBytesRle_load_stream_mem_add(_TBytesRle_load_stream* loader,
                                                  size_t* _decodeSize,TByte** _out_data){
    size_t  decodeSize=*_decodeSize;
    TByte* out_data=*_out_data;
    TStreamCacheClip* rleCodeClip=&loader->rleCodeClip;
    
    TUInt memSetLength=loader->memSetLength;
    if (memSetLength!=0){
        size_t memSetStep=((memSetLength<=decodeSize)?(size_t)memSetLength:decodeSize);
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
        size_t decodeStep=rleCodeClip->cacheEnd;
        if (decodeStep>loader->memCopyLength)
            decodeStep=(size_t)loader->memCopyLength;
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

static hpatch_BOOL _TBytesRle_load_stream_decode_add(_TBytesRle_load_stream* loader,
                                                     size_t decodeSize,TByte* out_data){
    if (!_TBytesRle_load_stream_mem_add(loader,&decodeSize,&out_data))
        return _hpatch_FALSE;

    while ((decodeSize>0)&&(!_TStreamCacheClip_isFinish(&loader->ctrlClip))){
        enum TByteRleType type;
        TUInt length;
        const TByte* pType=_TStreamCacheClip_accessData(&loader->ctrlClip,1);
        if (pType==0) return _hpatch_FALSE;
        type=(enum TByteRleType)((*pType)>>(8-kByteRleType_bit));
        _TStreamCacheClip_unpackUIntWithTagTo(&length,&loader->ctrlClip,kByteRleType_bit);
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

hpatch_inline
static hpatch_BOOL _TBytesRle_load_stream_decode_skip(_TBytesRle_load_stream* loader,
                                                      size_t decodeSize){
    return _TBytesRle_load_stream_decode_add(loader,decodeSize,0);
}

static  hpatch_BOOL _patch_copy_diff(const hpatch_TStreamOutput* out_newData,
                                     TUInt writeToPos,_TBytesRle_load_stream* rle_loader,
                                     TStreamCacheClip* diff,TUInt copyLength){
    while (copyLength>0){
        const TByte* data;
        size_t decodeStep=diff->cacheEnd;
        if (decodeStep>copyLength)
            decodeStep=(size_t)copyLength;
        data=_TStreamCacheClip_readData(diff,decodeStep);
        if (data==0) return _hpatch_FALSE;
        if (!_TBytesRle_load_stream_decode_skip(rle_loader,decodeStep)) return _hpatch_FALSE;
        if (!out_newData->write(out_newData,writeToPos,data,data+decodeStep))
            return _hpatch_FALSE;
        writeToPos+=decodeStep;
        copyLength-=decodeStep;
    }
    return hpatch_TRUE;
}

static  hpatch_BOOL _patch_add_old(const hpatch_TStreamOutput* out_newData,
                                   TUInt writeToPos,_TBytesRle_load_stream* rle_loader,
                                   const hpatch_TStreamInput* old,TUInt oldPos,
                                   TUInt addLength,TByte* aCache,size_t aCacheSize){
    TByte* data=aCache;
    while (addLength>0){
        size_t decodeStep=aCacheSize;
        if (decodeStep>addLength)
            decodeStep=(size_t)addLength;
        if (!old->read(old,oldPos,data,data+decodeStep)) return _hpatch_FALSE;
        if (!_TBytesRle_load_stream_decode_add(rle_loader,decodeStep,data)) return _hpatch_FALSE;
        if (!out_newData->write(out_newData,writeToPos,data,data+decodeStep)) return _hpatch_FALSE;
        oldPos+=decodeStep;
        writeToPos+=decodeStep;
        addLength-=decodeStep;
    }
    return hpatch_TRUE;
}


typedef struct _TCovers{
    hpatch_TCovers  ICovers;
    TUInt           coverCount;
    TUInt           oldPosBack;
    TUInt           newPosBack;
    TStreamCacheClip*   code_inc_oldPosClip;
    TStreamCacheClip*   code_inc_newPosClip;
    TStreamCacheClip*   code_lengthsClip;
    hpatch_BOOL     isOldPosBackNeedAddLength;
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
    TUInt oldPosBack=self->oldPosBack;
    TUInt newPosBack=self->newPosBack;
    TUInt coverCount=self->coverCount;
    if (coverCount>0)
        self->coverCount=coverCount-1;
    else
        return _hpatch_FALSE;
    
    {
        TUInt copyLength,coverLength, oldPos,inc_oldPos;
        TByte inc_oldPos_sign;
        const TByte* pSign=_TStreamCacheClip_accessData(self->code_inc_oldPosClip,1);
        if (pSign)
            inc_oldPos_sign=(*pSign)>>(8-kSignTagBit);
        else
            return _hpatch_FALSE;
        _TStreamCacheClip_unpackUIntWithTagTo(&inc_oldPos,self->code_inc_oldPosClip,kSignTagBit);
        oldPos=(inc_oldPos_sign==0)?(oldPosBack+inc_oldPos):(oldPosBack-inc_oldPos);
        _TStreamCacheClip_unpackUIntTo(&copyLength,self->code_inc_newPosClip);
        _TStreamCacheClip_unpackUIntTo(&coverLength,self->code_lengthsClip);
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


static void _covers_init(_TCovers* covers,TUInt coverCount,
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


static hpatch_BOOL patchByClip(const hpatch_TStreamOutput* out_newData,
                               const hpatch_TStreamInput*  oldData,
                               hpatch_TCovers* covers,
                               TStreamCacheClip* code_newDataDiffClip,
                               struct _TBytesRle_load_stream* rle_loader,
                               TByte* aCache,size_t aCacheSize){
    const TUInt newDataSize=out_newData->streamSize;
    const TUInt oldDataSize=oldData->streamSize;
    const TUInt coverCount=covers->leave_cover_count(covers);
    TUInt newPosBack=0;
    TUInt i;
    hpatch_BOOL result;
    assert(aCacheSize>=hpatch_kMaxPackedUIntBytes);
    
    for (i=0; i<coverCount; ++i){
        hpatch_TCover cover;
        if(!covers->read_cover(covers,&cover)) return _hpatch_FALSE;
#ifdef __RUN_MEM_SAFE_CHECK
        if (cover.newPos>newDataSize) return _hpatch_FALSE;
        if (cover.length>(TUInt)(newDataSize-cover.newPos)) return _hpatch_FALSE;
        if (cover.oldPos>oldDataSize) return _hpatch_FALSE;
        if (cover.length>(TUInt)(oldDataSize-cover.oldPos)) return _hpatch_FALSE;
        if (cover.newPos<newPosBack) return _hpatch_FALSE;
#endif
        if (cover.newPos>newPosBack){
            if (!_patch_copy_diff(out_newData,newPosBack,rle_loader,
                                  code_newDataDiffClip,cover.newPos-newPosBack)) return _hpatch_FALSE;
            newPosBack=cover.newPos;
        }
        if (!_patch_add_old(out_newData,newPosBack,rle_loader,
                            oldData,cover.oldPos,cover.length,aCache,aCacheSize)) return _hpatch_FALSE;
        newPosBack+=cover.length;
    }
    
    if (newPosBack<newDataSize){
        if (!_patch_copy_diff(out_newData,newPosBack,rle_loader,
                              code_newDataDiffClip,newDataSize-newPosBack)) return _hpatch_FALSE;
        //newPosBack=newDataSize;
    }
    
    if (   _TBytesRle_load_stream_isFinish(rle_loader)
        && covers->is_finish(covers)
        && _TStreamCacheClip_isFinish(code_newDataDiffClip) )
        result=hpatch_TRUE;
    else
        result=_hpatch_FALSE;
    return result;
}


#define _kCacheCount 7

#define _cache_alloc(dst,dst_type,memSize,temp_cache,temp_cache_end){   \
    if ((size_t)(temp_cache_end-temp_cache) <   \
        sizeof(hpatch_StreamPos_t)+(memSize))  return hpatch_FALSE;      \
    (dst)=(dst_type*)_hpatch_align_upper(temp_cache,sizeof(hpatch_StreamPos_t));\
    temp_cache=(TByte*)(dst)+(size_t)(memSize); \
}

typedef struct _TPackedCovers{
    _TCovers            base;
    TStreamCacheClip    code_inc_oldPosClip;
    TStreamCacheClip    code_inc_newPosClip;
    TStreamCacheClip    code_lengthsClip;
} _TPackedCovers;

typedef struct _THDiffHead{
    TUInt coverCount;
    TUInt lengthSize;
    TUInt inc_newPosSize;
    TUInt inc_oldPosSize;
    TUInt newDataDiffSize;
    TUInt headEndPos;
    TUInt coverEndPos;
} _THDiffHead;

static hpatch_BOOL read_diff_head(_THDiffHead* out_diffHead,
                                  const hpatch_TStreamInput* serializedDiff){
    TUInt       diffPos0;
    const TUInt diffPos_end=serializedDiff->streamSize;
    TByte       temp_cache[hpatch_kStreamCacheSize];
    TStreamCacheClip  diffHeadClip;
    _TStreamCacheClip_init(&diffHeadClip,serializedDiff,0,diffPos_end,temp_cache,hpatch_kStreamCacheSize);
    _TStreamCacheClip_unpackUIntTo(&out_diffHead->coverCount,&diffHeadClip);
    _TStreamCacheClip_unpackUIntTo(&out_diffHead->lengthSize,&diffHeadClip);
    _TStreamCacheClip_unpackUIntTo(&out_diffHead->inc_newPosSize,&diffHeadClip);
    _TStreamCacheClip_unpackUIntTo(&out_diffHead->inc_oldPosSize,&diffHeadClip);
    _TStreamCacheClip_unpackUIntTo(&out_diffHead->newDataDiffSize,&diffHeadClip);
    diffPos0=(TUInt)(_TStreamCacheClip_readPosOfSrcStream(&diffHeadClip));
    out_diffHead->headEndPos=diffPos0;
#ifdef __RUN_MEM_SAFE_CHECK
    if (out_diffHead->lengthSize>(TUInt)(diffPos_end-diffPos0)) return _hpatch_FALSE;
#endif
    diffPos0+=out_diffHead->lengthSize;
#ifdef __RUN_MEM_SAFE_CHECK
    if (out_diffHead->inc_newPosSize>(TUInt)(diffPos_end-diffPos0)) return _hpatch_FALSE;
#endif
    diffPos0+=out_diffHead->inc_newPosSize;
#ifdef __RUN_MEM_SAFE_CHECK
    if (out_diffHead->inc_oldPosSize>(TUInt)(diffPos_end-diffPos0)) return _hpatch_FALSE;
#endif
    diffPos0+=out_diffHead->inc_oldPosSize;
    out_diffHead->coverEndPos=diffPos0;
#ifdef __RUN_MEM_SAFE_CHECK
    if (out_diffHead->newDataDiffSize>(TUInt)(diffPos_end-diffPos0)) return _hpatch_FALSE;
#endif
    return hpatch_TRUE;
}

static hpatch_BOOL _packedCovers_open(_TPackedCovers** out_self,
                                      _THDiffHead* out_diffHead,
                                      const hpatch_TStreamInput* serializedDiff,
                                      TByte* temp_cache,TByte* temp_cache_end){
    size_t          cacheSize;
    _TPackedCovers* self=0;
    _cache_alloc(self,_TPackedCovers,sizeof(_TPackedCovers),temp_cache,temp_cache_end);
    cacheSize=(temp_cache_end-temp_cache)/3;
    {
        TUInt       diffPos0;
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
    TUInt       diffPos0;
    const TUInt diffPos_end=serializedDiff->streamSize;
    const size_t cacheSize=(temp_cache_end-temp_cache)/((cached_covers)?(_kCacheCount-3):_kCacheCount);

    assert(out_newData!=0);
    assert(out_newData->write!=0);
    assert(oldData!=0);
    assert(oldData->read!=0);
    assert(serializedDiff!=0);
    assert(serializedDiff->read!=0);
    
    //covers
    if (cached_covers==0){
        struct _TPackedCovers* packedCovers;
        if (!_packedCovers_open(&packedCovers,&diffHead,serializedDiff,temp_cache+cacheSize*(_kCacheCount-3),
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
        TUInt rleCtrlSize;
        TUInt rlePos0;
        TStreamCacheClip*  rleHeadClip=&rle_loader.ctrlClip;//rename, share address
#ifdef __RUN_MEM_SAFE_CHECK
        if (cacheSize<hpatch_kMaxPackedUIntBytes) return _hpatch_FALSE;
#endif
        _TStreamCacheClip_init(rleHeadClip,serializedDiff,diffPos0,diffPos_end,
                               temp_cache+cacheSize*1,hpatch_kMaxPackedUIntBytes);
        _TStreamCacheClip_unpackUIntTo(&rleCtrlSize,rleHeadClip);
        rlePos0=(TUInt)(_TStreamCacheClip_readPosOfSrcStream(rleHeadClip));
#ifdef __RUN_MEM_SAFE_CHECK
        if (rleCtrlSize>(TUInt)(diffPos_end-rlePos0)) return _hpatch_FALSE;
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

//assert(hpatch_kStreamCacheSize>=hpatch_kMaxPluginTypeLength+1);
struct __private_hpatch_check_kMaxCompressTypeLength {
    char _[(hpatch_kStreamCacheSize>=(hpatch_kMaxPluginTypeLength+1))?1:-1];};

hpatch_BOOL _TStreamCacheClip_readType_end(TStreamCacheClip* sclip,TByte endTag,
                                           char out_type[hpatch_kMaxPluginTypeLength+1]){
    const TByte* type_begin;
    size_t i;
    size_t readLen=hpatch_kMaxPluginTypeLength+1;
    if (readLen>_TStreamCacheClip_streamSize(sclip))
        readLen=(size_t)_TStreamCacheClip_streamSize(sclip);
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
    _TStreamCacheClip_unpackUIntTo(&out_diffInfo->newDataSize,diffHeadClip);
    _TStreamCacheClip_unpackUIntTo(&out_diffInfo->oldDataSize,diffHeadClip);
    _TStreamCacheClip_unpackUIntTo(&out_head->coverCount,diffHeadClip);
    out_head->compressSizeBeginPos=_TStreamCacheClip_readPosOfSrcStream(diffHeadClip);
    _TStreamCacheClip_unpackUIntTo(&out_head->cover_buf_size,diffHeadClip);
    _TStreamCacheClip_unpackUIntTo(&out_head->compress_cover_buf_size,diffHeadClip);
    _TStreamCacheClip_unpackUIntTo(&out_head->rle_ctrlBuf_size,diffHeadClip);
    _TStreamCacheClip_unpackUIntTo(&out_head->compress_rle_ctrlBuf_size,diffHeadClip);
    _TStreamCacheClip_unpackUIntTo(&out_head->rle_codeBuf_size,diffHeadClip);
    _TStreamCacheClip_unpackUIntTo(&out_head->compress_rle_codeBuf_size,diffHeadClip);
    _TStreamCacheClip_unpackUIntTo(&out_head->newDataDiff_size,diffHeadClip);
    _TStreamCacheClip_unpackUIntTo(&out_head->compress_newDataDiff_size,diffHeadClip);
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


    static hpatch_BOOL _decompress_read(const hpatch_TStreamInput* stream,
                                        const hpatch_StreamPos_t  readFromPos,
                                        TByte* out_data,TByte* out_data_end){
        _TDecompressInputSteram* self=(_TDecompressInputSteram*)stream->streamImport;
        return self->decompressPlugin->decompress_part(self->decompressHandle,out_data,out_data_end);
    }

    hpatch_BOOL getStreamClip(TStreamCacheClip* out_clip,_TDecompressInputSteram* out_stream,
                              TUInt dataSize,TUInt compressedSize,
                              const hpatch_TStreamInput* stream,TUInt* pCurStreamPos,
                              hpatch_TDecompress* decompressPlugin,TByte* aCache,size_t cacheSize){
        TUInt curStreamPos=*pCurStreamPos;
        if (compressedSize==0){
#ifdef __RUN_MEM_SAFE_CHECK
            if ((TUInt)(curStreamPos+dataSize)<curStreamPos) return _hpatch_FALSE;
            if ((TUInt)(curStreamPos+dataSize)>stream->streamSize) return _hpatch_FALSE;
#endif
            if (out_clip)
                _TStreamCacheClip_init(out_clip,stream,curStreamPos,curStreamPos+dataSize,aCache,cacheSize);
            curStreamPos+=dataSize;
        }else{
#ifdef __RUN_MEM_SAFE_CHECK
            if ((TUInt)(curStreamPos+compressedSize)<curStreamPos) return _hpatch_FALSE;
            if ((TUInt)(curStreamPos+compressedSize)>stream->streamSize) return _hpatch_FALSE;
#endif
            if (out_clip){
                out_stream->IInputSteram.streamImport=out_stream;
                out_stream->IInputSteram.streamSize=dataSize;
                out_stream->IInputSteram.read=_decompress_read;
                out_stream->decompressPlugin=decompressPlugin;
                out_stream->decompressHandle=decompressPlugin->open(decompressPlugin,dataSize,stream,
                                                                    curStreamPos,curStreamPos+compressedSize);
                if (!out_stream->decompressHandle) return _hpatch_FALSE;
                _TStreamCacheClip_init(out_clip,&out_stream->IInputSteram,0,
                                       out_stream->IInputSteram.streamSize,aCache,cacheSize);
            }
            curStreamPos+=compressedSize;
        }
        *pCurStreamPos=curStreamPos;
        return hpatch_TRUE;
    }

#define _clear_return(exitValue) {  result=exitValue; goto clear; }

#define _kCacheDeCount 5

static
hpatch_BOOL _patch_decompress_step(const hpatch_TStreamOutput*  out_newData,
                                   hpatch_TStreamInput*         once_in_newData,
                                   const hpatch_TStreamInput*   oldData,
                                   const hpatch_TStreamInput*   compressedDiff,
                                   hpatch_TDecompress*          decompressPlugin,
                                   hpatch_TCovers*              cached_covers,
                                   TByte*   temp_cache,TByte*  temp_cache_end,
                                   hpatch_BOOL is_copy_step,hpatch_BOOL is_add_rle_step){
    TStreamCacheClip              coverClip;
    TStreamCacheClip              code_newDataDiffClip;
    struct _TBytesRle_load_stream   rle_loader;
    _THDiffzHead                head;
    hpatch_compressedDiffInfo   diffInfo;
    _TDecompressInputSteram     decompressers[4];
    int          i;
    TUInt        coverCount;
    hpatch_BOOL  result=hpatch_TRUE;
    TUInt        diffPos0=0;
    const TUInt  diffPos_end=compressedDiff->streamSize;
    const size_t cacheSize=(temp_cache_end-temp_cache)/_kCacheDeCount;
    
    hpatch_TStreamInput virtual_ctrl;
    TByte virtual_buf[2+hpatch_kMaxPackedUIntBytes];
    TByte* virtual_buf_end=virtual_buf+2+hpatch_kMaxPackedUIntBytes;
    
    if (cacheSize<hpatch_kMaxPluginTypeLength) return _hpatch_FALSE;
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
        
        if ((!is_copy_step)||(!is_add_rle_step)){//request use step
            if (((head.compress_cover_buf_size==0)&&(head.compress_newDataDiff_size==0))
                ||((head.compress_rle_ctrlBuf_size==0)&&(head.compress_rle_codeBuf_size==0))){//not need step
                if (is_copy_step)
                    is_add_rle_step=hpatch_TRUE; //run in once
                else
                    return hpatch_TRUE; //done
            }
        }
    }
    
    for (i=0;i<sizeof(decompressers)/sizeof(_TDecompressInputSteram);++i)
        decompressers[i].decompressHandle=0;
    _TBytesRle_load_stream_init(&rle_loader);
    
    if (cached_covers){
        diffPos0=head.coverEndPos;
        assert(is_copy_step&&is_add_rle_step);
    }else{
        if (!getStreamClip(is_copy_step?(&coverClip):0,&decompressers[0],
                           head.cover_buf_size,head.compress_cover_buf_size,compressedDiff,&diffPos0,
                           decompressPlugin,temp_cache+cacheSize*0,cacheSize)) _clear_return(_hpatch_FALSE);
    }
    if (!getStreamClip(is_add_rle_step?(&rle_loader.ctrlClip):0,&decompressers[1],
                       head.rle_ctrlBuf_size,head.compress_rle_ctrlBuf_size,compressedDiff,&diffPos0,
                       decompressPlugin,temp_cache+cacheSize*1,cacheSize)) _clear_return(_hpatch_FALSE);
    if (!getStreamClip(is_add_rle_step?(&rle_loader.rleCodeClip):0,&decompressers[2],
                       head.rle_codeBuf_size,head.compress_rle_codeBuf_size,compressedDiff,&diffPos0,
                       decompressPlugin,temp_cache+cacheSize*2,cacheSize)) _clear_return(_hpatch_FALSE);
    if (!getStreamClip(is_copy_step?(&code_newDataDiffClip):0,&decompressers[3],
                       head.newDataDiff_size,head.compress_newDataDiff_size,compressedDiff,&diffPos0,
                       decompressPlugin,temp_cache+cacheSize*3,cacheSize)) _clear_return(_hpatch_FALSE);
#ifdef __RUN_MEM_SAFE_CHECK
    if (diffPos0!=diffPos_end) _clear_return(_hpatch_FALSE);
#endif
    
    assert(is_copy_step||is_add_rle_step);
    coverCount=head.coverCount;
    if (!is_add_rle_step){//rle set 0
        TByte* cur_buf=virtual_buf;
        if (diffInfo.newDataSize>0){
            if (!hpatch_packUIntWithTag(&cur_buf,virtual_buf_end,diffInfo.newDataSize-1,kByteRleType_rle0,
                                        kByteRleType_bit)) _clear_return(_hpatch_FALSE); //0's length
        }
        mem_as_hStreamInput(&virtual_ctrl,virtual_buf,cur_buf);
        
        _TStreamCacheClip_init(&rle_loader.ctrlClip,&virtual_ctrl,0,virtual_ctrl.streamSize,
                               temp_cache+cacheSize*1,cacheSize);
        _TStreamCacheClip_init(&rle_loader.rleCodeClip,&virtual_ctrl,0,0,
                               temp_cache+cacheSize*2,cacheSize);
    }
    if (!is_copy_step){ //savedNewData replace oldData
        TByte* cur_buf=virtual_buf;
        if (!hpatch_packUInt(&cur_buf,virtual_buf_end,0)) _clear_return(_hpatch_FALSE); //inc_oldPos
        if (!hpatch_packUInt(&cur_buf,virtual_buf_end,0)) _clear_return(_hpatch_FALSE); //inc_newPos
        if (!hpatch_packUInt(&cur_buf,virtual_buf_end,
                             diffInfo.newDataSize)) _clear_return(_hpatch_FALSE); //old_cover_length
        mem_as_hStreamInput(&virtual_ctrl,virtual_buf,cur_buf);
        
        cached_covers=0;
        _TStreamCacheClip_init(&coverClip,&virtual_ctrl,0,virtual_ctrl.streamSize,temp_cache+cacheSize*0,cacheSize);
        _TStreamCacheClip_init(&code_newDataDiffClip,&virtual_ctrl,0,0,temp_cache+cacheSize*3,cacheSize);
        
        assert(once_in_newData!=0);
        if ((once_in_newData->streamSize!=0)
            && (once_in_newData->streamSize!=diffInfo.newDataSize)) _clear_return(_hpatch_FALSE);
        once_in_newData->streamSize=diffInfo.newDataSize;
        oldData=once_in_newData;
        coverCount=1;
    }
    
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
                           temp_cache+(_kCacheDeCount-1)*cacheSize,cacheSize);
        //if ((pcovers!=cached_covers)&&(!pcovers->close(pcovers))) result=_hpatch_FALSE;
    }
clear:
    for (i=0;i<sizeof(decompressers)/sizeof(_TDecompressInputSteram);++i) {
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
    assert((size_t)(cache_end-cache)==data->streamSize);
    return data->read(data,0,cache,cache_end);
}

typedef struct _TCompressedCovers{
    _TCovers                base;
    TStreamCacheClip        coverClip;
    _TDecompressInputSteram decompresser;
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
    TUInt           diffPos0=0;
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
    size_t          coverCount;
    size_t          cur_index;
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
    size_t i=self->cur_index;
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
    size_t i;
    void*  pCovers;
    _TArrayCovers* self=0;
    size_t coverCount=(size_t)_coverCount;
    
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
static int _arrayCovers_comp_by_old_32(const void* _x, const void *_y){
    _arrayCovers_comp(hpatch_uint32_t,_x,_y,0);
}
static int _arrayCovers_comp_by_old(const void* _x, const void *_y){
    _arrayCovers_comp(hpatch_StreamPos_t,_x,_y,0);
}
static int _arrayCovers_comp_by_new_32(const void* _x, const void *_y){
    _arrayCovers_comp(hpatch_uint32_t,_x,_y,1);
}
static int _arrayCovers_comp_by_new(const void* _x, const void *_y){
    _arrayCovers_comp(hpatch_StreamPos_t,_x,_y,1);
}
static int _arrayCovers_comp_by_len_32(const void* _x, const void *_y){
    _arrayCovers_comp(hpatch_uint32_t,_x,_y,2);
}
static int _arrayCovers_comp_by_len(const void* _x, const void *_y){
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

static hpatch_uint32_t _getMaxCachedLen(const _TArrayCovers* src_covers,
                                        TByte* temp_cache,TByte* temp_cache_end,TByte* cache_buf_end){
    const hpatch_uint32_t kMaxCachedLen  =(1<<27);//允许缓存的最长单个数据长度;
    hpatch_StreamPos_t mlen=0;
    hpatch_StreamPos_t sum=0;
    const size_t coverCount=src_covers->coverCount;
    size_t        i;
    _TArrayCovers cur_covers=*src_covers;
    size_t        cacheSize=temp_cache_end-temp_cache;
    hpatch_StreamPos_t memSize=arrayCovers_memSize(src_covers->coverCount,src_covers->is32);
    _cache_alloc(cur_covers.pCCovers,void,memSize,temp_cache,temp_cache_end); //fail return 0
    memcpy(cur_covers.pCCovers,src_covers->pCCovers,(size_t)memSize);
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
    return (hpatch_uint32_t)mlen;
}

static size_t _set_cache_pos(_TArrayCovers* covers,size_t maxCachedLen,
                             hpatch_StreamPos_t* poldPosBegin,hpatch_StreamPos_t* poldPosEnd){
    const size_t coverCount=covers->coverCount;
    const size_t kMinCacheCoverCount=coverCount/8+1; //控制最小缓存数量,否则缓存的意义太小;
    hpatch_StreamPos_t oldPosBegin=~(hpatch_StreamPos_t)0;
    hpatch_StreamPos_t oldPosEnd=0;
    size_t cacheCoverCount=0;
    size_t sum=0;//result
    size_t i;
    for (i=0; i<coverCount;++i) {
        hpatch_StreamPos_t clen=_arrayCovers_get_len(covers,i);
        if (clen<=maxCachedLen){
            hpatch_StreamPos_t oldPos;
            _arrayCovers_set_cachePos(covers,i,sum);
            sum+=(size_t)clen;
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
                                   _TArrayCovers* arrayCovers,size_t maxCachedLen,size_t sumCacheLen,
                                   TByte* old_cache,TByte* old_cache_end,TByte* cache_buf_end){
    const size_t kMinSpaceLen   =(1<<(20+2));//跳过seekTime*speed长度的空间(SSD可以更小)时间上划得来,否则就顺序访问;
    const size_t kAccessPageSize=(1<<(10+2));//页面对齐访问;
    hpatch_BOOL result=hpatch_TRUE;
    size_t cur_i=0,i;
    const size_t coverCount=arrayCovers->coverCount;
    TByte* cache_buf=old_cache_end;
    assert((size_t)(old_cache_end-old_cache)>=sumCacheLen);
    
    if ((size_t)(cache_buf_end-cache_buf)>=kAccessPageSize*2){
        cache_buf=(TByte*)_hpatch_align_upper(cache_buf,kAccessPageSize);
        if ((size_t)(cache_buf_end-cache_buf)>=(kMinSpaceLen>>1))
            cache_buf_end=cache_buf+(kMinSpaceLen>>1);
        else
            cache_buf_end=(TByte*)_hpatch_align_lower(cache_buf_end,kAccessPageSize);
    }
    oldPos=_hpatch_align_lower(oldPos,kAccessPageSize);
    if (oldPos<kMinSpaceLen) oldPos=0;
    
    _arrayCovers_sort_by_old(arrayCovers);
    while ((oldPos<oldPosAllEnd)&(cur_i<coverCount)) {
        hpatch_StreamPos_t oldPosEnd;
        size_t readLen=(cache_buf_end-cache_buf);
        if (readLen>(oldPosAllEnd-oldPos)) readLen=(size_t)(oldPosAllEnd-oldPos);
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
                    size_t              copyLen;
                    hpatch_StreamPos_t  dstPos=_arrayCovers_get_cachePos(arrayCovers,i);
                    //assert(dstPos<=(size_t)(old_cache_end-old_cache));
                    if (ioldPos>=oldPos){
                //             [ioldPos      ioldPosEnd]----or----]
                        from=ioldPos;
                    }else{
                //  [ioldPos                 ioldPosEnd]----or----]
                        from=oldPos;
                        dstPos+=(oldPos-ioldPos);
                    }
                    copyLen=(size_t)(((ioldPosEnd<=oldPosEnd)?ioldPosEnd:oldPosEnd)-from);
                    //assert(dstPos+copyLen<=(size_t)(old_cache_end-old_cache));
                    //assert(sumCacheLen>=copyLen);
                    memcpy(old_cache+(size_t)dstPos,cache_buf+(from-oldPos),copyLen);
                    sumCacheLen-=copyLen;
                    if ((i==cur_i)&(oldPosEnd>=ioldPosEnd))
                        ++cur_i;
                }else{//后面覆盖线暂时都不会与当前数据有交集了,下一块数据;
                //  [oldPos     oldPosEnd]
                //                        [ioldPos      ioldPosEnd]
                    if ((i==cur_i)&&(ioldPos-oldPosEnd>=kMinSpaceLen))
                        oldPosEnd=_hpatch_align_lower(ioldPos,kAccessPageSize);
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
    hpatch_uint32_t     maxCachedLen;
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
    hpatch_StreamPos_t dataLen=(size_t)(self->readFromPosEnd-self->readFromPos);
    size_t readLen;
    if (dataLen==0){//next cover
        hpatch_StreamPos_t oldPos;
        size_t i=self->arrayCovers.cur_index++;
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
        assert(readLen<=(size_t)(self->cachesEnd-self->caches));
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
    size_t          sumCacheLen;
    hpatch_uint32_t maxCachedLen;
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


hpatch_BOOL _patch_cache(hpatch_TCovers** out_covers,
                         const hpatch_TStreamInput** poldData,hpatch_StreamPos_t newDataSize,
                         const hpatch_TStreamInput*  diffData,hpatch_BOOL isCompressedDiff,
                         hpatch_TDecompress* decompressPlugin,
                         TByte** ptemp_cache,TByte** ptemp_cache_end,hpatch_BOOL* out_isReadError){
    const hpatch_TStreamInput* oldData=*poldData;
    const size_t kMinCacheSize=hpatch_kStreamCacheSize*_kCacheCount;
#if (_IS_NEED_CACHE_OLD_BY_COVERS)
    const size_t kBestACacheSize=1024*64;   //内存足够时比较好的hpatch_kStreamCacheSize值;
    const size_t _minActiveSize=(1<<20)*8;
    const hpatch_StreamPos_t _betterActiveSize=kBestACacheSize*_kCacheCount*2+oldData->streamSize/8;
    const size_t kActiveCacheOldMemorySize = //尝试激活CacheOld功能的内存下限;
                (_betterActiveSize>_minActiveSize)?_minActiveSize:(size_t)_betterActiveSize;
#endif //_IS_NEED_CACHE_OLD_BY_COVERS
    TByte* temp_cache=*ptemp_cache;
    TByte* temp_cache_end=*ptemp_cache_end;
    *out_isReadError=hpatch_FALSE;
    if ((size_t)(temp_cache_end-temp_cache)>=oldData->streamSize+kMinCacheSize
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
    else if ((size_t)(temp_cache_end-temp_cache)>=kActiveCacheOldMemorySize) {
        hpatch_BOOL         isUsedCover32;
        TByte*              temp_cache_end_back=temp_cache_end;
        _TArrayCovers*      arrayCovers=0;
        assert((size_t)(temp_cache_end-temp_cache)>kBestACacheSize*_kCacheCount);
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
            old_cache_end=temp_cache_end-kBestACacheSize*_kCacheCount;
            // [       arrayCovers cache           |        ...        |      patch reserve cache       ]
            // [                                   |        ...        | (kBestACacheSize*_kCacheCount) ]
            if (((size_t)(temp_cache_end-temp_cache)<=kBestACacheSize*_kCacheCount)
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
                assert((size_t)(temp_cache_end-old_cache_end)>=kBestACacheSize*_kCacheCount);
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
                 &temp_cache,&temp_cache_end,&isReadError);
    if (isReadError) return _hpatch_FALSE;
    result=_patch_stream_with_cache(out_newData,oldData,serializedDiff,covers,
                                    temp_cache,temp_cache_end);
    //if ((covers!=0)&&(!covers->close(covers))) result=_hpatch_FALSE;
    return result;
}

hpatch_BOOL patch_stream(const hpatch_TStreamOutput* out_newData,
                         const hpatch_TStreamInput*  oldData,
                         const hpatch_TStreamInput*  serializedDiff){
    TByte temp_cache[hpatch_kStreamCacheSize*_kCacheCount];
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
                 decompressPlugin,&temp_cache,&temp_cache_end,&isReadError);
    if (isReadError) return _hpatch_FALSE;
    result=_patch_decompress_step(out_newData,0,oldData,compressedDiff,decompressPlugin,
                                  covers,temp_cache,temp_cache_end,hpatch_TRUE,hpatch_TRUE);
    if ((covers!=0)&&(!covers->close(covers))) result=_hpatch_FALSE;
    return result;
}

hpatch_BOOL patch_decompress(const hpatch_TStreamOutput* out_newData,
                             const hpatch_TStreamInput*  oldData,
                             const hpatch_TStreamInput*  compressedDiff,
                             hpatch_TDecompress* decompressPlugin){
    TByte temp_cache[hpatch_kStreamCacheSize*_kCacheDeCount];
    return _patch_decompress_step(out_newData,0,oldData,compressedDiff,decompressPlugin,
                                  0,temp_cache,temp_cache+sizeof(temp_cache)/sizeof(TByte),
                                  hpatch_TRUE,hpatch_TRUE);
}

hpatch_BOOL patch_decompress_repeat_out(const hpatch_TStreamOutput* repeat_out_newData,
                                        hpatch_TStreamInput*        in_newData,
                                        const hpatch_TStreamInput*  oldData,
                                        const hpatch_TStreamInput*  compressedDiff,
                                        hpatch_TDecompress*         decompressPlugin){
    TByte  temp_cache[hpatch_kStreamCacheSize*_kCacheDeCount];
    TByte* temp_cache_end=temp_cache+sizeof(temp_cache)/sizeof(TByte);
    if (!_patch_decompress_step(repeat_out_newData,0,oldData,compressedDiff,decompressPlugin,
                                0,temp_cache,temp_cache_end,hpatch_TRUE,hpatch_FALSE)) return _hpatch_FALSE;
    return _patch_decompress_step(repeat_out_newData,in_newData,oldData,compressedDiff,decompressPlugin,
                                  0,temp_cache,temp_cache_end,hpatch_FALSE,hpatch_TRUE);
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

