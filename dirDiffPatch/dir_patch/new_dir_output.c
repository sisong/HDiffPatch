// new_dir_output.c
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
#include "new_dir_output.h"
#include "../../file_for_patch.h"
#if (_IS_NEED_DIR_DIFF_PATCH)
#include "dir_patch_tools.h"
#include <stdio.h>
#include <string.h>

#define TUInt hpatch_StreamPos_t

#define  check(value) { if (!(value)){ fprintf(stderr,"check "#value" error!\n");  \
                                       result=hpatch_FALSE; goto clear; } }

static hpatch_BOOL _TDirPatcher_copyFile(const char* oldFileName_utf8,const char* newFileName_utf8,
                                         hpatch_ICopyDataListener* copyListener){
#define _tempCacheSize (hpatch_kStreamCacheSize*4)
    hpatch_BOOL result=hpatch_TRUE;
    TByte        temp_cache[_tempCacheSize];
    hpatch_StreamPos_t pos=0;
    hpatch_TFileStreamInput   oldFile;
    hpatch_TFileStreamOutput  newFile;
    hpatch_TFileStreamInput_init(&oldFile);
    hpatch_TFileStreamOutput_init(&newFile);
    
    check(hpatch_TFileStreamInput_open(&oldFile,oldFileName_utf8));
    if (newFileName_utf8)
        check(hpatch_TFileStreamOutput_open(&newFile,newFileName_utf8,oldFile.base.streamSize));
    while (pos<oldFile.base.streamSize) {
        size_t copyLen=_tempCacheSize;
        if (pos+copyLen>oldFile.base.streamSize)
            copyLen=(size_t)(oldFile.base.streamSize-pos);
        check(oldFile.base.read(&oldFile.base,pos,temp_cache,temp_cache+copyLen));
        if (newFileName_utf8)
            check(newFile.base.write(&newFile.base,pos,temp_cache,temp_cache+copyLen));
        if (copyListener)
            copyListener->copyedData(copyListener,temp_cache,temp_cache+copyLen);
        pos+=copyLen;
    }
    check(!oldFile.fileError);
    check(!newFile.fileError);
    if (newFileName_utf8)
        check(newFile.out_length==newFile.base.streamSize);
clear:
    hpatch_TFileStreamOutput_close(&newFile);
    hpatch_TFileStreamInput_close(&oldFile);
    return result;
#undef _tempCacheSize
}

hpatch_BOOL TDirPatcher_copyFile(const char* oldFileName_utf8,const char* newFileName_utf8,
                                 hpatch_ICopyDataListener* copyListener){
    hpatch_BOOL result=hpatch_TRUE;
    check(newFileName_utf8!=0);
    result=_TDirPatcher_copyFile(oldFileName_utf8,newFileName_utf8,copyListener);
clear:
    return result;
}
hpatch_BOOL TDirPatcher_readFile(const char* oldFileName_utf8,hpatch_ICopyDataListener* copyListener){
    return _TDirPatcher_copyFile(oldFileName_utf8,0,copyListener);
}

static hpatch_BOOL _makeNewDirOrEmptyFile(hpatch_INewStreamListener* listener,size_t newPathIndex){
    hpatch_BOOL  result=hpatch_TRUE;
    TNewDirOutput* self=(TNewDirOutput*)listener->listenerImport;
    const char*  pathName=TNewDirOutput_getNewPathByIndex(self,newPathIndex);
    check(pathName!=0);
    if (hpatch_getIsDirName(pathName)){ //
        check(self->_listener->makeNewDir(self->_listener,pathName));
    }else{ //empty file
        check(self->_listener->openNewFile(self->_listener,self->_curNewFile,pathName,0));
        check(!self->_curNewFile->fileError);
        check(self->_listener->closeNewFile(self->_listener,self->_curNewFile));
    }
clear:
    return result;
}

static const char* _getOldPathByIndex(TNewDirOutput* self,size_t oldPathIndex){
    assert(self->_oldPathListener.getOldPathByIndex!=0);
    return self->_oldPathListener.getOldPathByIndex(&self->_oldPathListener,oldPathIndex);
}

static hpatch_BOOL _copySameFile(hpatch_INewStreamListener* listener,size_t newPathIndex,size_t oldPathIndex){
    hpatch_BOOL  result=hpatch_TRUE;
    TNewDirOutput* self=(TNewDirOutput*)listener->listenerImport;
    const char*  newFileName=0;
    const char*  oldFileName=0;
    assert(newPathIndex<self->newPathCount);
    newFileName=TNewDirOutput_getNewPathByIndex(self,newPathIndex);
    check(newFileName!=0);
    check(self->_oldPathListener.getOldPathByIndex!=0);
    oldFileName=_getOldPathByIndex(self,oldPathIndex);
    check(oldFileName!=0);
    check(self->_listener->copySameFile(self->_listener,oldFileName,newFileName,
                                        self->isCheck_copyFileData?(&self->_sameFileCopyListener):0));
clear:
    return result;
}

static hpatch_BOOL _openNewFile(hpatch_INewStreamListener* listener,size_t newRefIndex,
                                const hpatch_TStreamOutput** out_newFileStream){
    hpatch_BOOL  result=hpatch_TRUE;
    TNewDirOutput* self=(TNewDirOutput*)listener->listenerImport;
    const char*  utf8fileName=0;
    hpatch_StreamPos_t fileSize=self->newRefSizeList[newRefIndex];
    assert(newRefIndex<self->newRefFileCount);
    assert(self->_curNewFile->m_file==0);
    if (fileSize==0){
        size_t newPathIndex=self->newRefList?self->newRefList[newRefIndex]:newRefIndex;
        check(_makeNewDirOrEmptyFile(listener,newPathIndex));
        *out_newFileStream=0;
    }else{
        utf8fileName=TNewDirOutput_getNewPathByRefIndex(self,newRefIndex);
        check(utf8fileName!=0);
        check(self->_listener->openNewFile(self->_listener,self->_curNewFile,
                                           utf8fileName,fileSize));
        *out_newFileStream=&self->_curNewFile->base;
    }
clear:
    return result;
}
static hpatch_BOOL _closeNewFile(hpatch_INewStreamListener* listener,const hpatch_TStreamOutput* newFileStream){
    hpatch_BOOL  result=hpatch_TRUE;
    TNewDirOutput* self=(TNewDirOutput*)listener->listenerImport;
    assert(self->_curNewFile!=0);
    assert(newFileStream==&self->_curNewFile->base);
    check(!self->_curNewFile->fileError);
    check(self->_listener->closeNewFile(self->_listener,self->_curNewFile));
    hpatch_TFileStreamOutput_init(self->_curNewFile);
clear:
    return result;
}

static void _writedNewRefData(struct hpatch_INewStreamListener* listener,const unsigned char* data,
                             const unsigned char* dataEnd){
    TNewDirOutput* self=(TNewDirOutput*)listener->listenerImport;
    if (self->isCheck_newRefData)
        self->_checksumPlugin->append(self->_newRefChecksumHandle,data,dataEnd);
}

static hpatch_BOOL _do_checksumEnd(TNewDirOutput* self,const TByte* checksumTest,TByte* checksumTemp,
                                   hpatch_checksumHandle* pcsHandle){
    size_t checksumByteSize=self->_checksumPlugin->checksumByteSize();
    hpatch_checksumHandle* csHandle=*pcsHandle;
    assert(csHandle!=0);
    *pcsHandle=0;
    self->_checksumPlugin->end(csHandle,checksumTemp,checksumTemp+checksumByteSize);
    self->_checksumPlugin->close(self->_checksumPlugin,csHandle);
    return (0==memcmp(checksumTest,checksumTemp,checksumByteSize));
}

#define _pchecksumNewRef(_self)     ((_self)->_pChecksum_newRefData_copyFileData)
#define _pchecksumCopyFile(_self)   ((_self)->_pChecksum_newRefData_copyFileData+(_self)->checksumByteSize)
#define _pchecksumTemp(_self)       ((_self)->_pChecksum_temp)

static hpatch_BOOL _writedFinish(struct hpatch_INewStreamListener* listener){
    hpatch_BOOL  result=hpatch_TRUE;
    TNewDirOutput* self=(TNewDirOutput*)listener->listenerImport;
    //checksum newRefData end
    if (self->isCheck_newRefData){
        if (!_do_checksumEnd(self,_pchecksumNewRef(self),_pchecksumTemp(self),&self->_newRefChecksumHandle))
            self->_isNewRefDataChecksumError=hpatch_TRUE;
    }
    //checksum sameFileData end
    if (self->isCheck_copyFileData){
        if (!_do_checksumEnd(self,_pchecksumCopyFile(self),_pchecksumTemp(self),&self->_sameFileChecksumHandle))
            self->_isCopyDataChecksumError=hpatch_TRUE;
    }
    check(!self->_isNewRefDataChecksumError);
    check(!self->_isCopyDataChecksumError);
clear:
    return result;
}

static void _sameFile_copyedData(hpatch_ICopyDataListener* listener,const unsigned char* data,
                                 const unsigned char* dataEnd){
    TNewDirOutput* self=(TNewDirOutput*)listener->listenerImport;
    if (self->isCheck_copyFileData)
        self->_checksumPlugin->append(self->_sameFileChecksumHandle,data,dataEnd);
}

hpatch_BOOL TNewDirOutput_openDir(TNewDirOutput* self,IDirPatchListener* listener,
                                  size_t kAlignSize,const hpatch_TStreamOutput** out_newDirStream){
    hpatch_BOOL result=hpatch_TRUE;
    size_t      refCount=self->newRefFileCount;
    assert(self->_pmem==0);
    assert(self->_newDirStream.stream==0);
    assert(self->_newDirStreamListener.listenerImport==0);
    self->_listener=listener;
    {//open new
        size_t      memSize=sizeof(hpatch_TFileStreamOutput);
        self->_pmem=malloc(memSize);
        check(self->_pmem!=0);
        self->_curNewFile=(hpatch_TFileStreamOutput*)self->_pmem;
        hpatch_TFileStreamOutput_init(self->_curNewFile);
        
        self->_newDirStreamListener.listenerImport=self;
        self->_newDirStreamListener.makeNewDirOrEmptyFile=_makeNewDirOrEmptyFile;
        self->_newDirStreamListener.copySameFile=_copySameFile;
        self->_newDirStreamListener.openNewFile=_openNewFile;
        self->_newDirStreamListener.closeNewFile=_closeNewFile;
        self->_newDirStreamListener.writedNewRefData=_writedNewRefData;
        self->_newDirStreamListener.writedFinish=_writedFinish;
    }
    //checksum newRefData begin
    if (self->isCheck_newRefData){
        assert(self->_newRefChecksumHandle==0);
        self->_newRefChecksumHandle=self->_checksumPlugin->open(self->_checksumPlugin);
        check(self->_newRefChecksumHandle!=0);
        self->_checksumPlugin->begin(self->_newRefChecksumHandle);
    }
    //checksum sameFileData begin
    if (self->isCheck_copyFileData){
        assert(self->_sameFileChecksumHandle==0);
        self->_sameFileChecksumHandle=self->_checksumPlugin->open(self->_checksumPlugin);
        check(self->_sameFileChecksumHandle!=0);
        self->_checksumPlugin->begin(self->_sameFileChecksumHandle);
        
        if (self->isCheck_copyFileData){
            self->_sameFileCopyListener.listenerImport=self;
            self->_sameFileCopyListener.copyedData=_sameFile_copyedData;
        }
    }
    check(hpatch_TNewStream_open(&self->_newDirStream,&self->_newDirStreamListener,
                                 self->newDataSize,self->newPathCount,
                                 self->newRefList,refCount,self->dataSamePairList,
                                 self->sameFilePairCount,kAlignSize));
    *out_newDirStream=self->_newDirStream.stream;
clear:
    return result;
}


hpatch_BOOL TNewDirOutput_closeNewDirHandles(TNewDirOutput* self){
    hpatch_BOOL result=hpatch_TNewStream_closeFileHandles(&self->_newDirStream);
    if (self->_curNewFile){
        if (!hpatch_TFileStreamOutput_close(self->_curNewFile))
            result=hpatch_FALSE;
        hpatch_TFileStreamOutput_init(self->_curNewFile);
    }
    return result;
}

hpatch_BOOL TNewDirOutput_close(TNewDirOutput* self){
    hpatch_BOOL result=TNewDirOutput_closeNewDirHandles(self);
    hpatch_TChecksum* checksumPlugin=self->_checksumPlugin;
    if (!hpatch_TNewStream_close(&self->_newDirStream))
        result=hpatch_FALSE;
    self->_newRootDir=0;
    self->_newRootDir_end=0;
    self->_newRootDir_bufEnd=0;
    self->_curNewFile=0;
    if (self->_newRefChecksumHandle){
        checksumPlugin->close(checksumPlugin,self->_newRefChecksumHandle);
        self->_newRefChecksumHandle=0;
    }
    if (self->_sameFileChecksumHandle){
        checksumPlugin->close(checksumPlugin,self->_sameFileChecksumHandle);
        self->_sameFileChecksumHandle=0;
    }
    if (self->_pmem) {
        free(self->_pmem);
        self->_pmem=0;
    }
    return result;
}

const char* TNewDirOutput_getNewPathRoot(TNewDirOutput* self){
    if (!setPath(self->_newRootDir_end,self->_newRootDir_bufEnd,"")) return 0; //error
    return self->_newRootDir;
}

const char* TNewDirOutput_getNewPathByIndex(TNewDirOutput* self,size_t newPathIndex){
    assert(newPathIndex<self->newPathCount);
    if (!setPath(self->_newRootDir_end,self->_newRootDir_bufEnd,
                 self->newUtf8PathList[newPathIndex])) return 0; //error
    return self->_newRootDir;
}

const char* TNewDirOutput_getNewPathByRefIndex(TNewDirOutput* self,size_t newRefIndex){
    size_t newPathIndex;
    assert(newRefIndex<self->newRefFileCount);
    newPathIndex=self->newRefList?self->newRefList[newRefIndex]:newRefIndex;
    return TNewDirOutput_getNewPathByIndex(self,newPathIndex);
}

const char* TNewDirOutput_getNewExecuteFileByIndex(TNewDirOutput* self,size_t newExecuteIndex){
    assert(newExecuteIndex<self->newExecuteCount);
    return TNewDirOutput_getNewPathByIndex(self,self->newExecuteList[newExecuteIndex]);
}

const char* TNewDirOutput_getOldPathBySameIndex(TNewDirOutput* self,size_t sameIndex){
    assert(sameIndex<self->sameFilePairCount);
    assert(self->dataSamePairList!=0);
    return _getOldPathByIndex(self,self->dataSamePairList[sameIndex].oldIndex);
}

const char* TNewDirOutput_getNewPathBySameIndex(TNewDirOutput* self,size_t sameIndex){
    assert(sameIndex<self->sameFilePairCount);
    assert(self->dataSamePairList!=0);
    return TNewDirOutput_getNewPathByIndex(self,self->dataSamePairList[sameIndex].newIndex);
}

#endif
