//  new_stream.cpp
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
#include "new_stream.h"
#if (_IS_NEED_DIR_DIFF_PATCH)
#include <string.h>
#include <stdlib.h>
#include <stdio.h> //printf


hpatch_BOOL hpatch_TNewStream_closeFileHandles(hpatch_TNewStream* self){
    hpatch_BOOL result=hpatch_TRUE;
    const hpatch_TStreamOutput* curNewFile=self->_curNewFile;
    if (curNewFile){
        self->_curNewFile=0;
        result=self->_listener->closeNewFile(self->_listener,curNewFile);
    }
    return result;
}

hpatch_BOOL hpatch_TNewStream_close(hpatch_TNewStream* self){
    return hpatch_TNewStream_closeFileHandles(self);
}

static hpatch_BOOL _file_entry_end(hpatch_TNewStream* self);
static hpatch_BOOL _file_append_ready(hpatch_TNewStream* self);
static hpatch_BOOL _file_append_part(hpatch_TNewStream* self,hpatch_StreamPos_t writeToPos,
                                     const unsigned char* data,const unsigned char* data_end);
static hpatch_BOOL _file_append_end(hpatch_TNewStream* self);

#define  check(value) { if (!(value)){ fprintf(stderr,"check "#value" error!\n"); return hpatch_FALSE; } }

static hpatch_BOOL _TNewStream_write(const hpatch_TStreamOutput* stream,hpatch_StreamPos_t writeToPos,
                                     const unsigned char* data,const unsigned char* data_end){
    hpatch_TNewStream* self=(hpatch_TNewStream*)stream->streamImport;
    self->_listener->writedNewRefData(self->_listener,data,data_end);
    while (data!=data_end) {
        size_t writeLen=(size_t)(data_end-data);
        check(!self->isFinish);
        check(writeToPos<self->_curWriteToPosEnd);
        if (writeToPos+writeLen>self->_curWriteToPosEnd)
            writeLen=(size_t)(self->_curWriteToPosEnd-writeToPos);
        check(self->_curPathIndex<self->_pathCount);
        check(_file_append_part(self,writeToPos,data,data+writeLen));
        data+=writeLen;
        writeToPos+=writeLen;
        
        if (writeToPos==self->_curWriteToPosEnd){//write one file end?
            check(_file_append_end(self));
            ++self->_curPathIndex;
            check(_file_append_ready(self));
        }
    }
    return hpatch_TRUE;
}

static hpatch_BOOL _file_append_ready(hpatch_TNewStream* self){
    assert(self->_curNewFile==0);
    assert(self->_curWriteToPos==self->_curWriteToPosEnd);
    while (self->_curPathIndex<self->_pathCount) {
        if((self->_curNewRefIndex<self->_newRefCount) //write file ?
           &&(self->_curPathIndex==self->_newRefList[self->_curNewRefIndex])){
            check(self->_listener->openNewFile(self->_listener,self->_curNewRefIndex,&self->_curNewFile));
            self->_curWriteToPosEnd+=self->_curNewFile->streamSize;
            ++self->_curNewRefIndex;
            return hpatch_TRUE;
        }else if ((self->_curSamePairIndex<self->_samePairCount) //copy file ?
            &&(self->_curPathIndex==self->_samePairList[self->_curSamePairIndex].newIndex)){
            const hpatch_TSameFilePair* pair=self->_samePairList+self->_curSamePairIndex;
            check(self->_listener->copySameFile(self->_listener,pair->newIndex,pair->oldIndex));
            ++self->_curSamePairIndex;
            ++self->_curPathIndex;
        }else{ //make dir or empty file
            check(self->_listener->makeNewDirOrEmptyFile(self->_listener,self->_curPathIndex));
            ++self->_curPathIndex;
        }
    }
    //file entry end
    check(_file_entry_end(self));
    return hpatch_TRUE;
}

static hpatch_BOOL _file_append_part(hpatch_TNewStream* self,hpatch_StreamPos_t g_writeToPos,
                                     const unsigned char* data,const unsigned char* data_end){
    hpatch_StreamPos_t curFilePos;
    assert(self->_curNewFile!=0);
    assert((g_writeToPos<=self->_curWriteToPosEnd)&&(g_writeToPos==self->_curWriteToPos));
    curFilePos=g_writeToPos-(self->_curWriteToPosEnd-self->_curNewFile->streamSize);
    self->_curWriteToPos=g_writeToPos+(size_t)(data_end-data);
    return self->_curNewFile->write(self->_curNewFile,curFilePos,data,data_end);
}

static hpatch_BOOL _file_append_end(hpatch_TNewStream* self){
    const hpatch_TStreamOutput* curNewFile=self->_curNewFile;
    assert(curNewFile!=0);
    assert(self->_curWriteToPos==self->_curWriteToPosEnd);
    self->_curNewFile=0;
    return self->_listener->closeNewFile(self->_listener,curNewFile);
}

static hpatch_BOOL _file_entry_end(hpatch_TNewStream* self){
    check(!self->isFinish);
    check(self->_curPathIndex==self->_pathCount);
    check(self->_curSamePairIndex==self->_samePairCount);
    self->isFinish=hpatch_TRUE;
    check(self->_listener->writedFinish(self->_listener));
    return hpatch_TRUE;
}

hpatch_BOOL hpatch_TNewStream_open(hpatch_TNewStream* self,hpatch_INewStreamListener* listener,
                            hpatch_StreamPos_t newRefDataSize, size_t newPathCount,
                            const size_t* newRefList,size_t newRefCount,
                            const hpatch_TSameFilePair* samePairList,size_t samePairCount){
    assert(self->_listener==0);
    assert(self->stream==0);
    assert(self->_curNewFile==0);
    self->_listener=listener;
    self->isFinish=hpatch_FALSE;
    self->_pathCount=newPathCount;
    self->_newRefList=newRefList;
    self->_newRefCount=newRefCount;
    self->_samePairList=samePairList;
    self->_samePairCount=samePairCount;
    
    self->_stream.streamImport=self;
    self->_stream.streamSize=newRefDataSize;
    self->_stream.read_writed=0;
    self->_stream.write=_TNewStream_write;
    self->stream=&self->_stream;
    
    self->_curPathIndex=0;
    self->_curWriteToPos=0;
    self->_curWriteToPosEnd=0;
    self->_curNewRefIndex=0;
    self->_curSamePairIndex=0;
    return _file_append_ready(self);
}

#endif
