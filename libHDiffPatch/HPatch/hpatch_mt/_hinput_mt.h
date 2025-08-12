//  _hinput_mt.h
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
#ifndef _hinput_mt_h
#define _hinput_mt_h
#include "_hpatch_mt.h"
#ifdef __cplusplus
extern "C" {
#endif
#if (_IS_USED_MULTITHREAD)

size_t               hinput_mt_t_memSize();

// create a new hpatch_TStreamInput* wrapper base_stream;
//   start a thread to read data from base_stream
hpatch_TStreamInput* hinput_mt_open(void* pmem,size_t memSize,struct hpatch_mt_t* h_mt,struct hpatch_TWorkBuf* freeBufList,
                                    const hpatch_TStreamInput* base_stream,hpatch_StreamPos_t curReadPos,hpatch_StreamPos_t endReadPos);

// same as hinput_mt_open, but auto decompress data when read from base_stream
hpatch_TStreamInput* hinput_dec_mt_open(void* pmem,size_t memSize,struct hpatch_mt_t* h_mt,struct hpatch_TWorkBuf* freeBufList,
                                        const hpatch_TStreamInput* base_stream,hpatch_StreamPos_t curReadPos,hpatch_StreamPos_t endReadPos,
                                        hpatch_TDecompress* decompressPlugin,hpatch_StreamPos_t uncompressedSize);

hpatch_BOOL          hinput_mt_close(hpatch_TStreamInput* hinput_mt_stream);

#endif //_IS_USED_MULTITHREAD
#ifdef __cplusplus
}
#endif
#endif //_hinput_mt_h
