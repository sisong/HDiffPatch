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
    size_t                  threadNum;
    HCondvar*               condvarList;
    volatile hpatch_BOOL    isOnFinish;
    volatile hpatch_BOOL    isOnError;
    HLocker                 _locker;
    _hthreads_waiter_t      threads_waiter;
//onFinishThread: JNI & ANDROID can call JavaVM->DetachCurrentThread() when thread end
    void*       finishThreadListener;
    void        (*onFinishThread)(void* finishThreadListener);
} hpatch_mt_t;

hpatch_force_inline static
hpatch_BOOL _hpatch_mt_init(hpatch_mt_t* self,size_t threadNum) {
    memset(self,0,sizeof(*self));
    self->threadNum=threadNum;
    self->condvarList=(HCondvar*)(self+1);
    memset(self->condvarList,0,threadNum*sizeof(HCondvar));
    self->_locker=c_locker_new();
    if (self->_locker==0) return hpatch_FALSE;
    if (!_hthreads_waiter_init(&self->threads_waiter))  return hpatch_FALSE;
    return hpatch_TRUE;
}
static void _hpatch_mt_free(hpatch_mt_t* self) {
    if (!self) return;
    _hthreads_waiter_free(&self->threads_waiter);
    _thread_obj_free(c_locker_delete,self->_locker);
#if (defined(_DEBUG) || defined(DEBUG))
    {
        size_t i;
        for (i=0;i<self->threadNum;++i)
            assert(self->condvarList[i]==0);
    }
#endif
}

static void _hpatch_mt_condvarList_broadcast(struct hpatch_mt_t* self){
    size_t i;
    for (i=0;i<self->threadNum;++i){
        if (self->condvarList[i])
            c_condvar_broadcast(self->condvarList[i]);
    }
}
hpatch_BOOL hpatch_mt_registeCondvar(struct hpatch_mt_t* self,HCondvar waitCondvar){
    size_t i;
    assert(waitCondvar!=0);
    for (i=0;i<self->threadNum;++i){
        if (self->condvarList[i]==0){
            self->condvarList[i]=waitCondvar;
            return hpatch_TRUE;
        }
    }
    assert(hpatch_FALSE);
    return hpatch_FALSE;
}
hpatch_BOOL hpatch_mt_unregisteCondvar(struct hpatch_mt_t* self,HCondvar waitCondvar){
    size_t i;
    assert(waitCondvar!=0);
    for (i=0;i<self->threadNum;++i){
        if (self->condvarList[i]==waitCondvar){
            self->condvarList[i]=0;
            return hpatch_TRUE;
        }
    }
    assert(hpatch_FALSE);
    return hpatch_FALSE;
}

hpatch_inline static
void __hpatch_mt_setOnError(hpatch_mt_t* self) {
    if (!self->isOnError){
        self->isOnFinish=hpatch_TRUE;
        self->isOnError=hpatch_TRUE;
        _hpatch_mt_condvarList_broadcast(self);
    }
}
hpatch_inline static
void __hpatch_mt_setOnFinish(hpatch_mt_t* self) {
    if (!self->isOnFinish){
        self->isOnFinish=hpatch_TRUE;
        _hpatch_mt_condvarList_broadcast(self);
    }
}

size_t hpatch_mt_t_memSize(size_t maxThreadNum){
    return sizeof(hpatch_mt_t)+maxThreadNum*sizeof(HCondvar);
}
hpatch_mt_t* hpatch_mt_open(void* pmem,size_t memSize,size_t maxThreadNum){
    hpatch_mt_t* self=(hpatch_mt_t*)pmem;
    if (memSize<hpatch_mt_t_memSize(maxThreadNum)) return 0;
    if (!_hpatch_mt_init(self,maxThreadNum)){
        _hpatch_mt_free(self);
        return 0;
    }
    return self;
}

hpatch_BOOL hpatch_mt_beforeThreadBegin(hpatch_mt_t* self){
    hpatch_BOOL isOnError=hpatch_mt_isOnError(self);
    if (!isOnError)
        _hthreads_waiter_inc(&self->threads_waiter);
    return (!isOnError);
}

void hpatch_mt_setOnThreadEnd(struct hpatch_mt_t* self,void* finishThreadListener,
                              void (*onFinishThread)(void* finishThreadListener)){
    self->finishThreadListener=finishThreadListener;
    self->onFinishThread=onFinishThread;
}

void hpatch_mt_onThreadEnd(hpatch_mt_t* self){
    if (self->onFinishThread)
        self->onFinishThread(self->finishThreadListener);
    _hthreads_waiter_dec(&self->threads_waiter);
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
    __hpatch_mt_setOnError(self);
    c_locker_leave(self->_locker);
}

void hpatch_mt_waitAllThreadEnd(hpatch_mt_t* self,hpatch_BOOL isOnError){
    c_locker_enter(self->_locker);
    if (isOnError)
        __hpatch_mt_setOnError(self);
    else
        __hpatch_mt_setOnFinish(self);
    c_locker_leave(self->_locker);

    _hthreads_waiter_waitAllThreadEnd(&self->threads_waiter);
}
hpatch_BOOL hpatch_mt_close(hpatch_mt_t* self,hpatch_BOOL isOnError){
    if (!self) return (!isOnError);
    hpatch_mt_waitAllThreadEnd(self,isOnError);
    isOnError|=hpatch_mt_isOnError(self);
    _hpatch_mt_free(self);
    return (!isOnError);
}


//hpatch_mt_base_t func

hpatch_BOOL _hpatch_mt_base_init(hpatch_mt_base_t* self,struct hpatch_mt_t* h_mt,hpatch_TWorkBuf* freeBufList,hpatch_size_t workBufSize){
    assert(self->h_mt==0);
    self->h_mt=h_mt;
    self->freeBufList=freeBufList;
    self->workBufSize=workBufSize;

    self->_locker=c_locker_new();
    self->_waitCondvar=c_condvar_new();
    if ((self->_waitCondvar!=0)&(self->h_mt!=0)){
        if (!hpatch_mt_registeCondvar(self->h_mt,self->_waitCondvar))
            return hpatch_FALSE;
    }
    return (self->_locker!=0)&(self->_waitCondvar!=0);
}

void _hpatch_mt_base_free(hpatch_mt_base_t* self){
    assert(self!=0);
#if (defined(_DEBUG) || defined(DEBUG))
    if (self->_locker) c_locker_enter(self->_locker);
    assert(self->threadIsRunning==0);
    if (self->_locker) c_locker_leave(self->_locker);
#endif 
    if ((self->_waitCondvar!=0)&(self->h_mt!=0))
        hpatch_mt_unregisteCondvar(self->h_mt,self->_waitCondvar);
    _thread_obj_free(c_condvar_delete,self->_waitCondvar);
    _thread_obj_free(c_locker_delete,self->_locker);
}

void hpatch_mt_base_setOnError_(hpatch_mt_base_t* self){
    hpatch_BOOL isNeedSetOnError;
    c_locker_enter(self->_locker);
    isNeedSetOnError=(!self->isOnError);
    if (isNeedSetOnError){
        self->isOnError=hpatch_TRUE;
        c_condvar_broadcast(self->_waitCondvar);
    }
    c_locker_leave(self->_locker);
    if (isNeedSetOnError&(self->h_mt!=0))
        hpatch_mt_setOnError(self->h_mt);
}

hpatch_TWorkBuf* hpatch_mt_base_onceWaitABuf_(hpatch_mt_base_t* self,hpatch_TWorkBuf** pBufList,hpatch_BOOL* _isOnError){
    hpatch_TWorkBuf* wbuf=0;
    c_locker_enter(self->_locker);
    wbuf=TWorkBuf_popABuf(pBufList);
    if ((wbuf==0)&(!self->isOnError)){
        c_condvar_wait(self->_waitCondvar,self->_locker);
        wbuf=TWorkBuf_popABuf(pBufList);
    }
    *_isOnError=self->isOnError;
    c_locker_leave(self->_locker);
    return wbuf;
}

hpatch_TWorkBuf* hpatch_mt_base_onceWaitAllBufs_(hpatch_mt_base_t* self,hpatch_TWorkBuf** pBufList,hpatch_BOOL* _isOnError){
    hpatch_TWorkBuf* wbufs=0;
    c_locker_enter(self->_locker);
    wbufs=TWorkBuf_popAllBufs(pBufList);
    if ((wbufs==0)&(!self->isOnError)){
        c_condvar_wait(self->_waitCondvar,self->_locker);
        wbufs=TWorkBuf_popAllBufs(pBufList);
    }
    *_isOnError=self->isOnError;
    c_locker_leave(self->_locker);
    return wbufs;
}

void hpatch_mt_base_pushABufAtEnd_(hpatch_mt_base_t* self,hpatch_TWorkBuf** pBufList,hpatch_TWorkBuf* wbuf,hpatch_BOOL* _isOnError){
    c_locker_enter(self->_locker);
    TWorkBuf_pushABufAtEnd(pBufList,wbuf);
    c_condvar_signal(self->_waitCondvar);
    *_isOnError=self->isOnError;
    c_locker_leave(self->_locker);
}

void hpatch_mt_base_pushABufAtHead_(hpatch_mt_base_t* self,hpatch_TWorkBuf** pBufList,hpatch_TWorkBuf* wbuf,hpatch_BOOL* _isOnError){
    c_locker_enter(self->_locker);
    TWorkBuf_pushABufAtHead(pBufList,wbuf);
    c_condvar_signal(self->_waitCondvar);
    *_isOnError=self->isOnError;
    c_locker_leave(self->_locker);
}

hpatch_BOOL hpatch_mt_base_threadsBegin_(hpatch_mt_base_t* self,int threadCount,TThreadRunCallBackProc threadProc,
                                         void* workData,int isUseThisThread,int threadIndexOffset){
    int i;
    for (i=0;i<threadCount;++i){
        if (self->h_mt&&(!hpatch_mt_beforeThreadBegin(self->h_mt)))
            return hpatch_FALSE;
    #if (defined(_DEBUG) || defined(DEBUG))
        ++self->threadIsRunning;
    #endif
        if (!c_thread_parallel(1,threadProc,workData,(isUseThisThread!=0)&(i+1==threadCount),i+threadIndexOffset)){
            if (self->h_mt) hpatch_mt_onThreadEnd(self->h_mt);
        #if (defined(_DEBUG) || defined(DEBUG))
            --self->threadIsRunning;
        #endif
            return hpatch_FALSE;
        }
    }
    return hpatch_TRUE;
}


#endif //_IS_USED_MULTITHREAD
