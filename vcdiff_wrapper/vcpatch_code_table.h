// vcpatch_code_table.h
// HDiffPatch
/*
 The MIT License (MIT)
 Copyright (c) 2022 HouSisong
 
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
#ifndef hpatch_vcpatch_code_table_h
#define hpatch_vcpatch_code_table_h
#include "../libHDiffPatch/HPatch/patch_types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct{
    hpatch_byte     inst; //type
    hpatch_byte     size;
    //hpatch_byte     mode; !saved(add) to inst
} vcdiff_code_t;

typedef struct{
    vcdiff_code_t   code1;
    vcdiff_code_t   code2;
} vcdiff_code_pair_t;

#define vcdiff_code_table_t vcdiff_code_pair_t*

#define vcdiff_code_NOOP        0
#define vcdiff_code_ADD         1
#define vcdiff_code_RUN         2
#define vcdiff_code_COPY        3
#define vcdiff_code_COPY_SELF   (vcdiff_code_COPY+0)
#define vcdiff_code_COPY_HERE   (vcdiff_code_COPY+1)
#define vcdiff_code_COPY_NEAR0  (vcdiff_code_COPY+2)
#define vcdiff_code_COPY_NEAR1  (vcdiff_code_COPY+3)
#define vcdiff_code_COPY_NEAR2  (vcdiff_code_COPY+4)
#define vcdiff_code_COPY_NEAR3  (vcdiff_code_COPY+5)
#define vcdiff_s_near           (vcdiff_code_COPY_NEAR3-vcdiff_code_COPY_NEAR0+1)
#define vcdiff_code_COPY_SAME0  (vcdiff_code_COPY+6)
#define vcdiff_code_COPY_SAME1  (vcdiff_code_COPY+7)
#define vcdiff_code_COPY_SAME2  (vcdiff_code_COPY+8)
#define vcdiff_s_same           (vcdiff_code_COPY_SAME2-vcdiff_code_COPY_SAME0+1)
#define vcdiff_code_MAX         vcdiff_code_COPY_SAME2

#define VCD_SOURCE  (1<<0)
#define VCD_TARGET  (1<<1)
#define VCD_ADLER32 (1<<2)

const vcdiff_code_table_t get_vcdiff_code_table_default();

#define vcdiff_update_addr(same_array,near_array,near_index,addr) do{ \
    same_array[(hpatch_size_t)((addr)%(vcdiff_s_same*256))]=addr;     \
    near_array[(hpatch_size_t)(((*(near_index))++)%vcdiff_s_near)]=addr; } while(0)

#ifdef __cplusplus
}
#endif
#endif