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
#include "../../diff.h" //for stream type

TCompressedStreamInput::TCompressedStreamInput(const hpatch_TStreamInput* _stream,
                                               hdiff_TStreamCompress* _compressPlugin)
:data_stream(_stream),compressPlugin(_compressPlugin),compresser(0),
_data_buf(kReadBufSize),curReadPos(0),curCodePos(0){
    this->streamHandle=this;
    this->streamSize=-1;
    this->read=_read;
    code_stream.streamHandle=this;
    code_stream.streamSize=0;
    code_stream.write=_write_code;
    compresser=compressPlugin->open(compressPlugin,&code_stream);
    if (compresser==0)
        throw std::runtime_error("TCompressedStreamInput::TCompressedStreamInput()"
                                 " compressPlugin->open() error!");
}

long TCompressedStreamInput::_read(hpatch_TStreamInputHandle streamHandle,
                                   const hpatch_StreamPos_t readFromPos,
                                   unsigned char* out_data,unsigned char* out_data_end){
    TCompressedStreamInput* self=(TCompressedStreamInput*)streamHandle;
    long sumReadedLen=0;
    while (out_data<out_data_end) {
        size_t code_buf_size=self->_code_buf.size();
        size_t curLen=code_buf_size-self->curCodePos;
        if (curLen>0){
            size_t readLen=(out_data_end-out_data);
            if (readLen>curLen) readLen=curLen;
            const unsigned char* pcode=&self->_code_buf[self->curCodePos];
            memcpy(out_data,pcode,readLen);
            sumReadedLen+=readLen;
            out_data+=readLen;
            self->curCodePos+=readLen;
        }else{
            if (self->curReadPos==self->data_stream->streamSize){
                if (self->compresser!=0){
                    self->compressPlugin->close(self->compressPlugin,
                                                (hdiff_compressHandle)self->compresser);
                    self->compresser=0;
                    continue;
                }else{
                    break;// compress finish, not need out enough data!
                }
            }else{
                assert(self->compresser!=0);
                size_t readLen=kReadBufSize;
                if (readLen+self->curReadPos>self->data_stream->streamSize)
                    readLen=(size_t)(self->data_stream->streamSize-self->curReadPos);
                unsigned char* pdata=self->_data_buf.data();
                if (readLen!=self->data_stream->read(self->data_stream->streamHandle,self->curReadPos,
                                                     pdata,pdata+readLen)) return -1;
                self->curReadPos+=readLen;
                
                if (readLen!=self->compressPlugin->compress_part(self->compressPlugin,self->compresser,
                                                                 pdata,pdata+readLen)) return -1;
            }
        }
    }
    return sumReadedLen;
};

long TCompressedStreamInput::_write_code(hpatch_TStreamOutputHandle streamHandle,
                                         const hpatch_StreamPos_t writeToPos,
                                         const unsigned char* code,const unsigned char* code_end){
    TCompressedStreamInput* self=(TCompressedStreamInput*)streamHandle;
    size_t bufSize=self->_code_buf.size();
    const size_t old_code_len=(bufSize-self->curCodePos);
    const size_t new_code_len=(code_end-code);
    const size_t code_len=old_code_len+new_code_len;
    if (code_len>bufSize){
        const size_t backSize=bufSize;
        bufSize=kReadBufSize;
        while (bufSize<code_len)
            bufSize<<=1;
        self->_code_buf.resize(bufSize);
        if (old_code_len>0){
            unsigned char* pcode=self->_code_buf.data();
            memmove(pcode+bufSize-old_code_len,pcode+backSize-old_code_len,old_code_len);
        }
    }
    unsigned char* pcode=self->_code_buf.data();
    if (old_code_len>0)
        memmove(pcode+bufSize-code_len,pcode+bufSize-old_code_len,old_code_len);
    memcpy(pcode+bufSize-new_code_len,code,new_code_len);
    self->curCodePos=bufSize-code_len;
    return new_code_len;
}


TCoversStream::TCoversStream(const TCovers& _covers,hpatch_StreamPos_t cover_buf_size)
:covers(_covers),_code_buf(kCodeBufSize),curCodePos(0),curCodePos_end(0),
readedCoverCount(0),lastOldEnd(0),lastNewEnd(0){
    assert(_code_buf.size()>=hpatch_kMaxPackedUIntBytes*3);
    this->streamHandle=this;
    this->streamSize=cover_buf_size;
    this->read=_read;
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
    size_t n=self->covers.coverCount();
    long sumReadedLen=0;
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
            unsigned char* pcode_cur=self->_code_buf.data();
            unsigned char* pcode_end=pcode_cur+self->_code_buf.size();
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
            self->curCodePos_end=pcode_cur-self->_code_buf.data();
        }
    }
    return sumReadedLen;
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


long TNewDataDiffStream::_read(hpatch_TStreamInputHandle streamHandle,
                               const hpatch_StreamPos_t readFromPos,
                               unsigned char* out_data,unsigned char* out_data_end){
    TNewDataDiffStream* self=(TNewDataDiffStream*)streamHandle;
    size_t n=self->covers.coverCount();
    long sumReadedLen=0;
    while (out_data<out_data_end) {
        hpatch_StreamPos_t curLen=self->curNewPos_end-self->curNewPos;
        if (curLen>0){
            size_t readLen=(out_data_end-out_data);
            if (readLen>curLen) readLen=(size_t)curLen;
            if (readLen!=self->newData->read(self->newData->streamHandle,self->curNewPos,
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
    return sumReadedLen;
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



void TDiffStream::pushBack(const unsigned char* src,size_t n){
    if (n==0) return;
    if (n!=out_diff->write(out_diff->streamHandle,writePos,src,src+n))
        throw std::runtime_error("TDiffStream::pushBack() write stream error!");
    writePos+=n;
}

void TDiffStream::_packUInt(hpatch_StreamPos_t uValue,size_t minOutSize){
    if (minOutSize>hpatch_kMaxPackedUIntBytes) throw minOutSize;
    unsigned char  codeBuf[hpatch_kMaxPackedUIntBytes];
    unsigned char* codeEnd=codeBuf;
    if (!hpatch_packUIntWithTag(&codeEnd,codeBuf+hpatch_kMaxPackedUIntBytes,
                                uValue,0,0)) throw uValue;
    assert(codeBuf<codeEnd);
    while ((size_t)(codeEnd-codeBuf)<minOutSize) {
        codeEnd[-1]|=(1<<7);
        codeEnd[0]=0;
        ++codeEnd;
    }
    pushBack(codeBuf,(size_t)(codeEnd-codeBuf));
}

void TDiffStream::packUInt_update(const TPlaceholder& pos,hpatch_StreamPos_t uValue){
    hpatch_StreamPos_t writePosBack=writePos;
    writePos=pos.pos;
    _packUInt(uValue,(size_t)(pos.pos_end-pos.pos));
    assert(writePos==pos.pos_end);
    writePos=writePosBack;
}

hpatch_StreamPos_t TDiffStream::_pushStream(const hpatch_TStreamInput* stream,
                                            hpatch_StreamPos_t kLimitReadedSize){
    const size_t kBufSize=1024*64;
    _temp_buf.resize(kBufSize);
    unsigned char* buf=_temp_buf.data();
    hpatch_StreamPos_t sumReadedLen=0;
    while (true) {
        size_t readLen=kBufSize;
        if (readLen+sumReadedLen>stream->streamSize)
            readLen=(size_t)(stream->streamSize-sumReadedLen);
        long readed=stream->read(stream->streamHandle,sumReadedLen,buf,buf+readLen);
        if ((readed<0)||((size_t)readed>readLen))
            throw std::runtime_error("TDiffStream::pushStream() stream->read() error!");
        sumReadedLen+=(size_t)readed;
        if (sumReadedLen>kLimitReadedSize)
            return sumReadedLen; // Fail, will larger than kLimitReadedSize!;
        if (readed==0)
            break;// ok finish
        else
            this->pushBack(buf,readed);
    }
    return sumReadedLen;
}

void TDiffStream::pushStream(const hpatch_TStreamInput* stream,
                             hpatch_StreamPos_t kLimitReadedSize,
                             hdiff_TStreamCompress* compressPlugin,
                             const TPlaceholder& update_compress_sizePos){
    if ((compressPlugin)&&(stream->streamSize>0)){
        TCompressedStreamInput compress_stream(stream,compressPlugin);
        hpatch_StreamPos_t compressed_size=_pushStream(&compress_stream,kLimitReadedSize);
        if (compressed_size>kLimitReadedSize){
            compressed_size=0;
            hpatch_StreamPos_t rtsize=_pushStream(stream,stream->streamSize);
            assert(rtsize==stream->streamSize);
        }
        packUInt_update(update_compress_sizePos,compressed_size);
    }else if (stream->streamSize>0){
        hpatch_StreamPos_t rtsize=_pushStream(stream,stream->streamSize);
        assert(rtsize==stream->streamSize);
    }
}
