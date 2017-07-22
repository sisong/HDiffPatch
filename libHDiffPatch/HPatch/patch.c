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

#define hpatch_TRUE     (!hpatch_FALSE)
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
void memory_as_inputStream(hpatch_TStreamInput* out_stream,
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
void memory_as_outputStream(hpatch_TStreamOutput* out_stream,
                            unsigned char* mem,unsigned char* mem_end){
    out_stream->streamHandle=(hpatch_TStreamOutputHandle)mem;
    out_stream->streamSize=mem_end-mem;
    out_stream->write=_write_mem_stream;
}



static hpatch_BOOL _bytesRle_load(TByte* out_data,TByte* out_dataEnd,
                                  const TByte* rle_code,const TByte* rle_code_end);
static void addData(TByte* dst,const TByte* src,TUInt length);
static hpatch_BOOL unpackPosWithTag(const TByte** src_code,const TByte* src_code_end,
                                    const unsigned int kTagBit,hpatch_StreamPos_t* result);

static hpatch_BOOL _unpackUIntWithTag(const TByte** src_code,const TByte* src_code_end,
                                      const unsigned int kTagBit,TUInt* result){
    if (sizeof(TUInt)==sizeof(hpatch_StreamPos_t)){
        return unpackPosWithTag(src_code,src_code_end,kTagBit,(hpatch_StreamPos_t*)result);
    }else{
        hpatch_StreamPos_t u64;
        hpatch_BOOL rt=unpackPosWithTag(src_code,src_code_end,kTagBit,&u64);
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
        { if (!_unpackUIntWithTag(src_code,src_code_end,kTagBit,puint)) return _hpatch_FALSE; }
#define unpackUIntTo(puint,src_code,src_code_end) \
        unpackUIntWithTagTo(puint,src_code,src_code_end,0)

hpatch_BOOL patch(TByte* out_newData,TByte* out_newData_end,
                  const TByte* oldData,const TByte* oldData_end,
                  const TByte* serializedDiff,const TByte* serializedDiff_end){
    const TByte *code_lengths, *code_lengths_end,
                *code_inc_newPos, *code_inc_newPos_end,
                *code_inc_oldPos, *code_inc_oldPos_end,
                *code_newDataDiff, *code_newDataDiff_end;
    TUInt       ctrlCount;

    assert(out_newData<=out_newData_end);
    assert(oldData<=oldData_end);
    assert(serializedDiff<=serializedDiff_end);
    unpackUIntTo(&ctrlCount,&serializedDiff, serializedDiff_end);
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
        for (i=0; i<ctrlCount; ++i){
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
static hpatch_BOOL unpackPosWithTag(const TByte** src_code,const TByte* src_code_end,
                                    const unsigned int kTagBit,hpatch_StreamPos_t* result){//读出整数并前进指针.
#ifdef __RUN_MEM_SAFE_CHECK
    const unsigned int kPackMaxTagBit=7;
#endif
    hpatch_StreamPos_t  value;
    TByte               code;
    const TByte*        pcode=*src_code;

#ifdef __RUN_MEM_SAFE_CHECK
    assert(kTagBit<=kPackMaxTagBit);
    if (src_code_end-pcode<=0) return _hpatch_FALSE;
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


//数据用rle压缩后的包类型2bit
typedef enum TByteRleType{
    kByteRleType_rle0  = 0,    //00表示后面存的压缩0;    (包中不需要字节数据)
    kByteRleType_rle255= 1,    //01表示后面存的压缩255;  (包中不需要字节数据)
    kByteRleType_rle   = 2,    //10表示后面存的压缩数据;  (包中字节数据只需储存一个字节数据)
    kByteRleType_unrle = 3     //11表示后面存的未压缩数据;(包中连续储存多个字节数据)
} TByteRleType;

static const int kByteRleType_bit=2;


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
    size_t      cacheBegin;//cacheEnd==hpatch_kStreamCacheSize
    TByte       cacheBuf[hpatch_kStreamCacheSize];
} TStreamClip;

static void _TStreamClip_init(struct TStreamClip* sclip,
                              const struct hpatch_TStreamInput* srcStream,
                              TUInt streamPos,TUInt streamPos_end){
    sclip->srcStream=srcStream;
    sclip->streamPos=streamPos;
    sclip->streamPos_end=streamPos_end;
    sclip->cacheBegin=hpatch_kStreamCacheSize;
}
#define _TStreamClip_isFinish(sclip)     ( 0==_TStreamClip_streamSize(sclip) )
#define _TStreamClip_isCacheEmpty(sclip) ( (sclip)->cacheBegin==hpatch_kStreamCacheSize )
#define _TStreamClip_cachedSize(sclip)   ( (size_t)(hpatch_kStreamCacheSize-(sclip)->cacheBegin) )
#define _TStreamClip_streamSize(sclip)   \
    (  (TUInt)((sclip)->streamPos_end-(sclip)->streamPos)  \
     + (TUInt)_TStreamClip_cachedSize(sclip)  )

static void _TStreamClip_updateCache(struct TStreamClip* sclip){
    TByte* buf0=&sclip->cacheBuf[0];
    const TUInt streamSize=(TUInt)(sclip->streamPos_end-sclip->streamPos);
    size_t readSize=sclip->cacheBegin;
    if (readSize>streamSize)
        readSize=(size_t)streamSize;
    if (readSize==0) return;
    if (!_TStreamClip_isCacheEmpty(sclip)){
        memmove(buf0+(size_t)(sclip->cacheBegin-readSize),
                buf0+sclip->cacheBegin,_TStreamClip_cachedSize(sclip));
    }
    if (sclip->srcStream->read(sclip->srcStream->streamHandle,sclip->streamPos,
                               buf0+(hpatch_kStreamCacheSize-readSize),buf0+hpatch_kStreamCacheSize)
                           == readSize ){
        sclip->cacheBegin-=readSize;
        sclip->streamPos+=readSize;
    }else{ //read error
        sclip->cacheBegin=hpatch_kStreamCacheSize;
        sclip->streamPos=sclip->streamPos_end;
    }
}

static TByte* _TStreamClip_accessData(struct TStreamClip* sclip,size_t readSize){
    assert(readSize<=hpatch_kStreamCacheSize);
    if (readSize>_TStreamClip_cachedSize(sclip))
        _TStreamClip_updateCache(sclip);
    if(readSize<=_TStreamClip_cachedSize(sclip)){
        return &sclip->cacheBuf[sclip->cacheBegin];
    }else{
        return 0; //read error
    }
}

#define _TStreamClip_skipData_noCheck(sclip,skipSize) ((sclip)->cacheBegin+=skipSize)

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

#define  kMaxPackedByte ((sizeof(TUInt)*8+6)/7+1)
//assert(hpatch_kStreamCacheSize>=kMaxPackedByte);
struct __private_hpatch_check_kMaxPackedByte {
    char _[hpatch_kStreamCacheSize-kMaxPackedByte]; };

static hpatch_BOOL _TStreamClip_unpackUIntWithTag(struct TStreamClip* sclip,
                                                  const int kTagBit,TUInt* result){
    TByte* curCode,*codeBegin;
    size_t readSize=kMaxPackedByte;
    const TUInt dataSize=_TStreamClip_streamSize(sclip);
#ifdef __RUN_MEM_SAFE_CHECK
    if (dataSize==0) return _hpatch_FALSE;
#endif
    if (readSize>dataSize)
        readSize=(size_t)dataSize;
    codeBegin=_TStreamClip_accessData(sclip,readSize);
    if (codeBegin==0) return _hpatch_FALSE;
    curCode=codeBegin;
    if (!unpackPosWithTag((const TByte**)&curCode,codeBegin+readSize,kTagBit,result))
        return _hpatch_FALSE;
    _TStreamClip_skipData_noCheck(sclip,(size_t)(curCode-codeBegin));
    return hpatch_TRUE;
}
#define _TStreamClip_unpackUIntWithTagTo(puint,sclip,kTagBit) \
    { if (!_TStreamClip_unpackUIntWithTag(sclip,kTagBit,puint)) return _hpatch_FALSE; }
#define _TStreamClip_unpackUIntTo(puint,sclip) \
    _TStreamClip_unpackUIntWithTagTo(puint,sclip,0)


typedef struct _TBytesRle_load_stream{
    TUInt                               memCopyLength;
    TUInt                               memSetLength;
    TByte                               memSetValue;
    struct TStreamClip                  ctrlClip;
    struct TStreamClip                  rleCodeClip;
} _TBytesRle_load_stream;

static void _TBytesRle_load_stream_init(_TBytesRle_load_stream* loader){
    loader->memSetLength=0;
    loader->memSetValue=0;//nil;
    loader->memCopyLength=0;
    _TStreamClip_init(&loader->ctrlClip,0,0,0);
    _TStreamClip_init(&loader->rleCodeClip,0,0,0);
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

    while ((loader->memSetLength>0)&&(decodeSize>0)) {
        size_t memSetStep=decodeSize;
        if (memSetStep>loader->memSetLength)
            memSetStep=(size_t)loader->memSetLength;
        if (out_data){
            memSet_add(out_data,loader->memSetValue,memSetStep);
            out_data+=memSetStep;
        }
        decodeSize-=memSetStep;
        loader->memSetLength-=memSetStep;
    }
    while ((loader->memCopyLength>0)&&(decodeSize>0)) {
        TByte* rleData;
        size_t decodeStep=hpatch_kStreamCacheSize;
        if (decodeStep>decodeSize)
            decodeStep=decodeSize;
        if (decodeStep>loader->memCopyLength)
            decodeStep=(size_t)loader->memCopyLength;
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
#ifdef __RUN_MEM_SAFE_CHECK
        if (length+1<length) return _hpatch_FALSE;
#endif
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
                const TByte* pSetValue;
#ifdef __RUN_MEM_SAFE_CHECK
                if (1>_TStreamClip_streamSize(&loader->rleCodeClip)) return _hpatch_FALSE;
#endif
                loader->memSetLength=length;
                pSetValue=_TStreamClip_readData(&loader->rleCodeClip,1);
                if (pSetValue==0) return _hpatch_FALSE;
                loader->memSetValue=*pSetValue;
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
static hpatch_BOOL _TBytesRle_load_stream_decode_skip(_TBytesRle_load_stream* loader,
                                                      size_t decodeSize){
    return _TBytesRle_load_stream_decode_add(loader,decodeSize,0);
}

static  hpatch_BOOL _patch_copy_diff(const struct hpatch_TStreamOutput* out_newData,
                                     TUInt writeToPos,_TBytesRle_load_stream* rle_loader,
                                     struct TStreamClip* diff,TUInt copyLength){
    while (copyLength>0){
        const TByte* data;
        size_t decodeStep=hpatch_kStreamCacheSize;
        if (decodeStep>copyLength)
            decodeStep=(size_t)copyLength;
        if (!_TBytesRle_load_stream_decode_skip(rle_loader,decodeStep)) return _hpatch_FALSE;
        data=_TStreamClip_readData(diff,decodeStep);
        if (data==0) return _hpatch_FALSE;
        if (decodeStep!=out_newData->write(out_newData->streamHandle,writeToPos,data,data+decodeStep))
            return _hpatch_FALSE;
        writeToPos+=decodeStep;
        copyLength-=decodeStep;
    }
    return hpatch_TRUE;
}

static  hpatch_BOOL _patch_add_old(const struct hpatch_TStreamOutput* out_newData,
                                   TUInt writeToPos,_TBytesRle_load_stream* rle_loader,
                                   const struct hpatch_TStreamInput* old,TUInt oldPos,
                                   TUInt addLength){
    TByte  data[hpatch_kStreamCacheSize];
    while (addLength>0){
        size_t decodeStep=hpatch_kStreamCacheSize;
        if (decodeStep>addLength)
            decodeStep=(size_t)addLength;
        if (decodeStep!=old->read(old->streamHandle,oldPos,data,data+decodeStep)) return _hpatch_FALSE;
        if (!_TBytesRle_load_stream_decode_add(rle_loader,decodeStep,data)) return _hpatch_FALSE;
        if (decodeStep!=out_newData->write(out_newData->streamHandle,writeToPos,data,data+decodeStep))
            return _hpatch_FALSE;
        oldPos+=decodeStep;
        writeToPos+=decodeStep;
        addLength-=decodeStep;
    }
    return hpatch_TRUE;
}


static hpatch_BOOL patchByClip(const struct hpatch_TStreamOutput* out_newData,
                               const struct hpatch_TStreamInput*  oldData,
                               struct TStreamClip* code_inc_oldPosClip,
                               struct TStreamClip* code_inc_newPosClip,
                               struct TStreamClip* code_lengthsClip,
                               struct TStreamClip* code_newDataDiffClip,
                               struct _TBytesRle_load_stream* rle_loader,
                               const TUInt ctrlCount,
                               int isOldPosBackNeedAddLength){
    const TUInt newDataSize=out_newData->streamSize;
    TUInt oldPosBack=0;
    TUInt newPosBack=0;
    TUInt i;
    for (i=0; i<ctrlCount; ++i){
        TUInt copyLength,addLength, oldPos,inc_oldPos;
        TByte inc_oldPos_sign;
        const TByte* pSign;
        
#ifdef __RUN_MEM_SAFE_CHECK
        if (_TStreamClip_isFinish(code_inc_oldPosClip)) return _hpatch_FALSE;
#endif
        pSign=_TStreamClip_accessData(code_inc_oldPosClip,1);
        if (pSign==0) return _hpatch_FALSE;
        inc_oldPos_sign=(*pSign)>>(8-kSignTagBit);
        _TStreamClip_unpackUIntWithTagTo(&inc_oldPos,code_inc_oldPosClip,kSignTagBit);
        if (inc_oldPos_sign==0)
            oldPos=oldPosBack+inc_oldPos;
        else
            oldPos=oldPosBack-inc_oldPos;
        _TStreamClip_unpackUIntTo(&copyLength,code_inc_newPosClip);
        _TStreamClip_unpackUIntTo(&addLength,code_lengthsClip);
        if (copyLength>0){
#ifdef __RUN_MEM_SAFE_CHECK
            if (copyLength>(TUInt)(newDataSize-newPosBack)) return _hpatch_FALSE;
            if (copyLength>_TStreamClip_streamSize(code_newDataDiffClip)) return _hpatch_FALSE;
#endif
            if (!_patch_copy_diff(out_newData,newPosBack,rle_loader,
                                  code_newDataDiffClip,copyLength)) return _hpatch_FALSE;
            newPosBack+=copyLength;
        }
#ifdef __RUN_MEM_SAFE_CHECK
        if ((addLength>(TUInt)(newDataSize-newPosBack))) return _hpatch_FALSE;
        if ( (oldPos>(oldData->streamSize)) ||
            (addLength>(TUInt)(oldData->streamSize-oldPos)) ) return _hpatch_FALSE;
#endif
        if (!_patch_add_old(out_newData,newPosBack,rle_loader,
                            oldData,oldPos,addLength)) return _hpatch_FALSE;
        oldPosBack=oldPos;
        if (isOldPosBackNeedAddLength)
            oldPosBack+=addLength;
        newPosBack+=addLength;
    }
    
    if (newPosBack<newDataSize){
        TUInt copyLength=newDataSize-newPosBack;
#ifdef __RUN_MEM_SAFE_CHECK
        if (copyLength>_TStreamClip_streamSize(code_newDataDiffClip)) return _hpatch_FALSE;
#endif
        if (!_patch_copy_diff(out_newData,newPosBack,rle_loader,
                              code_newDataDiffClip,copyLength)) return _hpatch_FALSE;
        //newPosBack=newDataSize;
    }
    
    if (   _TBytesRle_load_stream_isFinish(rle_loader)
        && _TStreamClip_isFinish(code_lengthsClip)
        && _TStreamClip_isFinish(code_inc_newPosClip)
        && _TStreamClip_isFinish(code_inc_oldPosClip)
        && _TStreamClip_isFinish(code_newDataDiffClip) )
        return hpatch_TRUE;
    else
        return _hpatch_FALSE;
}

hpatch_BOOL patch_stream(const struct hpatch_TStreamOutput* out_newData,
                         const struct hpatch_TStreamInput*  oldData,
                         const struct hpatch_TStreamInput*  serializedDiff){
    struct TStreamClip              code_inc_oldPosClip;
    struct TStreamClip              code_inc_newPosClip;
    struct TStreamClip              code_lengthsClip;
    struct TStreamClip              code_newDataDiffClip;
    struct _TBytesRle_load_stream   rle_loader;
    TUInt                           ctrlCount;
    
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
        _TStreamClip_init(diffHeadClip,serializedDiff,0,diffPos_end);
        _TStreamClip_unpackUIntTo(&ctrlCount,diffHeadClip);
        _TStreamClip_unpackUIntTo(&lengthSize,diffHeadClip);
        _TStreamClip_unpackUIntTo(&inc_newPosSize,diffHeadClip);
        _TStreamClip_unpackUIntTo(&inc_oldPosSize,diffHeadClip);
        _TStreamClip_unpackUIntTo(&newDataDiffSize,diffHeadClip);
        diffPos0=(TUInt)(diffPos_end-_TStreamClip_streamSize(diffHeadClip));
#ifdef __RUN_MEM_SAFE_CHECK
        if (lengthSize>(TUInt)(diffPos_end-diffPos0)) return _hpatch_FALSE;
#endif
        //eq. _TStreamClip_init(&code_lengthsClip,serializedDiff,diffPos0,diffPos0+lengthSize);
        _TStreamClip_resetPosEnd(&code_lengthsClip,diffPos0+lengthSize);
        diffPos0+=lengthSize;
#ifdef __RUN_MEM_SAFE_CHECK
        if (inc_newPosSize>(TUInt)(diffPos_end-diffPos0)) return _hpatch_FALSE;
#endif
        _TStreamClip_init(&code_inc_newPosClip,serializedDiff,diffPos0,diffPos0+inc_newPosSize);
        diffPos0+=inc_newPosSize;
#ifdef __RUN_MEM_SAFE_CHECK
        if (inc_oldPosSize>(TUInt)(diffPos_end-diffPos0)) return _hpatch_FALSE;
#endif
        _TStreamClip_init(&code_inc_oldPosClip,serializedDiff,diffPos0,diffPos0+inc_oldPosSize);
        diffPos0+=inc_oldPosSize;
#ifdef __RUN_MEM_SAFE_CHECK
        if (newDataDiffSize>(TUInt)(diffPos_end-diffPos0)) return _hpatch_FALSE;
#endif
        _TStreamClip_init(&code_newDataDiffClip,serializedDiff,diffPos0,diffPos0+newDataDiffSize);
        diffPos0+=newDataDiffSize;
        
        {//rle
            TUInt rleCtrlSize;
            TUInt rlePos0;
            struct TStreamClip*  rleHeadClip=&rle_loader.ctrlClip;//rename, share address
            _TStreamClip_init(rleHeadClip,serializedDiff,diffPos0,diffPos_end);
            _TStreamClip_unpackUIntTo(&rleCtrlSize,rleHeadClip);
            rlePos0=(TUInt)(diffPos_end-_TStreamClip_streamSize(rleHeadClip));
#ifdef __RUN_MEM_SAFE_CHECK
            if (rleCtrlSize>(TUInt)(diffPos_end-rlePos0)) return _hpatch_FALSE;
#endif
            _TBytesRle_load_stream_init(&rle_loader);
            _TStreamClip_init(&rle_loader.ctrlClip,serializedDiff,rlePos0,rlePos0+rleCtrlSize);
            _TStreamClip_init(&rle_loader.rleCodeClip,serializedDiff,rlePos0+rleCtrlSize,diffPos_end);
        }
    }
    
    return patchByClip(out_newData,oldData,
                       &code_inc_oldPosClip,&code_inc_newPosClip,&code_lengthsClip,
                       &code_newDataDiffClip,
                       &rle_loader, ctrlCount, hpatch_FALSE);
}


#define kVersionTypeLen 8

typedef struct _THDiffzHead{
    char VersionType[kVersionTypeLen+1];
    char compressType[hpatch_kMaxCompressTypeLength+1];
    TUInt newDataSize;
    TUInt oldDataSize;
    TUInt ctrlCount;
    int   compressedCount;
    
    TUInt cover_buf_size;
    TUInt compress_cover_buf_size;
    TUInt rle_ctrlBuf_size;
    TUInt compress_rle_ctrlBuf_size;
    TUInt rle_codeBuf_size;
    TUInt compress_rle_codeBuf_size;
    TUInt newDataDiff_size;
    TUInt compress_newDataDiff_size;
} _THDiffzHead;


//assert(hpatch_kStreamCacheSize>hpatch_kMaxCompressTypeLength+1);
struct __private_hpatch_check_kMaxCompressTypeLength {
    char _[hpatch_kStreamCacheSize-(hpatch_kMaxCompressTypeLength+1)-1];};

static hpatch_BOOL read_diffz_head(_THDiffzHead* out_head,TStreamClip* diffHeadClip){
    const TByte* versionType=_TStreamClip_readData(diffHeadClip,kVersionTypeLen);
    if (versionType==0) return _hpatch_FALSE;
    if (0!=memcmp(versionType,"HDIFF13&",kVersionTypeLen)) return _hpatch_FALSE;
    
    {//read compressType
        const TByte* compressType;
        size_t       compressTypeLen;
        size_t readLen=hpatch_kMaxCompressTypeLength+1+1;
        if (readLen>_TStreamClip_streamSize(diffHeadClip))
            readLen=(size_t)_TStreamClip_streamSize(diffHeadClip);
        compressType=_TStreamClip_accessData(diffHeadClip,readLen);
        if (compressType==0) return _hpatch_FALSE;
        compressTypeLen=0;
        while (compressTypeLen<readLen) {//like strlen()
            if (compressType[compressTypeLen]=='\0') break;
            ++compressTypeLen;
        }
        if (compressTypeLen==readLen) return _hpatch_FALSE;
        _TStreamClip_skipData_noCheck(diffHeadClip,compressTypeLen+1);
        memcpy(out_head->compressType,compressType,compressTypeLen+1);
    }
    
    _TStreamClip_unpackUIntTo(&out_head->newDataSize,diffHeadClip);
    _TStreamClip_unpackUIntTo(&out_head->oldDataSize,diffHeadClip);
    _TStreamClip_unpackUIntTo(&out_head->ctrlCount,diffHeadClip);
    
    _TStreamClip_unpackUIntTo(&out_head->cover_buf_size,diffHeadClip);
    _TStreamClip_unpackUIntTo(&out_head->compress_cover_buf_size,diffHeadClip);
    _TStreamClip_unpackUIntTo(&out_head->rle_ctrlBuf_size,diffHeadClip);
    _TStreamClip_unpackUIntTo(&out_head->compress_rle_ctrlBuf_size,diffHeadClip);
    _TStreamClip_unpackUIntTo(&out_head->rle_codeBuf_size,diffHeadClip);
    _TStreamClip_unpackUIntTo(&out_head->compress_rle_codeBuf_size,diffHeadClip);
    _TStreamClip_unpackUIntTo(&out_head->newDataDiff_size,diffHeadClip);
    _TStreamClip_unpackUIntTo(&out_head->compress_newDataDiff_size,diffHeadClip);
    
    out_head->compressedCount=((out_head->compress_cover_buf_size)?1:0)
                             +((out_head->compress_rle_ctrlBuf_size)?1:0)
                             +((out_head->compress_rle_codeBuf_size)?1:0)
                             +((out_head->compress_newDataDiff_size)?1:0);
    return hpatch_TRUE;
}

hpatch_BOOL compressedDiffInfo(TUInt* out_newDataSize,
                               TUInt* out_oldDataSize,
                               char* out_compressType,char* out_compressType_end,
                               int*  out_compressedCount,
                               const struct hpatch_TStreamInput* compressedDiff){
    TStreamClip  diffHeadClip;
    _THDiffzHead head;
    size_t       out_compressTypeLen;
    assert(out_compressType<=out_compressType_end);
    assert(compressedDiff!=0);
    assert(compressedDiff->read!=0);
    
    _TStreamClip_init(&diffHeadClip,compressedDiff,0,compressedDiff->streamSize);
    if (!read_diffz_head(&head,&diffHeadClip)) return _hpatch_FALSE;
    if (out_newDataSize)
        *out_newDataSize=head.newDataSize;
    if (out_oldDataSize)
        *out_oldDataSize=head.oldDataSize;
    if (out_compressedCount)
        *out_compressedCount=head.compressedCount;
    
    out_compressTypeLen=(out_compressType_end-out_compressType);
    if (out_compressType){
        size_t compressTypeLen=strlen(head.compressType);
        if (out_compressTypeLen<compressTypeLen+1) return _hpatch_FALSE;
        memcpy(out_compressType,head.compressType,compressTypeLen+1);
    }
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


    hpatch_BOOL getStreamClip(TStreamClip* out_clip,_TDecompressInputSteram* out_stream,
                              TUInt dataSize,TUInt compressedSize,
                              const hpatch_TStreamInput* stream,TUInt* pCurStreamPos,
                              const char* compressType,hpatch_TDecompress* decompressPlugin){
        TUInt curStreamPos=*pCurStreamPos;
        if (compressedSize==0){
#ifdef __RUN_MEM_SAFE_CHECK
            if ((TUInt)(curStreamPos+dataSize)<curStreamPos) return _hpatch_FALSE;
            if ((TUInt)(curStreamPos+dataSize)>stream->streamSize) return _hpatch_FALSE;
#endif
            _TStreamClip_init(out_clip,stream,curStreamPos,curStreamPos+dataSize);
            curStreamPos+=dataSize;
        }else{
#ifdef __RUN_MEM_SAFE_CHECK
            if ((TUInt)(curStreamPos+compressedSize)<curStreamPos) return _hpatch_FALSE;
            if ((TUInt)(curStreamPos+compressedSize)>stream->streamSize) return _hpatch_FALSE;
#endif
            out_stream->base.streamHandle=out_stream;
            out_stream->base.streamSize=dataSize;
            out_stream->base.read=_decompress_read;
            out_stream->decompressPlugin=decompressPlugin;
            out_stream->decompressHandle=decompressPlugin->open(decompressPlugin,compressType,
                                                                dataSize,stream,
                                                                curStreamPos,curStreamPos+compressedSize);
            if (!out_stream->decompressHandle) return _hpatch_FALSE;
            _TStreamClip_init(out_clip,&out_stream->base,0,out_stream->base.streamSize);
            curStreamPos+=compressedSize;
        }
        *pCurStreamPos=curStreamPos;
        return hpatch_TRUE;
    }


#define _clear_return(exitValue) {  result=exitValue; goto clear; }

hpatch_BOOL patch_decompress(const struct hpatch_TStreamOutput* out_newData,
                             const struct hpatch_TStreamInput*  oldData,
                             const struct hpatch_TStreamInput*  compressedDiff,
                             hpatch_TDecompress* decompressPlugin){
    struct TStreamClip              coverClip;
    struct TStreamClip              code_newDataDiffClip;
    struct _TBytesRle_load_stream   rle_loader;
    _THDiffzHead head;
    _TDecompressInputSteram decompressers[4];
    int          i;
    hpatch_BOOL  result=hpatch_TRUE;
    TUInt        diffPos0=0;
    const TUInt  diffPos_end=compressedDiff->streamSize;
    
    assert(out_newData!=0);
    assert(out_newData->write!=0);
    assert(oldData!=0);
    assert(oldData->read!=0);
    assert(compressedDiff!=0);
    assert(compressedDiff->read!=0);
    {//head
        TStreamClip* diffHeadClip=&coverClip;//rename, share address
        _TStreamClip_init(diffHeadClip,compressedDiff,0,diffPos_end);
        if (!read_diffz_head(&head,diffHeadClip)) return _hpatch_FALSE;
        if ((decompressPlugin==0)&&(head.compressedCount!=0)) return _hpatch_FALSE;
        diffPos0=(TUInt)(diffPos_end-_TStreamClip_streamSize(diffHeadClip));
    }
    
    for (i=0;i<sizeof(decompressers)/sizeof(_TDecompressInputSteram);++i)
        decompressers[i].decompressHandle=0;
    _TBytesRle_load_stream_init(&rle_loader);
    
    if (!getStreamClip(&coverClip,&decompressers[0],
                       head.cover_buf_size,head.compress_cover_buf_size,
                       compressedDiff,&diffPos0,head.compressType,
                       decompressPlugin)) _clear_return(_hpatch_FALSE);
    if (!getStreamClip(&rle_loader.ctrlClip,&decompressers[1],
                       head.rle_ctrlBuf_size,head.compress_rle_ctrlBuf_size,
                       compressedDiff,&diffPos0,head.compressType,
                       decompressPlugin)) _clear_return(_hpatch_FALSE);
    if (!getStreamClip(&rle_loader.rleCodeClip,&decompressers[2],
                       head.rle_codeBuf_size,head.compress_rle_codeBuf_size,
                       compressedDiff,&diffPos0,head.compressType,
                       decompressPlugin)) _clear_return(_hpatch_FALSE);
    if (!getStreamClip(&code_newDataDiffClip,&decompressers[3],
                       head.newDataDiff_size,head.compress_newDataDiff_size,
                       compressedDiff,&diffPos0,head.compressType,
                       decompressPlugin)) _clear_return(_hpatch_FALSE);
#ifdef __RUN_MEM_SAFE_CHECK
    if (diffPos0!=diffPos_end) return _hpatch_FALSE;
#endif
    result=patchByClip(out_newData,oldData,
                       &coverClip,&coverClip,&coverClip,//same
                       &code_newDataDiffClip,
                       &rle_loader, head.ctrlCount, hpatch_TRUE);
clear:
    for (i=0;i<sizeof(decompressers)/sizeof(_TDecompressInputSteram);++i) {
        if (decompressers[i].decompressHandle){
            decompressPlugin->close(decompressPlugin,decompressers[i].decompressHandle);
            decompressers[i].decompressHandle=0;
        }
    }
    return result;
}



