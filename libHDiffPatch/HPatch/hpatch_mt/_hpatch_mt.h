//  _hpatch_mt.h
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
#ifndef _hpatch_mt_h
#define _hpatch_mt_h
#include "../patch.h"
#include "../../../libParallel/parallel_import_c.h"
#ifdef __cplusplus
extern "C" {
#endif
#if (_IS_USED_MULTITHREAD)

struct hpatch_mt_t;
struct hpatch_TWorkBuf;

size_t                  hpatch_mt_t_memSize();

struct hpatch_mt_t*     hpatch_mt_open(void* pmem,size_t memSumSize,size_t threadNum,size_t workBufCount,size_t workBufNodeSize);
size_t                  hpatch_mt_workBufSize(const struct hpatch_mt_t* self);
struct hpatch_TWorkBuf* hpatch_mt_popFreeWorkBuf_fast(struct hpatch_mt_t* self,size_t needBufCount);//fast no locker & no wait
hpatch_BOOL             hpatch_mt_beforeThreadBegin(struct hpatch_mt_t* self); //a thread begin
void                    hpatch_mt_onThreadEnd(struct hpatch_mt_t* self); //a thread exit
hpatch_BOOL             hpatch_mt_registeCondvar(struct hpatch_mt_t* self,HCondvar waitCondvar); //when onError or onFinish, got broadcast
hpatch_BOOL             hpatch_mt_unregisteCondvar(struct hpatch_mt_t* self,HCondvar waitCondvar);
void                    hpatch_mt_setOnError(struct hpatch_mt_t* self); //set thread got a error
hpatch_BOOL             hpatch_mt_isOnError(struct hpatch_mt_t* self);
void                    hpatch_mt_waitAllThreadEnd(struct hpatch_mt_t* self,hpatch_BOOL isOnError);
hpatch_BOOL             hpatch_mt_isOnFinish(struct hpatch_mt_t* self);  //in wait all thread finish
hpatch_BOOL             hpatch_mt_close(struct hpatch_mt_t* self,hpatch_BOOL isOnError); //finish & wait all threads end & free self

#endif //_IS_USED_MULTITHREAD
#ifdef __cplusplus
}
#endif
#endif //_hpatch_mt_h
