//  _patch_private_mt.h
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
#ifndef _patch_private_mt_h
#define _patch_private_mt_h
#include "_hpatch_mt.h"
#ifdef __cplusplus
extern "C" {
#endif
#if (_IS_USED_MULTITHREAD)

#define _thread_obj_free(free_fn,th_obj) { if (th_obj) { free_fn(th_obj); th_obj=0; } }

typedef struct hpatch_TWorkBuf{
    struct hpatch_TWorkBuf* next;
    size_t                  data_size;
    //hpatch_byte           data[workBufSize];
} hpatch_TWorkBuf;
hpatch_force_inline static
hpatch_byte* TWorkBuf_data(hpatch_TWorkBuf* self) { return ((hpatch_byte*)self)+sizeof(hpatch_TWorkBuf); }
hpatch_force_inline static
hpatch_byte* TWorkBuf_data_end(hpatch_TWorkBuf* self) { return TWorkBuf_data(self)+self->data_size; }
hpatch_inline static
hpatch_size_t TWorkBuf_bufCount(const hpatch_TWorkBuf* self) {
    const hpatch_TWorkBuf* cur=self;
    hpatch_size_t count=0;
    while (cur){
        ++count;
        cur=cur->next;
    }
    return count;
}

hpatch_force_inline static
void TWorkBuf_pushABufAtEnd(hpatch_TWorkBuf** pnode,hpatch_TWorkBuf* data){
    while (*pnode)
        pnode=&(*pnode)->next;
    data->next=0;
    *pnode=data;
}

hpatch_force_inline static
void TWorkBuf_pushABufAtHead(hpatch_TWorkBuf** pnode,hpatch_TWorkBuf* data){
    data->next=*pnode;
    *pnode=data;
}

hpatch_force_inline static
hpatch_TWorkBuf* TWorkBuf_popABuf(hpatch_TWorkBuf** pnode){
    hpatch_TWorkBuf* result=*pnode;
    *pnode=(result)?result->next:0;
    return result;
}

hpatch_force_inline static
hpatch_TWorkBuf* TWorkBuf_popAllBufs(hpatch_TWorkBuf** pnode){
    hpatch_TWorkBuf* result=*pnode;
    *pnode=0;
    return result;
}

hpatch_force_inline static 
size_t TWorkBuf_getWorkBufSize(size_t workBufNodeSize){
                                    assert(workBufNodeSize>sizeof(hpatch_TWorkBuf));
                                    return workBufNodeSize-sizeof(hpatch_TWorkBuf); }
hpatch_inline static
hpatch_TWorkBuf* TWorkBuf_createFreeListAt(hpatch_byte* pbuf,size_t workBufCount,size_t workBufNodeSize) {
    hpatch_TWorkBuf* result = 0;
    for (; (workBufCount--)>0;pbuf+= workBufNodeSize)
        TWorkBuf_pushABufAtHead(&result, (hpatch_TWorkBuf*)pbuf);
    return result;
}
hpatch_force_inline static
hpatch_TWorkBuf* TWorkBuf_allocFreeList(void** pmem,size_t workBufCount,size_t workBufNodeSize) {
    hpatch_TWorkBuf* result=TWorkBuf_createFreeListAt((hpatch_byte*)*pmem,workBufCount,workBufNodeSize);
    (*(hpatch_byte**)pmem)+=workBufCount*workBufNodeSize;
    return result;
}

typedef struct hpatch_mt_base_t{
    struct hpatch_mt_t*         h_mt;
    size_t                      workBufSize;
    volatile hpatch_TWorkBuf*   freeBufList;
    volatile hpatch_TWorkBuf*   dataBufList;
    volatile hpatch_BOOL        isOnError;
#if (defined(_DEBUG) || defined(DEBUG))
    volatile size_t             threadIsRunning;
#endif
    HLocker     _locker;
    HCondvar    _waitCondvar;
} hpatch_mt_base_t;


hpatch_BOOL         _hpatch_mt_base_init(hpatch_mt_base_t* self,struct hpatch_mt_t* h_mt,hpatch_TWorkBuf* freeBufList,hpatch_size_t workBufSize);
void                _hpatch_mt_base_free(hpatch_mt_base_t* self);
void                hpatch_mt_base_setOnError_(hpatch_mt_base_t* self);
hpatch_TWorkBuf*    hpatch_mt_base_onceWaitABuf_(hpatch_mt_base_t* self,hpatch_TWorkBuf** pBufList,hpatch_BOOL* _isOnError);
hpatch_TWorkBuf*    hpatch_mt_base_onceWaitAllBufs_(hpatch_mt_base_t* self,hpatch_TWorkBuf** pBufList,hpatch_BOOL* _isOnError);
void                hpatch_mt_base_pushABufAtEnd_(hpatch_mt_base_t* self,hpatch_TWorkBuf** pBufList,hpatch_TWorkBuf* wbuf,hpatch_BOOL* _isOnError);
void                hpatch_mt_base_pushABufAtHead_(hpatch_mt_base_t* self,hpatch_TWorkBuf** pBufList,hpatch_TWorkBuf* wbuf,hpatch_BOOL* _isOnError);
hpatch_BOOL         hpatch_mt_base_threadsBegin_(hpatch_mt_base_t* self,int threadCount,TThreadRunCallBackProc threadProc,
                                                 void* workData,int isUseThisThread,int threadIndexOffset);
hpatch_force_inline static
hpatch_BOOL         hpatch_mt_base_aThreadBegin_(hpatch_mt_base_t* self,TThreadRunCallBackProc threadProc,void* workData){
                                                    return hpatch_mt_base_threadsBegin_(self,1,threadProc,workData,hpatch_FALSE,0); }
hpatch_force_inline static
void                hpatch_mt_base_aThreadEnd_(hpatch_mt_base_t* self){
                                                #if (defined(_DEBUG) || defined(DEBUG))
                                                    c_locker_enter(self->_locker);
                                                    assert(self->threadIsRunning);
                                                    --self->threadIsRunning;
                                                    c_locker_leave(self->_locker);
                                                #endif
                                                    if (self->h_mt)
                                                        hpatch_mt_onThreadEnd(self->h_mt);
                                                }

                                                
#define _DEF_hinput_mt_base_read() {    \
    hpatch_BOOL _isOnError=hpatch_FALSE;            \
    while ((!_isOnError)&(out_data<out_data_end)){  \
        if (self->curDataBuf==0){       \
            if (hpatch_mt_isOnError(self->mt_base.h_mt)) { _isOnError=hpatch_TRUE; break; }     \
            self->curDataBuf=hpatch_mt_base_onceWaitABuf_(&self->mt_base,(hpatch_TWorkBuf**)&self->mt_base.dataBufList,&_isOnError);       \
        }       \
        if ((self->curDataBuf!=0)&(!_isOnError)){   \
            size_t readLen=self->curDataBuf->data_size-self->curDataBuf_pos;                    \
            readLen=(readLen<(size_t)(out_data_end-out_data))?readLen:(size_t)(out_data_end-out_data);                  \
            memcpy(out_data,TWorkBuf_data(self->curDataBuf)+self->curDataBuf_pos,readLen);      \
            self->curDataBuf_pos+=readLen;          \
            out_data+=readLen;          \
            if (self->curDataBuf_pos==self->curDataBuf->data_size){ \
                hpatch_mt_base_pushABufAtHead_(&self->mt_base,(hpatch_TWorkBuf**)&self->mt_base.freeBufList,self->curDataBuf,&_isOnError); \
                self->curDataBuf=0;     \
                self->curDataBuf_pos=0; \
            }   \
        }       \
    }           \
    return (!_isOnError); }


typedef struct _hthreads_waiter_t{
    volatile unsigned int   runningThreads;
    HLocker                 _threadsEndLocker;
    HCondvar                _threadsEndCondvar;
} _hthreads_waiter_t;

hpatch_force_inline static
hpatch_BOOL _hthreads_waiter_init(_hthreads_waiter_t* self){
    assert((self->runningThreads==0)&&(self->_threadsEndLocker==0)&&(self->_threadsEndCondvar==0));
    self->_threadsEndLocker=c_locker_new();
    self->_threadsEndCondvar=c_condvar_new();
    return (self->_threadsEndCondvar!=0)&(self->_threadsEndLocker!=0);
}

hpatch_force_inline static
void _hthreads_waiter_free(_hthreads_waiter_t* self){
    assert(self->runningThreads==0);
    _thread_obj_free(c_locker_delete,self->_threadsEndLocker);
    _thread_obj_free(c_condvar_delete,self->_threadsEndCondvar);
}

hpatch_force_inline static
void _hthreads_waiter_inc(_hthreads_waiter_t* self){
    c_locker_enter(self->_threadsEndLocker);
    ++self->runningThreads;
    c_locker_leave(self->_threadsEndLocker);
}

hpatch_force_inline static
void _hthreads_waiter_dec(_hthreads_waiter_t* self){
    c_locker_enter(self->_threadsEndLocker);
    assert(self->runningThreads>0); //logic error!
    --self->runningThreads;
    if (self->runningThreads==0)
        c_condvar_signal(self->_threadsEndCondvar);
    c_locker_leave(self->_threadsEndLocker);
}

hpatch_force_inline static
void _hthreads_waiter_waitAllThreadEnd(_hthreads_waiter_t* self){
    c_locker_enter(self->_threadsEndLocker);
    while (self->runningThreads){
        c_condvar_wait(self->_threadsEndCondvar,self->_threadsEndLocker);
    }
    c_locker_leave(self->_threadsEndLocker);
}

#endif //_IS_USED_MULTITHREAD
#ifdef __cplusplus
}
#endif
#endif //_patch_private_mt_h
