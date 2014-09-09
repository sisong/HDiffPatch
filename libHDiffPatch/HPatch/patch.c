//
//  patch.cpp
//  HPatch
//
/*
 This is the HPatch copyright.
 
 Copyright (c) 2012-2014 HouSisong All Rights Reserved.
 
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
#include "string.h" //memcpy memset
#include "assert.h" //assert

typedef unsigned char TByte;
#define TUInt size_t

//__RUN_MEM_SAFE_CHECK用来启动内存访问越界检查.
#define __RUN_MEM_SAFE_CHECK

static hpatch_BOOL _bytesRle_load(TByte* out_data,TByte* out_dataEnd,const TByte* rle_code,const TByte* rle_code_end);
static void addData(TByte* dst,const TByte* src,TUInt length);
static TUInt unpackUIntWithTag(const TByte** src_code,const TByte* src_code_end,const int kTagBit);
#define unpackUInt(src_code,src_code_end) unpackUIntWithTag(src_code,src_code_end,0)

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
    ctrlCount=unpackUInt(&serializedDiff, serializedDiff_end);
    {   //head
        TUInt lengthSize=unpackUInt(&serializedDiff, serializedDiff_end);
        TUInt inc_newPosSize=unpackUInt(&serializedDiff, serializedDiff_end);
        TUInt inc_oldPosSize=unpackUInt(&serializedDiff, serializedDiff_end);
        TUInt newDataDiffSize=unpackUInt(&serializedDiff, serializedDiff_end);
#ifdef __RUN_MEM_SAFE_CHECK
        if (lengthSize>(TUInt)(serializedDiff_end-serializedDiff)) return hpatch_FALSE;
#endif
        code_lengths=serializedDiff;     serializedDiff+=lengthSize;
        code_lengths_end=serializedDiff;
#ifdef __RUN_MEM_SAFE_CHECK
        if (inc_newPosSize>(TUInt)(serializedDiff_end-serializedDiff)) return hpatch_FALSE;
#endif
        code_inc_newPos=serializedDiff; serializedDiff+=inc_newPosSize;
        code_inc_newPos_end=serializedDiff;
#ifdef __RUN_MEM_SAFE_CHECK
        if (inc_oldPosSize>(TUInt)(serializedDiff_end-serializedDiff)) return hpatch_FALSE;
#endif
        code_inc_oldPos=serializedDiff; serializedDiff+=inc_oldPosSize;
        code_inc_oldPos_end=serializedDiff;
#ifdef __RUN_MEM_SAFE_CHECK
        if (newDataDiffSize>(TUInt)(serializedDiff_end-serializedDiff)) return hpatch_FALSE;
#endif
        code_newDataDiff=serializedDiff; serializedDiff+=newDataDiffSize;
        code_newDataDiff_end=serializedDiff;
    }
    
    //decode rle ; rle data begin==serializedDiff;
    if (!_bytesRle_load(out_newData, out_newData_end, serializedDiff, serializedDiff_end))
        return hpatch_FALSE;
    
    {   //patch
        const TUInt newDataSize=(TUInt)(out_newData_end-out_newData);
        TUInt oldPosBack=0;
        TUInt newPosBack=0;
        TUInt i;
        for (i=0; i<ctrlCount; ++i){
            TUInt oldPos,inc_oldPos,inc_oldPos_sign;
            
            TUInt copyLength=unpackUInt(&code_inc_newPos, code_inc_newPos_end);
            TUInt addLength=unpackUInt(&code_lengths, code_lengths_end);
#ifdef __RUN_MEM_SAFE_CHECK
            if (code_inc_oldPos>=code_inc_oldPos_end) return hpatch_FALSE;
#endif
            inc_oldPos_sign=(*code_inc_oldPos)>>(8-1);
            inc_oldPos=unpackUIntWithTag(&code_inc_oldPos, code_inc_oldPos_end, 1);
            if (inc_oldPos_sign==0)
                oldPos=oldPosBack+inc_oldPos;
            else
                oldPos=oldPosBack-inc_oldPos;
            if (copyLength>0){
#ifdef __RUN_MEM_SAFE_CHECK
                if (copyLength>(TUInt)(newDataSize-newPosBack)) return hpatch_FALSE;
                if (copyLength>(TUInt)(code_newDataDiff_end-code_newDataDiff)) return hpatch_FALSE;
#endif
                memcpy(out_newData+newPosBack,code_newDataDiff,copyLength);
                code_newDataDiff+=copyLength;
                newPosBack+=copyLength;
            }
#ifdef __RUN_MEM_SAFE_CHECK
            if ((addLength>(TUInt)(newDataSize-newPosBack))) return hpatch_FALSE;
            if ((oldPos>(TUInt)(oldData_end-oldData))||(addLength>(TUInt)(oldData_end-oldData-oldPos))) return hpatch_FALSE;
#endif
            addData(out_newData+newPosBack,oldData+oldPos,addLength);
            oldPosBack=oldPos;
            newPosBack+=addLength;
        }
        
        if (newPosBack<newDataSize){
            TUInt copyLength=newDataSize-newPosBack;
#ifdef __RUN_MEM_SAFE_CHECK
            if (copyLength>(TUInt)(code_newDataDiff_end-code_newDataDiff)) return hpatch_FALSE;
#endif
            memcpy(out_newData+newPosBack,code_newDataDiff,copyLength);
            code_newDataDiff+=copyLength;
            newPosBack=newDataSize;
        }
    }
    
    return (code_lengths==code_lengths_end)&&(code_inc_newPos==code_inc_newPos_end)
            &&(code_inc_oldPos==code_inc_oldPos_end)&&(code_newDataDiff==code_newDataDiff_end);
}

//变长正整数编码方案(x bit额外类型标志位,x<=7),从高位开始输出1--n byte:
// x0*  7-x bit
// x1* 0*  7+7-x bit
// x1* 1* 0*  7+7+7-x bit
// x1* 1* 1* 0*  7+7+7+7-x bit
// x1* 1* 1* 1* 0*  7+7+7+7+7-x bit
// ......
static TUInt unpackUIntWithTag(const TByte** src_code,const TByte* src_code_end,const int kTagBit){//读出整数并前进指针.
#ifdef __RUN_MEM_SAFE_CHECK
    const int kPackMaxTagBit=7;
#endif
    TUInt           value;
    TByte           code;
    const TByte*    pcode=*src_code;
    
#ifdef __RUN_MEM_SAFE_CHECK
    assert((0<=kTagBit)&&(kTagBit<=kPackMaxTagBit));
    if (src_code_end-pcode<=0) return 0;
#endif
    code=*pcode; ++pcode;
    value=code&((1<<(7-kTagBit))-1);
    if ((code&(1<<(7-kTagBit)))!=0){
        do {
#ifdef __RUN_MEM_SAFE_CHECK
            assert((value>>(sizeof(value)*8-7))==0);
            if (src_code_end==pcode) break;
#endif
            code=*pcode; ++pcode;
            value=(value<<7) | (code&((1<<7)-1));
        } while ((code&(1<<7))!=0);
    }
    (*src_code)=pcode;
    return value;
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


static hpatch_BOOL _bytesRle_load(TByte* out_data,TByte* out_dataEnd,const TByte* rle_code,const TByte* rle_code_end){
    const TByte*    ctrlBuf,*ctrlBuf_end;
    
    TUInt ctrlSize= unpackUInt(&rle_code,rle_code_end);
#ifdef __RUN_MEM_SAFE_CHECK
    if (ctrlSize>(TUInt)(rle_code_end-rle_code)) return hpatch_FALSE;
#endif
    ctrlBuf=rle_code;
    rle_code+=ctrlSize;
    ctrlBuf_end=rle_code;
    while (ctrlBuf_end-ctrlBuf>0){
        enum TByteRleType type=(enum TByteRleType)((*ctrlBuf)>>(8-kByteRleType_bit));
        TUInt length= 1 + unpackUIntWithTag(&ctrlBuf,ctrlBuf_end,kByteRleType_bit);
#ifdef __RUN_MEM_SAFE_CHECK
        if (length>(TUInt)(out_dataEnd-out_data)) return hpatch_FALSE;
#endif
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
                if (1>(TUInt)(rle_code_end-rle_code)) return hpatch_FALSE;
#endif
                memset(out_data,*rle_code,length);
                ++rle_code;
                out_data+=length;
            }break;
            case kByteRleType_unrle:{
#ifdef __RUN_MEM_SAFE_CHECK
                if (length>(TUInt)(rle_code_end-rle_code)) return hpatch_FALSE;
#endif
                memcpy(out_data,rle_code,length);
                rle_code+=length;
                out_data+=length;
            }break;
        }
    }
    return (ctrlBuf==ctrlBuf_end)&&(rle_code==rle_code_end)&&(out_data==out_dataEnd);
}

//----------------------
//patch by stream
#undef  TUInt
#define TUInt hpatch_StreamPos_t

static TUInt _unpackUIntWithTag(const TByte** src_code,const TByte* src_code_end,const int kTagBit){
#ifdef __RUN_MEM_SAFE_CHECK
    const int kPackMaxTagBit=7;
#endif
    TUInt           value;
    TByte           code;
    const TByte*    pcode=*src_code;
    
#ifdef __RUN_MEM_SAFE_CHECK
    assert((0<=kTagBit)&&(kTagBit<=kPackMaxTagBit));
    if (src_code_end-pcode<=0) return 0;
#endif
    code=*pcode; ++pcode;
    value=code&((1<<(7-kTagBit))-1);
    if ((code&(1<<(7-kTagBit)))!=0){
        do {
#ifdef __RUN_MEM_SAFE_CHECK
            assert((value>>(sizeof(value)*8-7))==0);
            if (src_code_end==pcode) break;
#endif
            code=*pcode; ++pcode;
            value=(value<<7) | (code&((1<<7)-1));
        } while ((code&(1<<7))!=0);
    }
    (*src_code)=pcode;
    return value;
}
static TUInt unpackUIntWithTag_stream(const struct hpatch_TStreamInput*  srcData,TUInt* readFromPos,const TUInt pos_end,const int kTagBit){
    #define  kMaxPackedByte ((sizeof(TUInt)*8+6)/7+1)
    TByte tempMemBuf[kMaxPackedByte];
    TByte* code=&tempMemBuf[0];
    TUInt readSize=kMaxPackedByte;
    TUInt result;
    if (readSize>(TUInt)(srcData->streamSize-*readFromPos))
        readSize=(srcData->streamSize-*readFromPos);
    srcData->read(srcData->streamHandle,*readFromPos,code,code+readSize);
    
    result=_unpackUIntWithTag((const TByte**)&code,code+readSize,kTagBit);
    *readFromPos+=code-(&tempMemBuf[0]);
    return result;
}
#define unpackUInt_stream(serializedDiff,readFromPos,pos_end) unpackUIntWithTag_stream(serializedDiff,readFromPos,pos_end,0)

static TByte getByte_stream(const struct hpatch_TStreamInput*  srcData,TUInt readFromPos){
    TByte result=0;
    srcData->read(srcData->streamHandle,readFromPos,&result,(&result)+1);
    return result;
}

typedef struct _TBytesRle_load_stream{
    const struct hpatch_TStreamInput*   rle_stream;
    TUInt                               rle_code;
    TUInt                               rle_code_end;
    TUInt                               ctrlBuf;
    TUInt                               ctrlBuf_end;
    TUInt                               memSetSize;
    TByte                               memSetValue;
    TUInt                               memCopyPos;
    TUInt                               memCopy_end;
    TByte*                              tempMemBuf;
    TByte*                              tempMemBuf_end;
} _TBytesRle_load_stream;

static hpatch_BOOL _TBytesRle_load_stream_init(_TBytesRle_load_stream* loader,const struct hpatch_TStreamInput* rle_stream,
                                               TUInt rle_code,TUInt rle_code_end,TByte* tempMemBuf,TByte* tempMemBuf_end){
    TUInt ctrlSize;
    loader->rle_stream=rle_stream;
    loader->rle_code=rle_code;
    loader->rle_code_end=rle_code_end;
    ctrlSize= unpackUInt_stream(loader->rle_stream, &loader->rle_code, loader->rle_code_end);
#ifdef __RUN_MEM_SAFE_CHECK
    if (ctrlSize>(TUInt)(loader->rle_code_end-loader->rle_code)) return hpatch_FALSE;
#endif
    loader->ctrlBuf=loader->rle_code;
    loader->rle_code+=ctrlSize;
    loader->ctrlBuf_end=loader->rle_code;
    loader->memSetSize=0;
    loader->memSetValue=0;//nil;
    loader->memCopyPos=0;
    loader->memCopy_end=0;
    loader->tempMemBuf=tempMemBuf;
    loader->tempMemBuf_end=tempMemBuf_end;
    return !hpatch_FALSE;
}

static void memSet_add(TByte* dst,const TByte src,TUInt length){
    TUInt length_fast,i;
    
    length_fast=length&(~(TUInt)7);
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

static void _TBytesRle_load_stream_mem_add(_TBytesRle_load_stream* loader,TUInt* _decodeSize,TByte** _out_data){
    TUInt  decodeSize=*_decodeSize;
    TByte* out_data=*_out_data;
    const TUInt tempMemSize=(TUInt)(loader->tempMemBuf_end-loader->tempMemBuf);

    while ((loader->memSetSize>0)&&(decodeSize>0)) {
        TUInt memSetStep=loader->memSetSize;
        if (memSetStep>decodeSize) memSetStep=decodeSize;
        memSet_add(out_data,loader->memSetValue,memSetStep);
        out_data+=memSetStep;
        decodeSize-=memSetStep;
        loader->memSetSize-=memSetStep;
    }
    while ((loader->memCopy_end>loader->memCopyPos)&&(decodeSize>0)) {
        TUInt memCopyStep=(TUInt)(loader->memCopy_end-loader->memCopyPos);
        if (memCopyStep>decodeSize) memCopyStep=decodeSize;
        if (memCopyStep>tempMemSize) memCopyStep=tempMemSize;
        loader->rle_stream->read(loader->rle_stream->streamHandle,loader->memCopyPos,loader->tempMemBuf,loader->tempMemBuf+memCopyStep);
        addData(out_data,loader->tempMemBuf,(size_t)memCopyStep);
        out_data+=memCopyStep;
        decodeSize-=memCopyStep;
        loader->memCopyPos+=memCopyStep;
    }
    *_decodeSize=decodeSize;
    *_out_data=out_data;
}

static hpatch_BOOL _TBytesRle_load_stream_isFinish(const _TBytesRle_load_stream* loader){
    return (loader->memSetSize==0)&&(loader->memCopyPos==loader->memCopy_end)
         &&(loader->ctrlBuf==loader->ctrlBuf_end)&&(loader->rle_code==loader->rle_code_end);
}

static hpatch_BOOL _TBytesRle_load_stream_decode_add(_TBytesRle_load_stream* loader,TUInt decodeSize,TByte* out_data){
    _TBytesRle_load_stream_mem_add(loader,&decodeSize,&out_data);
    
    while ((decodeSize>0)&&(loader->ctrlBuf_end>loader->ctrlBuf)){
        enum TByteRleType type=(enum TByteRleType)(getByte_stream(loader->rle_stream,loader->ctrlBuf)>>(8-kByteRleType_bit));
        TUInt length= 1 + unpackUIntWithTag_stream(loader->rle_stream,&loader->ctrlBuf,loader->ctrlBuf_end,kByteRleType_bit);
        switch (type){
            case kByteRleType_rle0:{
                loader->memSetSize=length;
                loader->memSetValue=0;
            }break;
            case kByteRleType_rle255:{
                loader->memSetSize=length;
                loader->memSetValue=255;
            }break;
            case kByteRleType_rle:{
#ifdef __RUN_MEM_SAFE_CHECK
                if (1>(TUInt)(loader->rle_code_end-loader->rle_code)) return hpatch_FALSE;
#endif
                loader->memSetSize=length;
                loader->memSetValue=getByte_stream(loader->rle_stream, loader->rle_code);
                ++loader->rle_code;
            }break;
            case kByteRleType_unrle:{
#ifdef __RUN_MEM_SAFE_CHECK
                if (length>(TUInt)(loader->rle_code_end-loader->rle_code)) return hpatch_FALSE;
#endif
                loader->memCopyPos=loader->rle_code;
                loader->memCopy_end=loader->rle_code+length;
                loader->rle_code+=length;
            }break;
        }
        _TBytesRle_load_stream_mem_add(loader,&decodeSize,&out_data);
    }
    
    return (decodeSize==0);
}


static  hpatch_BOOL _patch_decode_stream(const struct hpatch_TStreamOutput* out_newData,TUInt writeToPos,
                                          _TBytesRle_load_stream* rle_loader,
                                          const struct hpatch_TStreamInput* srcData,
                                          TUInt readPos,TUInt decodeLength,TByte* tempMemBuf,TByte* tempMemBuf_end){
    TUInt kTempMemBufSize=(TUInt)(tempMemBuf_end-tempMemBuf);
    while (decodeLength>0){
        TUInt decodeStep=decodeLength;
        if (decodeStep>kTempMemBufSize)
            decodeStep=kTempMemBufSize;
        srcData->read(srcData->streamHandle,readPos,&tempMemBuf[0],&tempMemBuf[decodeStep]);
        if (!_TBytesRle_load_stream_decode_add(rle_loader,decodeStep,&tempMemBuf[0])) return hpatch_FALSE;
        out_newData->write(out_newData->streamHandle,writeToPos,&tempMemBuf[0],&tempMemBuf[decodeStep]);
        writeToPos+=decodeStep;
        decodeLength-=decodeStep;
        readPos+=decodeStep;
    }
    return !hpatch_FALSE;
}

hpatch_BOOL patch_stream(const struct hpatch_TStreamOutput* out_newData,
                         const struct hpatch_TStreamInput*  oldData,
                         const struct hpatch_TStreamInput*  serializedDiff){
    #define _kMinTempMemBufSize 1024*2
    TByte  _local_tempMemBuf[_kMinTempMemBufSize];
    TByte* tempMemBuf=&_local_tempMemBuf[0];
    TByte* tempMemBuf_end=tempMemBuf+_kMinTempMemBufSize;
    
    TUInt   code_lengths,code_lengths_end,code_inc_newPos,code_inc_newPos_end,
            code_inc_oldPos,code_inc_oldPos_end,code_newDataDiff,code_newDataDiff_end;
    TUInt   ctrlCount;
    struct _TBytesRle_load_stream rle_loader;
    
    assert(out_newData!=0);
    assert(out_newData->write!=0);
    assert(oldData!=0);
    assert(oldData->read!=0);
    assert(serializedDiff!=0);
    assert(serializedDiff->read!=0);
    assert(serializedDiff->streamSize>0);
    
    {   //head
        TUInt lengthSize,inc_newPosSize,inc_oldPosSize,newDataDiffSize;
        const TUInt diffPos_end=serializedDiff->streamSize;
        TUInt rle_tempMemBuf_size=(TUInt)(tempMemBuf_end-tempMemBuf)>>1;
        TUInt diffPos0=0;
        ctrlCount=unpackUInt_stream(serializedDiff,&diffPos0,diffPos_end);
        lengthSize=unpackUInt_stream(serializedDiff,&diffPos0,diffPos_end);
        inc_newPosSize=unpackUInt_stream(serializedDiff,&diffPos0,diffPos_end);
        inc_oldPosSize=unpackUInt_stream(serializedDiff,&diffPos0,diffPos_end);
        newDataDiffSize=unpackUInt_stream(serializedDiff,&diffPos0,diffPos_end);
#ifdef __RUN_MEM_SAFE_CHECK
        if (lengthSize>(TUInt)(serializedDiff->streamSize-diffPos0)) return hpatch_FALSE;
#endif
        code_lengths=diffPos0;     diffPos0+=lengthSize;
        code_lengths_end=diffPos0;
#ifdef __RUN_MEM_SAFE_CHECK
        if (inc_newPosSize>(TUInt)(serializedDiff->streamSize-diffPos0)) return hpatch_FALSE;
#endif
        code_inc_newPos=diffPos0; diffPos0+=inc_newPosSize;
        code_inc_newPos_end=diffPos0;
#ifdef __RUN_MEM_SAFE_CHECK
        if (inc_oldPosSize>(TUInt)(serializedDiff->streamSize-diffPos0)) return hpatch_FALSE;
#endif
        code_inc_oldPos=diffPos0; diffPos0+=inc_oldPosSize;
        code_inc_oldPos_end=diffPos0;
#ifdef __RUN_MEM_SAFE_CHECK
        if (newDataDiffSize>(TUInt)(serializedDiff->streamSize-diffPos0)) return hpatch_FALSE;
#endif
        code_newDataDiff=diffPos0; diffPos0+=newDataDiffSize;
        code_newDataDiff_end=diffPos0;
        
        if (!_TBytesRle_load_stream_init(&rle_loader,serializedDiff,diffPos0,diffPos_end,
                                  tempMemBuf,tempMemBuf+rle_tempMemBuf_size)) return hpatch_FALSE;
        tempMemBuf+=rle_tempMemBuf_size;
    }
    
    {   //patch
        const TUInt newDataSize=out_newData->streamSize;
        TUInt oldPosBack=0;
        TUInt newPosBack=0;
        TUInt i;
        for (i=0; i<ctrlCount; ++i){
            TUInt oldPos,inc_oldPos,inc_oldPos_sign;
            
            TUInt copyLength=unpackUInt_stream(serializedDiff,&code_inc_newPos,code_inc_newPos_end);
            TUInt addLength=unpackUInt_stream(serializedDiff,&code_lengths, code_lengths_end);
#ifdef __RUN_MEM_SAFE_CHECK
            if (code_inc_oldPos>=code_inc_oldPos_end) return hpatch_FALSE;
#endif
            inc_oldPos_sign=getByte_stream(serializedDiff,code_inc_oldPos)>>(8-1);
            inc_oldPos=unpackUIntWithTag_stream(serializedDiff,&code_inc_oldPos, code_inc_oldPos_end, 1);
            if (inc_oldPos_sign==0)
                oldPos=oldPosBack+inc_oldPos;
            else
                oldPos=oldPosBack-inc_oldPos;
            if (copyLength>0){
#ifdef __RUN_MEM_SAFE_CHECK
                if (copyLength>(TUInt)(newDataSize-newPosBack)) return hpatch_FALSE;
                if (copyLength>(TUInt)(code_newDataDiff_end-code_newDataDiff)) return hpatch_FALSE;
#endif
                if (!_patch_decode_stream(out_newData,newPosBack,&rle_loader,serializedDiff,code_newDataDiff,copyLength,
                                          tempMemBuf,tempMemBuf_end)) return hpatch_FALSE;
                code_newDataDiff+=copyLength;
                newPosBack+=copyLength;
            }
#ifdef __RUN_MEM_SAFE_CHECK
            if ((addLength>(TUInt)(newDataSize-newPosBack))) return hpatch_FALSE;
            if ((oldPos>(oldData->streamSize))||(addLength>(TUInt)(oldData->streamSize-oldPos))) return hpatch_FALSE;
#endif
            if (!_patch_decode_stream(out_newData,newPosBack,&rle_loader,oldData,oldPos,addLength,
                                      tempMemBuf,tempMemBuf_end)) return hpatch_FALSE;
            oldPosBack=oldPos;
            newPosBack+=addLength;
        }
        
        if (newPosBack<newDataSize){
            TUInt copyLength=newDataSize-newPosBack;
#ifdef __RUN_MEM_SAFE_CHECK
            if (copyLength>(TUInt)(code_newDataDiff_end-code_newDataDiff)) return hpatch_FALSE;
#endif
            if (!_patch_decode_stream(out_newData,newPosBack,&rle_loader,serializedDiff,code_newDataDiff,copyLength,
                                      tempMemBuf,tempMemBuf_end)) return hpatch_FALSE;
            code_newDataDiff+=copyLength;
            newPosBack=newDataSize;
        }
    }
    
    return  _TBytesRle_load_stream_isFinish(&rle_loader)
            &&(code_lengths==code_lengths_end)&&(code_inc_newPos==code_inc_newPos_end)
            &&(code_inc_oldPos==code_inc_oldPos_end)&&(code_newDataDiff==code_newDataDiff_end);
}



