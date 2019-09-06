//  res_handle_limit.cpp
//  dir patch
/*
 The MIT License (MIT)
 Copyright (c) 2018-2019 HouSisong
 
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
#include "res_handle_limit.h"
#if (_IS_NEED_DIR_DIFF_PATCH)
#include <stdio.h>
#include <stdlib.h>

#define  check(value) { if (!(value)){ fprintf(stderr,"check "#value" error!\n"); return hpatch_FALSE; } }

static hpatch_BOOL _TResHandleLimit_closeOneHandle(hpatch_TResHandleLimit* self){
    size_t              best_i=~(size_t)0;
    hpatch_StreamPos_t  best_hit=~(hpatch_StreamPos_t)0;
    hpatch_TStreamInput*    cur=0;
    hpatch_IResHandle*      res=0;
    size_t count=self->streamCount;
    size_t i;
    for (i=0; i<count;++i) {
        if (self->_in_streamList[i]){
            hpatch_StreamPos_t cur_hit=self->_ex_streamList[i].hit;
            if (cur_hit<best_hit){
                best_hit=cur_hit;
                best_i=i;
            }
        }
    }
    check(best_i<count);
    cur=self->_in_streamList[best_i];
    res=&self->_resList[best_i];
    self->_in_streamList[best_i]=0;
    check(res->close(res,cur));
    --self->_curOpenCount;
    return hpatch_TRUE;
}

static hpatch_BOOL _TResHandleLimit_openHandle(hpatch_TResHandleLimit* self,size_t index){
    hpatch_IResHandle*  res=&self->_resList[index];
    hpatch_TStreamInput** pCur=&self->_in_streamList[index];
    assert((*pCur)==0);
    if (self->_curOpenCount>=self->_limitMaxOpenCount)
        check(_TResHandleLimit_closeOneHandle(self));
    check(res->open(res,pCur));
    check((*pCur)!=0);
    check((*pCur)->streamSize==res->resStreamSize);
    ++self->_curOpenCount;
    return hpatch_TRUE;
}

static hpatch_BOOL _TResHandleLimit_read(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                         unsigned char* out_data,unsigned char* out_data_end){
    _hpatch_TResHandleBox*  ex=(_hpatch_TResHandleBox*)stream->streamImport;
    hpatch_TResHandleLimit* self=ex->owner;
    size_t           index=ex-self->_ex_streamList;
    const hpatch_TStreamInput* in_stream=self->_in_streamList[index];
    assert(index<self->streamCount);
    ex->hit= ++self->_curHit;
    if (!in_stream){
        check(_TResHandleLimit_openHandle(self,index));
        in_stream=self->_in_streamList[index];
    }
    return in_stream->read(in_stream,readFromPos,out_data,out_data_end);
}

hpatch_BOOL hpatch_TResHandleLimit_open(hpatch_TResHandleLimit* self,size_t limitMaxOpenCount,
                                 hpatch_IResHandle* resList,size_t resCount){
    unsigned char* curMem=0;
    size_t i=0;
    size_t memSize=resCount*(sizeof(const hpatch_TStreamInput*)
                             +sizeof(hpatch_TStreamInput*)+sizeof(_hpatch_TResHandleBox));
    assert(self->_buf==0);
    if (limitMaxOpenCount<1) limitMaxOpenCount=1;
    self->_buf=malloc(memSize);
    if (self->_buf==0) return hpatch_FALSE;
    curMem=self->_buf;
    self->streamList=(const hpatch_TStreamInput**)curMem;
        curMem+=resCount*sizeof(const hpatch_TStreamInput*);
    self->_in_streamList=(hpatch_TStreamInput**)curMem;
        curMem+=resCount*sizeof(hpatch_TStreamInput*);
    self->_ex_streamList=(_hpatch_TResHandleBox*)curMem;
        //curMem+=resCount*sizeof(_TResHandleBox);
    self->streamCount=resCount;
    self->_resList=resList;
    self->_limitMaxOpenCount=limitMaxOpenCount;
    self->_curHit=0;
    self->_curOpenCount=0;
    for (i=0; i<resCount; ++i) {
        _hpatch_TResHandleBox* ex=&self->_ex_streamList[i];
        ex->hit=0;
        ex->owner=self;
        ex->box.streamImport=ex;
        ex->box.streamSize=self->_resList[i].resStreamSize;
        ex->box.read=_TResHandleLimit_read;
        
        self->_in_streamList[i]=0;
        self->streamList[i]=&self->_ex_streamList[i].box;
    }
    return hpatch_TRUE;
}

hpatch_BOOL hpatch_TResHandleLimit_closeFileHandles(hpatch_TResHandleLimit* self){
    size_t count=self->streamCount;
    hpatch_BOOL result=hpatch_TRUE;
    size_t i;
    for (i=0; i<count; ++i) {
        const hpatch_TStreamInput* stream=self->_in_streamList[i];
        if (stream){
            hpatch_IResHandle* res=&self->_resList[i];
            if (!res->close(res,stream)) result=hpatch_FALSE;
            self->_in_streamList[i]=0;
            --self->_curOpenCount;
        }
    }
    assert(self->_curOpenCount==0);
    return result;
}

hpatch_BOOL hpatch_TResHandleLimit_close(hpatch_TResHandleLimit* self){
    hpatch_BOOL result=hpatch_TResHandleLimit_closeFileHandles(self);
    self->streamList=0;
    self->streamCount=0;
    if (self->_buf){
        free(self->_buf);
        self->_buf=0;
    }
    return result;
}

#endif
