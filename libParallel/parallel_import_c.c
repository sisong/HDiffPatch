//  parallel_import_c.c
/*
 The MIT License (MIT)
 Copyright (c) 2018-2025 HouSisong
 
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
#include "parallel_import_c.h"

#if (_IS_USED_PTHREAD)
#   include <pthread.h>
#   if ((defined _WIN32)||(defined WIN32))
#       include "windows.h" //for Sleep
#       //pragma comment(lib,"pthread.lib") //for static pthread lib
#       //define PTW32_STATIC_LIB  //for static pthread lib
#   else
#       include <sched.h> // sched_yield()
#   endif
#endif
#if (_IS_USED_WIN32THREAD)
#   include "windows.h"
#   ifndef UNDER_CE
#       include "process.h"
#   endif
#endif

#ifdef ANDROID
#   include <android/log.h>
#   define LOG_ERR(...) __android_log_print(ANDROID_LOG_ERROR, "hpatch", __VA_ARGS__)
#else
#   include <stdio.h>  //for stderr
#   define LOG_ERR(...) fprintf(stderr,__VA_ARGS__)
#endif

typedef struct{
    TThreadRunCallBackProc  threadProc;
    int                     threadIndex;
    void*                   workData;
} _TThreadData;

#if ((defined _DEBUG) || (defined DEBUG))
#   define _check_malloc(p) { if (!(p)) { LOG_ERR("malloc error"); return c_mt_NULL; } }
#else
#   define _check_malloc(p) { if (!(p)) return c_mt_NULL; }
#endif

int _parallel_import_c_exit_on_error=0;
#define _check_exit()   { if (_parallel_import_c_exit_on_error) exit(_parallel_import_c_exit_on_error); }

#if (_IS_USED_PTHREAD)
#include <string.h> //for memset
#include <stdlib.h>
#include <stdio.h>

#define _LOG_ERR_PT(rt,func_name)   { LOG_ERR("pthread error: %s() return %d",func_name,rt);}
#define _check_pt_init(rt,func_name){ if (rt!=0) { _LOG_ERR_PT(rt,func_name); } }
#define _check_pt(rt,func_name)     { if (rt!=0) { _LOG_ERR_PT(rt,func_name); _check_exit(); } }

HLocker c_locker_new(void){
    pthread_mutex_t* self=(pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    _check_malloc(self);
    int rt=pthread_mutex_init(self,0);
    if (rt!=0){
        free(self);
        _check_pt_init(rt,"pthread_mutex_init");
        return c_mt_NULL; //error
    }
    return  self;
}
c_mt_bool_t c_locker_delete(HLocker locker){
    int rt=0;
    if (locker!=0){
        pthread_mutex_t* self=(pthread_mutex_t*)locker;
        rt=pthread_mutex_destroy(self);
        free(self);
        _check_pt(rt,"pthread_mutex_destroy");
    }
    return (rt==0);
}

c_mt_bool_t c_locker_enter(HLocker locker){
    pthread_mutex_t* self=(pthread_mutex_t*)locker;
    int rt=pthread_mutex_lock(self);
    _check_pt(rt,"pthread_mutex_lock");
    return (rt==0);
}
c_mt_bool_t c_locker_leave(HLocker locker){
    pthread_mutex_t* self=(pthread_mutex_t*)locker;
    int rt=pthread_mutex_unlock(self);
    _check_pt(rt,"pthread_mutex_unlock");
    return (rt==0);
}

HCondvar c_condvar_new(void){
    pthread_cond_t* self=(pthread_cond_t*)malloc(sizeof(pthread_cond_t));
    _check_malloc(self);
    int rt=pthread_cond_init(self,0);
    if (rt!=0){
        free(self);
        _check_pt_init(rt,"pthread_cond_init");
        return c_mt_NULL; //error
    }
    return self;
}
c_mt_bool_t c_condvar_delete(HCondvar cond){
    int rt=0;
    if (cond){
        pthread_cond_t* self=(pthread_cond_t*)cond;
        rt=pthread_cond_destroy(self);
        free(self);
        _check_pt(rt,"pthread_cond_destroy");
    }
    return (rt==0);
}
c_mt_bool_t c_condvar_wait(HCondvar cond,HLocker locker){
    pthread_cond_t* self=(pthread_cond_t*)cond;
    int rt=pthread_cond_wait(self,(pthread_mutex_t*)locker);
    _check_pt(rt,"pthread_cond_wait");
    return (rt==0);
}
c_mt_bool_t c_condvar_signal(HCondvar cond){
    pthread_cond_t* self=(pthread_cond_t*)cond;
    int rt=pthread_cond_signal(self);
    _check_pt(rt,"pthread_cond_signal");
    return (rt==0);
}
c_mt_bool_t c_condvar_broadcast(HCondvar cond){
    pthread_cond_t* self=(pthread_cond_t*)cond;
    int rt=pthread_cond_broadcast(self);
    _check_pt(rt,"pthread_cond_broadcast");
    return (rt==0);
}

void c_this_thread_yield(){
#if ((defined _WIN32)||(defined WIN32))
    Sleep(0);
#else
    sched_yield();
#endif
}

static void* _pt_threadProc(void* _pt){
    _TThreadData* pt=(_TThreadData*)_pt;
    pt->threadProc(pt->threadIndex,pt->workData);
    free(pt);
    return 0;
}

c_mt_bool_t c_thread_parallel(int threadCount,TThreadRunCallBackProc threadProc,void* workData,
                              int isUseThisThread,int threadIndexOffset){
    for (int i=0; i<threadCount; ++i) {
        if ((i==threadCount-1)&&(isUseThisThread)){
            threadProc(i+threadIndexOffset,workData);
        }else{
            _TThreadData* pt=(_TThreadData*)malloc(sizeof(_TThreadData));
            _check_malloc(pt);
            pt->threadIndex=i+threadIndexOffset;
            pt->threadProc=threadProc;
            pt->workData=workData;
            pthread_t t; memset(&t,0,sizeof(pthread_t));
            int rt=pthread_create(&t,0,_pt_threadProc,pt);
            if (rt!=0){
                free(pt);
                _check_pt_init(rt,"pthread_create");
                return c_mt_bool_FALSE; //error
            }
            rt=pthread_detach(t);
            if (rt!=0){
                _check_pt(rt,"pthread_detach");
                return c_mt_bool_FALSE; //error
            }
        }
    }
    return c_mt_bool_TRUE;
}
#endif //_IS_USED_PTHREAD


#if (_IS_USED_WIN32THREAD)
#include <string.h> //for memset
#include <stdlib.h>
#include <stdio.h>


#define _LOG_ERR_WT(func_name)   { LOG_ERR("win32 thread error: %s() return false",func_name);}
#define _check_wt_init(func_name){ _LOG_ERR_WT(func_name); }
#define _check_wt(func_name)     { _LOG_ERR_WT(func_name); _check_exit(); }

HLocker c_locker_new(void){
    CRITICAL_SECTION* self=(CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION));
    _check_malloc(self);
    InitializeCriticalSection(self);
    return  self;
}
c_mt_bool_t c_locker_delete(HLocker locker){
    if (locker!=0){
        CRITICAL_SECTION* self=(CRITICAL_SECTION*)locker;
        DeleteCriticalSection(self);
        free(self);
    }
    return c_mt_bool_TRUE;
}

c_mt_bool_t c_locker_enter(HLocker locker){
    CRITICAL_SECTION* self=(CRITICAL_SECTION*)locker;
    EnterCriticalSection(self);
    return c_mt_bool_TRUE;
}
c_mt_bool_t c_locker_leave(HLocker locker){
    CRITICAL_SECTION* self=(CRITICAL_SECTION*)locker;
    LeaveCriticalSection(self);
    return c_mt_bool_TRUE;
}

HCondvar c_condvar_new(void){
    CONDITION_VARIABLE* self=(CONDITION_VARIABLE*)malloc(sizeof(CONDITION_VARIABLE));
    _check_malloc(self);
    InitializeConditionVariable(self);
    return self;
}
c_mt_bool_t c_condvar_delete(HCondvar cond){
    if (cond){
        CONDITION_VARIABLE* self=(CONDITION_VARIABLE*)cond;
        free(self);
    }
    return c_mt_bool_TRUE;
}
c_mt_bool_t c_condvar_wait(HCondvar cond,HLocker locker){
    CONDITION_VARIABLE* self=(CONDITION_VARIABLE*)cond;
    BOOL rt=SleepConditionVariableCS(self,(CRITICAL_SECTION*)locker,INFINITE);
    if (!rt) _check_wt("SleepConditionVariableCS");
    return (rt!=0);
}
c_mt_bool_t c_condvar_signal(HCondvar cond){
    CONDITION_VARIABLE* self=(CONDITION_VARIABLE*)cond;
    WakeConditionVariable(self);
    return c_mt_bool_TRUE;
}
c_mt_bool_t c_condvar_broadcast(HCondvar cond){
    CONDITION_VARIABLE* self=(CONDITION_VARIABLE*)cond;
    WakeAllConditionVariable(self);
    return c_mt_bool_TRUE;
}

void c_this_thread_yield(){
    Sleep(0);
}

#ifdef UNDER_CE
    typedef DWORD (WINAPI *_ThreadProc_t)(void*);
    static DWORD WINAPI _win_threadProc(void* _pt){
#else
    typedef unsigned int (WINAPI *_ThreadProc_t)(void*);
    static unsigned int WINAPI _win_threadProc(void* _pt){
#endif
        _TThreadData* pt=(_TThreadData*)_pt;
        pt->threadProc(pt->threadIndex,pt->workData);
        free(pt);
        return 0;
    }

static HANDLE _thread_create(_ThreadProc_t func,void* param){
#ifdef UNDER_CE
    DWORD threadId;
    return CreateThread(0,0,func,param,0,&threadId);
#else
    unsigned threadId;
    return (HANDLE)_beginthreadex(0,0,func,param,0,&threadId);
#endif
}

c_mt_bool_t c_thread_parallel(int threadCount,TThreadRunCallBackProc threadProc,void* workData,
                              int isUseThisThread,int threadIndexOffset){
    for (int i=0; i<threadCount; ++i) {
        if ((i==threadCount-1)&&(isUseThisThread)){
            threadProc(i+threadIndexOffset,workData);
        }else{
            _TThreadData* pt=(_TThreadData*)malloc(sizeof(_TThreadData));
            _check_malloc(pt);
            pt->threadIndex=i+threadIndexOffset;
            pt->threadProc=threadProc;
            pt->workData=workData;
            HANDLE rt=_thread_create(_win_threadProc,pt);
            if (rt==0){
                free(pt);
                _check_wt_init("_thread_create");
                return c_mt_bool_FALSE; //error
            } else {
                BOOL rtb=CloseHandle(rt);
                if (!rtb){
                    _check_wt("CloseHandle");
                    return c_mt_bool_FALSE; //error
                }
            }
        }
    }
    return c_mt_bool_TRUE;
}

#endif //_IS_USED_WIN32THREAD
