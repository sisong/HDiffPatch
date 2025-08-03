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
#if (_IS_USED_MULTITHREAD)
#include <stdexcept>

#define _check(value,func_name) { \
    if (!(value)) throw std::runtime_error(func_name "() run error!"); }

HLocker locker_new(void){
    HLocker self=c_locker_new();
    _check(self,"locker_new");
    return  self;
}
void locker_delete(HLocker locker){
    _check(c_locker_delete(locker),"locker_delete");
}

void locker_enter(HLocker locker){
    _check(c_locker_enter(locker),"locker_enter");
}
void locker_leave(HLocker locker){
    _check(c_locker_leave(locker),"locker_leave");
}

HCondvar condvar_new(void){
    HCondvar self=c_condvar_new();
    _check(self,"condvar_new");
    return self;
}
void    condvar_delete(HCondvar cond){
    _check(c_condvar_delete(cond),"condvar_delete");
}
void    condvar_wait(HCondvar cond,TLockerBox* lockerBox){
    _check(c_condvar_wait(cond,lockerBox),"condvar_wait");
}
void    condvar_signal(HCondvar cond){
    _check(c_condvar_signal(cond),"condvar_signal");
}
void    condvar_broadcast(HCondvar cond){
    _check(c_condvar_broadcast(cond),"condvar_broadcast");
}

void thread_parallel(int threadCount,TThreadRunCallBackProc threadProc,void* workData,
                     int isUseThisThread,int threadIndexOffset){
    _check(c_thread_parallel(threadCount,threadProc,workData,isUseThisThread,threadIndexOffset),"thread_parallel");
}

#endif //_IS_USED_MULTITHREAD
