// dir_patch_tools.c
// hdiffz dir patch
//
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
#include "dir_patch_tools.h"
#include "../../file_for_patch.h"
#if (_IS_NEED_DIR_DIFF_PATCH)
#include <stdio.h>
#include <string.h>

#define TUInt hpatch_StreamPos_t

#define  check(value) { if (!(value)){ fprintf(stderr,"check "#value" error!\n");  \
                                       result=hpatch_FALSE; goto clear; } }

void formatDirTagForLoad(char* utf8_path,char* utf8_pathEnd){
    if (kPatch_dirSeparator==kPatch_dirSeparator_saved) return;
    for (;utf8_path<utf8_pathEnd;++utf8_path){
        if ((*utf8_path)!=kPatch_dirSeparator_saved)
            continue;
        else
            *utf8_path=kPatch_dirSeparator;
    }
}

hpatch_BOOL clipCStrsTo(const char* cstrs,const char* cstrsEnd,
                        const char** out_cstrList,size_t cstrCount){
    if (cstrs<cstrsEnd){
        if (cstrsEnd[-1]!='\0') return hpatch_FALSE;
        while ((cstrs<cstrsEnd)&(cstrCount>0)) {
            *out_cstrList=cstrs;  ++out_cstrList; --cstrCount;
            cstrs+=strlen(cstrs)+1; //safe
        }
    }
    return (cstrs==cstrsEnd)&(cstrCount==0);
}

hpatch_BOOL readListTo(TStreamCacheClip* sclip,hpatch_StreamPos_t* out_list,size_t count){
    hpatch_BOOL result=hpatch_TRUE;
    size_t i;
    for (i=0; i<count; ++i) {
        check(_TStreamCacheClip_unpackUIntWithTag(sclip,&out_list[i],0));
    }
clear:
    return result;
}

hpatch_BOOL readIncListTo(TStreamCacheClip* sclip,size_t* out_list,
                          size_t count,size_t check_endValue){
    //endValue: maxValue+1
    hpatch_BOOL result=hpatch_TRUE;
    TUInt backValue=~(TUInt)0;
    size_t i;
    for (i=0; i<count; ++i) {
        TUInt incValue;
        check(_TStreamCacheClip_unpackUIntWithTag(sclip,&incValue,0));
        backValue+=1+incValue;
        check(backValue<check_endValue);
        out_list[i]=(size_t)backValue;
    }
clear:
    return result;
}

char* setPath(char* out_path,char* out_pathBufEnd,const char* fileName){
    char* result=0; //false
    size_t strLen=strlen(fileName);
    check(strLen+1<=(size_t)(out_pathBufEnd-out_path));
    memcpy(out_path,fileName,strLen+1);//with '\0'
    result=out_path+strLen;
clear:
    return result;
}

char* setDirPath(char* out_path,char* out_pathBufEnd,const char* dirName){
    char* result=0; //false
    size_t strLen=strlen(dirName);
    hpatch_BOOL isNeedDirSeparator=(strLen>0)&&(dirName[strLen-1]!=kPatch_dirSeparator);
    check((strLen+(isNeedDirSeparator?1:0)+1)<=(size_t)(out_pathBufEnd-out_path));
    memcpy(out_path,dirName,strLen);
    result=out_path+strLen;
    if (isNeedDirSeparator) { *result=kPatch_dirSeparator; ++result; }
    *result='\0'; //C string end
clear:
    return result;
}

#endif
