//  _hpatch_mt.c
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
#include "_hpatch_mt.h"
#include "_patch_private_mt.h"
#if (_IS_USED_MULTITHREAD)

typedef struct hpatch_mt_t{
    volatile hpatch_TWorkBuf*   freeBufList;
    size_t                      workBufSize;
    volatile hpatch_BOOL    isOnError;
    volatile hpatch_BOOL    isOnFinish;
    volatile unsigned int   runningThreads;
    HLocker     _locker;
    HCondvar    _waitCondvar;
} hpatch_mt_t;

hpatch_inline static
hpatch_BOOL _hpatch_mt_init(hpatch_mt_t* self) {
    memset(self,0,sizeof(*self));
    self->_locker=c_locker_new();
    self->_waitCondvar=c_condvar_new();
    return (self->_locker!=0)&(self->_waitCondvar!=0);
}
static void _hpatch_mt_free(hpatch_mt_t* self) {
    if (!self) return;
#if (defined(_DEBUG) || defined(DEBUG))
    if (self->_locker) c_locker_enter(self->_locker);
    assert(self->runningThreads==0);
    if (self->_locker) c_locker_leave(self->_locker); 
#endif
    _thread_obj_free(c_condvar_delete,self->_waitCondvar);
    _thread_obj_free(c_locker_delete,self->_locker);
}

static void _hpatch_mt_setOnError(hpatch_mt_t* self) {
    if (!self->isOnError){
        self->isOnFinish=hpatch_TRUE;
        self->isOnError=hpatch_TRUE;
    }
    c_condvar_signal(self->_waitCondvar);
}

size_t hpatch_mt_t_memSize(){
    return sizeof(hpatch_mt_t);
}
hpatch_mt_t* hpatch_mt_open(void* pmem,size_t memSumSize,size_t workBufCount,size_t workBufNodeSize){
    size_t i;
    hpatch_mt_t* self=(hpatch_mt_t*)pmem;
    hpatch_byte* temp_cache=((hpatch_byte*)self)+hpatch_mt_t_memSize();
    if (memSumSize<hpatch_mt_t_memSize()+workBufCount*workBufNodeSize) return 0;
    if (workBufCount) assert(workBufNodeSize>sizeof(hpatch_TWorkBuf));
    if (!_hpatch_mt_init(self)){
        _hpatch_mt_free(self);
        return 0;
    }
    self->workBufSize=(workBufCount>0)?workBufNodeSize-sizeof(hpatch_TWorkBuf):0;
    for (i=0;i<workBufCount;i++,temp_cache+=workBufNodeSize){
        hpatch_TWorkBuf* workBuf=(hpatch_TWorkBuf*)temp_cache;
        TWorkBuf_pushABufAtHead(&self->freeBufList,workBuf);
    }
    return self;
}
size_t hpatch_mt_workBufSize(const hpatch_mt_t* self){
    return self->workBufSize;
}

hpatch_TWorkBuf* hpatch_mt_popFreeWorkBuf_fast(struct hpatch_mt_t* self,size_t needBufCount){
    hpatch_TWorkBuf* bufList=0;
    if (self->isOnError) return 0;
    while (needBufCount--){
        hpatch_TWorkBuf* workBuf=TWorkBuf_popABuf(&self->freeBufList);
        assert(workBuf);
        if (workBuf){
            TWorkBuf_pushABufAtHead(&bufList,workBuf);
        }else{
            self->isOnError=hpatch_TRUE;
            return 0;
        }
    }
    return bufList;
}

hpatch_BOOL hpatch_mt_beforeThreadBegin(hpatch_mt_t* self){
    hpatch_BOOL result;
    c_locker_enter(self->_locker);
    result=(!self->isOnError);
    if (result)
        ++self->runningThreads;
    c_locker_leave(self->_locker);
    return result;
}
void hpatch_mt_onThreadEnd(hpatch_mt_t* self){
    c_locker_enter(self->_locker);
    assert(self->runningThreads>0);
    if (self->runningThreads>0){
        --self->runningThreads;
        if (self->runningThreads==0)
            c_condvar_signal(self->_waitCondvar);
    }else{
        LOG_ERR("hpatch_mt_t runningThreads logic error");
        _hpatch_mt_setOnError(self);
    }
    c_locker_leave(self->_locker);
}

hpatch_BOOL hpatch_mt_isOnFinish(hpatch_mt_t* self){
    hpatch_BOOL result;
    c_locker_enter(self->_locker);
    result=self->isOnFinish;
    c_locker_leave(self->_locker);
    return result;
}
hpatch_BOOL hpatch_mt_isOnError(hpatch_mt_t* self){
    hpatch_BOOL result;
    c_locker_enter(self->_locker);
    result=self->isOnError;
    c_locker_leave(self->_locker);
    return result;
}

void hpatch_mt_setOnError(struct hpatch_mt_t* self){
    c_locker_enter(self->_locker);
    _hpatch_mt_setOnError(self);
    c_locker_leave(self->_locker);
}

void hpatch_mt_waitAllThreadEnd(hpatch_mt_t* self,hpatch_BOOL isOnError){
    c_locker_enter(self->_locker);
    self->isOnFinish=hpatch_TRUE;
    if (isOnError) _hpatch_mt_setOnError(self);
    while (self->runningThreads){
        c_condvar_wait(self->_waitCondvar,self->_locker);
    }
    c_locker_leave(self->_locker);
}
hpatch_BOOL hpatch_mt_close(hpatch_mt_t* self,hpatch_BOOL isOnError){
    if (!self) return !isOnError;
    hpatch_mt_waitAllThreadEnd(self,isOnError);
    isOnError|=self->isOnError;
    _hpatch_mt_free(self);
    return !isOnError;
}

#endif //_IS_USED_MULTITHREAD
