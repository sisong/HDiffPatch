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
#include "../../libHDiffPatch/HPatch/patch_private.h"
#include "../../file_for_patch.h"

static const char* kVersionType="DirDiff19&";
static const TByte kPatchMode =0;

#define TUInt hpatch_StreamPos_t

#define  check(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        result=hpatch_FALSE; assert(hpatch_FALSE); goto clear; } }

#define unpackUIntTo(puint,sclip) \
    check(_TStreamCacheClip_unpackUIntWithTag(sclip,puint,0))


hpatch_BOOL getDirDiffInfoByFile(const char* diffFileName,TDirDiffInfo* out_info){
    hpatch_BOOL          result=hpatch_TRUE;
    TFileStreamInput     diffData;
    TFileStreamInput_init(&diffData);
    
    check(TFileStreamInput_open(&diffData,diffFileName));
    result=getDirDiffInfo(&diffData.base,out_info);
    check(TFileStreamInput_close(&diffData));
clear:
    return result;
}

static hpatch_BOOL _read_dirdiff_head(TDirDiffInfo* out_info,_TDirDiffHead* out_head,
                                      const hpatch_TStreamInput* dirDiffFile){
    hpatch_BOOL result=hpatch_TRUE;
    check(out_info!=0);
    out_info->isDirDiff=hpatch_FALSE;
    
    TStreamCacheClip  _headClip;
    TStreamCacheClip* headClip=&_headClip;
    TByte             temp_cache[hpatch_kStreamCacheSize];
    _TStreamCacheClip_init(headClip,dirDiffFile,0,dirDiffFile->streamSize,
                           temp_cache,hpatch_kStreamCacheSize);
    
    {//VersionType
        const size_t kVersionTypeLen=strlen(kVersionType);
        if (dirDiffFile->streamSize<kVersionTypeLen)
            return result;//not is dirDiff data
        //assert(tagSize<=hpatch_kStreamCacheSize);
        const TByte* versionType=_TStreamCacheClip_readData(headClip,kVersionTypeLen);
        check(versionType!=0);
        if (0!=memcmp(versionType,kVersionType,kVersionTypeLen))
            return result;//not is dirDiff data
        out_info->isDirDiff=hpatch_TRUE;
    }
    char savedCompressType[hpatch_kMaxCompressTypeLength+1];
    {//read compressType
        const TByte* compressType;
        size_t       compressTypeLen;
        size_t readLen=hpatch_kMaxCompressTypeLength+1;
        if (readLen>_TStreamCacheClip_streamSize(headClip))
            readLen=(size_t)_TStreamCacheClip_streamSize(headClip);
        compressType=_TStreamCacheClip_accessData(headClip,readLen);
        check(compressType!=0);
        compressTypeLen=strnlen((const char*)compressType,readLen);
        check(compressTypeLen<readLen);
        memcpy(savedCompressType,compressType,compressTypeLen+1);
        _TStreamCacheClip_skipData_noCheck(headClip,compressTypeLen+1);
    }
    {
        TUInt savedValue;
        unpackUIntTo(&savedValue,headClip);
        check(savedValue==kPatchMode); //now only support
        unpackUIntTo(&savedValue,headClip);  check(savedValue<=1);
        out_info->oldPathIsDir=(hpatch_BOOL)savedValue;
        unpackUIntTo(&savedValue,headClip);  check(savedValue<=1);
        out_info->newPathIsDir=(hpatch_BOOL)savedValue;
        
        unpackUIntTo(&out_head->oldPathCount,headClip);
        unpackUIntTo(&out_head->newPathCount,headClip);
        unpackUIntTo(&out_head->oldPathSumSize,headClip);
        unpackUIntTo(&out_head->newPathSumSize,headClip);
        unpackUIntTo(&out_head->oldRefFileCount,headClip);
        unpackUIntTo(&out_head->newRefFileCount,headClip);
        unpackUIntTo(&out_head->sameFilePairCount,headClip);
        unpackUIntTo(&out_head->headDataSize,headClip);
        unpackUIntTo(&out_head->headDataCompressedSize,headClip);
        out_info->dirDataIsCompressed=(out_head->headDataCompressedSize>0);
        unpackUIntTo(&out_info->externDataSize,headClip);
    }
    TUInt curPos=headClip->streamPos-_TStreamCacheClip_cachedSize(headClip);
    out_head->headDataOffset=curPos;
    curPos+=(out_head->headDataCompressedSize>0)?out_head->headDataCompressedSize:out_head->headDataSize;
    out_info->externDataOffset=curPos;
    curPos+=out_info->externDataSize;
    out_head->hdiffDataOffset=curPos;
    out_head->hdiffDataSize=dirDiffFile->streamSize-curPos;
    TStreamInputClip hdiffStream;
    TStreamInputClip_init(&hdiffStream,dirDiffFile,out_head->hdiffDataOffset,
                          out_head->hdiffDataOffset+out_head->hdiffDataSize);
    check(getCompressedDiffInfo(&out_info->hdiffInfo,&hdiffStream.base));
    check(0==strcmp(savedCompressType,out_info->hdiffInfo.compressType));
clear:
    return result;
}

hpatch_BOOL getDirDiffInfo(const hpatch_TStreamInput* diffFile,TDirDiffInfo* out_info){
    _TDirDiffHead head;
    return _read_dirdiff_head(out_info,&head,diffFile);
}


hpatch_BOOL TDirPatcher_open(TDirPatcher* self,const hpatch_TStreamInput* dirDiffData){
    assert(self->_dirDiffData==0);
    self->_dirDiffData=dirDiffData;
    return _read_dirdiff_head(&self->dirDiffInfo,&self->dirDiffHead,self->_dirDiffData);
}


static hpatch_BOOL _TStreamCacheClip_readDataTo(TStreamCacheClip* sclip,TByte* out_buf,TByte* bufEnd){
    const size_t maxReadLen=sclip->cacheEnd;
    while (out_buf<bufEnd) {
        size_t readLen=bufEnd-out_buf;
        if (readLen>maxReadLen) readLen=maxReadLen;
        const TByte* pdata=_TStreamCacheClip_readData(sclip,readLen);
        if (pdata==0) return hpatch_FALSE;
        memcpy(out_buf,pdata,readLen);
        out_buf+=readLen;
    }
    return out_buf==bufEnd;
}

static hpatch_BOOL clipCStrsTo(const char* cstrs,const char* cstrsEnd,
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

static hpatch_BOOL readIncListTo(TStreamCacheClip* sclip,size_t* out_list,size_t count,size_t endValue){
    //endValue: maxValue+1
    hpatch_BOOL result=hpatch_TRUE;
    TUInt backValue=~(TUInt)0;
    for (size_t i=0; i<count; ++i) {
        TUInt incValue;
        check(_TStreamCacheClip_unpackUIntWithTag(sclip,&incValue,0));
        backValue+=1+incValue;
        check(backValue<endValue);
        out_list[i]=(size_t)backValue;
    }
clear:
    return result;
}

static hpatch_BOOL readSamePairListTo(TStreamCacheClip* sclip,size_t* out_pairList,size_t pairCount,
                                      size_t endXValue,size_t endYValue){
    hpatch_BOOL result=hpatch_TRUE;
    TUInt backXValue=~(TUInt)0;
    TUInt backYValue=~(TUInt)0;
    for (size_t i=0; i<pairCount;i+=2) {
        const TByte*  psign;
        TUInt incYValue;
        TUInt incXValue;
        check(_TStreamCacheClip_unpackUIntWithTag(sclip,&incXValue,0));
        backXValue+=1+incXValue;
        check(backXValue<endXValue);
        out_pairList[i+0]=(size_t)backXValue;
        
        psign=_TStreamCacheClip_accessData(sclip,1);
        check(psign!=0);
        check(_TStreamCacheClip_unpackUIntWithTag(sclip,&incYValue,1));
        if (!(*psign)>>(8-1))
            backYValue+=1+incYValue;
        else
            backYValue=backYValue+1-incYValue;
        check(backYValue<endYValue);
        out_pairList[i+1]=(size_t)backYValue;
    }
clear:
    return result;
}

hpatch_BOOL TDirPatcher_loadDirData(TDirPatcher* self,hpatch_TDecompress* decompressPlugin){
    hpatch_BOOL result=hpatch_TRUE;
    hpatch_StreamPos_t   memSize=0;
    TByte*               curMem=0;
    const _TDirDiffHead* head=&self->dirDiffHead;
    const size_t         pathSumSize=head->oldPathSumSize+head->newPathSumSize;
    assert(self->_decompressPlugin==0);
    assert(self->_pmem==0);
    self->_decompressPlugin=decompressPlugin;
    
    //dir head data clip stream
    TStreamCacheClip headData;
    _TDecompressInputSteram decompresser;
    decompresser.decompressHandle=0;
    {
        const size_t cacheSize=hpatch_kStreamCacheSize;
        TByte temp_cache[cacheSize];
        hpatch_StreamPos_t curPos=self->dirDiffHead.headDataOffset;
        check(getStreamClip(&headData,&decompresser,self->dirDiffHead.headDataSize,
                            self->dirDiffHead.headDataCompressedSize,self->_dirDiffData,&curPos,
                            decompressPlugin,temp_cache,cacheSize));
    }
    //mem
    memSize = (head->oldPathCount+head->newPathCount)*sizeof(const char*) + pathSumSize
            + (head->oldRefFileCount+head->newRefFileCount+head->sameFilePairCount*2)*sizeof(size_t);
    self->_pmem=malloc(memSize);
    check(self->_pmem!=0);
    curMem=self->_pmem;
    self->oldUtf8PathList=(const char**)curMem; curMem+=head->oldPathCount*sizeof(const char*);
    self->newUtf8PathList=(const char**)curMem; curMem+=head->newPathCount*sizeof(const char*);
    self->oldRefList=(const size_t*)curMem; curMem+=head->oldRefFileCount*sizeof(size_t);
    self->newRefList=(const size_t*)curMem; curMem+=head->newRefFileCount*sizeof(size_t);
    self->dataSamePairList=(const size_t*)curMem; curMem+=head->sameFilePairCount*2*sizeof(size_t);
    //read old & new path List
    check(_TStreamCacheClip_readDataTo(&headData,curMem,curMem+pathSumSize));
    curMem+=pathSumSize;
    assert(curMem-memSize==self->_pmem);
    check(clipCStrsTo((const char*)curMem-pathSumSize,(const char*)curMem,
                      (const char**)self->oldUtf8PathList,head->oldPathCount+head->newPathCount));
    //read oldRefList
    check(readIncListTo(&headData,(size_t*)self->oldRefList,head->oldRefFileCount,head->oldPathCount));
    //read newRefList
    check(readIncListTo(&headData,(size_t*)self->newRefList,head->newRefFileCount,head->newPathCount));
    //read dataSamePairList
    check(readSamePairListTo(&headData,(size_t*)self->dataSamePairList,head->sameFilePairCount,
                             head->newPathCount,head->oldPathCount));
    check(_TStreamCacheClip_isFinish(&headData));
clear:
    if (decompresser.decompressHandle){
        decompressPlugin->close(decompressPlugin,decompresser.decompressHandle);
        decompresser.decompressHandle=0;
    }
    return result;
}


hpatch_BOOL TDirPatcher_patch(TDirPatcher* self,const hpatch_TStreamOutput* out_newData,
                              const hpatch_TStreamInput* oldData,
                              TByte* temp_cache,TByte* temp_cache_end){
    TStreamInputClip hdiffData;
    TStreamInputClip_init(&hdiffData,self->_dirDiffData,self->dirDiffHead.hdiffDataOffset,
                          self->dirDiffHead.hdiffDataOffset+self->dirDiffHead.hdiffDataSize);
    return patch_decompress_with_cache(out_newData,oldData,&hdiffData.base,
                                       self->_decompressPlugin,temp_cache,temp_cache_end);
}

void TDirPatcher_close(TDirPatcher* self){
    if (self->_pmem){
        free(self->_pmem);
        self->_pmem=0;
    }
}
