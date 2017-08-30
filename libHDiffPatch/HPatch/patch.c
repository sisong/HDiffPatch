//patch.c
//
/*
 The MIT License (MIT)
 Copyright (c) 2012-2017 HouSisong

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
#include <assert.h> //assert

//__RUN_MEM_SAFE_CHECK用来启动内存访问越界检查,用以防御可能被意外或故意损坏的数据.
#define __RUN_MEM_SAFE_CHECK

#define _hpatch_FALSE   hpatch_FALSE
//int __debug_check_false_x=0; //for debug
//#define _hpatch_FALSE (1/__debug_check_false_x)

const int kSignTagBit=1;
typedef unsigned char TByte;
#define TUInt size_t

    static long _read_mem_stream(hpatch_TStreamInputHandle streamHandle,
                                 const hpatch_StreamPos_t readFromPos,
                                 unsigned char* out_data,unsigned char* out_data_end){
        const unsigned char* src=(const unsigned char*)streamHandle;
        memcpy(out_data,src+readFromPos,out_data_end-out_data);
        return (long)(out_data_end-out_data);
    }
void mem_as_hStreamInput(hpatch_TStreamInput* out_stream,
                         const unsigned char* mem,const unsigned char* mem_end){
    out_stream->streamHandle=(hpatch_TStreamInputHandle)mem;
    out_stream->streamSize=mem_end-mem;
    out_stream->read=_read_mem_stream;
}

    static long _write_mem_stream(hpatch_TStreamOutputHandle streamHandle,
                                  const hpatch_StreamPos_t writeToPos,
                                  const unsigned char* data,const unsigned char* data_end){
        unsigned char* out_dst=(unsigned char*)streamHandle;
        memcpy(out_dst+writeToPos,data,data_end-data);
        return (long)(data_end-data);
    }
void mem_as_hStreamOutput(hpatch_TStreamOutput* out_stream,
                          unsigned char* mem,unsigned char* mem_end){
    out_stream->streamHandle=(hpatch_TStreamOutputHandle)mem;
    out_stream->streamSize=mem_end-mem;
    out_stream->write=_write_mem_stream;
}



static hpatch_BOOL _bytesRle_load(TByte* out_data,TByte* out_dataEnd,
                                  const TByte* rle_code,const TByte* rle_code_end);
static void addData(TByte* dst,const TByte* src,TUInt length);
hpatch_inline
static hpatch_BOOL _unpackUIntWithTag(const TByte** src_code,const TByte* src_code_end,
                                      TUInt* result,const unsigned int kTagBit){
    if (sizeof(TUInt)==sizeof(hpatch_StreamPos_t)){
        return hpatch_unpackUIntWithTag(src_code,src_code_end,(hpatch_StreamPos_t*)result,kTagBit);
    }else{
        hpatch_StreamPos_t u64;
        hpatch_BOOL rt=hpatch_unpackUIntWithTag(src_code,src_code_end,&u64,kTagBit);
#ifdef __RUN_MEM_SAFE_CHECK
        if (rt){
            TUInt u=(TUInt)u64;
            if (u==u64){
                *result=u;
                return hpatch_TRUE;
            }else{
                return _hpatch_FALSE;
            }
        }else{
            return _hpatch_FALSE;
        }
#else
        *result=(TUInt)u64;
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

    //decode rle ; rle data begin==serializedDiff;
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
                                   hpatch_StreamPos_t uValue,int highTag,const int kTagBit){//写入整数并前进指针.
    TByte*          pcode=*out_code;
    const TUInt     kMaxValueWithTag=(1<<(7-kTagBit))-1;
    unsigned char   codeBuf[hpatch_kMaxPackedUIntBytes];
    unsigned char*  codeEnd=codeBuf;
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
    *pcode=(TByte)( (highTag<<(8-kTagBit)) | uValue
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

unsigned int hpatch_packUIntWithTag_size(hpatch_StreamPos_t uValue,const int kTagBit){
    const TUInt     kMaxValueWithTag=(1<<(7-kTagBit))-1;
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

static void addData(TByte* dst,const TByte* src,TUInt length){
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

typedef  struct TStreamClip{
    TUInt       streamPos;
    TUInt       streamPos_end;
    const struct hpatch_TStreamInput*  srcStream;
    size_t      cacheBegin;
    size_t      cacheEnd;
    TByte*      cacheBuf;
} TStreamClip;

hpatch_inline
static void _TStreamClip_init(struct TStreamClip* sclip,
                              const struct hpatch_TStreamInput* srcStream,
                              TUInt streamPos,TUInt streamPos_end,
                              TByte* aCache,size_t cacheSize){
    sclip->srcStream=srcStream;
    sclip->streamPos=streamPos;
    sclip->streamPos_end=streamPos_end;
    sclip->cacheBegin=cacheSize;
    sclip->cacheEnd=cacheSize;
    sclip->cacheBuf=aCache;
}
#define _TStreamClip_isFinish(sclip)     ( 0==_TStreamClip_streamSize(sclip) )
#define _TStreamClip_isCacheEmpty(sclip) ( (sclip)->cacheBegin==(sclip)->cacheEnd )
#define _TStreamClip_cachedSize(sclip)   ( (size_t)((sclip)->cacheEnd-(sclip)->cacheBegin) )
#define _TStreamClip_streamSize(sclip)   \
    (  (TUInt)((sclip)->streamPos_end-(sclip)->streamPos)  \
     + (TUInt)_TStreamClip_cachedSize(sclip)  )

static hpatch_BOOL _TStreamClip_updateCache(struct TStreamClip* sclip){
    TByte* buf0=&sclip->cacheBuf[0];
    const TUInt streamSize=(TUInt)(sclip->streamPos_end-sclip->streamPos);
    size_t readSize=sclip->cacheBegin;
    if (readSize>streamSize)
        readSize=(size_t)streamSize;
    if (readSize==0) return hpatch_TRUE;
    if (!_TStreamClip_isCacheEmpty(sclip)){
        memmove(buf0+(size_t)(sclip->cacheBegin-readSize),
                buf0+sclip->cacheBegin,_TStreamClip_cachedSize(sclip));
    }
    if (sclip->srcStream->read(sclip->srcStream->streamHandle,sclip->streamPos,
                               buf0+(sclip->cacheEnd-readSize),buf0+sclip->cacheEnd)
                           == (long)readSize ){
        sclip->cacheBegin-=readSize;
        sclip->streamPos+=readSize;
        return hpatch_TRUE;
    }else{ //read error
        return _hpatch_FALSE;
    }
}

hpatch_inline  //error return 0
static TByte* _TStreamClip_accessData(struct TStreamClip* sclip,size_t readSize){
    //assert(readSize<=sclip->cacheEnd);
    if (readSize>_TStreamClip_cachedSize(sclip)){
        if (!_TStreamClip_updateCache(sclip)) return 0;
        if (readSize>_TStreamClip_cachedSize(sclip)) return 0;
    }
    return &sclip->cacheBuf[sclip->cacheBegin];
}

#define _TStreamClip_skipData_noCheck(sclip,skipSize) ((sclip)->cacheBegin+=skipSize)

hpatch_inline  //error return 0
static TByte* _TStreamClip_readData(struct TStreamClip* sclip,size_t readSize){
    TByte* result=_TStreamClip_accessData(sclip,readSize);
    _TStreamClip_skipData_noCheck(sclip,readSize);
    return result;
}

static void _TStreamClip_resetPosEnd(struct TStreamClip* sclip,TUInt new_posEnd){
    sclip->streamPos_end=new_posEnd;
    if (sclip->streamPos>new_posEnd){//cache overfull, need pop
        TByte* cacheBegin=&sclip->cacheBuf[sclip->cacheBegin];
        TUInt popSize=(TUInt)(sclip->streamPos-new_posEnd);
        assert(popSize<=_TStreamClip_cachedSize(sclip));
        memmove(cacheBegin+popSize,cacheBegin,(size_t)(_TStreamClip_cachedSize(sclip)-popSize));
        sclip->cacheBegin+=(size_t)popSize;
        sclip->streamPos=new_posEnd; //eq. sclip->streamPos-=popSize
    }
}

//assert(hpatch_kStreamCacheSize>=hpatch_kMaxPackedUIntBytes);
struct __private_hpatch_check_hpatch_kMaxPackedUIntBytes {
    char _[hpatch_kStreamCacheSize-hpatch_kMaxPackedUIntBytes]; };

static hpatch_BOOL _TStreamClip_unpackUIntWithTag(struct TStreamClip* sclip,
                                                  TUInt* result,const int kTagBit){
    TByte* curCode,*codeBegin;
    size_t readSize=hpatch_kMaxPackedUIntBytes;
    const TUInt dataSize=_TStreamClip_streamSize(sclip);
    if (readSize>dataSize)
        readSize=(size_t)dataSize;
    codeBegin=_TStreamClip_accessData(sclip,readSize);
    if (codeBegin==0) return _hpatch_FALSE;
    curCode=codeBegin;
    if (!hpatch_unpackUIntWithTag((const TByte**)&curCode,codeBegin+readSize,result,kTagBit))
        return _hpatch_FALSE;
    _TStreamClip_skipData_noCheck(sclip,(size_t)(curCode-codeBegin));
    return hpatch_TRUE;
}
#define _TStreamClip_unpackUIntWithTagTo(puint,sclip,kTagBit) \
    { if (!_TStreamClip_unpackUIntWithTag(sclip,puint,kTagBit)) return _hpatch_FALSE; }
#define _TStreamClip_unpackUIntTo(puint,sclip) \
    _TStreamClip_unpackUIntWithTagTo(puint,sclip,0)


typedef struct _TBytesRle_load_stream{
    TUInt                               memCopyLength;
    TUInt                               memSetLength;
    TByte                               memSetValue;
    struct TStreamClip                  ctrlClip;
    struct TStreamClip                  rleCodeClip;
} _TBytesRle_load_stream;

hpatch_inline
static void _TBytesRle_load_stream_init(_TBytesRle_load_stream* loader){
    loader->memSetLength=0;
    loader->memSetValue=0;//nil;
    loader->memCopyLength=0;
    _TStreamClip_init(&loader->ctrlClip,0,0,0,0,0);
    _TStreamClip_init(&loader->rleCodeClip,0,0,0,0,0);
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
    struct TStreamClip* rleCodeClip=&loader->rleCodeClip;
    
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
        rleData=_TStreamClip_readData(rleCodeClip,decodeStep);
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
        &&(_TStreamClip_isFinish(&loader->rleCodeClip))
        &&(_TStreamClip_isFinish(&loader->ctrlClip));
}

static hpatch_BOOL _TBytesRle_load_stream_decode_add(_TBytesRle_load_stream* loader,
                                                     size_t decodeSize,TByte* out_data){
    if (!_TBytesRle_load_stream_mem_add(loader,&decodeSize,&out_data))
        return _hpatch_FALSE;

    while ((decodeSize>0)&&(!_TStreamClip_isFinish(&loader->ctrlClip))){
        enum TByteRleType type;
        TUInt length;
        const TByte* pType=_TStreamClip_accessData(&loader->ctrlClip,1);
        if (pType==0) return _hpatch_FALSE;
        type=(enum TByteRleType)((*pType)>>(8-kByteRleType_bit));
        _TStreamClip_unpackUIntWithTagTo(&length,&loader->ctrlClip,kByteRleType_bit);
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
                const TByte* pSetValue=_TStreamClip_readData(&loader->rleCodeClip,1);
                if (pSetValue==0) return _hpatch_FALSE;
                loader->memSetValue=*pSetValue;
                loader->memSetLength=length;
            }break;
            case kByteRleType_unrle:{
#ifdef __RUN_MEM_SAFE_CHECK
                if (length>_TStreamClip_streamSize(&loader->rleCodeClip)) return _hpatch_FALSE;
#endif
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

static  hpatch_BOOL _patch_copy_diff(const struct hpatch_TStreamOutput* out_newData,
                                     TUInt writeToPos,_TBytesRle_load_stream* rle_loader,
                                     struct TStreamClip* diff,TUInt copyLength){
    while (copyLength>0){
        const TByte* data;
        size_t decodeStep=diff->cacheEnd;
        if (decodeStep>copyLength)
            decodeStep=(size_t)copyLength;
        data=_TStreamClip_readData(diff,decodeStep);
        if (data==0) return _hpatch_FALSE;
        if (!_TBytesRle_load_stream_decode_skip(rle_loader,decodeStep)) return _hpatch_FALSE;
        if ((long)decodeStep!=out_newData->write(out_newData->streamHandle,writeToPos,data,data+decodeStep))
            return _hpatch_FALSE;
        writeToPos+=decodeStep;
        copyLength-=decodeStep;
    }
    return hpatch_TRUE;
}

static  hpatch_BOOL _patch_add_old(const struct hpatch_TStreamOutput* out_newData,
                                   TUInt writeToPos,_TBytesRle_load_stream* rle_loader,
                                   const struct hpatch_TStreamInput* old,TUInt oldPos,
                                   TUInt addLength,TByte* aCache){
    const size_t data_size=rle_loader->rleCodeClip.cacheEnd;
    TByte* data=aCache;
    while (addLength>0){
        size_t decodeStep=data_size;
        if (decodeStep>addLength)
            decodeStep=(size_t)addLength;
        if ((long)decodeStep!=old->read(old->streamHandle,oldPos,data,data+decodeStep)) return _hpatch_FALSE;
        if (!_TBytesRle_load_stream_decode_add(rle_loader,decodeStep,data)) return _hpatch_FALSE;
        if ((long)decodeStep!=out_newData->write(out_newData->streamHandle,writeToPos,data,data+decodeStep))
            return _hpatch_FALSE;
        oldPos+=decodeStep;
        writeToPos+=decodeStep;
        addLength-=decodeStep;
    }
    return hpatch_TRUE;
}


typedef struct hpatch_TCovers{
    hpatch_StreamPos_t      coverCount;
    TUInt                   oldPosBack;
    TUInt                   newPosBack;
    TStreamClip*            code_inc_oldPosClip;
    TStreamClip*            code_inc_newPosClip;
    TStreamClip*            code_lengthsClip;
    hpatch_BOOL             isOldPosBackNeedAddLength;
} hpatch_TCovers;

hpatch_StreamPos_t covers_leaveCoverCount(const hpatch_TCovers* self){
    return self->coverCount;
}

hpatch_BOOL covers_read_cover(hpatch_TCovers* self,hpatch_TCover* out_cover){
    TUInt oldPosBack=self->oldPosBack;
    TUInt newPosBack=self->newPosBack;
    if (self->coverCount<=0) return _hpatch_FALSE;
    --self->coverCount;
    {
        TUInt copyLength,addLength, oldPos,inc_oldPos;
        TByte inc_oldPos_sign;
        const TByte* pSign;
        
        pSign=_TStreamClip_accessData(self->code_inc_oldPosClip,1);
        if (pSign==0) return _hpatch_FALSE;
        inc_oldPos_sign=(*pSign)>>(8-kSignTagBit);
        _TStreamClip_unpackUIntWithTagTo(&inc_oldPos,self->code_inc_oldPosClip,kSignTagBit);
        if (inc_oldPos_sign==0)
            oldPos=oldPosBack+inc_oldPos;
        else
            oldPos=oldPosBack-inc_oldPos;
        _TStreamClip_unpackUIntTo(&copyLength,self->code_inc_newPosClip);
        _TStreamClip_unpackUIntTo(&addLength,self->code_lengthsClip);
        newPosBack+=copyLength;
        oldPosBack=oldPos;
        if (self->isOldPosBackNeedAddLength)
            oldPosBack+=addLength;
        newPosBack+=addLength;
        
        out_cover->oldPos=oldPos;
        out_cover->newPos=newPosBack-addLength;
        out_cover->length=addLength;
    }
    self->oldPosBack=oldPosBack;
    self->newPosBack=newPosBack;
    return hpatch_TRUE;
}

static hpatch_BOOL patchByClip(const struct hpatch_TStreamOutput* out_newData,
                               const struct hpatch_TStreamInput*  oldData,
                               hpatch_TCovers* covers,
                               struct TStreamClip* code_newDataDiffClip,
                               struct _TBytesRle_load_stream* rle_loader,
                               TByte* aCache,int isOldPosBackNeedAddLength){
    const TUInt newDataSize=out_newData->streamSize;
    const TUInt oldDataSize=oldData->streamSize;
    const TUInt coverCount=covers_leaveCoverCount(covers);
    TUInt newPosBack=0;
    TUInt i;
    if (code_newDataDiffClip->cacheEnd<hpatch_kMaxPackedUIntBytes)
        return _hpatch_FALSE;
    for (i=0; i<coverCount; ++i){
        hpatch_TCover cover;
        if(!covers_read_cover(covers,&cover)) return _hpatch_FALSE;
#ifdef __RUN_MEM_SAFE_CHECK
        if (cover.newPos<newPosBack) return _hpatch_FALSE;
#endif
        if (cover.newPos>newPosBack){
#ifdef __RUN_MEM_SAFE_CHECK
            if (cover.newPos>newDataSize) return _hpatch_FALSE;
            if (cover.length>newDataSize-cover.newPos) return _hpatch_FALSE;
            if (cover.newPos-newPosBack>_TStreamClip_streamSize(code_newDataDiffClip))  return _hpatch_FALSE;
#endif
            if (!_patch_copy_diff(out_newData,newPosBack,rle_loader,
                                  code_newDataDiffClip,cover.newPos-newPosBack)) return _hpatch_FALSE;
            newPosBack=cover.newPos;
        }
#ifdef __RUN_MEM_SAFE_CHECK
        if (cover.oldPos>oldDataSize) return _hpatch_FALSE;
        if (cover.length>oldDataSize-cover.oldPos) return _hpatch_FALSE;
#endif
        if (!_patch_add_old(out_newData,newPosBack,rle_loader,
                            oldData,cover.oldPos,cover.length,aCache)) return _hpatch_FALSE;
        newPosBack+=cover.length;
    }
    
    if (newPosBack<newDataSize){
#ifdef __RUN_MEM_SAFE_CHECK
        if (newDataSize-newPosBack>_TStreamClip_streamSize(code_newDataDiffClip))  return _hpatch_FALSE;
#endif
        if (!_patch_copy_diff(out_newData,newPosBack,rle_loader,
                              code_newDataDiffClip,newDataSize-newPosBack)) return _hpatch_FALSE;
        //newPosBack=newDataSize;
    }
    
    if (   _TBytesRle_load_stream_isFinish(rle_loader)
        && _TStreamClip_isFinish(covers->code_lengthsClip)
        && _TStreamClip_isFinish(covers->code_inc_newPosClip)
        && _TStreamClip_isFinish(covers->code_inc_oldPosClip)
        && _TStreamClip_isFinish(code_newDataDiffClip) )
        return hpatch_TRUE;
    else
        return _hpatch_FALSE;
}

#define _kCacheCount 7

hpatch_BOOL patch_stream_with_cache(const struct hpatch_TStreamOutput* out_newData,
                                    const struct hpatch_TStreamInput*  oldData,
                                    const struct hpatch_TStreamInput*  serializedDiff,
                                    TByte*   temp_cache,TByte* temp_cache_end){
    struct TStreamClip              code_inc_oldPosClip;
    struct TStreamClip              code_inc_newPosClip;
    struct TStreamClip              code_lengthsClip;
    struct TStreamClip              code_newDataDiffClip;
    struct _TBytesRle_load_stream   rle_loader;
    TUInt                           coverCount;
    const size_t cacheSize=(temp_cache_end-temp_cache)/_kCacheCount;

    assert(out_newData!=0);
    assert(out_newData->write!=0);
    assert(oldData!=0);
    assert(oldData->read!=0);
    assert(serializedDiff!=0);
    assert(serializedDiff->read!=0);
    
    {   //head
        TUInt lengthSize,inc_newPosSize,inc_oldPosSize,newDataDiffSize;
        TUInt diffPos0;
        const TUInt diffPos_end=serializedDiff->streamSize;
        struct TStreamClip*  diffHeadClip=&code_lengthsClip;//rename, share address
        _TStreamClip_init(diffHeadClip,serializedDiff,0,diffPos_end,temp_cache+cacheSize*0,cacheSize);
        _TStreamClip_unpackUIntTo(&coverCount,diffHeadClip);
        _TStreamClip_unpackUIntTo(&lengthSize,diffHeadClip);
        _TStreamClip_unpackUIntTo(&inc_newPosSize,diffHeadClip);
        _TStreamClip_unpackUIntTo(&inc_oldPosSize,diffHeadClip);
        _TStreamClip_unpackUIntTo(&newDataDiffSize,diffHeadClip);
        diffPos0=(TUInt)(diffPos_end-_TStreamClip_streamSize(diffHeadClip));
#ifdef __RUN_MEM_SAFE_CHECK
        if (lengthSize>(TUInt)(diffPos_end-diffPos0)) return _hpatch_FALSE;
#endif
        //eq. _TStreamClip_init(&code_lengthsClip,serializedDiff,diffPos0,diffPos0+lengthSize,temp_cache,cacheSize);
        _TStreamClip_resetPosEnd(&code_lengthsClip,diffPos0+lengthSize);
        diffPos0+=lengthSize;
#ifdef __RUN_MEM_SAFE_CHECK
        if (inc_newPosSize>(TUInt)(diffPos_end-diffPos0)) return _hpatch_FALSE;
#endif
        _TStreamClip_init(&code_inc_newPosClip,serializedDiff,diffPos0,diffPos0+inc_newPosSize,
                          temp_cache+cacheSize*1,cacheSize);
        diffPos0+=inc_newPosSize;
#ifdef __RUN_MEM_SAFE_CHECK
        if (inc_oldPosSize>(TUInt)(diffPos_end-diffPos0)) return _hpatch_FALSE;
#endif
        _TStreamClip_init(&code_inc_oldPosClip,serializedDiff,diffPos0,diffPos0+inc_oldPosSize,
                          temp_cache+cacheSize*2,cacheSize);
        diffPos0+=inc_oldPosSize;
#ifdef __RUN_MEM_SAFE_CHECK
        if (newDataDiffSize>(TUInt)(diffPos_end-diffPos0)) return _hpatch_FALSE;
#endif
        _TStreamClip_init(&code_newDataDiffClip,serializedDiff,diffPos0,diffPos0+newDataDiffSize,
                          temp_cache+cacheSize*3,cacheSize);
        diffPos0+=newDataDiffSize;
        
        {//rle
            TUInt rleCtrlSize;
            TUInt rlePos0;
            struct TStreamClip*  rleHeadClip=&rle_loader.ctrlClip;//rename, share address
            _TStreamClip_init(rleHeadClip,serializedDiff,diffPos0,diffPos_end,
                              temp_cache+cacheSize*4,cacheSize);
            _TStreamClip_unpackUIntTo(&rleCtrlSize,rleHeadClip);
            rlePos0=(TUInt)(diffPos_end-_TStreamClip_streamSize(rleHeadClip));
#ifdef __RUN_MEM_SAFE_CHECK
            if (rleCtrlSize>(TUInt)(diffPos_end-rlePos0)) return _hpatch_FALSE;
#endif
            _TBytesRle_load_stream_init(&rle_loader);
            _TStreamClip_init(&rle_loader.ctrlClip,serializedDiff,rlePos0,rlePos0+rleCtrlSize,
                              temp_cache+cacheSize*4,cacheSize);
            _TStreamClip_init(&rle_loader.rleCodeClip,serializedDiff,rlePos0+rleCtrlSize,diffPos_end,
                              temp_cache+cacheSize*5,cacheSize);
        }
    }
    
    {
        hpatch_TCovers covers;
        covers.coverCount=coverCount;
        covers.newPosBack=0;
        covers.oldPosBack=0;
        covers.code_inc_oldPosClip=&code_inc_oldPosClip;
        covers.code_inc_newPosClip=&code_inc_newPosClip;
        covers.code_lengthsClip=&code_lengthsClip;
        covers.isOldPosBackNeedAddLength=hpatch_FALSE;
        return patchByClip(out_newData,oldData,&covers,
                           &code_newDataDiffClip,&rle_loader,
                           temp_cache+(_kCacheCount-1)*cacheSize,hpatch_FALSE);
    }
}

hpatch_BOOL patch_stream(const hpatch_TStreamOutput* out_newData,
                         const hpatch_TStreamInput*  oldData,
                         const hpatch_TStreamInput*  serializedDiff){
    unsigned char temp_cache[hpatch_kStreamCacheSize*_kCacheCount];
    return patch_stream_with_cache(out_newData,oldData,serializedDiff,
                                   temp_cache,temp_cache+sizeof(temp_cache)/sizeof(unsigned char));
}



#define kVersionTypeLen 8

typedef struct _THDiffzHead{
    TUInt coverCount;
    
    TUInt cover_buf_size;
    TUInt compress_cover_buf_size;
    TUInt rle_ctrlBuf_size;
    TUInt compress_rle_ctrlBuf_size;
    TUInt rle_codeBuf_size;
    TUInt compress_rle_codeBuf_size;
    TUInt newDataDiff_size;
    TUInt compress_newDataDiff_size;
} _THDiffzHead;


//assert(hpatch_kStreamCacheSize>=hpatch_kMaxCompressTypeLength+1);
struct __private_hpatch_check_kMaxCompressTypeLength {
    char _[hpatch_kStreamCacheSize-(hpatch_kMaxCompressTypeLength+1)];};

static hpatch_BOOL read_diffz_head(hpatch_compressedDiffInfo* out_diffInfo,
                                   _THDiffzHead* out_head,TStreamClip* diffHeadClip){
    const TByte* versionType=_TStreamClip_readData(diffHeadClip,kVersionTypeLen);
    if (versionType==0) return _hpatch_FALSE;
    if (0!=memcmp(versionType,"HDIFF13&",kVersionTypeLen)) return _hpatch_FALSE;
    
    {//read compressType
        const TByte* compressType;
        size_t       compressTypeLen;
        size_t readLen=hpatch_kMaxCompressTypeLength+1;
        if (readLen>_TStreamClip_streamSize(diffHeadClip))
            readLen=(size_t)_TStreamClip_streamSize(diffHeadClip);
        compressType=_TStreamClip_accessData(diffHeadClip,readLen);
        if (compressType==0) return _hpatch_FALSE;
        compressTypeLen=strnlen((const char*)compressType,readLen);
        if (compressTypeLen>=readLen) return _hpatch_FALSE;
        memcpy(out_diffInfo->compressType,compressType,compressTypeLen+1);
        _TStreamClip_skipData_noCheck(diffHeadClip,compressTypeLen+1);
    }
    
    _TStreamClip_unpackUIntTo(&out_diffInfo->newDataSize,diffHeadClip);
    _TStreamClip_unpackUIntTo(&out_diffInfo->oldDataSize,diffHeadClip);
    _TStreamClip_unpackUIntTo(&out_head->coverCount,diffHeadClip);
    
    _TStreamClip_unpackUIntTo(&out_head->cover_buf_size,diffHeadClip);
    _TStreamClip_unpackUIntTo(&out_head->compress_cover_buf_size,diffHeadClip);
    _TStreamClip_unpackUIntTo(&out_head->rle_ctrlBuf_size,diffHeadClip);
    _TStreamClip_unpackUIntTo(&out_head->compress_rle_ctrlBuf_size,diffHeadClip);
    _TStreamClip_unpackUIntTo(&out_head->rle_codeBuf_size,diffHeadClip);
    _TStreamClip_unpackUIntTo(&out_head->compress_rle_codeBuf_size,diffHeadClip);
    _TStreamClip_unpackUIntTo(&out_head->newDataDiff_size,diffHeadClip);
    _TStreamClip_unpackUIntTo(&out_head->compress_newDataDiff_size,diffHeadClip);
    
    out_diffInfo->compressedCount=((out_head->compress_cover_buf_size)?1:0)
                                 +((out_head->compress_rle_ctrlBuf_size)?1:0)
                                 +((out_head->compress_rle_codeBuf_size)?1:0)
                                 +((out_head->compress_newDataDiff_size)?1:0);
    return hpatch_TRUE;
}

hpatch_BOOL getCompressedDiffInfo(hpatch_compressedDiffInfo* out_diffInfo,
                                  const hpatch_TStreamInput* compressedDiff){
    TStreamClip  diffHeadClip;
    _THDiffzHead head;
    TByte aCache[hpatch_kStreamCacheSize];
    const size_t cacheSize=hpatch_kStreamCacheSize;
    assert(out_diffInfo!=0);
    assert(compressedDiff!=0);
    assert(compressedDiff->read!=0);
    
    _TStreamClip_init(&diffHeadClip,compressedDiff,0,compressedDiff->streamSize,aCache,cacheSize);
    if (!read_diffz_head(out_diffInfo,&head,&diffHeadClip)) return _hpatch_FALSE;
    return hpatch_TRUE;
}


    typedef struct _TDecompressInputSteram{
        hpatch_TStreamInput         base;
        hpatch_TDecompress*         decompressPlugin;
        hpatch_decompressHandle     decompressHandle;
    } _TDecompressInputSteram;
    static long _decompress_read(hpatch_TStreamInputHandle streamHandle,
                                 const hpatch_StreamPos_t  readFromPos,
                                 unsigned char* out_data,unsigned char* out_data_end){
        _TDecompressInputSteram* self=(_TDecompressInputSteram*)streamHandle;
        return self->decompressPlugin->decompress_part(self->decompressPlugin,
                                                       self->decompressHandle,
                                                       out_data,out_data_end);
    }


    static hpatch_BOOL getStreamClip(TStreamClip* out_clip,_TDecompressInputSteram* out_stream,
                                     TUInt dataSize,TUInt compressedSize,
                                     const hpatch_TStreamInput* stream,TUInt* pCurStreamPos,
                                     hpatch_TDecompress* decompressPlugin,
                                     TByte* aCache,size_t cacheSize){
        TUInt curStreamPos=*pCurStreamPos;
        if (compressedSize==0){
#ifdef __RUN_MEM_SAFE_CHECK
            if ((TUInt)(curStreamPos+dataSize)<curStreamPos) return _hpatch_FALSE;
            if ((TUInt)(curStreamPos+dataSize)>stream->streamSize) return _hpatch_FALSE;
#endif
            if (out_clip)
                _TStreamClip_init(out_clip,stream,curStreamPos,curStreamPos+dataSize,aCache,cacheSize);
            curStreamPos+=dataSize;
        }else{
#ifdef __RUN_MEM_SAFE_CHECK
            if ((TUInt)(curStreamPos+compressedSize)<curStreamPos) return _hpatch_FALSE;
            if ((TUInt)(curStreamPos+compressedSize)>stream->streamSize) return _hpatch_FALSE;
#endif
            if (out_clip){
                out_stream->base.streamHandle=out_stream;
                out_stream->base.streamSize=dataSize;
                out_stream->base.read=_decompress_read;
                out_stream->decompressPlugin=decompressPlugin;
                out_stream->decompressHandle=decompressPlugin->open(decompressPlugin,dataSize,stream,
                                                                    curStreamPos,curStreamPos+compressedSize);
                if (!out_stream->decompressHandle) return _hpatch_FALSE;
                _TStreamClip_init(out_clip,&out_stream->base,0,out_stream->base.streamSize,aCache,cacheSize);
            }
            curStreamPos+=compressedSize;
        }
        *pCurStreamPos=curStreamPos;
        return hpatch_TRUE;
    }

typedef struct _TCompressedCovers{
    hpatch_TCovers          base;
    TStreamClip             coverClip;
    _TDecompressInputSteram decompresser;
    hpatch_TDecompress*     decompressPlugin;
} _TCompressedCovers;

void _compressedCovers_close(hpatch_TCovers* coversHandle){
    _TCompressedCovers* self=(_TCompressedCovers*)coversHandle;
    if (self){
        if (self->decompresser.decompressHandle){
            self->decompressPlugin->close(self->decompressPlugin,
                                          self->decompresser.decompressHandle);
            self->decompresser.decompressHandle=0;
        }
    }
}

hpatch_TCovers* _compressedCovers_open(hpatch_compressedDiffInfo* out_diffInfo,
                                       const hpatch_TStreamInput* compressedDiff,
                                       hpatch_TDecompress* decompressPlugin,
                                       TByte* temp_cache,TByte* temp_cache_end){
    TStreamClip                 diffHeadClip;
    _THDiffzHead                head;
    _TCompressedCovers*         self=0;
    TUInt        diffPos0=0;
    const TUInt  diffPos_end=compressedDiff->streamSize;
    assert(hpatch_kStreamCacheSize>=sizeof(_TCompressedCovers));
    if (temp_cache_end-temp_cache<hpatch_kStreamCacheSize+sizeof(_TCompressedCovers))
        return 0; //false;
    
    self=(_TCompressedCovers*)temp_cache;
    temp_cache+=sizeof(_TCompressedCovers);
    _TStreamClip_init(&diffHeadClip,compressedDiff,0,compressedDiff->streamSize,
                      temp_cache,temp_cache_end-temp_cache);
    if (!read_diffz_head(out_diffInfo,&head,&diffHeadClip)) return 0;
    diffPos0=(TUInt)(diffPos_end-_TStreamClip_streamSize(&diffHeadClip));
    if (head.compress_cover_buf_size>0){
        if (decompressPlugin==0) return 0;
        if (!decompressPlugin->is_can_open(decompressPlugin,out_diffInfo)) return 0;
    }
    
    memset(self,0,sizeof(_TCompressedCovers));
    self->base.coverCount=head.coverCount;
    self->base.oldPosBack=0;
    self->base.newPosBack=0;
    self->base.code_inc_oldPosClip=&self->coverClip;
    self->base.code_inc_newPosClip=&self->coverClip;
    self->base.code_lengthsClip=&self->coverClip;
    self->base.isOldPosBackNeedAddLength=hpatch_TRUE;
    self->decompressPlugin=decompressPlugin;
    if (!getStreamClip(&self->coverClip,&self->decompresser,
                       head.cover_buf_size,head.compress_cover_buf_size,
                       compressedDiff,&diffPos0,decompressPlugin,
                       temp_cache,temp_cache_end-temp_cache)) {
        _compressedCovers_close(&self->base);
        return 0;
    };
    return &self->base;
}



#define _clear_return(exitValue) {  result=exitValue; goto clear; }

#undef  _kCacheCount
#define _kCacheDeCount 5

hpatch_BOOL _patch_decompress_step(const hpatch_TStreamOutput*  out_newData,
                                   hpatch_TStreamInput*         once_in_newData,
                                   const hpatch_TStreamInput*   oldData,
                                   const hpatch_TStreamInput*   compressedDiff,
                                   hpatch_TDecompress*          decompressPlugin,
                                   hpatch_TCovers*              cached_covers,
                                   TByte*   temp_cache,TByte*  temp_cache_end,
                                   hpatch_BOOL is_copy_step,hpatch_BOOL is_add_rle_step){
    struct TStreamClip              coverClip;
    struct TStreamClip              code_newDataDiffClip;
    struct _TBytesRle_load_stream   rle_loader;
    _THDiffzHead                head;
    hpatch_compressedDiffInfo   diffInfo;
    _TDecompressInputSteram     decompressers[4];
    int          i;
    hpatch_BOOL  result=hpatch_TRUE;
    TUInt        diffPos0=0;
    const TUInt  diffPos_end=compressedDiff->streamSize;
    const size_t cacheSize=(temp_cache_end-temp_cache)/_kCacheDeCount;
    
    hpatch_TStreamInput virtual_ctrl;
    TByte virtual_buf[2+hpatch_kMaxPackedUIntBytes];
    TByte* virtual_buf_end=virtual_buf+2+hpatch_kMaxPackedUIntBytes;
    TUInt coverCount;
    
    if (cacheSize<hpatch_kMaxCompressTypeLength) return _hpatch_FALSE;
    assert(out_newData!=0);
    assert(out_newData->write!=0);
    assert(oldData!=0);
    assert(oldData->read!=0);
    assert(compressedDiff!=0);
    assert(compressedDiff->read!=0);
    {//head
        TStreamClip* diffHeadClip=&coverClip;//rename, share address
        _TStreamClip_init(diffHeadClip,compressedDiff,0,diffPos_end,temp_cache,cacheSize);
        if (!read_diffz_head(&diffInfo,&head,diffHeadClip)) return _hpatch_FALSE;
        if ((decompressPlugin==0)&&(diffInfo.compressedCount!=0)) return _hpatch_FALSE;
        if ((decompressPlugin)&&(diffInfo.compressedCount>0))
            if (!decompressPlugin->is_can_open(decompressPlugin,&diffInfo)) return _hpatch_FALSE;
        diffPos0=(TUInt)(diffPos_end-_TStreamClip_streamSize(diffHeadClip));
        
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
        diffPos0+=(head.compress_cover_buf_size==0)?head.cover_buf_size:head.compress_cover_buf_size;
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
            if (!hpatch_packUIntWithTag(&cur_buf,virtual_buf_end,diffInfo.newDataSize-1,
                                        kByteRleType_rle0,kByteRleType_bit)) return _hpatch_FALSE; //0's length
        }
        mem_as_hStreamInput(&virtual_ctrl,virtual_buf,cur_buf);
        
        _TStreamClip_init(&rle_loader.ctrlClip,&virtual_ctrl,0,virtual_ctrl.streamSize,temp_cache+cacheSize*1,cacheSize);
        _TStreamClip_init(&rle_loader.rleCodeClip,&virtual_ctrl,0,0,temp_cache+cacheSize*2,cacheSize);
    }
    if (!is_copy_step){ //savedNewData replace oldData
        TByte* cur_buf=virtual_buf;
        if (!hpatch_packUInt(&cur_buf,virtual_buf_end,0)) return _hpatch_FALSE; //inc_oldPos
        if (!hpatch_packUInt(&cur_buf,virtual_buf_end,0)) return _hpatch_FALSE; //inc_newPos
        if (!hpatch_packUInt(&cur_buf,virtual_buf_end,diffInfo.newDataSize)) return _hpatch_FALSE; //old_cover_length
        mem_as_hStreamInput(&virtual_ctrl,virtual_buf,cur_buf);
        
        _TStreamClip_init(&coverClip,&virtual_ctrl,0,virtual_ctrl.streamSize,temp_cache+cacheSize*0,cacheSize);
        _TStreamClip_init(&code_newDataDiffClip,&virtual_ctrl,0,0,temp_cache+cacheSize*3,cacheSize);
        
        assert(once_in_newData!=0);
        if ((once_in_newData->streamSize!=0)
            && (once_in_newData->streamSize!=diffInfo.newDataSize)) return _hpatch_FALSE;
        once_in_newData->streamSize=diffInfo.newDataSize;
        oldData=once_in_newData;
        coverCount=1;
    }
    
    {
        hpatch_TCovers covers;
        hpatch_TCovers* pcovers=0;
        if (cached_covers){
            pcovers=cached_covers;
        }else{
            covers.coverCount=coverCount;
            covers.newPosBack=0;
            covers.oldPosBack=0;
            covers.code_inc_oldPosClip=&coverClip;
            covers.code_inc_newPosClip=&coverClip;
            covers.code_lengthsClip=&coverClip;
            covers.isOldPosBackNeedAddLength=hpatch_TRUE;
            pcovers=&covers;
        }
        result=patchByClip(out_newData,oldData,pcovers,
                           &code_newDataDiffClip,&rle_loader,
                           temp_cache+(_kCacheDeCount-1)*cacheSize,hpatch_TRUE);
    }
clear:
    for (i=0;i<sizeof(decompressers)/sizeof(_TDecompressInputSteram);++i) {
        if (decompressers[i].decompressHandle){
            decompressPlugin->close(decompressPlugin,decompressers[i].decompressHandle);
            decompressers[i].decompressHandle=0;
        }
    }
    return result;
}



static hpatch_BOOL _step_read(TByte* buf,TByte* buf_end,
                              const hpatch_TStreamInput* data,hpatch_StreamPos_t pos){
    while (buf<buf_end) {
        const size_t kMaxStepRead =(1<<20);
        size_t readLen=(size_t)(buf_end-buf);
        if (readLen>kMaxStepRead) readLen=kMaxStepRead;
        if (data->streamSize-pos<readLen) return _hpatch_FALSE;
        if ((long)readLen!=data->read(data->streamHandle,pos,buf,
                                      buf+readLen)) return _hpatch_FALSE;
        buf+=readLen;
        pos+=readLen;
    }
    return hpatch_TRUE;
}

static hpatch_BOOL _do_cache_all(hpatch_TStreamInput* out_stream,const hpatch_TStreamInput* data,
                                 TByte* cache,TByte** pcache_end){
    TByte* cache_end=*pcache_end;
    if (!_step_read(cache_end-data->streamSize,cache_end,data,0)) return _hpatch_FALSE;
    mem_as_hStreamInput(out_stream,cache_end-data->streamSize,cache_end);
    *pcache_end-=data->streamSize;
    return hpatch_TRUE;
}

hpatch_BOOL patch_decompress_with_cache(const hpatch_TStreamOutput* out_newData,
                                        const hpatch_TStreamInput*  oldData,
                                        const hpatch_TStreamInput*  compressedDiff,
                                        hpatch_TDecompress* decompressPlugin,
                                        unsigned char* temp_cache,unsigned char* temp_cache_end){
    const size_t kMinCacheSize=hpatch_kStreamCacheSize*_kCacheDeCount;
    const size_t cacheSize=(size_t)(temp_cache_end-temp_cache);
    hpatch_TCovers* covers=0;
    /*hpatch_TStreamInput oldStream;
    if (cacheSize>=oldData->streamSize+kMinCacheSize){
        if (!_do_cache_all(&oldStream,oldData,temp_cache,&temp_cache_end)) return _hpatch_FALSE;
        oldData=&oldStream;
    }else if(cacheSize>=(1<<20)+kMinCacheSize) {
        unsigned char* cover_cache=temp_cache_end-(1<<20);
        hpatch_compressedDiffInfo diffInfo;
        covers=_compressedCovers_open(&diffInfo,compressedDiff,decompressPlugin,
                                      cover_cache,cover_cache+kMinCacheSize);
        temp_cache_end=cover_cache;
        assert(covers!=0);
        //if (!_do_cache_as_stream?(&oldStream,oldData,temp_cache,&temp_cache_end)) return _hpatch_FALSE;
        //oldData=&oldStream;
        
        //_compressedCovers_close
    }*/
    return _patch_decompress_step(out_newData,0,oldData,compressedDiff,decompressPlugin,
                                  covers,temp_cache,temp_cache_end,hpatch_TRUE,hpatch_TRUE);
}

hpatch_BOOL patch_decompress(const hpatch_TStreamOutput* out_newData,
                             const hpatch_TStreamInput*  oldData,
                             const hpatch_TStreamInput*  compressedDiff,
                             hpatch_TDecompress* decompressPlugin){
    unsigned char temp_cache[hpatch_kStreamCacheSize*_kCacheDeCount];
    return patch_decompress_with_cache(out_newData,oldData,compressedDiff,decompressPlugin,
                                       temp_cache,temp_cache+sizeof(temp_cache)/sizeof(unsigned char));
}

hpatch_BOOL patch_decompress_repeat_out(const hpatch_TStreamOutput* repeat_out_newData,
                                        hpatch_TStreamInput*        in_newData,
                                        const hpatch_TStreamInput*  oldData,
                                        const hpatch_TStreamInput*  compressedDiff,
                                        hpatch_TDecompress*         decompressPlugin){
    TByte  temp_cache[hpatch_kStreamCacheSize*_kCacheDeCount];
    TByte* temp_cache_end=temp_cache+sizeof(temp_cache)/sizeof(unsigned char);
    if (!_patch_decompress_step(repeat_out_newData,0,oldData,compressedDiff,decompressPlugin,
                                0,temp_cache,temp_cache_end,hpatch_TRUE,hpatch_FALSE)) return _hpatch_FALSE;
    return _patch_decompress_step(repeat_out_newData,in_newData,oldData,compressedDiff,decompressPlugin,
                                  0,temp_cache,temp_cache_end,hpatch_FALSE,hpatch_TRUE);
}



