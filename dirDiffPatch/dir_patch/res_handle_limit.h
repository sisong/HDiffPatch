//  res_handle_limit.h
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
#ifndef DirPatch_res_handle_limit_h
#define DirPatch_res_handle_limit_h
#include "dir_patch_types.h"
#if (_IS_NEED_DIR_DIFF_PATCH)
#ifdef __cplusplus
extern "C" {
#endif

typedef struct hpatch_IResHandle{
    void*               resImport;
    hpatch_StreamPos_t  resStreamSize;
    hpatch_BOOL  (*open)(struct hpatch_IResHandle* self,hpatch_TStreamInput** out_stream);
    hpatch_BOOL (*close)(struct hpatch_IResHandle* self,const hpatch_TStreamInput* stream);
} hpatch_IResHandle;
    
struct hpatch_TResHandleLimit;
typedef struct _hpatch_TResHandleBox{
    hpatch_TStreamInput     box;
    struct hpatch_TResHandleLimit* owner;
    hpatch_StreamPos_t      hit;
} _hpatch_TResHandleBox;

//内部控制同时最大打开的句柄数,对外模拟成所有资源都能使用的完整流列表;
typedef struct hpatch_TResHandleLimit{
    const hpatch_TStreamInput** streamList;
    size_t                      streamCount;
//private:
    _hpatch_TResHandleBox*      _ex_streamList;
    hpatch_TStreamInput**       _in_streamList;
    hpatch_IResHandle*          _resList;
    hpatch_StreamPos_t          _curHit;
    size_t                      _curOpenCount;
    size_t                      _limitMaxOpenCount;
    //mem
    unsigned char*              _buf;
} hpatch_TResHandleLimit;

hpatch_inline static
void        hpatch_TResHandleLimit_init(hpatch_TResHandleLimit* self) { memset(self,0,sizeof(*self)); }
hpatch_BOOL hpatch_TResHandleLimit_open(hpatch_TResHandleLimit* self,size_t limitMaxOpenCount,
                                        hpatch_IResHandle* resList,size_t resCount);
hpatch_BOOL hpatch_TResHandleLimit_closeFileHandles(hpatch_TResHandleLimit* self);
hpatch_BOOL hpatch_TResHandleLimit_close(hpatch_TResHandleLimit* self);
    
#ifdef __cplusplus
}
#endif
#endif
#endif //DirPatch_res_handle_limit_h
