// vcpatch_wrapper.c
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
#include "vcpatch_wrapper.h"
#include "../libHDiffPatch/HPatch/patch_types.h"
#include "../libHDiffPatch/HPatch/patch_private.h"
#include "../libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.h"
#include "vcpatch_code_table.h"
#include <string.h>
#define _hpatch_FALSE   hpatch_FALSE
//hpatch_uint __vcpatch_debug_check_false_x=0; //for debug
//#define _hpatch_FALSE (1/__vcpatch_debug_check_false_x)

#ifndef _IS_RUN_MEM_SAFE_CHECK
#   define _IS_RUN_MEM_SAFE_CHECK  1
#endif

#if (_IS_RUN_MEM_SAFE_CHECK)
//__RUN_MEM_SAFE_CHECK用来启动内存访问越界检查,用以防御可能被意外或故意损坏的数据.
#   define __RUN_MEM_SAFE_CHECK
#endif

static const hpatch_byte kVcDiffType[3]={('V'|(1<<7)),('C'|(1<<7)),('D'|(1<<7))};
#define     kVcDiffDefaultVersion   0
#define     kVcDiffGoogleVersion    'S'
#define     kVcDiffMinHeadLen       (sizeof(kVcDiffType)+1+1)

static const char* kHDiffzAppHead="$hdiffz-VCDIFF&";
static const char* kHDiffzAppHead_version="#a";
#define kHDiffzAppHead_len          15 // = strlen(kHDiffzAppHead);
#define kHDiffzAppHead_version_len  2  // = strlen(kHDiffzAppHead_version);
#define kHDiffzAppHead_maxLen       (kHDiffzAppHead_len+kHDiffzAppHead_version_len+hpatch_kMaxPluginTypeLength)

#define _clip_unpackUInt64(_clip,_result) { \
    if (!_TStreamCacheClip_unpackUIntWithTag(_clip,_result,0)) \
        return _hpatch_FALSE; \
}
#define _clip_readUInt8(_clip,_result) { \
    const hpatch_byte* buf=_TStreamCacheClip_readData(_clip,1); \
    if (buf!=0) *(_result)=*buf; \
    else return _hpatch_FALSE; \
}
#define _clip_readUInt32_BigEndian(_clip,_result) { \
    const hpatch_byte* buf=_TStreamCacheClip_readData(_clip,4); \
    if (buf!=0) *(_result)=(((hpatch_uint32_t)buf[0])<<24)|(((hpatch_uint32_t)buf[1])<<16) \
                           |(((hpatch_uint32_t)buf[2])<<8)|((hpatch_uint32_t)buf[3]); \
    else return _hpatch_FALSE; \
}


#define N   vcdiff_code_NOOP
#define A   vcdiff_code_ADD
#define R   vcdiff_code_RUN
#define C   vcdiff_code_COPY
#define C0  (C+0)
#define C1  (C+1)
#define C2  (C+2)
#define C3  (C+3)
#define C4  (C+4)
#define C5  (C+5)
#define C6  (C+6)
#define C7  (C+7)
#define C8  (C+8)

/*
        TYPE      SIZE     MODE    TYPE     SIZE     MODE     INDEX
       ---------------------------------------------------------------
    1.  RUN         0        0     NOOP       0        0        0
    2.  ADD    0, [1,17]     0     NOOP       0        0      [1,18]
    3.  COPY   0, [4,18]     0     NOOP       0        0     [19,34]
    4.  COPY   0, [4,18]     1     NOOP       0        0     [35,50]
    5.  COPY   0, [4,18]     2     NOOP       0        0     [51,66]
    6.  COPY   0, [4,18]     3     NOOP       0        0     [67,82]
    7.  COPY   0, [4,18]     4     NOOP       0        0     [83,98]
    8.  COPY   0, [4,18]     5     NOOP       0        0     [99,114]
    9.  COPY   0, [4,18]     6     NOOP       0        0    [115,130]
   10.  COPY   0, [4,18]     7     NOOP       0        0    [131,146]
   11.  COPY   0, [4,18]     8     NOOP       0        0    [147,162]
   12.  ADD       [1,4]      0     COPY     [4,6]      0    [163,174]
   13.  ADD       [1,4]      0     COPY     [4,6]      1    [175,186]
   14.  ADD       [1,4]      0     COPY     [4,6]      2    [187,198]
   15.  ADD       [1,4]      0     COPY     [4,6]      3    [199,210]
   16.  ADD       [1,4]      0     COPY     [4,6]      4    [211,222]
   17.  ADD       [1,4]      0     COPY     [4,6]      5    [223,234]
   18.  ADD       [1,4]      0     COPY       4        6    [235,238]
   19.  ADD       [1,4]      0     COPY       4        7    [239,242]
   20.  ADD       [1,4]      0     COPY       4        8    [243,246]
   21.  COPY        4      [0,8]   ADD        1        0    [247,255]
       ---------------------------------------------------------------
*/

/* //gen _vcdiff_code_table_default
static hpatch_byte* __incset(hpatch_byte* cur,hpatch_byte blockLen,hpatch_byte v,hpatch_byte runCount){
    while (runCount--){
        hpatch_byte runLen=blockLen;
        while (runLen--){
            *cur=v;
            cur+=sizeof(vcdiff_code_pair_t);
        }
        v++;
    }
    return cur;
}

static hpatch_inline hpatch_byte* _inc(hpatch_byte* cur,hpatch_byte blockLen,hpatch_byte v,hpatch_byte indexs){
    hpatch_byte runCount=(indexs+1)/blockLen;
    assert(blockLen*runCount==(indexs+1));
    return __incset(cur,blockLen,v,runCount);
}

static hpatch_inline hpatch_byte* _set(hpatch_byte* cur,hpatch_byte v,hpatch_byte indexs){
                                            return __incset(cur,(indexs+1),v,1); }

static hpatch_byte* _cpy(hpatch_byte* cur, hpatch_byte blockLen, hpatch_byte indexs) {
    hpatch_byte runCount=(indexs+1)/blockLen;
    assert(blockLen*runCount==(indexs+1));
    const hpatch_size_t offset=(hpatch_size_t)blockLen*sizeof(vcdiff_code_pair_t);
    while (runCount--){
        hpatch_byte runLen=blockLen;
        while (runLen--){
            *cur=cur[-offset];
            cur+=sizeof(vcdiff_code_pair_t);
        }
    }
    return cur;
}

static int _set_vcdiff_code_table_default(vcdiff_code_table_t code_table){
    hpatch_byte* cur0,* cur;

    cur0=&code_table[0].code1.inst;
    cur=cur0;
    cur=_set(cur,R,0);
    cur=_set(cur,A,18-1);
    cur=_inc(cur,16,C,162-19);
    cur=_set(cur,A,246-163);
    cur=_inc(cur,1,C,255-247);
    assert((cur-cur0)==256*sizeof(vcdiff_code_pair_t));

    cur0=&code_table[0].code2.inst;
    cur=cur0;
    cur=_set(cur,N,162-0);
    cur=_inc(cur,12,C,234-163);
    cur=_inc(cur,4,C+6,246-235);
    cur=_set(cur,A,255-247);
    assert((cur-cur0)==256*sizeof(vcdiff_code_pair_t));

    cur0=&code_table[0].code1.size;
    cur=cur0;
    cur=_inc(cur,1,0,0);
    cur=_inc(cur,1,0,18-1);
    cur=_inc(cur,1,0,19-19);
    cur=_inc(cur,1,4,34-20);
    cur=_cpy(cur,16,162-35);
    cur=_inc(cur,3,1,174-163);
    cur=_cpy(cur,12,234-175);
    cur=_inc(cur,1,1,238-235);
    cur=_cpy(cur,4,246-239);
    cur=_set(cur,4,255-247);
    assert((cur-cur0)==256*sizeof(vcdiff_code_pair_t));

    cur0=&code_table[0].code2.size;
    cur=cur0;
    cur=_set(cur,0,162-0);
    cur=_inc(cur,1,4,165-163);
    cur=_cpy(cur,3,234-166);
    cur=_set(cur,4,246-235);
    cur=_set(cur,1,255-247);
    assert((cur-cur0)==256*sizeof(vcdiff_code_pair_t));

    return 0;
}

vcdiff_code_pair_t g_code_table[256];
int _print_vcdiff_code_table_default(){
    _set_vcdiff_code_table_default(g_code_table);
    const char* inst2s[4+8]={" N"," A"," R","C0","C1","C2","C3","C4","C5","C6","C7","C8"};
    for (int i=0;i<256;++i){
        vcdiff_code_pair_t c=g_code_table[i];
        printf("{{%s,%2d},{%s,%2d}},",inst2s[c.code1.inst],c.code1.size,inst2s[c.code2.inst],c.code2.size);
        if (((i+1)&7)==0) printf("\n");
    }
    return 0;
}
*/

//_vcdiff_code_table_default gen by _print_vcdiff_code_table_default()
static const vcdiff_code_pair_t _vcdiff_code_table_default[256]={
    {{ R, 0},{ N, 0}},{{ A, 0},{ N, 0}},{{ A, 1},{ N, 0}},{{ A, 2},{ N, 0}},{{ A, 3},{ N, 0}},{{ A, 4},{ N, 0}},{{ A, 5},{ N, 0}},{{ A, 6},{ N, 0}},
    {{ A, 7},{ N, 0}},{{ A, 8},{ N, 0}},{{ A, 9},{ N, 0}},{{ A,10},{ N, 0}},{{ A,11},{ N, 0}},{{ A,12},{ N, 0}},{{ A,13},{ N, 0}},{{ A,14},{ N, 0}},
    {{ A,15},{ N, 0}},{{ A,16},{ N, 0}},{{ A,17},{ N, 0}},{{C0, 0},{ N, 0}},{{C0, 4},{ N, 0}},{{C0, 5},{ N, 0}},{{C0, 6},{ N, 0}},{{C0, 7},{ N, 0}},
    {{C0, 8},{ N, 0}},{{C0, 9},{ N, 0}},{{C0,10},{ N, 0}},{{C0,11},{ N, 0}},{{C0,12},{ N, 0}},{{C0,13},{ N, 0}},{{C0,14},{ N, 0}},{{C0,15},{ N, 0}},
    {{C0,16},{ N, 0}},{{C0,17},{ N, 0}},{{C0,18},{ N, 0}},{{C1, 0},{ N, 0}},{{C1, 4},{ N, 0}},{{C1, 5},{ N, 0}},{{C1, 6},{ N, 0}},{{C1, 7},{ N, 0}},
    {{C1, 8},{ N, 0}},{{C1, 9},{ N, 0}},{{C1,10},{ N, 0}},{{C1,11},{ N, 0}},{{C1,12},{ N, 0}},{{C1,13},{ N, 0}},{{C1,14},{ N, 0}},{{C1,15},{ N, 0}},
    {{C1,16},{ N, 0}},{{C1,17},{ N, 0}},{{C1,18},{ N, 0}},{{C2, 0},{ N, 0}},{{C2, 4},{ N, 0}},{{C2, 5},{ N, 0}},{{C2, 6},{ N, 0}},{{C2, 7},{ N, 0}},
    {{C2, 8},{ N, 0}},{{C2, 9},{ N, 0}},{{C2,10},{ N, 0}},{{C2,11},{ N, 0}},{{C2,12},{ N, 0}},{{C2,13},{ N, 0}},{{C2,14},{ N, 0}},{{C2,15},{ N, 0}},
    {{C2,16},{ N, 0}},{{C2,17},{ N, 0}},{{C2,18},{ N, 0}},{{C3, 0},{ N, 0}},{{C3, 4},{ N, 0}},{{C3, 5},{ N, 0}},{{C3, 6},{ N, 0}},{{C3, 7},{ N, 0}},
    {{C3, 8},{ N, 0}},{{C3, 9},{ N, 0}},{{C3,10},{ N, 0}},{{C3,11},{ N, 0}},{{C3,12},{ N, 0}},{{C3,13},{ N, 0}},{{C3,14},{ N, 0}},{{C3,15},{ N, 0}},
    {{C3,16},{ N, 0}},{{C3,17},{ N, 0}},{{C3,18},{ N, 0}},{{C4, 0},{ N, 0}},{{C4, 4},{ N, 0}},{{C4, 5},{ N, 0}},{{C4, 6},{ N, 0}},{{C4, 7},{ N, 0}},
    {{C4, 8},{ N, 0}},{{C4, 9},{ N, 0}},{{C4,10},{ N, 0}},{{C4,11},{ N, 0}},{{C4,12},{ N, 0}},{{C4,13},{ N, 0}},{{C4,14},{ N, 0}},{{C4,15},{ N, 0}},
    {{C4,16},{ N, 0}},{{C4,17},{ N, 0}},{{C4,18},{ N, 0}},{{C5, 0},{ N, 0}},{{C5, 4},{ N, 0}},{{C5, 5},{ N, 0}},{{C5, 6},{ N, 0}},{{C5, 7},{ N, 0}},
    {{C5, 8},{ N, 0}},{{C5, 9},{ N, 0}},{{C5,10},{ N, 0}},{{C5,11},{ N, 0}},{{C5,12},{ N, 0}},{{C5,13},{ N, 0}},{{C5,14},{ N, 0}},{{C5,15},{ N, 0}},
    {{C5,16},{ N, 0}},{{C5,17},{ N, 0}},{{C5,18},{ N, 0}},{{C6, 0},{ N, 0}},{{C6, 4},{ N, 0}},{{C6, 5},{ N, 0}},{{C6, 6},{ N, 0}},{{C6, 7},{ N, 0}},
    {{C6, 8},{ N, 0}},{{C6, 9},{ N, 0}},{{C6,10},{ N, 0}},{{C6,11},{ N, 0}},{{C6,12},{ N, 0}},{{C6,13},{ N, 0}},{{C6,14},{ N, 0}},{{C6,15},{ N, 0}},
    {{C6,16},{ N, 0}},{{C6,17},{ N, 0}},{{C6,18},{ N, 0}},{{C7, 0},{ N, 0}},{{C7, 4},{ N, 0}},{{C7, 5},{ N, 0}},{{C7, 6},{ N, 0}},{{C7, 7},{ N, 0}},
    {{C7, 8},{ N, 0}},{{C7, 9},{ N, 0}},{{C7,10},{ N, 0}},{{C7,11},{ N, 0}},{{C7,12},{ N, 0}},{{C7,13},{ N, 0}},{{C7,14},{ N, 0}},{{C7,15},{ N, 0}},
    {{C7,16},{ N, 0}},{{C7,17},{ N, 0}},{{C7,18},{ N, 0}},{{C8, 0},{ N, 0}},{{C8, 4},{ N, 0}},{{C8, 5},{ N, 0}},{{C8, 6},{ N, 0}},{{C8, 7},{ N, 0}},
    {{C8, 8},{ N, 0}},{{C8, 9},{ N, 0}},{{C8,10},{ N, 0}},{{C8,11},{ N, 0}},{{C8,12},{ N, 0}},{{C8,13},{ N, 0}},{{C8,14},{ N, 0}},{{C8,15},{ N, 0}},
    {{C8,16},{ N, 0}},{{C8,17},{ N, 0}},{{C8,18},{ N, 0}},{{ A, 1},{C0, 4}},{{ A, 1},{C0, 5}},{{ A, 1},{C0, 6}},{{ A, 2},{C0, 4}},{{ A, 2},{C0, 5}},
    {{ A, 2},{C0, 6}},{{ A, 3},{C0, 4}},{{ A, 3},{C0, 5}},{{ A, 3},{C0, 6}},{{ A, 4},{C0, 4}},{{ A, 4},{C0, 5}},{{ A, 4},{C0, 6}},{{ A, 1},{C1, 4}},
    {{ A, 1},{C1, 5}},{{ A, 1},{C1, 6}},{{ A, 2},{C1, 4}},{{ A, 2},{C1, 5}},{{ A, 2},{C1, 6}},{{ A, 3},{C1, 4}},{{ A, 3},{C1, 5}},{{ A, 3},{C1, 6}},
    {{ A, 4},{C1, 4}},{{ A, 4},{C1, 5}},{{ A, 4},{C1, 6}},{{ A, 1},{C2, 4}},{{ A, 1},{C2, 5}},{{ A, 1},{C2, 6}},{{ A, 2},{C2, 4}},{{ A, 2},{C2, 5}},
    {{ A, 2},{C2, 6}},{{ A, 3},{C2, 4}},{{ A, 3},{C2, 5}},{{ A, 3},{C2, 6}},{{ A, 4},{C2, 4}},{{ A, 4},{C2, 5}},{{ A, 4},{C2, 6}},{{ A, 1},{C3, 4}},
    {{ A, 1},{C3, 5}},{{ A, 1},{C3, 6}},{{ A, 2},{C3, 4}},{{ A, 2},{C3, 5}},{{ A, 2},{C3, 6}},{{ A, 3},{C3, 4}},{{ A, 3},{C3, 5}},{{ A, 3},{C3, 6}},
    {{ A, 4},{C3, 4}},{{ A, 4},{C3, 5}},{{ A, 4},{C3, 6}},{{ A, 1},{C4, 4}},{{ A, 1},{C4, 5}},{{ A, 1},{C4, 6}},{{ A, 2},{C4, 4}},{{ A, 2},{C4, 5}},
    {{ A, 2},{C4, 6}},{{ A, 3},{C4, 4}},{{ A, 3},{C4, 5}},{{ A, 3},{C4, 6}},{{ A, 4},{C4, 4}},{{ A, 4},{C4, 5}},{{ A, 4},{C4, 6}},{{ A, 1},{C5, 4}},
    {{ A, 1},{C5, 5}},{{ A, 1},{C5, 6}},{{ A, 2},{C5, 4}},{{ A, 2},{C5, 5}},{{ A, 2},{C5, 6}},{{ A, 3},{C5, 4}},{{ A, 3},{C5, 5}},{{ A, 3},{C5, 6}},
    {{ A, 4},{C5, 4}},{{ A, 4},{C5, 5}},{{ A, 4},{C5, 6}},{{ A, 1},{C6, 4}},{{ A, 2},{C6, 4}},{{ A, 3},{C6, 4}},{{ A, 4},{C6, 4}},{{ A, 1},{C7, 4}},
    {{ A, 2},{C7, 4}},{{ A, 3},{C7, 4}},{{ A, 4},{C7, 4}},{{ A, 1},{C8, 4}},{{ A, 2},{C8, 4}},{{ A, 3},{C8, 4}},{{ A, 4},{C8, 4}},{{C0, 4},{ A, 1}},
    {{C1, 4},{ A, 1}},{{C2, 4},{ A, 1}},{{C3, 4},{ A, 1}},{{C4, 4},{ A, 1}},{{C5, 4},{ A, 1}},{{C6, 4},{ A, 1}},{{C7, 4},{ A, 1}},{{C8, 4},{ A, 1}}
};

const vcdiff_code_table_t get_vcdiff_code_table_default(){
    return &_vcdiff_code_table_default[0];
}

#undef N
#undef A
#undef R
#undef C
#undef C0
#undef C1
#undef C2
#undef C3
#undef C4
#undef C5
#undef C6
#undef C7
#undef C8

#define _kWindowCacheSize 1024
static hpatch_BOOL _getVcDiffWindowSizes(hpatch_VcDiffInfo* out_diffinfo,const hpatch_TStreamInput* diffStream);

hpatch_BOOL getVcDiffInfo(hpatch_VcDiffInfo* out_diffinfo,const hpatch_TStreamInput* diffStream,
                          hpatch_BOOL isNeedWindowSize){
    TStreamCacheClip  diffClip;
    hpatch_byte _window_cache[_kWindowCacheSize];
    hpatch_byte Hdr_Indicator;
    memset(out_diffinfo,0,sizeof(*out_diffinfo));
    if (diffStream->streamSize<kVcDiffMinHeadLen)
        return _hpatch_FALSE;
    assert(kVcDiffMinHeadLen<=_kWindowCacheSize);
    _TStreamCacheClip_init(&diffClip,diffStream,0,diffStream->streamSize,_window_cache,sizeof(_window_cache));

    {//head
        const hpatch_byte* buf=_TStreamCacheClip_readData(&diffClip,kVcDiffMinHeadLen);
        if (buf==0)
            return _hpatch_FALSE;
        if (0!=memcmp(buf,kVcDiffType,sizeof(kVcDiffType)))
            return _hpatch_FALSE; //not VCDIFF
        buf+=sizeof(kVcDiffType);
        switch (*buf++){ // version
            case kVcDiffDefaultVersion: out_diffinfo->isGoogleVersion=hpatch_FALSE; break;
            case kVcDiffGoogleVersion: out_diffinfo->isGoogleVersion=hpatch_TRUE; break;
            default:
                return _hpatch_FALSE; //unsupport version
        }
        Hdr_Indicator=*buf++; // Hdr_Indicator
    }

    if (Hdr_Indicator&(1<<0)){ // VCD_DECOMPRESS
        _clip_readUInt8(&diffClip,&out_diffinfo->compressorID);
    }
    if (Hdr_Indicator&(1<<1)) // VCD_CODETABLE
        return _hpatch_FALSE; // unsupport code table
    if (Hdr_Indicator&(1<<2)) // VCD_APPHEADER
        _clip_unpackUInt64(&diffClip,&out_diffinfo->appHeadDataLen);
    out_diffinfo->appHeadDataOffset=_TStreamCacheClip_readPosOfSrcStream(&diffClip);
    out_diffinfo->windowOffset=out_diffinfo->appHeadDataOffset+out_diffinfo->appHeadDataLen;
#ifdef __RUN_MEM_SAFE_CHECK
    {
        const hpatch_StreamPos_t maxPos=_TStreamCacheClip_leaveSize(&diffClip);
        if ( (out_diffinfo->appHeadDataLen>maxPos)|(out_diffinfo->windowOffset>maxPos) )
            return _hpatch_FALSE; //data size error
    }
#endif
    assert(kHDiffzAppHead_len==strlen(kHDiffzAppHead));
    assert(kHDiffzAppHead_version_len==strlen(kHDiffzAppHead_version));
    assert(kHDiffzAppHead_maxLen<=_kWindowCacheSize);
    if ((out_diffinfo->appHeadDataLen>=kHDiffzAppHead_len+kHDiffzAppHead_version_len)
        &&(out_diffinfo->appHeadDataLen<=kHDiffzAppHead_maxLen)){
        const hpatch_byte* pAppHead=_TStreamCacheClip_accessData(&diffClip,(size_t)out_diffinfo->appHeadDataLen);
        if (pAppHead==0)
            return _hpatch_FALSE;
        if ((0==memcmp(pAppHead,kHDiffzAppHead,kHDiffzAppHead_len))
          &&(0==memcmp(pAppHead+out_diffinfo->appHeadDataLen-kHDiffzAppHead_version_len,kHDiffzAppHead_version,kHDiffzAppHead_version_len)))
            out_diffinfo->isHDiffzAppHead_a=hpatch_TRUE;
    }
    if (isNeedWindowSize){
        return _getVcDiffWindowSizes(out_diffinfo,diffStream);
    }else{
        out_diffinfo->maxSrcWindowsSize=hpatch_kNullStreamPos;
        out_diffinfo->maxTargetWindowsSize=hpatch_kNullStreamPos;
        out_diffinfo->sumTargetWindowsSize=hpatch_kNullStreamPos;
        return hpatch_TRUE;
    }
}

hpatch_BOOL getVcDiffInfo_mem(hpatch_VcDiffInfo* out_diffinfo,const hpatch_byte* diffData,
                              const hpatch_byte* diffData_end,hpatch_BOOL isNeedWindowSize){
    hpatch_TStreamInput diffStream;
    mem_as_hStreamInput(&diffStream,diffData,diffData_end);
    return getVcDiffInfo(out_diffinfo,&diffStream,isNeedWindowSize);
}

hpatch_BOOL getIsVcDiff(const hpatch_TStreamInput* diffData){
    hpatch_VcDiffInfo diffinfo;
    return getVcDiffInfo(&diffinfo,diffData,hpatch_FALSE);
}
hpatch_BOOL getIsVcDiff_mem(const hpatch_byte* diffData,const hpatch_byte* diffData_end){
    hpatch_TStreamInput diffStream;
    mem_as_hStreamInput(&diffStream,diffData,diffData_end);
    return getIsVcDiff(&diffStream);
}

typedef struct{
    hpatch_BOOL isGoogleVersion;
    hpatch_BOOL isNeedChecksum;
    hpatch_BOOL     window_isHaveAdler32;
    hpatch_uint32_t window_savedAdler32;
    hpatch_uint32_t window_curAdler32;

    const hpatch_TStreamOutput* _dstStream;
    hpatch_TStreamOutput        _hookStream;
} vcpatch_checksum_t;

static hpatch_inline void vcpatch_checksum_init(vcpatch_checksum_t* self,hpatch_BOOL isGoogleVersion,
                                                hpatch_BOOL isNeedChecksum,hpatch_BOOL window_isHaveAdler32){
    memset(self,0,sizeof(*self));
    self->isGoogleVersion=isGoogleVersion;
    self->isNeedChecksum=isNeedChecksum;
    self->window_isHaveAdler32=window_isHaveAdler32;
}

#define vcpatch_checksum_readAdler32(self,diffClip){ \
    if ((self)->window_isHaveAdler32){ \
        if ((self)->isGoogleVersion){  \
            hpatch_StreamPos_t _v;     \
            _clip_unpackUInt64(diffClip,&_v); \
            if ((_v>>32)>0) return _hpatch_FALSE; /*error data or no data*/ \
            (self)->window_savedAdler32=(hpatch_uint32_t)_v; \
        }else{ \
            _clip_readUInt32_BigEndian(diffClip,&(self)->window_savedAdler32); \
        } \
    } }


    static hpatch_BOOL __vcpatch_checksum_read_writed(const struct hpatch_TStreamOutput* stream,hpatch_StreamPos_t readFromPos,
                                                    hpatch_byte* out_data,hpatch_byte* out_data_end){
        vcpatch_checksum_t* self=(vcpatch_checksum_t*)stream->streamImport;
        return self->_dstStream->read_writed(self->_dstStream,readFromPos,out_data,out_data_end);      
    }
    static hpatch_BOOL __vcpatch_checksum_write(const struct hpatch_TStreamOutput* stream,hpatch_StreamPos_t writeToPos,
                                                const hpatch_byte* data,const hpatch_byte* data_end){
        vcpatch_checksum_t* self=(vcpatch_checksum_t*)stream->streamImport;
        self->window_curAdler32=adler32_append(self->window_curAdler32,data,data_end-data);
        return self->_dstStream->write(self->_dstStream,writeToPos,data,data_end);
    }
static hpatch_BOOL __vcpatch_checksum_begin_(vcpatch_checksum_t* self,_TOutStreamCache* outCache){
    assert(self->_dstStream==0);
    assert(_TOutStreamCache_cachedDataSize(outCache)==0);

    self->_hookStream.streamImport=self;
    self->_hookStream.streamSize=outCache->dstStream->streamSize;
    self->_hookStream.read_writed=(outCache->dstStream->read_writed?__vcpatch_checksum_read_writed:0);
    self->_hookStream.write=__vcpatch_checksum_write;

    self->_dstStream=outCache->dstStream;
    outCache->dstStream=&self->_hookStream;
    self->window_curAdler32=(self->isGoogleVersion?0:1);
    return hpatch_TRUE;
}

static hpatch_BOOL __vcpatch_checksum_end_(vcpatch_checksum_t* self,_TOutStreamCache* outCache){
    assert(self->_dstStream!=0);
    assert(outCache->dstStream==&self->_hookStream);
    assert(_TOutStreamCache_cachedDataSize(outCache)==0);

    outCache->dstStream=self->_dstStream;
    self->_dstStream=0;
    return self->window_curAdler32==self->window_savedAdler32;
}

static hpatch_inline hpatch_BOOL vcpatch_checksum_begin(vcpatch_checksum_t* self,_TOutStreamCache* outCache){
    if ((self)->isNeedChecksum&(self)->window_isHaveAdler32)
        return __vcpatch_checksum_begin_(self,outCache);
    else
        return hpatch_TRUE;
}

static hpatch_inline hpatch_BOOL vcpatch_checksum_end(vcpatch_checksum_t* self,_TOutStreamCache* outCache){
    if ((self)->isNeedChecksum&(self)->window_isHaveAdler32)
        return __vcpatch_checksum_end_(self,outCache);
    else
        return hpatch_TRUE;
}


hpatch_BOOL _vcpatch_delta(_TOutStreamCache* outCache,hpatch_StreamPos_t targetLen,
                           const hpatch_TStreamInput* srcData,hpatch_StreamPos_t srcPos,hpatch_StreamPos_t srcLen,
                           TStreamCacheClip* dataClip,TStreamCacheClip* instClip,TStreamCacheClip* addrClip){
    hpatch_StreamPos_t same_array[vcdiff_s_same*256]={0};
    hpatch_StreamPos_t near_array[vcdiff_s_near]={0};
    hpatch_StreamPos_t here=0;
    hpatch_StreamPos_t near_index=0;
    const vcdiff_code_table_t code_table=get_vcdiff_code_table_default();
    while (here<targetLen){
        const vcdiff_code_t*    codes;
        const vcdiff_code_t*    codes_end;
        {
            hpatch_byte insti;
            _clip_readUInt8(instClip,&insti);
            codes=(const vcdiff_code_t*)&code_table[insti];
            assert(codes[0].inst!=vcdiff_code_NOOP);
            codes_end=codes+((codes[1].inst==vcdiff_code_NOOP)?1:2);
        }
        for (;codes<codes_end;++codes){
            hpatch_StreamPos_t  addr;
            hpatch_StreamPos_t  cur_here=here;
            hpatch_StreamPos_t  size=codes->size;
            const hpatch_size_t inst=codes->inst;
            assert(inst<=vcdiff_code_MAX);
            if (size==0)
                _clip_unpackUInt64(instClip,&size);
            here+=size;
#ifdef __RUN_MEM_SAFE_CHECK
            if (here>targetLen)
                return _hpatch_FALSE;
#endif
            switch (inst){
                case vcdiff_code_ADD: {
                    if (!_TOutStreamCache_copyFromClip(outCache,dataClip,size))
                        return _hpatch_FALSE;
                } continue;
                case vcdiff_code_RUN: {
                    hpatch_byte v;
                    _clip_readUInt8(dataClip,&v);
                    if (!_TOutStreamCache_fill(outCache,v,size))
                        return _hpatch_FALSE;
                } continue;
                case vcdiff_code_COPY_SELF:
                case vcdiff_code_COPY_HERE:
                case vcdiff_code_COPY_NEAR0:
                case vcdiff_code_COPY_NEAR1:
                case vcdiff_code_COPY_NEAR2:
                case vcdiff_code_COPY_NEAR3: {
                    _clip_unpackUInt64(addrClip,&addr);
                    switch (inst){
                        case vcdiff_code_COPY_SELF: {
                        } break;
                        case vcdiff_code_COPY_HERE: {
                            addr=srcLen+cur_here-addr;
                        } break;
                        default: {
                            addr+=near_array[inst-vcdiff_code_COPY_NEAR0];
                        } break;
                    }
                } break;
                default: { // vcdiff_code_COPY_SAME
                    hpatch_byte samei;
                    _clip_readUInt8(addrClip,&samei);
                    addr=same_array[((hpatch_size_t)(inst-vcdiff_code_COPY_SAME0))*256+samei];
                } break;
            }//switch inst
            vcdiff_update_addr(same_array,near_array,&near_index,addr);
            
            if (addr<srcLen){//copy from src
                hpatch_StreamPos_t copyLen=(addr+size<=srcLen)?size:srcLen-addr;
                if (!_TOutStreamCache_copyFromStream(outCache,srcData,srcPos+addr,copyLen))
                    return _hpatch_FALSE;
                cur_here+=copyLen;
                addr+=copyLen;
                size-=copyLen;
            }
            if (size>0){ //copy from outCache
                addr-=srcLen;
#ifdef __RUN_MEM_SAFE_CHECK
                if (addr>=cur_here)
                    return _hpatch_FALSE;
#endif
                if (!_TOutStreamCache_copyFromSelf(outCache,cur_here-addr,size))
                    return _hpatch_FALSE;
            }
        }//for codes
    }//while
    return hpatch_TRUE;
}


static hpatch_BOOL _getStreamClip(TStreamCacheClip* clip,hpatch_byte Delta_Indicator,hpatch_byte index,
                                  const hpatch_TStreamInput* diffStream,hpatch_StreamPos_t* curDiffOffset,
                                  hpatch_TDecompress* decompressPlugin,_TDecompressInputStream* decompressers,
                                  hpatch_StreamPos_t dataLen,hpatch_byte* temp_cache,hpatch_size_t cache_size){
    hpatch_StreamPos_t uncompressedLen;
    hpatch_StreamPos_t compressedLen;
    if ((Delta_Indicator&(1<<index))){
        hpatch_StreamPos_t readedBytes;
        TStreamCacheClip   tempClip;
        hpatch_byte  _temp_cache[hpatch_kMaxPackedUIntBytes];
        _TStreamCacheClip_init(&tempClip,diffStream,*curDiffOffset,
                                diffStream->streamSize,_temp_cache,sizeof(_temp_cache));
        _clip_unpackUInt64(&tempClip,&uncompressedLen);
        readedBytes=_TStreamCacheClip_readPosOfSrcStream(&tempClip)-(*curDiffOffset);
        (*curDiffOffset)+=readedBytes;
        compressedLen=dataLen-readedBytes;
    }else{
        uncompressedLen=dataLen;
        compressedLen=0;
    }
    return getStreamClip(clip,&decompressers[index],uncompressedLen,compressedLen,diffStream,
                         curDiffOffset,decompressPlugin,temp_cache,cache_size);
}



static hpatch_BOOL _getVcDiffWindowSizes(hpatch_VcDiffInfo* out_diffinfo,const hpatch_TStreamInput* diffStream){
    hpatch_StreamPos_t windowOffset=out_diffinfo->windowOffset;
    hpatch_StreamPos_t maxSrcWindowsSize=0;
    hpatch_StreamPos_t maxTargetWindowsSize=0;
    hpatch_StreamPos_t sumTargetWindowsSize=0;
    hpatch_byte _cacheBuf[1024];
    //window loop
    while (windowOffset<diffStream->streamSize){
        hpatch_StreamPos_t srcPos=0;
        hpatch_StreamPos_t srcLen=0;
        hpatch_StreamPos_t targetLen;
        hpatch_StreamPos_t deltaLen;
        hpatch_StreamPos_t dataLen;
        hpatch_StreamPos_t instLen;
        hpatch_StreamPos_t addrLen;
        hpatch_StreamPos_t curDiffOffset;
        hpatch_BOOL isHave_srcData;
        vcpatch_checksum_t checksumer;
        hpatch_byte Delta_Indicator;
        {
            TStreamCacheClip   diffClip;
            _TStreamCacheClip_init(&diffClip,diffStream,windowOffset,diffStream->streamSize,
                                   _cacheBuf,sizeof(_cacheBuf));
            {
                hpatch_BOOL window_isHaveAdler32;
                hpatch_byte Win_Indicator;
                _clip_readUInt8(&diffClip,&Win_Indicator);
                window_isHaveAdler32=(0!=(Win_Indicator&VCD_ADLER32));
                if (window_isHaveAdler32) Win_Indicator-=VCD_ADLER32;
                vcpatch_checksum_init(&checksumer,out_diffinfo->isGoogleVersion,hpatch_FALSE,window_isHaveAdler32);
                switch (Win_Indicator){
                    case 0         :{ isHave_srcData=hpatch_FALSE; } break;
                    case VCD_SOURCE:
                    case VCD_TARGET:{ isHave_srcData=hpatch_TRUE; } break;
                    default: return _hpatch_FALSE; //error data or unsupport
                }
            }
            if (isHave_srcData){
                _clip_unpackUInt64(&diffClip,&srcLen);
                _clip_unpackUInt64(&diffClip,&srcPos);
            }
            _clip_unpackUInt64(&diffClip,&deltaLen);
            {
#ifdef __RUN_MEM_SAFE_CHECK
                hpatch_StreamPos_t deltaHeadSize=_TStreamCacheClip_readPosOfSrcStream(&diffClip);
                if (deltaLen>_TStreamCacheClip_leaveSize(&diffClip))
                    return _hpatch_FALSE; //error data
#endif
                _clip_unpackUInt64(&diffClip,&targetLen);
                _clip_readUInt8(&diffClip,&Delta_Indicator);
                _clip_unpackUInt64(&diffClip,&dataLen);
                _clip_unpackUInt64(&diffClip,&instLen);
                _clip_unpackUInt64(&diffClip,&addrLen);
                vcpatch_checksum_readAdler32(&checksumer,&diffClip);
                curDiffOffset=_TStreamCacheClip_readPosOfSrcStream(&diffClip);
#ifdef __RUN_MEM_SAFE_CHECK
                deltaHeadSize=curDiffOffset-deltaHeadSize;
                if (deltaLen!=deltaHeadSize+dataLen+instLen+addrLen)
                    return _hpatch_FALSE; //error data
#endif
            }
            windowOffset=curDiffOffset+dataLen+instLen+addrLen;

            {
                hpatch_StreamPos_t sumSize=sumTargetWindowsSize+targetLen;
#ifdef __RUN_MEM_SAFE_CHECK
                if (sumSize<sumTargetWindowsSize)
                    return _hpatch_FALSE; //error data
#endif
                sumTargetWindowsSize=sumSize;
                maxSrcWindowsSize=(maxSrcWindowsSize>=srcLen)?maxSrcWindowsSize:srcLen;
                maxTargetWindowsSize=(maxTargetWindowsSize>=targetLen)?maxTargetWindowsSize:targetLen;
            }
        }
    }
    out_diffinfo->maxSrcWindowsSize=maxSrcWindowsSize;
    out_diffinfo->maxTargetWindowsSize=maxTargetWindowsSize;
    out_diffinfo->sumTargetWindowsSize=sumTargetWindowsSize;
    return hpatch_TRUE;
}


#define _kCacheVcDecCount (1+3)

typedef struct{
    hpatch_TStreamInput srcStream;
    hpatch_StreamPos_t srcPos_bck;
    hpatch_size_t      srcLen_bck;
    hpatch_size_t      mem_offset;
    hpatch_size_t      mem_size;
    hpatch_byte*       mem_cache;
    const hpatch_TStreamInput* srcData_bck;
}src_cache_t;

static hpatch_inline void _src_cache_move_by_offset(src_cache_t* self,hpatch_size_t dstPos,hpatch_size_t movePos){
    hpatch_size_t mem_size=self->mem_size;
    hpatch_StreamPos_t noff=(hpatch_StreamPos_t)self->mem_offset+mem_size+movePos-dstPos;
    if (noff>mem_size){
        noff-=mem_size;
        if (noff>mem_size)
            noff-=mem_size;
    }
    self->mem_offset=(hpatch_size_t)noff;
}
static hpatch_BOOL _src_cache_update_by_offset(src_cache_t* self,const hpatch_TStreamInput* srcData,
                                               hpatch_StreamPos_t srcPos,hpatch_StreamPos_t dstPos,hpatch_size_t updateLen){
    const hpatch_size_t mem_size=self->mem_size;
    hpatch_byte*  temp_cache=self->mem_cache;
    srcPos+=dstPos;
    dstPos+=self->mem_offset;
    if (dstPos<mem_size){
        hpatch_size_t readLen=updateLen;
        if (dstPos+readLen>mem_size) readLen=mem_size-(hpatch_size_t)dstPos;
        if (!srcData->read(srcData,srcPos,temp_cache+(hpatch_size_t)dstPos,temp_cache+(hpatch_size_t)dstPos+readLen))
            return _hpatch_FALSE;
        srcPos+=readLen;
        dstPos+=readLen;
        updateLen-=readLen;
        if (updateLen==0) return hpatch_TRUE;
    }
    dstPos-=mem_size;
    if (!srcData->read(srcData,srcPos,temp_cache+(hpatch_size_t)dstPos,temp_cache+(hpatch_size_t)dstPos+updateLen))
        return _hpatch_FALSE;
    return hpatch_TRUE;
}

static hpatch_BOOL _src_cache_read(const struct hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                   unsigned char* out_data,unsigned char* out_data_end){
    src_cache_t* self=(src_cache_t*)stream->streamImport;
    hpatch_size_t readLen;
    const hpatch_size_t mem_size=self->mem_size;
    assert(mem_size>=readFromPos+(hpatch_size_t)(out_data_end-out_data));
    readFromPos+=self->mem_offset;
    if (readFromPos<mem_size){
        readLen=out_data_end-out_data;
        if (readFromPos+readLen>mem_size)
            readLen=mem_size-(hpatch_size_t)readFromPos;
        memcpy(out_data,self->mem_cache+(hpatch_size_t)readFromPos,readLen);
        readFromPos+=readLen;
        out_data+=readLen;
        if (out_data==out_data_end) return hpatch_TRUE;
    }
    readFromPos-=mem_size;
    readLen=out_data_end-out_data;
    assert(mem_size>=readFromPos+readLen);
    memcpy(out_data,self->mem_cache+(hpatch_size_t)readFromPos,readLen);
    return hpatch_TRUE;
}

static hpatch_inline void src_cache_init(src_cache_t* self,hpatch_StreamPos_t srcLenMax,hpatch_byte** mem_cache,hpatch_size_t* allCacheSize){
    hpatch_size_t mem_size=0;
    if ((srcLenMax>0)&&(srcLenMax+_kCacheVcDecCount*hpatch_kStreamCacheSize<=(*allCacheSize))){
        mem_size=(hpatch_size_t)srcLenMax;
    }
    self->srcPos_bck=0;
    self->srcData_bck=0;
    self->mem_cache=*mem_cache;
    self->mem_offset=0;
    self->mem_size=mem_size;
    (*allCacheSize)-=mem_size;
    (*mem_cache)+=mem_size;
}

static hpatch_inline hpatch_BOOL src_cache_isCanCache(src_cache_t* self,hpatch_StreamPos_t srcLen){
    return (srcLen>0)&&(srcLen<=self->mem_size);
}

static hpatch_TStreamInput* src_cache_update(src_cache_t* self,const hpatch_TStreamInput* srcData,
                                             hpatch_size_t srcLen,hpatch_StreamPos_t srcPos){
    if ((self->srcData_bck!=srcData)||(self->srcPos_bck+self->srcLen_bck<=srcPos)||(self->srcPos_bck>=srcPos+srcLen)){
        self->mem_offset=0;
        if (!srcData->read(srcData,srcPos,self->mem_cache,self->mem_cache+srcLen))
            return _hpatch_FALSE;
    }else{//hit cache
        hpatch_size_t dstPos;
        hpatch_size_t movePos;
        hpatch_size_t moveLen;
        if (self->srcPos_bck<=srcPos){
            dstPos=0;
            movePos=(hpatch_size_t)(srcPos-self->srcPos_bck);
            moveLen=self->srcLen_bck-movePos;
        }else{
            dstPos=(hpatch_size_t)(self->srcPos_bck-srcPos);
            movePos=0;
            moveLen=self->srcLen_bck;
        }
        if (dstPos+moveLen>=srcLen)
            moveLen=srcLen-dstPos;
        _src_cache_move_by_offset(self,dstPos,movePos);
        if (dstPos>0){
            if (!_src_cache_update_by_offset(self,srcData,srcPos,0,dstPos))
                return _hpatch_FALSE;
        }
        dstPos+=moveLen;
        if (dstPos<srcLen){
            if (!_src_cache_update_by_offset(self,srcData,srcPos,dstPos,srcLen-dstPos))
                return _hpatch_FALSE;
        }
    }

    self->srcStream.streamImport=self;
    self->srcStream.streamSize=srcLen;
    self->srcStream.read=_src_cache_read;
    self->srcStream.streamImport=self;
    self->srcStream.streamImport=self;

    self->srcPos_bck=srcPos;
    self->srcLen_bck=srcLen;
    self->srcData_bck=srcData;
    return &self->srcStream;
}


hpatch_BOOL _vcpatch_window(const hpatch_TStreamOutput* out_newData,const hpatch_TStreamInput* oldData,
                            const hpatch_TStreamInput* diffStream,hpatch_TDecompress* decompressPlugin,
                            _TDecompressInputStream* decompressers,hpatch_StreamPos_t windowOffset,
                            hpatch_StreamPos_t srcLenMax,hpatch_BOOL isGoogleVersion,
                            hpatch_BOOL isNeedChecksum,hpatch_byte* tempCaches,hpatch_size_t allCacheSize){
    src_cache_t src_cache;
    _TOutStreamCache outCache;
    if (allCacheSize<hpatch_kMaxPackedUIntBytes*_kCacheVcDecCount) return _hpatch_FALSE;
    _TOutStreamCache_init(&outCache,out_newData,0,0); //need reset cache
    src_cache_init(&src_cache,srcLenMax,&tempCaches,&allCacheSize);

    //window loop
    while (windowOffset<diffStream->streamSize){
        hpatch_StreamPos_t srcPos=0;
        hpatch_StreamPos_t srcLen=0;
        hpatch_StreamPos_t targetLen;
        hpatch_StreamPos_t deltaLen;
        hpatch_StreamPos_t dataLen;
        hpatch_StreamPos_t instLen;
        hpatch_StreamPos_t addrLen;
        hpatch_StreamPos_t curDiffOffset;
        const hpatch_TStreamInput* srcData;
        vcpatch_checksum_t checksumer;
        hpatch_byte Delta_Indicator;
        {
            TStreamCacheClip   diffClip;
            hpatch_byte _window_cache[_kWindowCacheSize];
            _TStreamCacheClip_init(&diffClip,diffStream,windowOffset,diffStream->streamSize,
                                   _window_cache,sizeof(_window_cache));
            {
                hpatch_BOOL window_isHaveAdler32;
                hpatch_byte Win_Indicator;
                _clip_readUInt8(&diffClip,&Win_Indicator);
                window_isHaveAdler32=(0!=(Win_Indicator&VCD_ADLER32));
                if (window_isHaveAdler32) Win_Indicator-=VCD_ADLER32;
                vcpatch_checksum_init(&checksumer,isGoogleVersion,isNeedChecksum,window_isHaveAdler32);
                switch (Win_Indicator){
                    case 0         :{ srcData=0; } break;
                    case VCD_SOURCE:{ srcData=oldData; } break;
                    case VCD_TARGET:{
                        srcData=(const hpatch_TStreamInput*)outCache.dstStream;
                        if (srcData->read==0)
                            return _hpatch_FALSE; //unsupport, target can't read  
                    } break;
                    default:
                        return _hpatch_FALSE; //error data or unsupport
                }
            }
            if (srcData){
                _clip_unpackUInt64(&diffClip,&srcLen);
                _clip_unpackUInt64(&diffClip,&srcPos);
#ifdef __RUN_MEM_SAFE_CHECK
                {
                    hpatch_StreamPos_t streamSafeEnd=(srcData==oldData)?srcData->streamSize:outCache.writeToPos;
                    hpatch_StreamPos_t srcPosEnd=srcPos+srcLen;
                    if ((srcPosEnd<srcPos)|(srcPosEnd<srcLen)|(srcPosEnd>streamSafeEnd))
                        return _hpatch_FALSE; //error data
                }
#endif
            }
            _clip_unpackUInt64(&diffClip,&deltaLen);
            {
#ifdef __RUN_MEM_SAFE_CHECK
                hpatch_StreamPos_t deltaHeadSize=_TStreamCacheClip_readPosOfSrcStream(&diffClip);
                if (deltaLen>_TStreamCacheClip_leaveSize(&diffClip))
                    return _hpatch_FALSE; //error data
#endif
                _clip_unpackUInt64(&diffClip,&targetLen);
                _clip_readUInt8(&diffClip,&Delta_Indicator);
                if (decompressPlugin==0) Delta_Indicator=0;
                _clip_unpackUInt64(&diffClip,&dataLen);
                _clip_unpackUInt64(&diffClip,&instLen);
                _clip_unpackUInt64(&diffClip,&addrLen);
                vcpatch_checksum_readAdler32(&checksumer,&diffClip);
                curDiffOffset=_TStreamCacheClip_readPosOfSrcStream(&diffClip);
#ifdef __RUN_MEM_SAFE_CHECK
                deltaHeadSize=curDiffOffset-deltaHeadSize;
                if (deltaLen!=deltaHeadSize+dataLen+instLen+addrLen)
                    return _hpatch_FALSE; //error data
#endif
            }
            windowOffset=curDiffOffset+dataLen+instLen+addrLen;
        }

        if (src_cache_isCanCache(&src_cache,srcLen)){//old all in cache?
            assert(srcData!=0);
            srcData=src_cache_update(&src_cache,srcData,(hpatch_size_t)srcLen,srcPos);
            if (srcData==0) return _hpatch_FALSE;
            srcPos=0;
        }
        {
            hpatch_BOOL       ret;
            const hpatch_BOOL isInterleaved=((dataLen==0)&(addrLen==0));
            TStreamCacheClip  dataClip;
            TStreamCacheClip  instClip;
            TStreamCacheClip  addrClip;
            hpatch_byte*      temp_cache=tempCaches;
            hpatch_size_t     cache_size=allCacheSize;
            {
                hpatch_size_t out_cache_size;
                if ((targetLen>0)&&(targetLen+3*hpatch_kStreamCacheSize<=cache_size)){//target all in cache?
                    out_cache_size=(hpatch_size_t)targetLen;
                    cache_size=(cache_size-out_cache_size)/(isInterleaved?1:3);
                }else{
                    #define kBetterDecompressBufSize (1024*32)
                    hpatch_size_t dec_size=cache_size/_kCacheVcDecCount;
                    dec_size=(dec_size<=kBetterDecompressBufSize)?dec_size:kBetterDecompressBufSize;
                    out_cache_size=cache_size-dec_size*(isInterleaved?1:3);
                    cache_size=dec_size;
                }
                _TOutStreamCache_resetCache(&outCache,temp_cache,out_cache_size);
                temp_cache+=out_cache_size;
            }

            #define __getStreamClip(clip,index,len) { \
                if (!(_getStreamClip(clip,Delta_Indicator,index,diffStream,&curDiffOffset, \
                                    decompressPlugin,decompressers,len,temp_cache,cache_size))) \
                    return _hpatch_FALSE; \
                temp_cache+=cache_size; }

            if (!isInterleaved){
                __getStreamClip(&dataClip,0,dataLen);
                __getStreamClip(&instClip,1,instLen);
                __getStreamClip(&addrClip,2,addrLen);
            }else{
                __getStreamClip(&instClip,0,instLen);
            }
            assert(curDiffOffset==windowOffset);
            if (!vcpatch_checksum_begin(&checksumer,&outCache))
                return _hpatch_FALSE;
            ret=_vcpatch_delta(&outCache,targetLen,srcData,srcPos,srcLen,
                               isInterleaved?&instClip:&dataClip,&instClip,
                               isInterleaved?&instClip:&addrClip);
            
            if (!_TOutStreamCache_flush(&outCache))
                ret=_hpatch_FALSE;
            if ((!vcpatch_checksum_end(&checksumer,&outCache))||(!ret))
                return _hpatch_FALSE;
        }
    }

    if (_TOutStreamCache_isFinish(&outCache)||(outCache.dstStream->streamSize==hpatch_kNullStreamPos))
        return hpatch_TRUE;
    else
        return _hpatch_FALSE;
}


hpatch_BOOL vcpatch_with_cache(const hpatch_TStreamOutput* out_newData,
                               const hpatch_TStreamInput*  oldData,
                               const hpatch_TStreamInput*  diffStream,
                               hpatch_TDecompress* decompressPlugin,hpatch_BOOL isNeedChecksum,
                               hpatch_byte* temp_cache,hpatch_byte* temp_cache_end){
    hpatch_VcDiffInfo diffInfo;
    hpatch_size_t i;
    hpatch_BOOL  result=hpatch_TRUE;
    _TDecompressInputStream decompressers[3];
    for (i=0;i<sizeof(decompressers)/sizeof(_TDecompressInputStream);++i)
        decompressers[i].decompressHandle=0;
    assert(out_newData!=0);
    assert(out_newData->write!=0);
    assert(oldData!=0);
    assert(oldData->read!=0);
    assert(diffStream!=0);
    assert(diffStream->read!=0);
    if (!getVcDiffInfo(&diffInfo,diffStream,hpatch_TRUE))
        return _hpatch_FALSE;

    if (diffInfo.compressorID){
        if (decompressPlugin==0)
            return _hpatch_FALSE;
    }else{
        decompressPlugin=0;
    }

    result=_vcpatch_window(out_newData,oldData,diffStream,decompressPlugin,decompressers,
                           diffInfo.windowOffset,diffInfo.maxSrcWindowsSize,diffInfo.isGoogleVersion,
                           isNeedChecksum,temp_cache,temp_cache_end-temp_cache);

    for (i=0;i<sizeof(decompressers)/sizeof(_TDecompressInputStream);++i) {
        if (decompressers[i].decompressHandle){
            if (!decompressPlugin->close(decompressPlugin,decompressers[i].decompressHandle))
                result=_hpatch_FALSE;
            decompressers[i].decompressHandle=0;
        }
    }
    return result;
}