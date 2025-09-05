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
#include "stddef.h" //size_t
#include "assert.h" //assert
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
    extern void (*_parallel_import_c_on_error)(); //only for c only mode; set call back when MT error.

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

#ifndef _IS_USED_C_ATOMIC
# if defined(__STDC__) && (__STDC_VERSION__>=201112L) && (!defined(__STDC_NO_ATOMICS__))  //C11
#   include <stdatomic.h>
#   define _IS_USED_C_ATOMIC    1
    static inline void         c_atomic_uint_store(volatile unsigned int* p,unsigned int newv){ atomic_store((atomic_uint*)p,newv); }
    static inline unsigned int c_atomic_uint_load(volatile unsigned int* p){ return atomic_load((atomic_uint*)p); }
    //static inline c_mt_bool_t c_atomic_uint_compare_exchange_strong(volatile unsigned int* p,unsigned int expected,unsigned int newv){ return c_atomic_compare_exchange_strong((atomic_uint*)p,expected,newv); }
    static inline void   c_atomic_store(volatile size_t* p,size_t newv){ atomic_store((atomic_size_t*)p,newv); }
    static inline size_t c_atomic_load(volatile size_t* p){ return atomic_load((atomic_size_t*)p); }
    static inline size_t c_atomic_fetch_add(volatile size_t* p,size_t v){ return atomic_fetch_add((atomic_size_t*)p,v); }
  
# else
#  if (_IS_USED_WIN32THREAD)
#   include <intrin.h> //VC?
#   define _IS_USED_C_ATOMIC    1
    static inline void         c_atomic_uint_store(volatile unsigned int* p,unsigned int newv){ _InterlockedExchange((volatile long*)p,(long)newv); }
    static inline unsigned int c_atomic_uint_load(volatile unsigned int* p){ return (unsigned int)_InterlockedCompareExchange((volatile long*)p,0,0); }
    static inline void   c_atomic_store(volatile size_t* p,size_t newv){ _InterlockedExchangePointer((void*volatile*)p,(void*)newv); }
    static inline size_t c_atomic_load(volatile size_t* p) { return (size_t)_InterlockedCompareExchangePointer((void*volatile*)p,0,0); }
    static inline size_t c_atomic_fetch_add(volatile size_t* p, size_t v) { 
                            if (sizeof(size_t)==8) { return (size_t)_InterlockedExchangeAdd64((volatile __int64*)p,(__int64)v); }
                            else { assert(sizeof(size_t)==4); return (size_t)_InterlockedExchangeAdd((volatile long*)p,(long)v); } }

#  else
#   define _IS_USED_C_ATOMIC    0
#  endif
# endif
#endif //_IS_USED_C_ATOMIC

//C Channel API

    //Channel interaction data;
    typedef void* TChanData;
    //Channel;
    struct c_channel_t;
    size_t c_channel_t_memSize(size_t maxChanDataCount);

    struct c_channel_t* c_channel_new(void* pmem,size_t memSize,size_t maxChanDataCount);
    void                c_channel_delete(struct c_channel_t* self);
    c_mt_bool_t         c_channel_send(struct c_channel_t* self,TChanData data,c_mt_bool_t isWait); //can't send null
    TChanData           c_channel_accept(struct c_channel_t* self,c_mt_bool_t isWait); //return null when no data & closed
    void                c_channel_close(struct c_channel_t* self); //close channel, can't send after close


#endif //_IS_USED_MULTITHREAD
#ifdef __cplusplus
} // extern "C"
#endif
#endif //parallel_import_c_h
