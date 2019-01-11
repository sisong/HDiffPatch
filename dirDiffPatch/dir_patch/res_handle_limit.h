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
#include "../../libHDiffPatch/HPatch/patch_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct IResHandle{
    void*               resImport;
    hpatch_StreamPos_t  resStreamSize;
    hpatch_BOOL  (*open)(struct IResHandle* self,hpatch_TStreamInput** out_stream);
    hpatch_BOOL (*close)(struct IResHandle* self,const hpatch_TStreamInput* stream);
} IResHandle;
    
struct TResHandleLimit;
typedef struct _TResHandleBox{
    hpatch_TStreamInput     box;
    struct TResHandleLimit* owner;
    hpatch_StreamPos_t      hit;
} _TResHandleBox;

//内部控制同时最大打开的句柄数,对外模拟成所有资源都能使用的完整流列表;
typedef struct TResHandleLimit{
    const hpatch_TStreamInput** streamList;
    size_t                      streamCount;
//private:
    _TResHandleBox*             _ex_streamList;
    hpatch_TStreamInput**       _in_streamList;
    IResHandle*                 _resList;
    hpatch_StreamPos_t          _curHit;
    size_t                      _curOpenCount;
    size_t                      _limitMaxOpenCount;
    //mem
    unsigned char*              _buf;
} TResHandleLimit;

hpatch_inline static
void        TResHandleLimit_init(TResHandleLimit* self) { memset(self,0,sizeof(*self)); }
hpatch_BOOL TResHandleLimit_open(TResHandleLimit* self,size_t limitMaxOpenCount,
                                 IResHandle* resList,size_t resCount);
hpatch_BOOL TResHandleLimit_close(TResHandleLimit* self);

#ifdef __cplusplus
}
#endif
#endif //DirPatch_res_handle_limit_h
