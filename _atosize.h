//_atosize.h
//
/*
 This is the HDiffPatch copyright.

 Copyright (c) 2012-2017 HouSisong All Rights Reserved.

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
#ifndef HDiffPatch_atosize_h
#define HDiffPatch_atosize_h
#include "libHDiffPatch/HPatch/patch_types.h"

#ifdef __cplusplus
extern "C" {
#endif
#define _kSizeMax (~(size_t)0)

hpatch_inline static
hpatch_BOOL a_to_size(const char* pnum,size_t slen,size_t* out_size){
    const size_t _kSizeMaxDiv10=_kSizeMax/10;
    const size_t _kSizeMaxMod10=_kSizeMax-_kSizeMaxDiv10*10;
    size_t v=0;
    size_t s;
    if (slen==0) return hpatch_FALSE;
    if ((slen>=2)&(pnum[0]=='0')) return hpatch_FALSE;
    for (s=0; s<slen; ++s) {
        size_t c=pnum[s];
        if (('0'<=c)&(c<='9'))
            ;//empty ok
        else
            return hpatch_FALSE;
        c-='0';
        if (v<_kSizeMaxDiv10)
            ;//empty ok
        else if ((v>_kSizeMaxDiv10)|(c>_kSizeMaxMod10))
            return hpatch_FALSE;
        v=v*10+c;
    }
    *out_size=v;
    return hpatch_TRUE;
}

hpatch_inline static
hpatch_BOOL kmg_to_size(const char* pkmgnum,size_t slen,size_t* out_size){
    size_t shl;
    size_t v;
    if (slen==0) return hpatch_FALSE;
    switch (pkmgnum[slen-1]) {
        case 'k': case 'K': { shl=10; --slen;} break;
        case 'm': case 'M': { shl=20; --slen;} break;
        case 'g': case 'G': { shl=30; --slen;} break;
        default:            { shl= 0;        } break;
    }
    if (!a_to_size(pkmgnum,slen,&v)) return hpatch_FALSE;
    if (v>(_kSizeMax>>shl)) return hpatch_FALSE;
    *out_size=v<<shl;
    return hpatch_TRUE;
}

#ifdef __cplusplus
}
#endif
#endif //HDiffPatch_atosize_h
