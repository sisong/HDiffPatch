//  patch_private_mt.h
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
#ifndef _patch_private_mt_h
#define _patch_private_mt_h
#include "_hpatch_mt.h"
#include "../../../libParallel/parallel_import_c.h"
#ifdef __cplusplus
extern "C" {
#endif
#if (_IS_USED_MULTITHREAD)

#define _thread_obj_free(feee_fn,th_obj) { if (th_obj) { feee_fn(th_obj); th_obj=0; } }

typedef struct hpatch_TWorkBuf{
    struct hpatch_TWorkBuf* next;
    size_t                  data_size;
    //hpatch_byte           data[];
} hpatch_TWorkBuf;
hpatch_force_inline static
hpatch_byte* TWorkBuf_data(hpatch_TWorkBuf* self) { return ((hpatch_byte*)self)+sizeof(hpatch_TWorkBuf); }
hpatch_force_inline static
hpatch_byte* TWorkBuf_data_end(hpatch_TWorkBuf* self) { return TWorkBuf_data(self)+self->data_size; }

hpatch_force_inline static
void TWorkBuf_pushABufAtEnd(hpatch_TWorkBuf** pnode,hpatch_TWorkBuf* data){
    while (*pnode)
        pnode=&(*pnode)->next;
    data->next=0;
    *pnode=data;
}

hpatch_force_inline static
void TWorkBuf_pushABufAtHead(hpatch_TWorkBuf** pnode,hpatch_TWorkBuf* data){
    data->next=*pnode;
    *pnode=data;
}

hpatch_force_inline static
hpatch_TWorkBuf* TWorkBuf_popABuf(hpatch_TWorkBuf** pnode){
    hpatch_TWorkBuf* result=*pnode;
    *pnode=(result)?result->next:0;
    return result;
}

hpatch_force_inline static
hpatch_TWorkBuf* TWorkBuf_popAllBufs(hpatch_TWorkBuf** pnode){
    hpatch_TWorkBuf* result=*pnode;
    *pnode=0;
    return result;
}

#endif //_IS_USED_MULTITHREAD
#ifdef __cplusplus
}
#endif
#endif //_patch_private_mt_h
