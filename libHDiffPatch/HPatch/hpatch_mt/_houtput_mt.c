//  _houtput_mt.c
//  hpatch
/*
 The MIT License (MIT)
 Copyright (c) 2025 HouSisong
 
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
#include "_houtput_mt.h"
#include "_patch_private_mt.h"
#if (_IS_USED_MULTITHREAD)

typedef struct houtput_mt_t{
    hpatch_TStreamOutput        base;
    const hpatch_TStreamOutput* base_stream;
    struct hpatch_mt_t*         h_mt;
    size_t                      workBufSize;
    hpatch_TWorkBuf*            curDataBuf;
    volatile hpatch_StreamPos_t curWritePos;
    volatile hpatch_TWorkBuf*   freeBufList;
    volatile hpatch_TWorkBuf*   dataBufList;
    volatile hpatch_BOOL        isOnError;
#if (defined(_DEBUG) || defined(DEBUG))
    hpatch_StreamPos_t          curOutedPos;
    volatile hpatch_BOOL        threadIsRunning;
#endif
    HLocker     _locker;
    HCondvar    _waitCondvar;
} houtput_mt_t;

    static hpatch_BOOL houtput_mt_write_(const struct hpatch_TStreamOutput* stream,hpatch_StreamPos_t writeToPos,
                                        const unsigned char* data,const unsigned char* data_end);

hpatch_inline static
hpatch_BOOL _houtput_mt_init(houtput_mt_t* self,struct hpatch_mt_t* h_mt,hpatch_TWorkBuf* freeBufList,
                             const hpatch_TStreamOutput* base_stream,hpatch_StreamPos_t curWritePos){
    memset(self,0,sizeof(*self));
    assert(freeBufList);
    self->base.streamImport=self;
    self->base.streamSize=base_stream->streamSize;
    self->base.write=houtput_mt_write_;
    self->h_mt=h_mt;
    self->base_stream=base_stream;
    self->curWritePos=curWritePos;
    self->freeBufList=freeBufList;
#if (defined(_DEBUG) || defined(DEBUG))
    self->curOutedPos=curWritePos;
#endif
    
    self->_locker=c_locker_new();
    self->_waitCondvar=c_condvar_new();
    return (self->_locker!=0)&(self->_waitCondvar!=0);
}
static void _houtput_mt_free(houtput_mt_t* self){
    if (self==0) return;
#if (defined(_DEBUG) || defined(DEBUG))
    if (self->_locker) c_locker_enter(self->_locker);
    assert(!self->threadIsRunning);
    if (self->_locker) c_locker_leave(self->_locker);
#endif
    self->base.streamImport=0;
    _thread_obj_free(c_condvar_delete,self->_waitCondvar);
    _thread_obj_free(c_locker_delete,self->_locker);
}

static void houtput_mt_setOnError_(houtput_mt_t* self) {
    hpatch_BOOL isNeedSetOnError=hpatch_FALSE;
    c_locker_enter(self->_locker);
    isNeedSetOnError=(!self->isOnError);
    if (isNeedSetOnError){
        self->isOnError=hpatch_TRUE;
        c_condvar_signal(self->_waitCondvar);
    }
    c_locker_leave(self->_locker);
    if (isNeedSetOnError)
        hpatch_mt_setOnError(self->h_mt);
}

hpatch_force_inline static
hpatch_BOOL _houtput_mt_writeAData(houtput_mt_t* self,hpatch_TWorkBuf* data){
    hpatch_StreamPos_t writePos=self->curWritePos;
    self->curWritePos+=data->data_size;
    return self->base_stream->write(self->base_stream,writePos,TWorkBuf_data(data),TWorkBuf_data_end(data));
}

static void houtput_thread_(int threadIndex,void* workData){
    houtput_mt_t* self=(houtput_mt_t*)workData;
    while ((!hpatch_mt_isOnError(self->h_mt))&(self->curWritePos<self->base.streamSize)){
        hpatch_TWorkBuf* datas=0;
        hpatch_BOOL _isOnError;
        c_locker_enter(self->_locker);
        if (!self->isOnError){
            datas=TWorkBuf_popAllBufs(&self->dataBufList);
            if (datas==0)
                c_condvar_wait(self->_waitCondvar,self->_locker);
        }
        _isOnError=self->isOnError;
        c_locker_leave(self->_locker);
        
        while (datas){
            if (_houtput_mt_writeAData(self,datas)){
                c_locker_enter(self->_locker);
                TWorkBuf_pushABufAtHead(&self->freeBufList,datas);
                c_condvar_signal(self->_waitCondvar);
                c_locker_leave(self->_locker);
                datas=datas->next;
            }else{
                houtput_mt_setOnError_(self);
                _isOnError=hpatch_TRUE;
                break;
            }
        }
        if (_isOnError) break;
    }

    hpatch_mt_onThreadEnd(self->h_mt);
#if (defined(_DEBUG) || defined(DEBUG))
    c_locker_enter(self->_locker);
    self->threadIsRunning=hpatch_FALSE;
    c_locker_leave(self->_locker);
#endif
}

static hpatch_BOOL houtput_mt_write_(const struct hpatch_TStreamOutput* stream,hpatch_StreamPos_t writeToPos,
                                     const unsigned char* data,const unsigned char* data_end){
    houtput_mt_t* self=(houtput_mt_t*)stream->streamImport;
    hpatch_BOOL result=hpatch_TRUE;
    const hpatch_BOOL isNeedFlush=(writeToPos+(size_t)(data_end-data)==self->base.streamSize);
#if (defined(_DEBUG) || defined(DEBUG))
    assert(self->curOutedPos==writeToPos);
    self->curOutedPos+=(data_end-data);
    assert(self->curOutedPos<=self->base.streamSize);
#endif
    if (writeToPos+(size_t)(data_end-data)>self->base.streamSize) return hpatch_FALSE;
    while (result&(data<data_end)){
        if (self->curDataBuf){
            size_t writeLen=self->workBufSize-self->curDataBuf->data_size;
            writeLen=(writeLen<(size_t)(data_end-data))?writeLen:(size_t)(data_end-data);
            memcpy(TWorkBuf_data_end(self->curDataBuf),data,writeLen);
            self->curDataBuf->data_size+=writeLen;
            data+=writeLen;
            if ((self->workBufSize==self->curDataBuf->data_size)|((data==data_end)&isNeedFlush)){
                c_locker_enter(self->_locker);
                TWorkBuf_pushABufAtEnd(&self->dataBufList,self->curDataBuf);
                c_condvar_signal(self->_waitCondvar);
                result=(!self->isOnError);
                c_locker_leave(self->_locker);
                self->curDataBuf=0;
            }
        }else{
            c_locker_enter(self->_locker);
            self->curDataBuf=TWorkBuf_popABuf(&self->freeBufList);
            if (self->curDataBuf==0)
                c_condvar_wait(self->_waitCondvar,self->_locker);
            else
                self->curDataBuf->data_size=0;
            result=(!self->isOnError);
            c_locker_leave(self->_locker);
        }
    }
    return result;
}

size_t houtput_mt_t_memSize(){
    return sizeof(houtput_mt_t);
}

hpatch_TStreamOutput* houtput_mt_open(void* pmem,size_t memSize,struct hpatch_mt_t* h_mt,struct hpatch_TWorkBuf* freeBufList,
                                      const hpatch_TStreamOutput* base_stream,hpatch_StreamPos_t curWritePos){
    houtput_mt_t* self=pmem;
    if (memSize<houtput_mt_t_memSize()) return 0;
    if (!_houtput_mt_init(self,h_mt,freeBufList,base_stream,curWritePos))
        goto _on_error;

    //start a thread to write
    if (!hpatch_mt_beforeThreadBegin(self->h_mt))
        goto _on_error;
#if (defined(_DEBUG) || defined(DEBUG))
    self->threadIsRunning=hpatch_TRUE;
#endif
    if (!c_thread_parallel(1,houtput_thread_,self,hpatch_FALSE,0)){
        hpatch_mt_onThreadEnd(self->h_mt);
    #if (defined(_DEBUG) || defined(DEBUG))
        self->threadIsRunning=hpatch_FALSE;
    #endif
        goto _on_error;
    }

    return &self->base;

_on_error:
    _houtput_mt_free(self);
    return 0;
}

hpatch_BOOL houtput_mt_close(hpatch_TStreamOutput* houtput_mt_stream){
    hpatch_BOOL result;
    houtput_mt_t* self=0;
    if (!houtput_mt_stream) return hpatch_TRUE;
    self=(houtput_mt_t*)houtput_mt_stream->streamImport;
    if (!self) return hpatch_TRUE;
    houtput_mt_stream->streamImport=0;

    result=(!self->isOnError)&(self->curWritePos==self->base.streamSize);
    _houtput_mt_free(self);
    return result;
}

#endif //_IS_USED_MULTITHREAD
