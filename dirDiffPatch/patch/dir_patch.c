// dir_patch.c
// hdiffz dir diff
//
/*
 The MIT License (MIT)
 Copyright (c) 2012-2019 HouSisong
 
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
#include "dir_patch.h"
#include <stdio.h>
#include <string.h>
#include "../../libHDiffPatch/HPatch/patch.h"
#include "../../file_for_patch.h"

static const char* kVersionType="DirDiff19&";

#define  check(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        result=hpatch_FALSE; assert(hpatch_FALSE); goto clear; } }

hpatch_BOOL getDirDiffInfoByFile(const char* diffFileName,
                                 char* out_compressType/*[hpatch_kMaxCompressTypeLength+1]*/,
                                 hpatch_BOOL* out_newIsDir,hpatch_BOOL* out_oldIsDir){
    hpatch_BOOL          result=hpatch_TRUE;
    TFileStreamInput     diffData;
    TFileStreamInput_init(&diffData);
    
    check(TFileStreamInput_open(&diffData,diffFileName));
    result=getDirDiffInfo(&diffData.base,out_compressType,out_newIsDir,out_oldIsDir);
    check(TFileStreamInput_close(&diffData));
clear:
    return result;
}

hpatch_BOOL getDirDiffInfo(const hpatch_TStreamInput* diffFile,
                           char* out_compressType/*[hpatch_kMaxCompressTypeLength+1]*/,
                           hpatch_BOOL* out_newIsDir,hpatch_BOOL* out_oldIsDir){
    hpatch_BOOL result=hpatch_TRUE;
    size_t tagSize=strlen(kVersionType);
    if (diffFile->streamSize<tagSize) return hpatch_FALSE;
    //assert(tagSize<=hpatch_kStreamCacheSize);
    assert(hpatch_kStreamCacheSize>=hpatch_kMaxCompressTypeLength+1);
    unsigned char buf[hpatch_kStreamCacheSize+1];
    check((long)tagSize==diffFile->read(diffFile->streamHandle,0,
                                        buf,buf+tagSize));
    if (0!=memcmp(buf,kVersionType,tagSize))
        return hpatch_FALSE;
    if (out_compressType||out_newIsDir||out_oldIsDir){
        size_t readLen=hpatch_kMaxCompressTypeLength+1;
        buf[readLen]='\0';
        if (readLen+tagSize>diffFile->streamSize) readLen=diffFile->streamSize-tagSize;
        check((long)readLen==diffFile->read(diffFile->streamHandle,tagSize,
                                            buf,buf+readLen));
        check(strlen((char*)buf)<readLen);
        if (out_compressType) strcpy(out_compressType,(char*)buf);//safe
    }
    if (out_newIsDir||out_oldIsDir){
        //const TByte kPatchModel=0;
        //hpatch_unpackUInt(<#src_code#>, <#src_code_end#>, <#result#>)
        //packUInt(out_data,kPatchModel);
        //packUInt(out_data,newIsDir?1:0);
        //packUInt(out_data,oldIsDir?1:0);
        //todo:
    }
clear:
    return result;
}

TDirPatchResult dir_patch(const hpatch_TStreamOutput* out_newData,
                          const char* oldPatch,const hpatch_TStreamInput*  diffData,
                          hpatch_TDecompress* decompressPlugin,
                          hpatch_BOOL isLoadOldAll,size_t patchCacheSize){
    return 1;
}
