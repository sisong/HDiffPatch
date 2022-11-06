// vcpatch_code_table.c
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
#include "vcpatch_code_table.h"

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