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

#define _kU64Max (~(hpatch_uint64_t)0)
hpatch_inline static
hpatch_BOOL a_to_u64(const char* pnum,size_t slen,hpatch_uint64_t* out_size){
    const hpatch_uint64_t _kSizeMaxDiv10=_kU64Max/10;
    const hpatch_uint64_t _kSizeMaxMod10=_kU64Max-_kSizeMaxDiv10*10;
    hpatch_uint64_t v=0;
    size_t s;
    if (slen==0) return hpatch_FALSE;
    if ((slen>=2)&(pnum[0]=='0')) return hpatch_FALSE;
    for (s=0; s<slen; ++s) {
        hpatch_uint64_t c=pnum[s];
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
hpatch_BOOL kmg_to_u64(const char* pkmgnum,size_t slen,hpatch_uint64_t* out_size){
    size_t shl;
    hpatch_uint64_t v;
    if (slen==0) return hpatch_FALSE;
    switch (pkmgnum[slen-1]) {
        case 'k': case 'K': { shl=10; --slen;} break;
        case 'm': case 'M': { shl=20; --slen;} break;
        case 'g': case 'G': { shl=30; --slen;} break;
        default:            { shl= 0;        } break;
    }
    if (!a_to_u64(pkmgnum,slen,&v)) return hpatch_FALSE;
    if (v>(_kU64Max>>shl)) return hpatch_FALSE;
    *out_size=v<<shl;
    return hpatch_TRUE;
}


hpatch_inline static
hpatch_BOOL a_to_size(const char* pnum,size_t slen,size_t* out_size){
    hpatch_uint64_t v;
    if (!a_to_u64(pnum,slen,&v)) return hpatch_FALSE;
    if ((sizeof(size_t)!=sizeof(hpatch_uint64_t)) && (v!=(size_t)v)) return hpatch_FALSE;
    *out_size=(size_t)v;
    return hpatch_TRUE;
}

hpatch_inline static
hpatch_BOOL kmg_to_size(const char* pkmg_num,size_t slen,size_t* out_size){
    hpatch_uint64_t v;
    if (!kmg_to_u64(pkmg_num,slen,&v)) return hpatch_FALSE;
    if ((sizeof(size_t)!=sizeof(hpatch_uint64_t)) && (v!=(size_t)v)) return hpatch_FALSE;
    *out_size=(size_t)v;
    return hpatch_TRUE;
}

#define          _u64_to_a_kMaxLen   21
static const char*  _i09_to_one_a_="0123456789";

hpatch_inline static
char* u64_to_enough_a(hpatch_uint64_t size,char temp_buf_enough[_u64_to_a_kMaxLen]){
    char* out_a=temp_buf_enough+_u64_to_a_kMaxLen;
    *(--out_a)='\0';
    do{
        hpatch_uint64_t nsize=size/10;
        *(--out_a)=_i09_to_one_a_[size-nsize*10];
        size=nsize;
    }while(size);
    return out_a;
}

hpatch_inline static
hpatch_BOOL u64_to_a(hpatch_uint64_t size,char* out_a,size_t a_buflen){
    char enough_a[_u64_to_a_kMaxLen];
    const char* cur_a=u64_to_enough_a(size,enough_a);
    const size_t datalen=_u64_to_a_kMaxLen-(cur_a-enough_a);
    if (a_buflen<datalen) return hpatch_FALSE;
    memcpy(out_a,cur_a,datalen);
    return hpatch_TRUE;
}

#ifdef __cplusplus
}
#endif
#endif //HDiffPatch_atosize_h
