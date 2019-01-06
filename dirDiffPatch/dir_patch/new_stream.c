//  new_stream.cpp
//  dir patch
/*
 The MIT License (MIT)
 Copyright (c) 2018 HouSisong
 
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
#include "new_stream.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h> //printf

const size_t kBufSize=1024*128;

hpatch_BOOL TNewStream_close(TNewStream* self){
    hpatch_BOOL result=hpatch_TRUE;
    const hpatch_TStreamOutput* curNewFile=self->_curNewFile;
    if (curNewFile){
        self->_curNewFile=0;
        result=self->_listener->closeNewFile(self->_listener,curNewFile);
    }
    return result;
}

static hpatch_BOOL _file_entry_end(TNewStream* self);
static hpatch_BOOL _file_append_ready(TNewStream* self);
static hpatch_BOOL _file_append_part(TNewStream* self,hpatch_StreamPos_t writeToPos,
                                     const unsigned char* data,const unsigned char* data_end);
static hpatch_BOOL _file_append_end(TNewStream* self);

#define  check(value) { if (!(value)){ fprintf(stderr,#value" error!\n"); return hpatch_FALSE; } }

static hpatch_BOOL _TNewStream_write(const hpatch_TStreamOutput* stream,hpatch_StreamPos_t writeToPos,
                                     const unsigned char* data,const unsigned char* data_end){
    TNewStream* self=(TNewStream*)stream->streamImport;
    size_t dataSize=data_end-data;
    check(!self->isFinish);
    check(writeToPos<self->_curWriteToPosEnd);
    if (writeToPos+dataSize>self->_curWriteToPosEnd){
        size_t leftLen=(size_t)(self->_curWriteToPosEnd-writeToPos);
        check(_TNewStream_write(stream,writeToPos,data,data+leftLen));
        return _TNewStream_write(stream,writeToPos+leftLen,data+leftLen,data_end);
    }
    //write data
    check(self->_curPathIndex<self->_pathCount);
    check(_file_append_part(self,writeToPos,data,data_end));
    if (writeToPos+dataSize<self->_curWriteToPosEnd)//write continue
        return hpatch_TRUE;
    //write one end
    check(_file_append_end(self));
    ++self->_curPathIndex;
    return _file_append_ready(self);
}

static hpatch_BOOL _file_append_ready(TNewStream* self){
    
    while (self->_curPathIndex<self->_pathCount) {
        if ((self->_curSamePairIndex<self->_samePairCount)
            &&(self->_curPathIndex==self->_samePairList[self->_curSamePairIndex*2])){
            const size_t* pairNewiOldi=self->_samePairList+self->_curSamePairIndex*2;
            check(self->_listener->copySameFile(self->_listener,pairNewiOldi[0],pairNewiOldi[1]));
            ++self->_curPathIndex;
        }else if ((self->_curNewRefIndex<self->_newRefCount)
                  &&(self->_curPathIndex!=self->_newRefList[self->_curNewRefIndex])){
            check(self->_listener->outNewDir(self->_listener,self->_curPathIndex));
            ++self->_curPathIndex;
        }else{
            break;
        }
    }
    
    assert(self->_curNewFile==0);
    assert(self->_curWriteToPos==self->_curWriteToPosEnd);
    if (self->_curPathIndex<self->_pathCount){//open file for write
        check((self->_curNewRefIndex<self->_newRefCount)&&
              (self->_curPathIndex==self->_newRefList[self->_curNewRefIndex]));
        ++self->_curNewRefIndex;
        check(self->_listener->openNewFile(self->_listener,self->_curPathIndex,&self->_curNewFile));
        self->_curWriteToPosEnd+=self->_curNewFile->streamSize;
        return hpatch_TRUE;
    }
    //file entry end
    check(_file_entry_end(self));
    return hpatch_TRUE;
}

static hpatch_BOOL _file_append_part(TNewStream* self,hpatch_StreamPos_t g_writeToPos,
                                     const unsigned char* data,const unsigned char* data_end){
    hpatch_StreamPos_t curWPos;
    assert(self->_curNewFile!=0);
    assert((g_writeToPos<=self->_curWriteToPosEnd)&&(g_writeToPos==self->_curWriteToPos));
    curWPos=g_writeToPos-(self->_curWriteToPosEnd-self->_curNewFile->streamSize);
    self->_curWriteToPos=curWPos+(data_end-data);
    return self->_curNewFile->write(self->_curNewFile,curWPos,data,data_end);
}

static hpatch_BOOL _file_append_end(TNewStream* self){
    const hpatch_TStreamOutput* curNewFile=self->_curNewFile;
    assert(curNewFile!=0);
    assert(self->_curWriteToPos==self->_curWriteToPosEnd);
    self->_curNewFile=0;
    return self->_listener->closeNewFile(self->_listener,curNewFile);
}

static hpatch_BOOL _file_entry_end(TNewStream* self){
    check(!self->isFinish);
    check(self->_curPathIndex==self->_pathCount);
    check(self->_curSamePairIndex==self->_samePairCount);
    self->isFinish=hpatch_TRUE;
    return hpatch_TRUE;
}

hpatch_BOOL TNewStream_open(TNewStream* self,INewStreamListener* listener,
                            hpatch_StreamPos_t newRefDataSize, size_t newPathCount,
                            const size_t* newRefList,size_t newRefCount,
                            const size_t* samePairList,size_t samePairCount){
    assert(self->_listener==0);
    assert(self->stream==0);
    assert(self->_curNewFile==0);
    self->isFinish=hpatch_FALSE;
    self->_pathCount=newPathCount;
    self->_newRefList=newRefList;
    self->_newRefCount=newRefCount;
    self->_samePairList=samePairList;
    self->_samePairCount=samePairCount;
    
    self->_stream.streamImport=self;
    self->_stream.streamSize=newRefDataSize;
    self->_stream.write=_TNewStream_write;
    self->stream=&self->_stream;
    
    self->_curPathIndex=0;
    self->_curWriteToPos=0;
    self->_curWriteToPosEnd=0;
    self->_curNewRefIndex=0;
    self->_curSamePairIndex=0;
    return _file_append_ready(self);
}
