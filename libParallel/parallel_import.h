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
#include "../libHDiffPatch/HPatch/patch_types.h" //for *inline
#if (_IS_USED_MULTITHREAD)
#include <stdexcept>

//cpp version of MT API
//  if have error, throw std::runtime_error

#define _check_fn_throw(value) { \
    if (!(value)) throw std::runtime_error(#value " run error!"); }
#define _check_fn_ret_throw(T,value) { T _v=value; if (!_v) throw std::runtime_error(#value " run error!"); return _v; }

hpatch_force_inline static
HLocker     locker_new(void)                { _check_fn_ret_throw(HLocker,c_locker_new()); }
hpatch_force_inline static 
void        locker_delete(HLocker locker)   { _check_fn_throw(c_locker_delete(locker)); }
hpatch_force_inline static
void        locker_enter(HLocker locker)    { _check_fn_throw(c_locker_enter(locker)); }
hpatch_force_inline static
void        locker_leave(HLocker locker)    { _check_fn_throw(c_locker_leave(locker)); }
hpatch_force_inline static
HCondvar    condvar_new(void)               { _check_fn_ret_throw(HCondvar,c_condvar_new()); }
hpatch_force_inline static
void        condvar_delete(HCondvar cond)   { _check_fn_throw(c_condvar_delete(cond)); }
hpatch_force_inline static
void        condvar_wait_at(HCondvar cond,HLocker locker) { _check_fn_throw(c_condvar_wait(cond,locker)); }
hpatch_force_inline static
void        condvar_signal(HCondvar cond)   { _check_fn_throw(c_condvar_signal(cond)); }
hpatch_force_inline static
void        condvar_broadcast(HCondvar cond){ _check_fn_throw(c_condvar_broadcast(cond)); }
#define     this_thread_yield               c_this_thread_yield
hpatch_force_inline static
void        thread_parallel(int threadCount,TThreadRunCallBackProc threadProc,void* workData,
                            int isUseThisThread,int threadIndexOffset){
                _check_fn_throw(c_thread_parallel(threadCount,threadProc,workData,isUseThisThread,threadIndexOffset)); }

#endif //_IS_USED_MULTITHREAD
#endif //parallel_import_h
