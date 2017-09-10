//stream_serialize.cpp
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
#include "stream_serialize.h"
#include <assert.h>
#include <stdlib.h> //malloc free
#include <string.h> //memcpy
#include <stdexcept> //std::runtime_error
#include "../../diff.h" //for stream type
namespace hdiff_private{
TCompressedStream::TCompressedStream(const hpatch_TStreamOutput*  _out_code,
                                     hpatch_StreamPos_t _writePos,hpatch_StreamPos_t _kLimitOutCodeSize,
                                     const hpatch_TStreamInput*   _in_stream)
:out_code(_out_code),out_pos(_writePos),out_posLimitEnd(_writePos+_kLimitOutCodeSize),
in_stream(_in_stream),_writeToPos_back(0),_is_overLimit(false){
    this->streamHandle=this;
    this->streamSize=0;
    this->write=_write_code;
}

long TCompressedStream::_write_code(hpatch_TStreamOutputHandle streamHandle,
                                    const hpatch_StreamPos_t writeToPos,
                                    const unsigned char* data,const unsigned char* data_end){
    assert(data<data_end);
    TCompressedStream* self=(TCompressedStream*)streamHandle;
    if (self->_writeToPos_back!=writeToPos)
        throw std::runtime_error("TCompressedStream::write() writeToPos error!");
    size_t dataLen=(size_t)(data_end-data);
    self->_writeToPos_back=writeToPos+dataLen;
    
    if (self->out_pos+dataLen>self->out_posLimitEnd){
        self->_is_overLimit=true;
        return hdiff_kStreamOutputCancel;
    }
    if ((long)dataLen!=self->out_code->write(self->out_code->streamHandle,self->out_pos,
                                             data,data_end)) return -1;
    self->out_pos+=dataLen;
    return (long)dataLen;
}


TCoversStream::TCoversStream(const TCovers& _covers,hpatch_StreamPos_t cover_buf_size)
:covers(_covers),_code_buf(0),curCodePos(0),curCodePos_end(0),
readedCoverCount(0),lastOldEnd(0),lastNewEnd(0),_readFromPos_back(0){
    assert(kCodeBufSize>=hpatch_kMaxPackedUIntBytes*3);
    _code_buf=(unsigned char*)malloc(kCodeBufSize);
    if (!_code_buf)
        throw std::runtime_error("TCoversStream::TCoversStream() malloc() error!");
    this->streamHandle=this;
    this->streamSize=cover_buf_size;
    this->read=_read;
}

TCoversStream::~TCoversStream(){
    if (_code_buf) free(_code_buf);
}

#define __private_packCover(_TUInt,packWithTag,pack,dst,dst_end,cover,lastOldEnd,lastNewEnd){ \
    if (cover.oldPos>=lastOldEnd) /*save inc_oldPos*/ \
        packWithTag(dst,dst_end,(_TUInt)(cover.oldPos-lastOldEnd), 0, 1); \
    else \
        packWithTag(dst,dst_end,(_TUInt)(lastOldEnd-cover.oldPos), 1, 1);/*sub safe*/ \
    pack(dst,dst_end,(_TUInt)(cover.newPos-lastNewEnd)); /*save inc_newPos*/ \
    pack(dst,dst_end,(_TUInt)cover.length); \
}

long TCoversStream::_read(hpatch_TStreamInputHandle streamHandle,
                          const hpatch_StreamPos_t readFromPos,
                          unsigned char* out_data,unsigned char* out_data_end){
    TCoversStream* self=(TCoversStream*)streamHandle;
    if (readFromPos==0){
        self->curCodePos=0;
        self->curCodePos_end=0;
        self->lastOldEnd=0;
        self->lastNewEnd=0;
        self->readedCoverCount=0;
    }else{
        if (self->_readFromPos_back!=readFromPos)
            throw std::runtime_error("TCoversStream::read() readFromPos error!");
    }
    self->_readFromPos_back=readFromPos+(size_t)(out_data_end-out_data);
    
    size_t n=self->covers.coverCount();
    size_t sumReadedLen=0;
    while (out_data<out_data_end) {
        size_t curLen=self->curCodePos_end-self->curCodePos;
        if (curLen>0){
            size_t readLen=(out_data_end-out_data);
            if (readLen>curLen) readLen=curLen;
            const unsigned char* pcode=&self->_code_buf[self->curCodePos];
            memcpy(out_data,pcode,readLen);
            sumReadedLen+=readLen;
            out_data+=readLen;
            self->curCodePos+=readLen;
        }else{
            size_t cur_index=self->readedCoverCount;
            if (cur_index>=n) return -1; //error
            unsigned char* pcode_cur=&self->_code_buf[0];
            unsigned char* pcode_end=pcode_cur+kCodeBufSize;
            for (;((size_t)(pcode_end-pcode_cur)>=hpatch_kMaxPackedUIntBytes*3)
                 &&(cur_index<n);++cur_index) {
                TCover cover;
                self->covers.covers(cur_index,&cover);
                __private_packCover(hpatch_StreamPos_t,hpatch_packUIntWithTag,hpatch_packUInt,
                                    &pcode_cur,pcode_end,cover,self->lastOldEnd,self->lastNewEnd);
                self->lastOldEnd=cover.oldPos+cover.length;//! +length
                self->lastNewEnd=cover.newPos+cover.length;
            }
            self->readedCoverCount=cur_index;
            self->curCodePos=0;
            self->curCodePos_end=pcode_cur-&self->_code_buf[0];
        }
    }
    return (long)sumReadedLen;
};


inline static void _packUIntWithTag_size(hpatch_StreamPos_t* curSize,int _,
                                         hpatch_StreamPos_t uValue,int highTag,const int kTagBit){
    *curSize+=hpatch_packUIntWithTag_size(uValue,kTagBit);
}
inline static void _packUInt_size(hpatch_StreamPos_t* curSize,int _, hpatch_StreamPos_t uValue){
    *curSize+=hpatch_packUIntWithTag_size(uValue,0);
}

hpatch_StreamPos_t TCoversStream::getDataSize(const TCovers& covers){
    size_t n=covers.coverCount();
    hpatch_StreamPos_t cover_buf_size=0;
    hpatch_StreamPos_t lastOldEnd=0;
    hpatch_StreamPos_t lastNewEnd=0;
    for (size_t i=0; i<n; ++i) {
        TCover cover;
        covers.covers(i,&cover);
        assert(cover.newPos>=lastNewEnd);
        __private_packCover(hpatch_StreamPos_t,_packUIntWithTag_size,_packUInt_size,
                            &cover_buf_size,0,cover,lastOldEnd,lastNewEnd);
        lastOldEnd=cover.oldPos+cover.length;//! +length
        lastNewEnd=cover.newPos+cover.length;
    }
    return cover_buf_size;
}



TNewDataDiffStream::TNewDataDiffStream(const TCovers& _covers,const hpatch_TStreamInput* _newData,
                                       hpatch_StreamPos_t newDataDiff_size)
:covers(_covers),newData(_newData),curNewPos(0),curNewPos_end(0),
lastNewEnd(0),readedCoverCount(0),_readFromPos_back(0){
    this->streamHandle=this;
    this->streamSize=newDataDiff_size;
    this->read=_read;
}

long TNewDataDiffStream::_read(hpatch_TStreamInputHandle streamHandle,
                               const hpatch_StreamPos_t readFromPos,
                               unsigned char* out_data,unsigned char* out_data_end){
    TNewDataDiffStream* self=(TNewDataDiffStream*)streamHandle;
    if (readFromPos==0){
        self->curNewPos=0;
        self->curNewPos_end=0;
        self->lastNewEnd=0;
        self->readedCoverCount=0;
    }else{
        if (self->_readFromPos_back!=readFromPos)
            throw std::runtime_error("TNewDataDiffStream::read() readFromPos error!");
    }
    self->_readFromPos_back=readFromPos+(size_t)(out_data_end-out_data);
    
    size_t n=self->covers.coverCount();
    size_t sumReadedLen=0;
    while (out_data<out_data_end) {
        hpatch_StreamPos_t curLen=self->curNewPos_end-self->curNewPos;
        if (curLen>0){
            size_t readLen=(out_data_end-out_data);
            if (readLen>curLen) readLen=(size_t)curLen;
            if ((long)readLen!=self->newData->read(self->newData->streamHandle,self->curNewPos,
                                                   out_data,out_data+readLen)) return -1;
            sumReadedLen+=readLen;
            out_data+=readLen;
            self->curNewPos+=readLen;
        }else{
            TCover curCover;
            if (self->readedCoverCount==n){
                if (self->lastNewEnd>=self->newData->streamSize) return -1; //error;
                curCover.newPos=self->newData->streamSize;
                curCover.length=0;
                curCover.oldPos=0;
            }else{
                self->covers.covers(self->readedCoverCount,&curCover);
                ++self->readedCoverCount;
            }
            assert(self->lastNewEnd<=curCover.newPos);
            self->curNewPos=self->lastNewEnd;
            self->curNewPos_end=curCover.newPos;
            self->lastNewEnd=curCover.newPos+curCover.length;
        }
    }
    return (long)sumReadedLen;
};

hpatch_StreamPos_t TNewDataDiffStream::getDataSize(const TCovers& covers,hpatch_StreamPos_t newDataSize){
    size_t n=covers.coverCount();
    hpatch_StreamPos_t newDataDiff_size=0;
    hpatch_StreamPos_t lastNewEnd=0;
    for (size_t i=0; i<n; ++i) {
        TCover cover;
        covers.covers(i,&cover);
        assert(cover.newPos>=lastNewEnd);
        newDataDiff_size+=(hpatch_StreamPos_t)(cover.newPos-lastNewEnd);
        lastNewEnd=cover.newPos+cover.length;
    }
    assert(newDataSize>=lastNewEnd);
    newDataDiff_size+=(hpatch_StreamPos_t)(newDataSize-lastNewEnd);
    return newDataDiff_size;
}


TDiffStream::TDiffStream(hpatch_TStreamOutput* _out_diff,const TCovers& _covers)
:out_diff(_out_diff),covers(_covers),writePos(0),_temp_buf(0){
    _temp_buf=(unsigned char*)malloc(kBufSize);
    if (!_temp_buf) throw std::runtime_error("TDiffStream::TDiffStream() malloc() error!");
}

TDiffStream::~TDiffStream(){
    if (_temp_buf) free(_temp_buf);
}
    
void TDiffStream::pushBack(const unsigned char* src,size_t n){
    if (n==0) return;
    if ((long)n!=out_diff->write(out_diff->streamHandle,writePos,src,src+n))
        throw std::runtime_error("TDiffStream::pushBack() write stream error!");
    writePos+=n;
}


void TDiffStream::packUInt(hpatch_StreamPos_t uValue){
    unsigned char  codeBuf[hpatch_kMaxPackedUIntBytes];
    unsigned char* codeEnd=codeBuf;
    if (!hpatch_packUInt(&codeEnd,codeBuf+hpatch_kMaxPackedUIntBytes,uValue)) throw uValue;
    pushBack(codeBuf,(size_t)(codeEnd-codeBuf));
}

void TDiffStream::_packUInt_limit(hpatch_StreamPos_t uValue,size_t limitOutSize){
    if (limitOutSize>hpatch_kMaxPackedUIntBytes) throw limitOutSize;
    unsigned char  _codeBuf[hpatch_kMaxPackedUIntBytes*2];
    unsigned char* codeBegin=_codeBuf+hpatch_kMaxPackedUIntBytes;
    unsigned char* codeEnd=codeBegin;
    if (!hpatch_packUInt(&codeEnd,codeBegin+hpatch_kMaxPackedUIntBytes,uValue)) throw uValue;
    if ((size_t)(codeEnd-codeBegin)>limitOutSize) throw limitOutSize;
    while ((size_t)(codeEnd-codeBegin)<limitOutSize) {
        --codeBegin;
        codeBegin[0]=(1<<7);
    }
    pushBack(codeBegin,(size_t)(codeEnd-codeBegin));
}

void TDiffStream::packUInt_update(const TPlaceholder& pos,hpatch_StreamPos_t uValue){
    hpatch_StreamPos_t writePosBack=writePos;
    writePos=pos.pos;
    _packUInt_limit(uValue,(size_t)(pos.pos_end-pos.pos));
    assert(writePos==pos.pos_end);
    writePos=writePosBack;
}

void TDiffStream::_pushStream(const hpatch_TStreamInput* stream){
    unsigned char* buf=&_temp_buf[0];
    hpatch_StreamPos_t sumReadedLen=0;
    while (sumReadedLen<stream->streamSize) {
        size_t readLen=kBufSize;
        if (readLen+sumReadedLen>stream->streamSize)
            readLen=(size_t)(stream->streamSize-sumReadedLen);
        if ((long)readLen!=stream->read(stream->streamHandle,sumReadedLen,buf,buf+readLen))
            throw std::runtime_error("TDiffStream::_pushStream() stream->read() error!");
        this->pushBack(buf,readLen);
        sumReadedLen+=readLen;
    }
}

void TDiffStream::pushStream(const hpatch_TStreamInput* stream,
                             const hdiff_TStreamCompress* compressPlugin,
                             const TPlaceholder& update_compress_sizePos){
    if ((compressPlugin)&&(stream->streamSize>0)){
        hpatch_StreamPos_t kLimitOutCodeSize=stream->streamSize-1;
        TCompressedStream  out_stream(out_diff,writePos,kLimitOutCodeSize,stream);
        hpatch_StreamPos_t compressed_size=
                                compressPlugin->compress_stream(compressPlugin,&out_stream,stream);
        writePos+=compressed_size;
        if (compressed_size==0)//NOTICE: compress is canceled
            _pushStream(stream);
        packUInt_update(update_compress_sizePos,compressed_size);
    }else if (stream->streamSize>0){
        _pushStream(stream);
    }
}

}//namespace hdiff_private
