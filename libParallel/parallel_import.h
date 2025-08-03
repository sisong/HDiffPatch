//  parallel_import.h
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

#ifndef parallel_import_h
#define parallel_import_h
#include "parallel_import_c.h"
#if (_IS_USED_MULTITHREAD)

//cpp version of MT API
//  if have error, throw std::runtime_error

    HLocker     locker_new(void);
    void        locker_delete(HLocker locker);
    void        locker_enter(HLocker locker);
    void        locker_leave(HLocker locker);
    
    HCondvar    condvar_new(void);
    void        condvar_delete(HCondvar cond);
    void        condvar_wait(HCondvar cond,TLockerBox* lockerBox);
    void        condvar_signal(HCondvar cond);
    void        condvar_broadcast(HCondvar cond);
    
    #define this_thread_yield c_this_thread_yield

    void  thread_parallel(int threadCount,TThreadRunCallBackProc threadProc,void* workData,
                          int isUseThisThread,int threadIndexOffset);

#endif //_IS_USED_MULTITHREAD
#endif //parallel_import_h
