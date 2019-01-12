// dir_patch.c
// hdiffz dir diff
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
#include "dir_patch.h"
#include <stdio.h>
#include <string.h>
#include "../../libHDiffPatch/HPatch/patch.h"
#include "../../libHDiffPatch/HPatch/patch_private.h"
#include "../../file_for_patch.h"
#include "../file_for_dirPatch.h"

static const char* kVersionType="DirDiff19&";
static const TByte kPatchMode =0;

#define TUInt hpatch_StreamPos_t

#define  check(value) { if (!(value)){ fprintf(stderr,#value" error!\n");  \
                                       result=hpatch_FALSE; goto clear; } }

#define unpackUIntTo(puint,sclip) \
    check(_TStreamCacheClip_unpackUIntWithTag(sclip,puint,0))


#define unpackToSize(psize,sclip) { \
    TUInt v; check(_TStreamCacheClip_unpackUIntWithTag(sclip,&v,0)); \
    if (sizeof(TUInt)!=sizeof(size_t)) check(v==(size_t)v); \
    *(psize)=(size_t)v; }


char* pushDirPath(char* out_path,char* out_pathEnd,const char* rootDir){
    char*          result=0; //false
    size_t rootDirLen=strlen(rootDir);
    hpatch_BOOL isNeedDirSeparator=(rootDirLen>0)&&(rootDir[rootDirLen-1]!=kPatch_dirSeparator);
    check((rootDirLen+isNeedDirSeparator+1)<=(size_t)(out_pathEnd-out_path));
    memcpy(out_path,rootDir,rootDirLen);
    out_path+=rootDirLen;
    if (isNeedDirSeparator) *out_path++=kPatch_dirSeparator;
    *out_path='\0'; //C string end
    result=out_path;
clear:
    return result;
}
static hpatch_BOOL getPath(char* out_pathBegin,char* out_pathBufEnd,const char* utf8fileName){
    hpatch_BOOL          result=hpatch_TRUE;
    size_t utf8fileNameSize=strlen(utf8fileName);
    check(utf8fileNameSize+1<=(size_t)(out_pathBufEnd-out_pathBegin));
    memcpy(out_pathBegin,utf8fileName,utf8fileNameSize+1);
clear:
    return result;
}

static void formatDirTagForLoad(char* utf8_path,char* utf8_pathEnd){
    if (kPatch_dirSeparator==kPatch_dirSeparator_saved) return;
    for (;utf8_path<utf8_pathEnd;++utf8_path){
        if ((*utf8_path)!=kPatch_dirSeparator_saved)
            continue;
        else
            *utf8_path=kPatch_dirSeparator;
    }
}

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
    TStreamCacheClip  _headClip;
    TStreamCacheClip* headClip=&_headClip;
    TByte             temp_cache[hpatch_kStreamCacheSize];
    char savedCompressType[hpatch_kMaxCompressTypeLength+1];
    check(out_info!=0);
    out_info->isDirDiff=hpatch_FALSE;
    
    _TStreamCacheClip_init(headClip,dirDiffFile,0,dirDiffFile->streamSize,
                           temp_cache,hpatch_kStreamCacheSize);
    
    {//VersionType
        const TByte* versionType;
        const size_t kVersionTypeLen=strlen(kVersionType);
        if (dirDiffFile->streamSize<kVersionTypeLen)
            return result;//not is dirDiff data
        //assert(tagSize<=hpatch_kStreamCacheSize);
        versionType=_TStreamCacheClip_readData(headClip,kVersionTypeLen);
        check(versionType!=0);
        if (0!=memcmp(versionType,kVersionType,kVersionTypeLen))
            return result;//not is dirDiff data
        out_info->isDirDiff=hpatch_TRUE;
    }
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
        
        unpackToSize(&out_head->oldPathCount,headClip);
        unpackToSize(&out_head->newPathCount,headClip);
        unpackToSize(&out_head->oldPathSumSize,headClip);
        unpackToSize(&out_head->newPathSumSize,headClip);
        unpackToSize(&out_head->oldRefFileCount,headClip);
        unpackToSize(&out_head->newRefFileCount,headClip);
        unpackToSize(&out_head->sameFilePairCount,headClip);
        unpackUIntTo(&out_head->headDataSize,headClip);
        unpackUIntTo(&out_head->headDataCompressedSize,headClip);
        out_info->dirDataIsCompressed=(out_head->headDataCompressedSize>0);
        unpackUIntTo(&out_info->externDataSize,headClip);
    }
    {
        TStreamInputClip hdiffStream;
        TUInt curPos=headClip->streamPos-_TStreamCacheClip_cachedSize(headClip);
        out_head->headDataOffset=curPos;
        curPos+=(out_head->headDataCompressedSize>0)?out_head->headDataCompressedSize:out_head->headDataSize;
        out_info->externDataOffset=curPos;
        curPos+=out_info->externDataSize;
        out_head->hdiffDataOffset=curPos;
        out_head->hdiffDataSize=dirDiffFile->streamSize-curPos;
        TStreamInputClip_init(&hdiffStream,dirDiffFile,out_head->hdiffDataOffset,
                              out_head->hdiffDataOffset+out_head->hdiffDataSize);
        check(getCompressedDiffInfo(&out_info->hdiffInfo,&hdiffStream.base));
        check(0==strcmp(savedCompressType,out_info->hdiffInfo.compressType));
    }
clear:
    return result;
}

hpatch_BOOL getDirDiffInfo(const hpatch_TStreamInput* diffFile,TDirDiffInfo* out_info){
    _TDirDiffHead head;
    return _read_dirdiff_head(out_info,&head,diffFile);
}


hpatch_BOOL TDirPatcher_open(TDirPatcher* self,const hpatch_TStreamInput* dirDiffData,
                             const TDirDiffInfo** out_dirDiffInfo){
    hpatch_BOOL result;
    assert(self->_dirDiffData==0);
    self->_dirDiffData=dirDiffData;
    result=_read_dirdiff_head(&self->dirDiffInfo,&self->dirDiffHead,self->_dirDiffData);
    if (result)
        *out_dirDiffInfo=&self->dirDiffInfo;
    return result;
}


static hpatch_BOOL _TStreamCacheClip_readDataTo(TStreamCacheClip* sclip,TByte* out_buf,TByte* bufEnd){
    const size_t maxReadLen=sclip->cacheEnd;
    while (out_buf<bufEnd) {
        const TByte* pdata;
        size_t readLen=bufEnd-out_buf;
        if (readLen>maxReadLen) readLen=maxReadLen;
        pdata=_TStreamCacheClip_readData(sclip,readLen);
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

static hpatch_BOOL readIncListTo(TStreamCacheClip* sclip,size_t* out_list,size_t count,size_t check_endValue){
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

static hpatch_BOOL readListTo(TStreamCacheClip* sclip,hpatch_StreamPos_t* out_list,size_t count){
    hpatch_BOOL result=hpatch_TRUE;
    size_t i;
    for (i=0; i<count; ++i) {
        check(_TStreamCacheClip_unpackUIntWithTag(sclip,&out_list[i],0));
    }
clear:
    return result;
}


static hpatch_BOOL readSamePairListTo(TStreamCacheClip* sclip,size_t* out_pairList,size_t pairCount,
                                      size_t check_endXValue,size_t check_endYValue){
    hpatch_BOOL result=hpatch_TRUE;
    TUInt backXValue=~(TUInt)0;
    TUInt backYValue=~(TUInt)0;
    size_t i;
    for (i=0; i<pairCount;i+=2) {
        const TByte*  psign;
        TUInt incYValue;
        TUInt incXValue;
        check(_TStreamCacheClip_unpackUIntWithTag(sclip,&incXValue,0));
        backXValue+=1+incXValue;
        check(backXValue<check_endXValue);
        out_pairList[i+0]=(size_t)backXValue;
        
        psign=_TStreamCacheClip_accessData(sclip,1);
        check(psign!=0);
        check(_TStreamCacheClip_unpackUIntWithTag(sclip,&incYValue,1));
        if (!(*psign)>>(8-1))
            backYValue+=1+incYValue;
        else
            backYValue=backYValue+1-incYValue;
        check(backYValue<check_endYValue);
        out_pairList[i+1]=(size_t)backYValue;
    }
clear:
    return result;
}

hpatch_BOOL TDirPatcher_loadDirData(TDirPatcher* self,hpatch_TDecompress* decompressPlugin){
    hpatch_BOOL result=hpatch_TRUE;
    size_t               memSize=0;
    TByte*               curMem=0;
    const _TDirDiffHead* head=&self->dirDiffHead;
    const size_t         pathSumSize=head->oldPathSumSize+head->newPathSumSize;
    TStreamCacheClip        headStream;
    _TDecompressInputSteram decompresser;
    decompresser.decompressHandle=0;
    assert(self->_decompressPlugin==0);
    assert(self->_pDiffDataMem==0);
    self->_decompressPlugin=decompressPlugin;
    
    //dir head data clip stream
    {
        TByte temp_cache[hpatch_kStreamCacheSize];
        hpatch_StreamPos_t curPos=self->dirDiffHead.headDataOffset;
        check(getStreamClip(&headStream,&decompresser,self->dirDiffHead.headDataSize,
                            self->dirDiffHead.headDataCompressedSize,self->_dirDiffData,&curPos,
                            decompressPlugin,temp_cache,hpatch_kStreamCacheSize));
    }
    //mem
    memSize = (head->oldPathCount+head->newPathCount)*sizeof(const char*)
            + (head->oldRefFileCount+head->newRefFileCount+head->sameFilePairCount*2)*sizeof(size_t)
            + (head->newRefFileCount)*sizeof(hpatch_StreamPos_t) + pathSumSize;
    self->_pDiffDataMem=malloc(memSize);
    check(self->_pDiffDataMem!=0);
    curMem=self->_pDiffDataMem;
    self->newRefSizeList=(hpatch_StreamPos_t*)curMem; curMem+=head->newRefFileCount*sizeof(hpatch_StreamPos_t);
    self->oldUtf8PathList=(const char**)curMem; curMem+=head->oldPathCount*sizeof(const char*);
    self->newUtf8PathList=(const char**)curMem; curMem+=head->newPathCount*sizeof(const char*);
    self->oldRefList=(const size_t*)curMem; curMem+=head->oldRefFileCount*sizeof(size_t);
    self->newRefList=(const size_t*)curMem; curMem+=head->newRefFileCount*sizeof(size_t);
    self->dataSamePairList=(const size_t*)curMem; curMem+=head->sameFilePairCount*2*sizeof(size_t);
    //read old & new path List
    check(_TStreamCacheClip_readDataTo(&headStream,curMem,curMem+pathSumSize));
    formatDirTagForLoad((char*)curMem,(char*)curMem+pathSumSize);
    curMem+=pathSumSize;
    assert(curMem-memSize==self->_pDiffDataMem);
    check(clipCStrsTo((const char*)curMem-pathSumSize,(const char*)curMem,
                      (const char**)self->oldUtf8PathList,head->oldPathCount+head->newPathCount));
    //read oldRefList
    check(readIncListTo(&headStream,(size_t*)self->oldRefList,head->oldRefFileCount,head->oldPathCount));
    //read newRefList
    check(readIncListTo(&headStream,(size_t*)self->newRefList,head->newRefFileCount,head->newPathCount));
    //read newRefSizeList
    check(readListTo(&headStream,(hpatch_StreamPos_t*)self->newRefSizeList,head->newRefFileCount));
    //read dataSamePairList
    check(readSamePairListTo(&headStream,(size_t*)self->dataSamePairList,head->sameFilePairCount,
                             head->newPathCount,head->oldPathCount));
    check(_TStreamCacheClip_isFinish(&headStream));
clear:
    if (decompresser.decompressHandle){
        if (!decompressPlugin->close(decompressPlugin,decompresser.decompressHandle))
            result=hpatch_FALSE;
        decompresser.decompressHandle=0;
    }
    return result;
}


hpatch_BOOL  _openRes(IResHandle* res,hpatch_TStreamInput** out_stream){
    hpatch_BOOL  result=hpatch_TRUE;
    TDirPatcher*        self=(TDirPatcher*)res->resImport;
    size_t              resIndex=res-self->_resList;
    TFileStreamInput*   file=self->_fileList+resIndex;
    const char*         utf8fileName=0;
    assert(resIndex<self->dirDiffHead.oldRefFileCount);
    assert(file->m_file==0);
    utf8fileName=self->oldUtf8PathList[self->oldRefList[resIndex]];
    check(getPath(self->_oldRootDir_end,self->_oldRootDir_bufEnd,utf8fileName));
    check(TFileStreamInput_open(file,self->_oldRootDir));
    *out_stream=&file->base;
clear:
    return result;
}
hpatch_BOOL _closeRes(IResHandle* res,const hpatch_TStreamInput* stream){
    hpatch_BOOL  result=hpatch_TRUE;
    TDirPatcher*        self=(TDirPatcher*)res->resImport;
    size_t              resIndex=res-self->_resList;
    TFileStreamInput*   file=self->_fileList+resIndex;
    assert(resIndex<self->dirDiffHead.oldRefFileCount);
    assert(file->m_file!=0);
    assert(stream==&file->base);
    check(TFileStreamInput_close(file));
clear:
    return result;
}

hpatch_BOOL TDirPatcher_openOldRefAsStream(TDirPatcher* self,const char* oldRootDir_utf8,
                                           size_t kMaxOpenFileCount,
                                           const hpatch_TStreamInput** out_oldRefStream){
    hpatch_BOOL result=hpatch_TRUE;
    size_t      refCount=self->dirDiffHead.oldRefFileCount;
    size_t      memSize=0;
    size_t      i;
    size_t              maxFileNameLen=0;
    size_t              oldRootDirLen=strlen(oldRootDir_utf8);
    hpatch_StreamPos_t  sumFSize=0;
    assert(kMaxOpenFileCount>=kMaxOpenFileCount_limit_min);
    kMaxOpenFileCount-=2;// for diffFile and one newFile
    //mem
    assert(self->_pOldRefMem==0);
    for (i=0;i<refCount;++i) {
        const char* utf8fileName=self->oldUtf8PathList[self->oldRefList[i]];
        size_t len=strlen(utf8fileName); //'\0'
        if (len<=maxFileNameLen) continue; else maxFileNameLen=len;
    }
    memSize=oldRootDirLen+2+maxFileNameLen+1; //'/'+'\0'+'\0'
    memSize+=(sizeof(IResHandle)+sizeof(TFileStreamInput))*refCount;
    self->_pOldRefMem=malloc(memSize);
    check(self->_pOldRefMem!=0);
    self->_resList=(IResHandle*)self->_pOldRefMem;
    self->_fileList=(TFileStreamInput*)&self->_resList[refCount];
    self->_oldRootDir=(char*)&self->_fileList[refCount];
    self->_oldRootDir_end=pushDirPath(self->_oldRootDir,self->_oldRootDir+oldRootDirLen+2,oldRootDir_utf8);
    check(0!=self->_oldRootDir_end);
    self->_oldRootDir_bufEnd=self->_oldRootDir_end+1+maxFileNameLen+1;
    assert(1>=(size_t)(((char*)self->_pOldRefMem+memSize)-self->_oldRootDir_bufEnd));
    
    //init
    memset(self->_resList,0,sizeof(IResHandle)*refCount);
    memset(self->_fileList,0,sizeof(TFileStreamInput)*refCount);
    for (i=0; i<refCount;++i){
        TPathType          ftype;
        hpatch_StreamPos_t fileSize;
        const char* utf8fileName=self->oldUtf8PathList[self->oldRefList[i]];
        check(getPath(self->_oldRootDir_end,self->_oldRootDir_bufEnd,utf8fileName));
        check(getPathType(self->_oldRootDir,&ftype,&fileSize));
        check(ftype==kPathType_file)
        self->_resList[i].resImport=self;
        self->_resList[i].resStreamSize=fileSize;
        self->_resList[i].open=_openRes;
        self->_resList[i].close=_closeRes;
        sumFSize+=fileSize;
    }
    check(sumFSize==self->dirDiffInfo.hdiffInfo.oldDataSize);
    //open
    check(TResHandleLimit_open(&self->_resLimit,kMaxOpenFileCount,self->_resList,refCount));
    check(TRefStream_open(&self->_oldRefStream,self->_resLimit.streamList,self->_resLimit.streamCount));
    *out_oldRefStream=self->_oldRefStream.stream;
clear:
    return result;
}

hpatch_BOOL TDirPatcher_closeOldRefStream(TDirPatcher* self){
    hpatch_BOOL result;
    TRefStream_close(&self->_oldRefStream);
    result=TResHandleLimit_close(&self->_resLimit);
    self->_oldRootDir=0;
    self->_oldRootDir_end=0;
    self->_oldRootDir_bufEnd=0;
    self->_resList=0;
    self->_fileList=0;
    if (self->_pOldRefMem){
        free(self->_pOldRefMem);
        self->_pOldRefMem=0;
    }
    return result;
}


static hpatch_BOOL _makeNewDir(INewStreamListener* listener,size_t newPathIndex){
    TDirPatcher* self=(TDirPatcher*)listener->listenerImport;
    //return self->_listener->makeNewDir();
    //todo:
    return hpatch_FALSE;
}
static hpatch_BOOL _copySameFile(INewStreamListener* listener,size_t oldPathIndex,size_t newPathIndex){
    TDirPatcher* self=(TDirPatcher*)listener->listenerImport;
    //return self->_listener->copySameFile();
    //todo:
    return hpatch_FALSE;
}
static hpatch_BOOL _openNewFile(INewStreamListener* listener,size_t newPathIndex,
                                const hpatch_TStreamOutput** out_newFileStream){
    TDirPatcher* self=(TDirPatcher*)listener->listenerImport;
    //todo:
    // TFileStreamInput_open(&hhhh,fileName));
    return hpatch_FALSE;
}
static hpatch_BOOL _closeNewFile(INewStreamListener* listener,const hpatch_TStreamOutput* newFileStream){
    TDirPatcher* self=(TDirPatcher*)listener->listenerImport;
    //todo:
    //TFileStreamInput_close
    return hpatch_FALSE;
}

hpatch_BOOL TDirPatcher_openNewDirAsStream(TDirPatcher* self,const char* newRootDir,IDirPatchListener* listener,
                                           const hpatch_TStreamOutput** out_newDirStream){
    assert(self->_newDirStream.stream==0);
    assert(self->_nsListener.listenerImport==0);
    self->_nsListener.listenerImport=self;
    self->_nsListener.makeNewDir=_makeNewDir;
    self->_nsListener.copySameFile=_copySameFile;
    self->_nsListener.openNewFile=_openNewFile;
    self->_nsListener.closeNewFile=_closeNewFile;
    return TNewStream_open(&self->_newDirStream,&self->_nsListener,self->dirDiffInfo.hdiffInfo.newDataSize,
                           self->dirDiffHead.newPathCount,self->newRefList,self->dirDiffHead.newRefFileCount,
                           self->dataSamePairList,self->dirDiffHead.sameFilePairCount);
}

hpatch_BOOL TDirPatcher_closeNewDirStream(TDirPatcher* self){
    return TNewStream_close(&self->_newDirStream);
}

hpatch_BOOL TDirPatcher_patch(const TDirPatcher* self,const hpatch_TStreamOutput* out_newData,
                              const hpatch_TStreamInput* oldData,
                              TByte* temp_cache,TByte* temp_cache_end){
    TStreamInputClip hdiffData;
    TStreamInputClip_init(&hdiffData,self->_dirDiffData,self->dirDiffHead.hdiffDataOffset,
                          self->dirDiffHead.hdiffDataOffset+self->dirDiffHead.hdiffDataSize);
    return patch_decompress_with_cache(out_newData,oldData,&hdiffData.base,
                                       self->_decompressPlugin,temp_cache,temp_cache_end);
}

hpatch_BOOL TDirPatcher_close(TDirPatcher* self){
    hpatch_BOOL result=TDirPatcher_closeNewDirStream(self);
    result=TDirPatcher_closeOldRefStream(self) & result;
    if (self->_pDiffDataMem){
        free(self->_pDiffDataMem);
        self->_pDiffDataMem=0;
    }
    return result;
}
