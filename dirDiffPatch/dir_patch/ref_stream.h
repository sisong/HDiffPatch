//  ref_stream.h
//  dir patch
/*
 The MIT License (MIT)
 Copyright (c) 2018-2019 HouSisong
 
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
#ifndef DirPatch_ref_stream_h
#define DirPatch_ref_stream_h
#include "dir_patch_types.h"
#if (_IS_NEED_DIR_DIFF_PATCH)
#ifdef __cplusplus
extern "C" {
#endif
    
//利用refList模拟成一个输入流;
typedef struct hpatch_TRefStream{
    const hpatch_TStreamInput*  stream;
//private:
    hpatch_TStreamInput         _stream;
    
    const hpatch_TStreamInput** _refList;
    hpatch_StreamPos_t*         _rangeEndList;
    size_t                      _rangeCount;//==refCount
    size_t                      _curRangeIndex;
    //mem
    unsigned char*              _buf;
} hpatch_TRefStream;

hpatch_inline static
void        hpatch_TRefStream_init(hpatch_TRefStream* self) { memset(self,0,sizeof(*self)); }
hpatch_BOOL hpatch_TRefStream_open(hpatch_TRefStream* self,const hpatch_TStreamInput** refList,size_t refCount);
void        hpatch_TRefStream_close(hpatch_TRefStream* self);

#ifdef __cplusplus
}
#endif
#endif
#endif //DirPatch_ref_stream_h
