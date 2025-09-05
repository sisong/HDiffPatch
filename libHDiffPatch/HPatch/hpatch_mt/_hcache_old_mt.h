//  _hcache_old_mt.h
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
#ifndef _hcache_old_mt_h
#define _hcache_old_mt_h
#include "_hpatch_mt.h"
#ifdef __cplusplus
extern "C" {
#endif
#if (_IS_USED_MULTITHREAD)

size_t               hcache_old_mt_t_memSize();

// create a new hpatch_TStreamInput* wrapper old_stream;
//   start a thread to read data from old_stream
//   isOnStepCoversInThread default true, same as onStepCovers call back in thread
hpatch_TStreamInput* hcache_old_mt_open(void* pmem,size_t memSize,struct hpatch_mt_t* h_mt,
                                        struct hpatch_TWorkBuf* freeBufList,hpatch_size_t workBufSize,
                                        const hpatch_TStreamInput* old_stream,sspatch_coversListener_t** out_coversListener,
                                        sspatch_coversListener_t* nextCoverlistener,hpatch_BOOL isOnStepCoversInThread);
hpatch_BOOL          hcache_old_mt_close(hpatch_TStreamInput* hcache_old_mt_stream);

#endif //_IS_USED_MULTITHREAD
#ifdef __cplusplus
}
#endif
#endif //_hcache_old_mt_h
