//  parallel_import_c.h
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

#ifndef parallel_import_c_h
#define parallel_import_c_h
#ifdef __cplusplus
extern "C" {
#endif

#ifndef _IS_USED_MULTITHREAD
#   define _IS_USED_MULTITHREAD 1
#endif

#if ((_IS_USED_PTHREAD>0) || (_IS_USED_WIN32THREAD>0))
#   //ok have one
#   define _IS_USED_MULTITHREAD 1
#else
#   if (_IS_USED_MULTITHREAD>0)
#       if ( (!(defined _IS_USED_WIN32THREAD)) && ((defined _WIN32)||(defined WIN32)) )
#           define  _IS_USED_WIN32THREAD        1
#       else
#           if (!(defined _IS_USED_PTHREAD))
#               define _IS_USED_PTHREAD         1
#           endif
#       endif
#   endif
#endif

#if (_IS_USED_MULTITHREAD)

//c version of MT API

    typedef int c_mt_bool_t;
    #define c_mt_NULL           0
    #define c_mt_bool_FALSE     0
    #define c_mt_bool_TRUE      (!c_mt_bool_FALSE)
    extern int _parallel_import_c_exit_on_error; //only for c only mode; set when error, exit program.

    //parallel critical section lock;
    typedef void*   HLocker;
    HLocker     c_locker_new(void);
    c_mt_bool_t c_locker_delete(HLocker locker);
    c_mt_bool_t c_locker_enter(HLocker locker);
    c_mt_bool_t c_locker_leave(HLocker locker);
    
    //synchronization variable;
    typedef void*   HCondvar;
    HCondvar    c_condvar_new(void);
    c_mt_bool_t c_condvar_delete(HCondvar cond);
    c_mt_bool_t c_condvar_wait(HCondvar cond,HLocker locker);
    c_mt_bool_t c_condvar_signal(HCondvar cond);
    c_mt_bool_t c_condvar_broadcast(HCondvar cond);

    void c_this_thread_yield(void);

    typedef void (*TThreadRunCallBackProc)(int threadIndex,void* workData);
    //parallel run
    c_mt_bool_t c_thread_parallel(int threadCount,TThreadRunCallBackProc threadProc,void* workData,
                                  int isUseThisThread,int threadIndexOffset);

#endif //_IS_USED_MULTITHREAD
#ifdef __cplusplus
} // extern "C"
#endif
#endif //parallel_import_c_h
