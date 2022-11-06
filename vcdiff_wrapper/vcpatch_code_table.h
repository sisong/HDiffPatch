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
    //hpatch_byte     mode; !saved to inst
} vcdiff_code_t;

typedef struct{
    vcdiff_code_t   code1;
    vcdiff_code_t   code2;
} vcdiff_code_pair_t;

#define vcdiff_code_table_t vcdiff_code_pair_t*

#define vcdiff_code_NOOP    0
#define vcdiff_code_ADD     1
#define vcdiff_code_RUN     2
#define vcdiff_code_COPY    3

const vcdiff_code_table_t get_vcdiff_code_table_default();

#ifdef __cplusplus
}
#endif
#endif