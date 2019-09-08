//  parallel_import.cpp
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
#include "parallel_import.h"
#include <assert.h>

#if (_IS_USED_PTHREAD)
#   ifdef WIN32
#       include "windows.h" //for Sleep
#       //pragma comment(lib,"pthread.lib") //for static pthread lib
#       //define PTW32_STATIC_LIB  //for static pthread lib
#   endif
#   include <pthread.h>
#   ifdef __ANDROID__
#       include <sched.h> // sched_yield()
#   endif
#endif
#if (_IS_USED_CPP11THREAD)
#   include <thread>
#endif
#if (_IS_USED_WIN32THREAD)
#   include "windows.h"
#   ifndef UNDER_CE
#       include "process.h"
#   endif
#endif

#if (_IS_USED_PTHREAD)
#include <string.h> //for memset
#include <stdlib.h>
#include <stdexcept>
#include <string>
#include <stdio.h>

static std::string _i2a(int ivalue){
    const int kMaxNumCharSize =32;
    std::string result;
    result.resize(kMaxNumCharSize);
#ifdef _MSC_VER
    int len=sprintf_s(&result[0],kMaxNumCharSize,"%d",ivalue);
#else
    int len=sprintf(&result[0],"%d",ivalue);
#endif
    result.resize(len);
    return result;
}
#define _check_pthread(result,func_name) { \
    if (result!=0) throw std::runtime_error(func_name "() return "+_i2a(result)+" error!"); }

HLocker locker_new(void){
    pthread_mutex_t* self=new pthread_mutex_t();
    int rt=pthread_mutex_init(self, 0);
    if (rt!=0){
        delete self;
        _check_pthread(rt,"pthread_mutex_init");
    }
    return  self;
}
void locker_delete(HLocker locker){
    if (locker!=0){
        pthread_mutex_t* self=(pthread_mutex_t*)locker;
        int rt=pthread_mutex_destroy(self);
        delete self;
        _check_pthread(rt,"pthread_mutex_destroy");
    }
}

void locker_enter(HLocker locker){
    pthread_mutex_t* self=(pthread_mutex_t*)locker;
    int rt=pthread_mutex_lock(self);
    _check_pthread(rt,"pthread_mutex_lock");
}
void locker_leave(HLocker locker){
    pthread_mutex_t* self=(pthread_mutex_t*)locker;
    int rt=pthread_mutex_unlock(self);
    _check_pthread(rt,"pthread_mutex_unlock");
}

HCondvar condvar_new(void){
    pthread_cond_t* self=new pthread_cond_t();
    int rt=pthread_cond_init(self,0);
    if (rt!=0){
        delete self;
        _check_pthread(rt,"pthread_cond_init");
    }
    return self;
}
void    condvar_delete(HCondvar cond){
    if (cond){
        pthread_cond_t* self=(pthread_cond_t*)cond;
        int rt=pthread_cond_destroy(self);
        delete self;
        _check_pthread(rt,"pthread_cond_destroy");
    }
}
void    condvar_wait(HCondvar cond,TLockerBox* lockerBox){
    pthread_cond_t* self=(pthread_cond_t*)cond;
    int rt=pthread_cond_wait(self,(pthread_mutex_t*)(lockerBox->locker));
    _check_pthread(rt,"pthread_cond_wait");
}
void    condvar_signal(HCondvar cond){
    pthread_cond_t* self=(pthread_cond_t*)cond;
    int rt=pthread_cond_signal(self);
    _check_pthread(rt,"pthread_cond_signal");
}
void    condvar_broadcast(HCondvar cond){
    pthread_cond_t* self=(pthread_cond_t*)cond;
    int rt=pthread_cond_broadcast(self);
    _check_pthread(rt,"pthread_cond_broadcast");
}

void this_thread_yield(){
#ifdef __APPLE__
    pthread_yield_np();
#else
#   ifdef WIN32
    Sleep(0);
#   else
#       ifdef __ANDROID__
            sched_yield();
#       else
            pthread_yield();
#       endif
#   endif
#endif
}

struct _TThreadData{
    TThreadRunCallBackProc  threadProc;
    int                     threadIndex;
    void*                   workData;
};
static void* _pt_threadProc(void* _pt){
    _TThreadData* pt=(_TThreadData*)_pt;
    pt->threadProc(pt->threadIndex,pt->workData);
    delete pt;
    return 0;
}

void thread_parallel(int threadCount,TThreadRunCallBackProc threadProc,void* workData,
                     int isUseThisThread,int threadIndexStart){
    for (int i=0; i<threadCount; ++i) {
        if ((i==threadCount-1)&&(isUseThisThread)){
            threadProc(i+threadIndexStart,workData);
        }else{
            _TThreadData* pt=new _TThreadData();
            pt->threadIndex=i+threadIndexStart;
            pt->threadProc=threadProc;
            pt->workData=workData;
            pthread_t t; memset(&t,0,sizeof(pthread_t));
            int rt=pthread_create(&t,0,_pt_threadProc,pt);
            if (rt!=0){
                delete pt;
                _check_pthread(rt,"pthread_create");
            }
            rt=pthread_detach(t);
            _check_pthread(rt,"pthread_detach");
        }
    }
}
#endif //_IS_USED_PTHREAD

#if (_IS_USED_CPP11THREAD)
HLocker locker_new(void){
    return new std::mutex();
}
void locker_delete(HLocker locker){
    if (locker!=0)
        delete (std::mutex*)locker;
}

void locker_enter(HLocker locker){
    std::mutex* self=(std::mutex*)locker;
    self->lock();
    
}
void locker_leave(HLocker locker){
    std::mutex* self=(std::mutex*)locker;
    self->unlock();
}

HCondvar condvar_new(void){
    std::condition_variable* self=new std::condition_variable();
    return self;
} 
void    condvar_delete(HCondvar cond){
    if (cond){
        std::condition_variable* self=(std::condition_variable*)cond;
        delete self;
    }
}
void    condvar_wait(HCondvar cond,TLockerBox* locker){
    std::condition_variable* self=(std::condition_variable*)cond;
    _TLockerBox_name* _locker=(_TLockerBox_name*)locker;
    self->wait(*_locker);
}
void    condvar_signal(HCondvar cond){
    std::condition_variable* self=(std::condition_variable*)cond;
    self->notify_one();
}
void    condvar_broadcast(HCondvar cond){
    std::condition_variable* self=(std::condition_variable*)cond;
    self->notify_all();
}

void this_thread_yield(){
    std::this_thread::yield();
}


struct _TThreadProc{
    TThreadRunCallBackProc threadProc;
    int   threadIndex;
    void* workData;
    inline void operator()(){  threadProc(threadIndex,workData);  }
};
void thread_parallel(int threadCount,TThreadRunCallBackProc threadProc,void* workData,
                     int isUseThisThread,int threadIndexStart){
    for (int i=0; i<threadCount; ++i) {
        if ((i==threadCount-1)&&(isUseThisThread)){
            threadProc(i+threadIndexStart,workData);
        }else{
            _TThreadProc tp={threadProc,i+threadIndexStart,workData};
            std::thread t(tp);
            t.detach();
        }
    }
}
#endif //_IS_USED_CPP11THREAD


#if (_IS_USED_WIN32THREAD)
#include <string.h> //for memset
#include <stdlib.h>
#include <stdexcept>
#include <string>
#include <stdio.h>

#define _check(value,func_name) { \
    if (!(value)) throw std::runtime_error(func_name "() run error!"); }


HLocker locker_new(void){
    CRITICAL_SECTION* self=new CRITICAL_SECTION();
    InitializeCriticalSection(self);
    return  self;
}
void locker_delete(HLocker locker){
    if (locker!=0){
        CRITICAL_SECTION* self=(CRITICAL_SECTION*)locker;
        DeleteCriticalSection(self);
        delete self;
    }
}

void locker_enter(HLocker locker){
    CRITICAL_SECTION* self=(CRITICAL_SECTION*)locker;
    EnterCriticalSection(self);
}
void locker_leave(HLocker locker){
    CRITICAL_SECTION* self=(CRITICAL_SECTION*)locker;
    LeaveCriticalSection(self);
}

HCondvar condvar_new(void){
    CONDITION_VARIABLE* self=new CONDITION_VARIABLE();
    InitializeConditionVariable(self);
    return self;
}
void    condvar_delete(HCondvar cond){
    if (cond){
        CONDITION_VARIABLE* self=(CONDITION_VARIABLE*)cond;
        delete self;
    }
}
void    condvar_wait(HCondvar cond,TLockerBox* lockerBox){
    CONDITION_VARIABLE* self=(CONDITION_VARIABLE*)cond;
    BOOL rt=SleepConditionVariableCS(self,(CRITICAL_SECTION*)(lockerBox->locker),INFINITE);
    _check(rt,"SleepConditionVariableCS");
}
void    condvar_signal(HCondvar cond){
    CONDITION_VARIABLE* self=(CONDITION_VARIABLE*)cond;
    WakeConditionVariable(self);
}
void    condvar_broadcast(HCondvar cond){
    CONDITION_VARIABLE* self=(CONDITION_VARIABLE*)cond;
    WakeAllConditionVariable(self);
}

void this_thread_yield(){
    Sleep(0);
}

struct _TThreadData{
    TThreadRunCallBackProc  threadProc;
    int                     threadIndex;
    void*                   workData;
};

#ifdef UNDER_CE
    static DWORD WINAPI _win_threadProc(void* _pt){
#else
    static unsigned int WINAPI _win_threadProc(void* _pt){
#endif
        _TThreadData* pt=(_TThreadData*)_pt;
        pt->threadProc(pt->threadIndex,pt->workData);
        delete pt;
        return 0;
    }

template <class TProc>
inline static HANDLE _thread_create(TProc func,void* param){
#ifdef UNDER_CE
    DWORD threadId;
    return CreateThread(0,0,func,param,0,&threadId);
#else
    unsigned threadId;
    return (HANDLE)_beginthreadex(0,0,func,param,0,&threadId);
#endif
}

void thread_parallel(int threadCount,TThreadRunCallBackProc threadProc,void* workData,
                     int isUseThisThread,int threadIndexStart){
    for (int i=0; i<threadCount; ++i) {
        if ((i==threadCount-1)&&(isUseThisThread)){
            threadProc(i+threadIndexStart,workData);
        }else{
            _TThreadData* pt=new _TThreadData();
            pt->threadIndex=i+threadIndexStart;
            pt->threadProc=threadProc;
            pt->workData=workData;
            HANDLE rt=_thread_create(_win_threadProc,pt);
            if (rt==0){
                delete pt;
                _check(rt!=0,"_thread_create");
            }
        }
    }
}
#endif //_IS_USED_WIN32THREAD
