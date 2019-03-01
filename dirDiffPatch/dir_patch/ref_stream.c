//  ref_stream.cpp
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
#include "ref_stream.h"
#if (_IS_NEED_DIR_DIFF_PATCH)
#include <stdio.h>
#include <stdlib.h>

void hpatch_TRefStream_close(hpatch_TRefStream* self){
    if (self->_buf) { free(self->_buf); self->_buf=0; }
}

#define  check(value) { if (!(value)){ fprintf(stderr,"check "#value" error!\n");  \
                                       result=hpatch_FALSE; goto clear; } }

static hpatch_BOOL _TRefStream_read_do(hpatch_TRefStream* self,hpatch_StreamPos_t readFromPos,
                               unsigned char* out_data,unsigned char* out_data_end,size_t curRangeIndex){
    hpatch_StreamPos_t readPos=readFromPos - self->_rangeEndList[curRangeIndex-1];
    const hpatch_TStreamInput* ref=self->_refList[curRangeIndex];
    return ref->read(ref,readPos,out_data,out_data_end);
}

static size_t findRangeIndex(const hpatch_StreamPos_t* ranges,size_t rangeCount,hpatch_StreamPos_t pos){
    //not find return rangeCount
    //optimize, binary search?
    size_t i;
    for (i=0; i<rangeCount; ++i) {
        if (pos>=ranges[i]) continue;
        return i;
    }
    return rangeCount;
}

static hpatch_BOOL _refStream_read(const hpatch_TStreamInput* stream,
                                   hpatch_StreamPos_t readFromPos,
                                   unsigned char* out_data,unsigned char* out_data_end){
    hpatch_BOOL  result=hpatch_TRUE;
    hpatch_TRefStream* self=(hpatch_TRefStream*)stream->streamImport;
    const hpatch_StreamPos_t* ranges=self->_rangeEndList;
    size_t curRangeIndex=self->_curRangeIndex;
    while (out_data<out_data_end) {
        size_t readLen=(out_data_end-out_data);
        if (ranges[curRangeIndex-1]<=readFromPos){ //-1 safe
            if (readFromPos+readLen<=ranges[curRangeIndex]){//hit all
                check(_TRefStream_read_do(self,readFromPos,out_data,out_data_end,curRangeIndex));
                break; //ok out while
            }else if (readFromPos<=ranges[curRangeIndex]){//hit left
                hpatch_StreamPos_t leftLen=(ranges[curRangeIndex]-readFromPos);
                if (leftLen>0)
                    check(_TRefStream_read_do(self,readFromPos,out_data,out_data+leftLen,curRangeIndex));
                ++curRangeIndex;
                readFromPos+=leftLen;
                out_data+=leftLen;
                continue; //next
            }//else
        }//else
        
        curRangeIndex=findRangeIndex(ranges,self->_rangeCount,readFromPos);
        check(curRangeIndex<self->_rangeCount);
    }
    self->_curRangeIndex=curRangeIndex;
clear:
    return result;
}

hpatch_BOOL _createRange(hpatch_TRefStream* self,const hpatch_TStreamInput** refList,size_t refCount){
    hpatch_BOOL result=hpatch_TRUE;
    size_t   i;
    size_t   rangIndex=0;
    hpatch_StreamPos_t curSumSize=0;
    assert(self->_buf==0);
    assert(self->_refList==0);
    
    self->_refList=refList;
    self->_rangeCount=refCount;
    self->_buf=(unsigned char*)malloc(sizeof(hpatch_StreamPos_t)*(self->_rangeCount+1));
    check(self->_buf!=0);
    self->_rangeEndList=((hpatch_StreamPos_t*)self->_buf)+1; //+1 for _rangeEndList[-1] safe
    self->_rangeEndList[-1]=0;
    for (i=0; i<refCount; ++i) {
        hpatch_StreamPos_t rangeSize=refList[i]->streamSize;
        curSumSize+=rangeSize;
        self->_rangeEndList[rangIndex]=curSumSize;
        ++rangIndex;
    }
    assert(rangIndex==self->_rangeCount);
    self->_curRangeIndex=0;
clear:
    return result;
}

hpatch_BOOL hpatch_TRefStream_open(hpatch_TRefStream* self,const hpatch_TStreamInput** refList,size_t refCount){
    hpatch_BOOL result=hpatch_TRUE;
    check(self->stream==0);
    check(_createRange(self,refList,refCount));
    
    self->_stream.streamImport=self;
    self->_stream.streamSize=self->_rangeEndList[self->_rangeCount-1]; //safe
    self->_stream.read=_refStream_read;
    self->stream=&self->_stream;
clear:
    return result;
}

#endif
