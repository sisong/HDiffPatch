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
#include "string.h" //memcpy memset size_t
#include "assert.h" //assert

typedef unsigned char TByte;

//typedef unsigned int  TUInt;
typedef size_t  TUInt;

//PATCH_RUN_MEM_SAFE_CHECK用来启动内存访问越界检查.
#define PATCH_RUN_MEM_SAFE_CHECK

static hpatch_BOOL _bytesRle_load(TByte* out_data,TByte* out_dataEnd,const TByte* rle_code,const TByte* rle_code_end);
static void addData(TByte* dst,const TByte* src,TUInt length);
static TUInt unpackUIntWithTag(const TByte** src_code,const TByte* src_code_end,const int kTagBit);
#define unpackUInt(src_code,src_code_end) unpackUIntWithTag(src_code,src_code_end,0)

hpatch_BOOL patch(TByte* newData,TByte* newData_end,
            const TByte* oldData,const TByte* oldData_end,
            const TByte* serializedDiff,const TByte* serializedDiff_end){
    const TByte *code_lengths, *code_lengths_end,
                *code_inc_newPos, *code_inc_newPos_end,
                *code_inc_oldPos, *code_inc_oldPos_end,
                *code_newDataDiff, *code_newDataDiff_end;
    TUInt       ctrlCount;
    
    assert(newData<=newData_end);
    assert(oldData<=oldData_end);
    assert(serializedDiff<=serializedDiff_end);
    {   //head
        TUInt lengthSize,inc_newPosSize,inc_oldPosSize,newDataDiffSize;
        
        ctrlCount=unpackUInt(&serializedDiff, serializedDiff_end);
        lengthSize=unpackUInt(&serializedDiff, serializedDiff_end);
        inc_newPosSize=unpackUInt(&serializedDiff, serializedDiff_end);
        inc_oldPosSize=unpackUInt(&serializedDiff, serializedDiff_end);
        newDataDiffSize=unpackUInt(&serializedDiff, serializedDiff_end);
#ifdef PATCH_RUN_MEM_SAFE_CHECK
        if (lengthSize>(TUInt)(serializedDiff_end-serializedDiff)) return hpatch_FALSE;
#endif
        code_lengths=serializedDiff;     serializedDiff+=lengthSize;
        code_lengths_end=serializedDiff;
#ifdef PATCH_RUN_MEM_SAFE_CHECK
        if (inc_newPosSize>(TUInt)(serializedDiff_end-serializedDiff)) return hpatch_FALSE;
#endif
        code_inc_newPos=serializedDiff; serializedDiff+=inc_newPosSize;
        code_inc_newPos_end=serializedDiff;
#ifdef PATCH_RUN_MEM_SAFE_CHECK
        if (inc_oldPosSize>(TUInt)(serializedDiff_end-serializedDiff)) return hpatch_FALSE;
#endif
        code_inc_oldPos=serializedDiff; serializedDiff+=inc_oldPosSize;
        code_inc_oldPos_end=serializedDiff;
#ifdef PATCH_RUN_MEM_SAFE_CHECK
        if (newDataDiffSize>(TUInt)(serializedDiff_end-serializedDiff)) return hpatch_FALSE;
#endif
        code_newDataDiff=serializedDiff; serializedDiff+=newDataDiffSize;
        code_newDataDiff_end=serializedDiff;
    }
    
    //decode rle ; rle data begin==serializedDiff;
    if (!_bytesRle_load(newData, newData_end, serializedDiff, serializedDiff_end))
        return hpatch_FALSE;
    
    {   //patch
        TUInt i,oldPosBack,newPos_end,newDataSize;
        
        oldPosBack=0;
        newPos_end=0;
        for (i=0; i<ctrlCount; ++i){
            TUInt inc_newPos,newPos,addLength;
            TUInt oldPos,inc_oldPos,inc_oldPos_sign,copyLength;
            
            inc_newPos=unpackUInt(&code_inc_newPos, code_inc_newPos_end);
            newPos=newPos_end+inc_newPos;
            addLength=unpackUInt(&code_lengths, code_lengths_end);
            inc_oldPos_sign=(*code_inc_oldPos)>>(8-1);
            inc_oldPos=unpackUIntWithTag(&code_inc_oldPos, code_inc_oldPos_end, 1);
            if (inc_oldPos_sign==0)
                oldPos=oldPosBack+inc_oldPos;
            else
                oldPos=oldPosBack-inc_oldPos;
#ifdef PATCH_RUN_MEM_SAFE_CHECK
            if ((oldPos>(TUInt)(oldData_end-oldData))||(addLength>(TUInt)(oldData_end-oldData-oldPos))) return hpatch_FALSE;
            if ((newPos>(TUInt)(newData_end-newData))||(addLength>(TUInt)(newData_end-newData-newPos))) return hpatch_FALSE;
#endif
            if (newPos>newPos_end){
                copyLength=newPos-newPos_end;
#ifdef PATCH_RUN_MEM_SAFE_CHECK
                if (copyLength>(TUInt)(code_newDataDiff_end-code_newDataDiff)) return hpatch_FALSE;
#endif
                memcpy(newData+newPos_end,code_newDataDiff,copyLength);
                code_newDataDiff+=copyLength;
            }
            addData(newData+newPos,oldData+oldPos,addLength);
            oldPosBack=oldPos;
            newPos_end=newPos+addLength;
        }
        
        newDataSize=(TUInt)(newData_end-newData);
        if (newPos_end<newDataSize){
            TUInt copyLength=newDataSize-newPos_end;
#ifdef PATCH_RUN_MEM_SAFE_CHECK
            if (copyLength>(TUInt)(code_newDataDiff_end-code_newDataDiff)) return hpatch_FALSE;
#endif
            memcpy(newData+newPos_end,code_newDataDiff,copyLength);
            code_newDataDiff+=copyLength;
            newPos_end=newDataSize;
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
#ifdef PATCH_RUN_MEM_SAFE_CHECK
    const int kPackMaxTagBit=7;
#endif
    TUInt           value;
    TByte           code;
    const TByte*    pcode=*src_code;
    
#ifdef PATCH_RUN_MEM_SAFE_CHECK
    assert((0<=kTagBit)&&(kTagBit<=kPackMaxTagBit));
    if (src_code_end-pcode<=0) return 0;
#endif
    code=*pcode; ++pcode;
    value=code&((1<<(7-kTagBit))-1);
    if ((code&(1<<(7-kTagBit)))!=0){
        do {
#ifdef PATCH_RUN_MEM_SAFE_CHECK
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
#ifdef PATCH_RUN_MEM_SAFE_CHECK
    if (ctrlSize>(TUInt)(rle_code_end-rle_code)) return hpatch_FALSE;
#endif
    ctrlBuf=rle_code;
    rle_code+=ctrlSize;
    ctrlBuf_end=rle_code;
    while (ctrlBuf_end-ctrlBuf>0){
        enum TByteRleType type=(enum TByteRleType)((*ctrlBuf)>>(8-kByteRleType_bit));
        TUInt length= 1 + unpackUIntWithTag(&ctrlBuf,ctrlBuf_end,kByteRleType_bit);
#ifdef PATCH_RUN_MEM_SAFE_CHECK
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
#ifdef PATCH_RUN_MEM_SAFE_CHECK
                if (1>(TUInt)(rle_code_end-rle_code)) return hpatch_FALSE;
#endif
                memset(out_data,*rle_code,length);
                ++rle_code;
                out_data+=length;
            }break;
            case kByteRleType_unrle:{
#ifdef PATCH_RUN_MEM_SAFE_CHECK
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

