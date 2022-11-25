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
#include <algorithm>

#define checki(value,info) { if (!(value)) { throw std::runtime_error(info); } }
#define check(value) checki(value,"check "#value" error!")
#define kSubDiffCacheSize hdiff_kFileIOBufBestSize

namespace hdiff_private{

struct TCompressedStream:public hpatch_TStreamOutput{
    TCompressedStream(const hpatch_TStreamOutput*  _out_code,
                      hpatch_StreamPos_t _writePos,hpatch_StreamPos_t _kLimitOutCodeSize);
    inline bool  is_overLimit()const { return _is_overLimit; }
private:
    const hpatch_TStreamOutput*  out_code;
    hpatch_StreamPos_t           out_pos;
    hpatch_StreamPos_t           out_posLimitEnd;
    hpatch_StreamPos_t           _writeToPos_back;
    bool                         _is_overLimit;
    static hpatch_BOOL _write_code(const hpatch_TStreamOutput* stream,hpatch_StreamPos_t writeToPos,
                                   const unsigned char* data,const unsigned char* data_end);
};

TCompressedStream::TCompressedStream(const hpatch_TStreamOutput*  _out_code,
                                     hpatch_StreamPos_t _writePos,hpatch_StreamPos_t _kLimitOutCodeSize)
:out_code(_out_code),out_pos(_writePos),out_posLimitEnd(_writePos+_kLimitOutCodeSize),
_writeToPos_back(0),_is_overLimit(false){
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

void TNewDataDiffStream::_init(hpatch_StreamPos_t newDataDiff_size){
    curNewPos=0;
    curNewPos_end=0;
    lastNewEnd=0;
    readedCoverCount=0;
    _readFromPos_back=0;
    _coveri=0;
    _newDataPos=0;
    this->streamImport=this;
    this->streamSize=newDataDiff_size;
    this->read=_read;
    this->_private_reserved=0;
}

void TNewDataDiffStream::_initByRange(size_t coveri,hpatch_StreamPos_t newDataPos,hpatch_StreamPos_t newDataPosEnd){
    _init(getDataSizeByRange(covers,coveri,newDataPos,newDataPosEnd));
    _coveri=coveri;
    _newDataPos=newDataPos;

    readedCoverCount=_coveri;
    lastNewEnd=_newDataPos;
}

hpatch_BOOL TNewDataDiffStream::_read(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                      unsigned char* out_data,unsigned char* out_data_end){
    TNewDataDiffStream* self=(TNewDataDiffStream*)stream->streamImport;
    if (readFromPos==0){
        self->curNewPos=0;
        self->curNewPos_end=0;
        self->readedCoverCount=self->_coveri;
        self->lastNewEnd=self->_newDataPos;
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
 
hpatch_StreamPos_t TNewDataDiffStream::getDataSizeByRange(const TCovers& covers,size_t coveri,
                                                          hpatch_StreamPos_t newDataPos,hpatch_StreamPos_t newDataPosEnd){
    size_t n=covers.coverCount();
    hpatch_StreamPos_t newDataDiff_size=0;
    hpatch_StreamPos_t lastNewEnd=newDataPos;
    for (size_t i=coveri;i<n;++i) {
        TCover cover;
        covers.covers(i,&cover);
        assert(cover.newPos>=lastNewEnd);
        hpatch_StreamPos_t inc=cover.newPos-lastNewEnd;
        if (lastNewEnd+inc>=newDataPosEnd)
            inc=newDataPosEnd-lastNewEnd;
        newDataDiff_size+=inc;
        if (lastNewEnd+inc==newDataPosEnd)
            return newDataDiff_size;
        lastNewEnd=cover.newPos+cover.length;
        if (lastNewEnd>=newDataPosEnd)
            return newDataDiff_size;
    }

    assert(newDataPosEnd>=lastNewEnd);
    hpatch_StreamPos_t inc=newDataPosEnd-lastNewEnd;
    newDataDiff_size+=inc;
    return newDataDiff_size;
}

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


TNewDataSubDiffStream::TNewDataSubDiffStream(const hdiff_TStreamInput* _newData,const hdiff_TStreamInput* _oldData,
                                             const TCovers& _covers,bool _isOnlySubCover,bool _isZeroSubDiff)
:newData(_newData),oldData(_oldData),covers(_covers),
  isOnlySubCover(_isOnlySubCover),isZeroSubDiff(_isZeroSubDiff),_cache(kSubDiffCacheSize){
    initRead();
    streamImport=this;
    if (isOnlySubCover){
        streamSize=0;
        for (size_t i=0;i<covers.coverCount();++i){
            hpatch_TCover cover;
            covers.covers(i,&cover);
            streamSize+=cover.length;
        }
    }else{
        streamSize=newData->streamSize;
    }
    read=_read;
}

static const hpatch_StreamPos_t _kNullPos = hpatch_kNullStreamPos;
void TNewDataSubDiffStream::initRead(){
    curReadNewPos=0;
    curReadOldPos=_kNullPos;
    curReadPos=0;
    nextCoveri=0;
    curDataLen=0;
}

    static inline void _subData(unsigned char* dst,const unsigned char* src,size_t length){
        while (length--)
            (*dst++)-=*src++;
    }
    static void _subData(unsigned char* out_data,size_t len,
                         const hdiff_TStreamInput*oldData,hpatch_StreamPos_t oldPos,
                         unsigned char* cache,size_t cacheSize){
        while (len){
            size_t rlen=len;
            if (rlen>cacheSize) rlen=cacheSize;
            check(oldData->read(oldData,oldPos,cache,cache+rlen));
            _subData(out_data,cache,rlen);
            oldPos+=rlen;
            out_data+=rlen;
            len-=rlen;
        }
    }
void TNewDataSubDiffStream::readTo(unsigned char* out_data,unsigned char* out_data_end){
    size_t readLen=out_data_end-out_data;
    while (readLen>0){
        if (curDataLen==0){
            if (nextCoveri<covers.coverCount()){
                hpatch_TCover cover;
                covers.covers(nextCoveri,&cover);
                if ((size_t)cover.newPos>curReadNewPos){
                    curDataLen=(size_t)(cover.newPos-curReadNewPos);
                    curReadOldPos=_kNullPos;
                }else{
                    assert((size_t)cover.newPos==curReadNewPos);
                    curDataLen=(size_t)cover.length;
                    curReadOldPos=cover.oldPos;
                    ++nextCoveri;
                }
            }else{
                curDataLen=(size_t)(streamSize-curReadNewPos);
                curReadOldPos=_kNullPos;
            }
        }
        if ((curReadOldPos==_kNullPos)&&isOnlySubCover){
            curReadNewPos+=curDataLen;
            curDataLen=0;
            continue;
        }
        
        size_t len=(size_t)std::min(curDataLen,(hpatch_StreamPos_t)readLen);
        if ((!isZeroSubDiff)&&(curReadOldPos!=_kNullPos)){
            check(newData->read(newData,curReadNewPos,out_data,out_data+len));
            _subData(out_data,len,oldData,curReadOldPos,_cache.data(),_cache.size());
        }else{
            memset(out_data,0,len);
        }
        if (curReadOldPos!=_kNullPos) curReadOldPos+=len;
        curReadNewPos+=len;
        curDataLen-=len;
        out_data+=len;
        readLen-=len;
    }
}
hpatch_BOOL TNewDataSubDiffStream::_read(const struct hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                             unsigned char* out_data,unsigned char* out_data_end){
    TNewDataSubDiffStream* self=(TNewDataSubDiffStream*)stream->streamImport;
    if (readFromPos==0) self->initRead();
    if (readFromPos!=self->curReadPos) return hpatch_FALSE;
    self->curReadPos+=(size_t)(out_data_end-out_data);
    assert(self->curReadPos<=self->streamSize);
    self->readTo(out_data,out_data_end);
    return hpatch_TRUE;
}

TNewDataSubDiffStream_mem::TNewDataSubDiffStream_mem(const unsigned char* newData,const unsigned char* newData_end,
                              const unsigned char* oldData,const unsigned char* oldData_end,
                              const TCovers& _covers,bool _isOnlySubCover,bool _isZeroSubDiff)
:TNewDataSubDiffStream(mem_as_hStreamInput(&mem_newData,newData,newData_end),
                       mem_as_hStreamInput(&mem_oldData,oldData,oldData_end),
                       _covers,_isOnlySubCover,_isZeroSubDiff){}


TNewDataSubDiffCoverStream::TNewDataSubDiffCoverStream(const hpatch_TStreamInput* _newStream,
                                                       const hpatch_TStreamInput* _oldStream,
                                                       bool _isZeroSubDiff)
:isZeroSubDiff(_isZeroSubDiff),newStream(_newStream),oldStream(_oldStream),
newData(0),oldData(0),_cache(_isZeroSubDiff?0:kSubDiffCacheSize*2){
    setCover(cover,0,0,0);
    streamImport=this;
    read=_read;
    if (!isZeroSubDiff){
        newData=_cache.data();
        oldData=_cache.data()+kSubDiffCacheSize;
    }
    initRead();
}

void TNewDataSubDiffCoverStream::initRead(){
    streamSize=cover.length;
    inStreamLen=cover.length;
    curDataLen=0;
}

void TNewDataSubDiffCoverStream::resetCover(const TCover& _cover){
    cover=_cover;
    initRead();
}
void TNewDataSubDiffCoverStream::resetCoverLen(hpatch_StreamPos_t coverLen){
    const hpatch_StreamPos_t skipLen=cover.length-(curDataLen+inStreamLen);
    if (coverLen<=skipLen){ // all cache data invalid
        cover.length=coverLen;
        initRead();
    }else{
        if (coverLen<skipLen+curDataLen)
            curDataLen=(size_t)(coverLen-skipLen);
        inStreamLen=coverLen-(skipLen+curDataLen);
        cover.length=coverLen;
        streamSize=coverLen;
    }
}

hpatch_BOOL TNewDataSubDiffCoverStream::_read(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                              unsigned char* out_data,unsigned char* out_data_end){
    TNewDataSubDiffCoverStream* self=(TNewDataSubDiffCoverStream*)stream->streamImport;
    if (self->isZeroSubDiff){
        memset(out_data,0,out_data_end-out_data);
        return hpatch_TRUE;
    }
    return self->readTo(readFromPos,out_data,out_data_end);
}

hpatch_BOOL TNewDataSubDiffCoverStream::readTo(hpatch_StreamPos_t readFromPos,
                                               unsigned char* out_data,unsigned char* out_data_end){
    // newStream:[       |                                          |              ]
    //              cover.newPos                        cover.newPos+cover.length
    // _cache:           |  skipLen  [  curDataLen  ]  inStreamLen  |
    //                                          |
    //                                     readFromPos   
    while (out_data!=out_data_end){
        hpatch_StreamPos_t skipLen=cover.length-(curDataLen+inStreamLen);
        if ((readFromPos<skipLen)||(readFromPos>=(skipLen+curDataLen))){
            if (!_updateCache(readFromPos)) return hpatch_FALSE;
            skipLen=cover.length-(curDataLen+inStreamLen);
        }
        size_t srcPos=(size_t)(readFromPos-skipLen);
        size_t len=std::min((size_t)(curDataLen-srcPos),(size_t)(out_data_end-out_data));
        for (size_t i=0;i<len;i++)
            out_data[i]=newData[srcPos+i]-oldData[srcPos+i];
        readFromPos+=len;
        out_data+=len;
    }
    return hpatch_TRUE;
}

hpatch_BOOL TNewDataSubDiffCoverStream::_updateCache(hpatch_StreamPos_t readFromPos){
    assert(readFromPos<cover.length);
    curDataLen=kSubDiffCacheSize;
    if (curDataLen+readFromPos>cover.length)
        curDataLen=(size_t)(cover.length-readFromPos);
    inStreamLen=cover.length-(curDataLen+readFromPos);
    if (!newStream->read(newStream,cover.newPos+readFromPos,newData,newData+curDataLen))
        return hpatch_FALSE;
    if (!oldStream->read(oldStream,cover.oldPos+readFromPos,oldData,oldData+curDataLen))
        return hpatch_FALSE;
    return hpatch_TRUE;
}


TStepStream::TStepStream(const hpatch_TStreamInput* newStream,const hpatch_TStreamInput* oldStream,
                         bool isZeroSubDiff,const TCovers& _covers,size_t _patchStepMemSize)
:subDiff(newStream,oldStream,isZeroSubDiff),newDataDiff(_covers,newStream),
covers(_covers),patchStepMemSize(_patchStepMemSize),newDataSize(newStream->streamSize),
readFromPosBack(0),readBufPos(0){
    initStream();
    streamImport=this;
    read=_read;
    streamSize=sumBufSize_forInit;
}

void TStepStream::initStream(){
    isInInit=true;
    sumBufSize_forInit=0;
    beginStep();
    while(doStep()) { }
    isInInit=false;
    endCoverCount=curCoverCount;
    endMaxStepMemSize=curMaxStepMemSize;
    beginStep();
    //assert(buf.empty());
}

hpatch_BOOL TStepStream::readTo(unsigned char* out_data,unsigned char* out_data_end){
    while (out_data!=out_data_end){
        hpatch_StreamPos_t stepDataLen=step_buf.size()+step_dataDiff.streamSize;
        if (readBufPos==stepDataLen){
            step_buf.clear();
            step_dataDiff.clear();
            readBufPos=0;
            while (doStep()) { 
                stepDataLen=step_buf.size()+step_dataDiff.streamSize;
                if (stepDataLen>0) 
                    break; 
            }
            check(stepDataLen>0);
        }
        size_t len=out_data_end-out_data;
        if (readBufPos+len>stepDataLen)
            len=(size_t)(stepDataLen-readBufPos);
        if (readBufPos<step_buf.size()){//copy data from step_buf
            size_t clen=len;
            if (readBufPos+clen>step_buf.size())
                clen=(size_t)(step_buf.size()-readBufPos);
            memcpy(out_data,step_buf.data()+(size_t)readBufPos,clen);
            readBufPos+=clen;
            out_data+=clen;
            len-=clen;
        }
        if (len>0){//copy data from step_dataDiff
            if (!step_dataDiff.read(&step_dataDiff,readBufPos-step_buf.size(),
                                    out_data,out_data+len)) return hpatch_FALSE;
            readBufPos+=len;
            out_data+=len;
        }
    }
    return hpatch_TRUE;
}

hpatch_BOOL TStepStream::_read(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                               unsigned char* out_data,unsigned char* out_data_end){
    TStepStream* self=(TStepStream*)stream->streamImport;
    if (readFromPos!=self->readFromPosBack){
        check(readFromPos==0);
        self->beginStep();
    }
    self->readFromPosBack=readFromPos+(size_t)(out_data_end-out_data);
    return self->readTo(out_data,out_data_end);
}

void TStepStream::beginStep(){
    curCoverCount=covers.coverCount();
    isHaveLeftCover=false;
    isHaveRightCover=false;
    isHaveLastCover=false;
    if (curCoverCount==0){
        isHaveLastCover=true;
        setCover(lastCover,0,newDataSize,0);
    }else{
        TCover back;
        covers.covers(curCoverCount-1,&back);
        if (back.newPos+back.length<newDataSize){
            isHaveLastCover=true;
            setCover(lastCover,back.oldPos+back.length,newDataSize,0);
        }
    }
    if (isHaveLastCover) ++curCoverCount;

    step_buf.clear();
    step_dataDiff.clear();
    readBufPos=0;
    curMaxStepMemSize=0;
    lastOldEnd=0;
    lastNewEnd=0;
    step_bufCover.clear();
    step_bufCover_size=0;
    step_bufRle.clear();
    step_dataDiffSize=0;
    newDataDiffReadPos=0;
    cur_i=0;
    pCurCover=0;
}

bool TStepStream::doStep(){
    if (pCurCover==0){
        if (isHaveLeftCover)      { isHaveLeftCover=false;  pCurCover=&leftCover;  }
        else if (isHaveRightCover){ isHaveRightCover=false; pCurCover=&rightCover; }
        else if (cur_i<covers.coverCount()) { covers.covers(cur_i++,&cover); pCurCover=&cover; }
        else if (isHaveLastCover) { isHaveLastCover=false;  pCurCover=&lastCover;  }
        else if (step_bufCover_size>0){ _last_flush_step(); return true; }
        else { return false; } //end while
    }
    cover=*pCurCover;
    const size_t step_bufCover_backSize=step_bufCover_size;

    hpatch_StreamPos_t backNewLen=cover.newPos-lastNewEnd;
    if (cover.oldPos>=lastOldEnd){ //save inc_oldPos
        if (isInInit)
            step_bufCover_size+=hpatch_packUIntWithTag_size((hpatch_StreamPos_t)(cover.oldPos-lastOldEnd), 1);
        else 
            packUIntWithTag(step_bufCover,(hpatch_StreamPos_t)(cover.oldPos-lastOldEnd), 0, 1);
    }else{
        if (isInInit)
            step_bufCover_size+=hpatch_packUIntWithTag_size((hpatch_StreamPos_t)(lastOldEnd-cover.oldPos), 1);
        else
            packUIntWithTag(step_bufCover,(hpatch_StreamPos_t)(lastOldEnd-cover.oldPos), 1, 1);//sub safe
    }
    if (isInInit){
        step_bufCover_size+=hpatch_packUInt_size(backNewLen)+hpatch_packUInt_size(cover.length);
    }else{
        packUInt(step_bufCover,backNewLen); //save inc_newPos
        packUInt(step_bufCover,cover.length);
        step_bufCover_size=step_bufCover.size();
    }
    
    subDiff.resetCover(cover);
    const hpatch_StreamPos_t curMaxNeedSize = step_bufCover_size +
            (subDiff.isZeroSubDiff?step_bufRle.maxCodeSizeByZeroLen(subDiff.streamSize):step_bufRle.maxCodeSize(&subDiff));
    if (curMaxNeedSize<=patchStepMemSize){ //append
        if (subDiff.isZeroSubDiff)
            step_bufRle.appendByZeroLen(subDiff.streamSize);
        else
            step_bufRle.append(&subDiff);
        if (backNewLen>0){
            newDataDiffReadPos+=backNewLen;
            step_dataDiffSize+=backNewLen;
        }

        lastOldEnd=cover.oldPos+cover.length;//! +length
        lastNewEnd=cover.newPos+cover.length;
        pCurCover=0; // next 
        return true;
    }else{
        if (step_bufCover_backSize+step_bufRle.curCodeSize()>=(patchStepMemSize/2)){//flush step
            step_bufCover_size=step_bufCover_backSize;
            if (!isInInit) step_bufCover.resize(step_bufCover_backSize);
            
            step_dataDiff.reset(&newDataDiff,newDataDiffReadPos-step_dataDiffSize,newDataDiffReadPos);
            step_dataDiffSize=0;
            _flush_step_code();
            return true;  // pCurCover!
        }else{ //clip one cover to two cover
            check((!isHaveLeftCover)&&(!isHaveRightCover));
            isHaveLeftCover=true;
            isHaveRightCover=true;
            leftCover=cover;
            rightCover=cover;
            hpatch_StreamPos_t clen=cover.length;
            while (1) {
                clen=clen*3/4;
                subDiff.resetCoverLen(clen);
                check(clen>0); // stepMemSize error
                const hpatch_StreamPos_t _curMaxNeedSize = step_bufCover_size +
                    (subDiff.isZeroSubDiff?step_bufRle.maxCodeSizeByZeroLen(subDiff.streamSize):step_bufRle.maxCodeSize(&subDiff));
                if (_curMaxNeedSize<=patchStepMemSize)
                    break;
            }
            leftCover.length=clen;
            rightCover.length-=clen;
            rightCover.oldPos+=clen;
            rightCover.newPos+=clen;

            step_bufCover_size=step_bufCover_backSize;
            if (!isInInit) step_bufCover.resize(step_bufCover_backSize);
            ++curCoverCount;
            pCurCover=0;
            return true;  // leftCover
        }
    }
}

void TStepStream::_last_flush_step(){
    if (step_bufCover_size>0){
        step_dataDiff.reset(&newDataDiff,newDataDiffReadPos-step_dataDiffSize,newDataDiffReadPos);
        step_dataDiffSize=0;
        _flush_step_code();
    }else{
        assert(step_dataDiff.streamSize==0);
        assert(step_dataDiffSize==0);
    }
    assert(curMaxStepMemSize<=patchStepMemSize);
    assert(newDataDiffReadPos==newDataDiff.streamSize);
}

void TStepStream::_flush_step_code() {
    step_bufRle.finishAppend();
    const size_t curStepMemSize=step_bufCover_size+step_bufRle.fixed_code.size();
    if (curMaxStepMemSize<curStepMemSize)
        curMaxStepMemSize=curStepMemSize;
    if (isInInit){
        assert(step_buf.empty());
        sumBufSize_forInit+=hpatch_packUInt_size(step_bufCover_size) +
                            hpatch_packUInt_size(step_bufRle.curCodeSize())+
                            curStepMemSize + step_dataDiff.streamSize;
    }else{
        //step head
        assert(step_bufCover_size==step_bufCover.size());
        packUInt(step_buf,step_bufCover.size());
        packUInt(step_buf,step_bufRle.curCodeSize());
        //step cache data
        pushBack(step_buf,step_bufCover);            
        pushBack(step_buf,step_bufRle.fixed_code);
    }
    step_bufCover_size=0;
    step_bufCover.clear();
    step_bufRle.clear();
}


TDiffStream::TDiffStream(const hpatch_TStreamOutput* _out_diff,hpatch_StreamPos_t out_diff_curPos)
:out_diff(_out_diff),writePos(out_diff_curPos){
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


size_t TDiffStream::packUInt(hpatch_StreamPos_t uValue){
    unsigned char  codeBuf[hpatch_kMaxPackedUIntBytes];
    unsigned char* codeEnd=codeBuf;
    check(hpatch_packUInt(&codeEnd,codeBuf+hpatch_kMaxPackedUIntBytes,uValue));
    pushBack(codeBuf,(size_t)(codeEnd-codeBuf));
    return codeEnd-codeBuf;
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

void TDiffStream::stream_read(const TPlaceholder& pos,hpatch_byte* out_data){
    assert(pos.pos<=pos.pos_end);
    assert(pos.size()==(size_t)pos.size());
    check(pos.pos_end<=writePos);
    checki(out_diff->read_writed!=0,"TDiffStream::stream_read() out_diff can't read error!");
    checki(out_diff->read_writed(out_diff,pos.pos,out_data,out_data+(size_t)pos.size()),
            "TDiffStream::stream_read() out_diff read error!");
}
void TDiffStream::stream_update(const TPlaceholder& pos,const hpatch_byte* in_data){
    assert(pos.pos<=pos.pos_end);
    assert(pos.size()==(size_t)pos.size());
    check(pos.pos_end<=writePos);

    hpatch_StreamPos_t writePosBack=writePos;
    writePos=pos.pos;
    pushBack(in_data,(size_t)pos.size());
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

hpatch_StreamPos_t TDiffStream::pushStream(const hpatch_TStreamInput* stream,
                                           const hdiff_TCompress* compressPlugin,
                                           const TPlaceholder& update_compress_sizePos,
                                           bool isMustCompress,const hpatch_StreamPos_t cancelSizeOnCancelCompress){
    hpatch_StreamPos_t compressed_size=0;
    check(writePos>=cancelSizeOnCancelCompress);
    check(stream->streamSize>=cancelSizeOnCancelCompress);
    if ((compressPlugin)&&(isMustCompress||(stream->streamSize>=1+cancelSizeOnCancelCompress))){
        hpatch_StreamPos_t kLimitOutCodeSize=isMustCompress?compressPlugin->maxCompressedSize(stream->streamSize+1)
                                            :(stream->streamSize-(1+cancelSizeOnCancelCompress));
        TCompressedStream  out_stream(out_diff,writePos,kLimitOutCodeSize);
        compressed_size=compressPlugin->compress(compressPlugin,&out_stream,stream);
        if (out_stream.is_overLimit()||(compressed_size==0)||(compressed_size>kLimitOutCodeSize)){
            check(!isMustCompress);
            compressed_size=0;//NOTICE: compress is canceled
        }else{
            writePos+=compressed_size; //compress ok
        }
    }

    if (compressed_size==0){
        writePos-=cancelSizeOnCancelCompress;//revoke some data
        _pushStream(stream);
    }
    if (!update_compress_sizePos.isNullPos())
        packUInt_update(update_compress_sizePos,compressed_size);
    return (compressed_size==0)?stream->streamSize:compressed_size;
}

void TStreamClip::reset(const hpatch_TStreamInput* stream,
                        hpatch_StreamPos_t clipBeginPos,hpatch_StreamPos_t clipEndPos,
                        hpatch_TDecompress* decompressPlugin,hpatch_StreamPos_t uncompressSize){
    clear();
    assert(clipBeginPos<=clipEndPos);
    assert(clipEndPos<=stream->streamSize);
    _src=stream;
    _src_begin=clipBeginPos;
    _src_end=clipEndPos;
    _decompressPlugin=decompressPlugin;
    _decompressHandle=0;
    _read_uncompress_pos=0;
    if ((uncompressSize==0)&&(decompressPlugin==0))
        uncompressSize=clipEndPos-clipBeginPos;
    this->streamImport=this;
    this->streamSize=uncompressSize;
    this->read=_clip_read;

    if (decompressPlugin)
        openDecompressHandle();
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
    

_TCheckOutNewDataStream::_TCheckOutNewDataStream(const hpatch_TStreamInput*  _newData,
                                                 unsigned char* _buf,size_t _bufSize)
:newData(_newData),writedLen(0),buf(_buf),bufSize(_bufSize){
    streamImport=this;
    streamSize=newData->streamSize;
    read_writed=_read_writed;
    write=_write_check;
}

hpatch_BOOL _TCheckOutNewDataStream::_read_writed(const struct hpatch_TStreamOutput* stream,hpatch_StreamPos_t readFromPos,
                                                  unsigned char* out_data,unsigned char* out_data_end){
    _TCheckOutNewDataStream* self=(_TCheckOutNewDataStream*)stream->streamImport;
    _test_rt(readFromPos<=self->writedLen);
    _test_rt((hpatch_size_t)(out_data_end-out_data)<=(hpatch_StreamPos_t)(self->writedLen-readFromPos)); 
    return self->newData->read(self->newData,readFromPos,out_data,out_data_end); 
}
hpatch_BOOL _TCheckOutNewDataStream::_write_check(const hpatch_TStreamOutput* stream,hpatch_StreamPos_t writeToPos,
                                                  const unsigned char* data,const unsigned char* data_end){
    _TCheckOutNewDataStream* self=(_TCheckOutNewDataStream*)stream->streamImport;
    _test_rt(self->writedLen==writeToPos);
    self->writedLen+=(size_t)(data_end-data);
    _test_rt(self->writedLen<=self->streamSize);
    
    hpatch_StreamPos_t readPos=writeToPos;
    while (data<data_end) {
        size_t readLen=(size_t)(data_end-data);
        if (readLen>self->bufSize) readLen=self->bufSize;
        _test_rt(self->newData->read(self->newData,readPos,self->buf,self->buf+readLen));
        _test_rt(0==memcmp(data,self->buf,readLen));
        data+=readLen;
        readPos+=readLen;
    }
    return hpatch_TRUE;
}


void do_compress(std::vector<unsigned char>& out_code,const hpatch_TStreamInput* data,
                        const hdiff_TCompress* compressPlugin,bool isMustCompress){
    out_code.clear();
    if (!compressPlugin) return;
    if (data->streamSize==0) return;
    hpatch_StreamPos_t maxCodeSize=compressPlugin->maxCompressedSize(data->streamSize);
    if ((maxCodeSize<=data->streamSize)||(maxCodeSize!=(size_t)maxCodeSize)) return; //error
    out_code.resize((size_t)maxCodeSize);
    hpatch_TStreamOutput codeStream;
    mem_as_hStreamOutput(&codeStream,out_code.data(),out_code.data()+out_code.size());
    hpatch_StreamPos_t codeSize=compressPlugin->compress(compressPlugin,&codeStream,data);
    if ((codeSize>0)&&(isMustCompress||(codeSize<data->streamSize)))
        out_code.resize((size_t)codeSize); //ok
    else
        out_code.clear();//error or cancel
}

}//namespace hdiff_private
