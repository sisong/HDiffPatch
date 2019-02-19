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
#include <string.h> //memcpy
#include <stdexcept> //std::runtime_error
#include "../../diff.h" //for stream type

#define checki(value,info) { if (!(value)) { throw std::runtime_error(info); } }
#define check(value) checki(value,"check "#value" error!")

namespace hdiff_private{
TCompressedStream::TCompressedStream(const hpatch_TStreamOutput*  _out_code,
                                     hpatch_StreamPos_t _writePos,hpatch_StreamPos_t _kLimitOutCodeSize,
                                     const hpatch_TStreamInput*   _in_stream)
:out_code(_out_code),out_pos(_writePos),out_posLimitEnd(_writePos+_kLimitOutCodeSize),
in_stream(_in_stream),_writeToPos_back(0),_is_overLimit(false){
    this->streamImport=this;
    this->streamSize=0;
    this->read_writed=0;
    this->write=_write_code;
}

hpatch_BOOL TCompressedStream::_write_code(const hpatch_TStreamOutput* stream,hpatch_StreamPos_t writeToPos,
                                           const unsigned char* data,const unsigned char* data_end){
    assert(data<data_end);
    TCompressedStream* self=(TCompressedStream*)stream->streamImport;
    checki(self->_writeToPos_back==writeToPos,"TCompressedStream::write() writeToPos error!");
    size_t dataLen=(size_t)(data_end-data);
    self->_writeToPos_back=writeToPos+dataLen;
    
    if ((self->_is_overLimit)||(self->out_pos+dataLen>self->out_posLimitEnd)){
        self->_is_overLimit=true;
    }else{
        if (!self->out_code->write(self->out_code,self->out_pos,data,data_end)) return hpatch_FALSE;
        self->out_pos+=dataLen;
    }
    return hpatch_TRUE;
}


TCoversStream::TCoversStream(const TCovers& _covers,hpatch_StreamPos_t cover_buf_size)
:covers(_covers),curCodePos(0),curCodePos_end(0),
readedCoverCount(0),lastOldEnd(0),lastNewEnd(0),_readFromPos_back(0){
    assert(kCodeBufSize>=hpatch_kMaxPackedUIntBytes*3);
    _code_mem.realloc(kCodeBufSize);
    this->streamImport=this;
    this->streamSize=cover_buf_size;
    this->read=_read;
}

TCoversStream::~TCoversStream(){
}

#define __private_packCover(_TUInt,packWithTag,pack,dst,dst_end,cover,lastOldEnd,lastNewEnd){ \
    if (cover.oldPos>=lastOldEnd) /*save inc_oldPos*/ \
        packWithTag(dst,dst_end,(_TUInt)(cover.oldPos-lastOldEnd), 0, 1); \
    else \
        packWithTag(dst,dst_end,(_TUInt)(lastOldEnd-cover.oldPos), 1, 1);/*sub safe*/ \
    pack(dst,dst_end,(_TUInt)(cover.newPos-lastNewEnd)); /*save inc_newPos*/ \
    pack(dst,dst_end,(_TUInt)cover.length); \
}

hpatch_BOOL TCoversStream::_read(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                 unsigned char* out_data,unsigned char* out_data_end){
    TCoversStream* self=(TCoversStream*)stream->streamImport;
    if (readFromPos==0){
        self->curCodePos=0;
        self->curCodePos_end=0;
        self->lastOldEnd=0;
        self->lastNewEnd=0;
        self->readedCoverCount=0;
    }else{
        checki(self->_readFromPos_back==readFromPos,"TCoversStream::read() readFromPos error!");
    }
    self->_readFromPos_back=readFromPos+(size_t)(out_data_end-out_data);
    
    size_t n=self->covers.coverCount();
    while (out_data<out_data_end) {
        size_t curLen=self->curCodePos_end-self->curCodePos;
        if (curLen>0){
            size_t readLen=(out_data_end-out_data);
            if (readLen>curLen) readLen=curLen;
            const unsigned char* pcode=self->_code_mem.data()+self->curCodePos;
            memcpy(out_data,pcode,readLen);
            out_data+=readLen;
            self->curCodePos+=readLen;
        }else{
            size_t cur_index=self->readedCoverCount;
            if (cur_index>=n) return hpatch_FALSE; //error
            unsigned char* pcode_cur=self->_code_mem.data();
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
            self->curCodePos_end=pcode_cur-self->_code_mem.data();
        }
    }
    return hpatch_TRUE;
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
    this->streamImport=this;
    this->streamSize=newDataDiff_size;
    this->read=_read;
}

hpatch_BOOL TNewDataDiffStream::_read(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                      unsigned char* out_data,unsigned char* out_data_end){
    TNewDataDiffStream* self=(TNewDataDiffStream*)stream->streamImport;
    if (readFromPos==0){
        self->curNewPos=0;
        self->curNewPos_end=0;
        self->lastNewEnd=0;
        self->readedCoverCount=0;
    }else{
        checki(self->_readFromPos_back==readFromPos,"TNewDataDiffStream::read() readFromPos error!");
    }
    self->_readFromPos_back=readFromPos+(size_t)(out_data_end-out_data);
    
    size_t n=self->covers.coverCount();
    while (out_data<out_data_end) {
        hpatch_StreamPos_t curLen=self->curNewPos_end-self->curNewPos;
        if (curLen>0){
            size_t readLen=(out_data_end-out_data);
            if (readLen>curLen) readLen=(size_t)curLen;
            if (!self->newData->read(self->newData,self->curNewPos,
                                     out_data,out_data+readLen)) return hpatch_FALSE;
            out_data+=readLen;
            self->curNewPos+=readLen;
        }else{
            TCover curCover;
            if (self->readedCoverCount==n){
                if (self->lastNewEnd>=self->newData->streamSize) return hpatch_FALSE; //error;
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
    return hpatch_TRUE;
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


TDiffStream::TDiffStream(const hpatch_TStreamOutput* _out_diff)
:out_diff(_out_diff),writePos(0){
    _temp_mem.realloc(kBufSize);
}

TDiffStream::~TDiffStream(){
}
    
void TDiffStream::pushBack(const unsigned char* src,size_t n){
    if (n==0) return;
    checki(out_diff->write(out_diff,writePos,src,src+n),
           "TDiffStream::pushBack() write stream error!");
    writePos+=n;
}


void TDiffStream::packUInt(hpatch_StreamPos_t uValue){
    unsigned char  codeBuf[hpatch_kMaxPackedUIntBytes];
    unsigned char* codeEnd=codeBuf;
    check(hpatch_packUInt(&codeEnd,codeBuf+hpatch_kMaxPackedUIntBytes,uValue));
    pushBack(codeBuf,(size_t)(codeEnd-codeBuf));
}

void TDiffStream::_packUInt_limit(hpatch_StreamPos_t uValue,size_t limitOutSize){
    check(limitOutSize<=hpatch_kMaxPackedUIntBytes);
    unsigned char  _codeBuf[hpatch_kMaxPackedUIntBytes];
    packUInt_fixSize(_codeBuf,_codeBuf+limitOutSize,uValue);
    pushBack(_codeBuf,limitOutSize);
}

void TDiffStream::packUInt_update(const TPlaceholder& pos,hpatch_StreamPos_t uValue){
    hpatch_StreamPos_t writePosBack=writePos;
    writePos=pos.pos;
    _packUInt_limit(uValue,(size_t)(pos.pos_end-pos.pos));
    assert(writePos==pos.pos_end);
    writePos=writePosBack;
}

void TDiffStream::_pushStream(const hpatch_TStreamInput* stream){
    unsigned char* buf=_temp_mem.data();
    hpatch_StreamPos_t sumReadedLen=0;
    while (sumReadedLen<stream->streamSize) {
        size_t readLen=kBufSize;
        if (readLen+sumReadedLen>stream->streamSize)
            readLen=(size_t)(stream->streamSize-sumReadedLen);
        checki(stream->read(stream,sumReadedLen,buf,buf+readLen),
               "TDiffStream::_pushStream() stream->read() error!");
        this->pushBack(buf,readLen);
        sumReadedLen+=readLen;
    }
}

void TDiffStream::pushStream(const hpatch_TStreamInput* stream,
                             const hdiff_TCompress* compressPlugin,
                             const TPlaceholder& update_compress_sizePos){
    if ((compressPlugin)&&(stream->streamSize>0)){
        hpatch_StreamPos_t kLimitOutCodeSize=stream->streamSize-1;
        TCompressedStream  out_stream(out_diff,writePos,kLimitOutCodeSize,stream);
        hpatch_StreamPos_t compressed_size=
                                compressPlugin->compress(compressPlugin,&out_stream,stream);
        if (out_stream.is_overLimit()||(compressed_size==0)||(compressed_size>kLimitOutCodeSize)){
            compressed_size=0;//NOTICE: compress is canceled
            _pushStream(stream);
        }else{
            writePos+=compressed_size;
        }
        packUInt_update(update_compress_sizePos,compressed_size);
    }else if (stream->streamSize>0){
        _pushStream(stream);
    }
}

TStreamClip::TStreamClip(const hpatch_TStreamInput* stream,
                         hpatch_StreamPos_t clipBeginPos,hpatch_StreamPos_t clipEndPos,
                         hpatch_TDecompress* decompressPlugin,hpatch_StreamPos_t uncompressSize)
:_src(stream),_src_begin(clipBeginPos),_src_end(clipEndPos),
_decompressPlugin(decompressPlugin),_read_uncompress_pos(0),_decompressHandle(0){
    assert(clipBeginPos<=clipEndPos);
    assert(clipEndPos<=stream->streamSize);
    if ((uncompressSize==0)&&(decompressPlugin==0))
        uncompressSize=clipEndPos-clipBeginPos;
    this->streamImport=this;
    this->streamSize=uncompressSize;
    this->read=_clip_read;
    
    if (decompressPlugin)
        openDecompressHandle();
}
    
TStreamClip::~TStreamClip(){
    closeDecompressHandle();
}
    
    
void TStreamClip::closeDecompressHandle(){
    hpatch_decompressHandle handle=_decompressHandle;
    _decompressHandle=0;
    if (handle)
        check(_decompressPlugin->close(_decompressPlugin,handle));
}
void TStreamClip::openDecompressHandle(){
    assert(_decompressHandle==0);
    assert(_decompressPlugin!=0);
    _decompressHandle=_decompressPlugin->open(_decompressPlugin,this->streamSize,_src,_src_begin,_src_end);
    check(_decompressHandle!=0);
}

hpatch_BOOL TStreamClip::_clip_read(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                    unsigned char* out_data,unsigned char* out_data_end){
    TStreamClip* self=(TStreamClip*)stream->streamImport;
    assert(out_data<out_data_end);
    if (readFromPos!=self->_read_uncompress_pos){
        if (self->_decompressPlugin){
            check(readFromPos==0); //not support random read compressed cdata
            //reset
            self->closeDecompressHandle();
            self->openDecompressHandle();
            self->_read_uncompress_pos=0;
        }else{
            self->_read_uncompress_pos=readFromPos;
        }
    }
    size_t readLen=out_data_end-out_data;
    assert(readFromPos+readLen <= self->streamSize);
    self->_read_uncompress_pos+=readLen;
    
    if (self->_decompressPlugin){
        return self->_decompressPlugin->decompress_part(self->_decompressHandle,out_data,out_data_end);
    }else{
        return self->_src->read(self->_src,self->_src_begin+readFromPos,out_data,out_data_end);
    }
}
    
}//namespace hdiff_private
