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
    struct hpatch_mt_t*         h_mt;
    const hpatch_TStreamOutput* base_stream;
    volatile hpatch_StreamPos_t curWritePos;
    volatile hpatch_TWorkBuf*   dataBufList;
    volatile hpatch_BOOL        isOnError;
#if (defined(_DEBUG) || defined(DEBUG))
    volatile hpatch_BOOL        threadIsRunning;
#endif
    HLocker     _locker;
    HCondvar    _waitCondvar;
} houtput_mt_t;

hpatch_inline static
hpatch_BOOL _houtput_mt_init(houtput_mt_t* self,struct hpatch_mt_t* h_mt,
                             const hpatch_TStreamOutput* base_stream,hpatch_StreamPos_t curWritePos){
    memset(self,0,sizeof(*self));
    self->h_mt=h_mt;
    self->base_stream=base_stream;
    self->curWritePos=curWritePos;
    
    self->_locker=c_locker_new();
    self->_waitCondvar=c_condvar_new();
    return (self->_locker)&&(self->_waitCondvar);
}
static void _houtput_mt_free(houtput_mt_t* self){
    if (self==0) return;
#if (defined(_DEBUG) || defined(DEBUG))
    if (self->_locker) c_locker_enter(self->_locker);
    assert(!self->threadIsRunning);
    if (self->_locker) c_locker_leave(self->_locker);
#endif
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

static hpatch_BOOL _houtput_mt_writeAData(houtput_mt_t* self,hpatch_TWorkBuf* data){
    hpatch_StreamPos_t writePos=self->curWritePos;
    self->curWritePos+=data->data_size;
    return self->base_stream->write(self->base_stream,writePos,TWorkBuf_data(data),TWorkBuf_data_end(data));
}

static void houtput_thread_(int threadIndex,void* workData){
    houtput_mt_t* self=(houtput_mt_t*)workData;
    while ((!hpatch_mt_isOnError(self->h_mt))&&(self->curWritePos<self->base_stream->streamSize)){
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
                hpatch_mt_pushAFreeWorkBuf(self->h_mt,datas);
                datas=datas->next;
            }else{
                houtput_mt_setOnError_(self);
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

size_t houtput_mt_t_memSize(){
    return sizeof(houtput_mt_t);
}

houtput_mt_t* houtput_mt_open(houtput_mt_t* pmem,size_t memSize,struct hpatch_mt_t* h_mt,
                              const hpatch_TStreamOutput* base_stream,hpatch_StreamPos_t curWritePos){
    houtput_mt_t* self=pmem;
    if (memSize<houtput_mt_t_memSize()) return 0;
    if (!_houtput_mt_init(self,h_mt,base_stream,curWritePos))
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

    return self;

_on_error:
    _houtput_mt_free(self);
    return 0;
}

hpatch_BOOL houtput_mt_write(houtput_mt_t* self,hpatch_TWorkBuf* data){
    hpatch_BOOL result;
    c_locker_enter(self->_locker);
    TWorkBuf_pushABufAtEnd(&self->dataBufList,data);
    c_condvar_signal(self->_waitCondvar);
    result=(!self->isOnError);
    c_locker_leave(self->_locker);
    return result;
}
hpatch_BOOL houtput_mt_close(houtput_mt_t* self){
    hpatch_BOOL result;
    if (!self) return hpatch_TRUE;

    //must call hpatch_mt_waitAllThreadEnd(h_mt,) befor houtput_mt_close
    result=(!self->isOnError)&&(self->curWritePos==self->base_stream->streamSize);
    _houtput_mt_free(self);
    return result;
}

#endif //_IS_USED_MULTITHREAD
