//hpatch_dir_listener.h
// patch dir listener
//
/*
 This is the HDiffPatch copyright.

 Copyright (c) 2018-2019 HouSisong All Rights Reserved.

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
#ifndef HPatch_dir_listener_h
#define HPatch_dir_listener_h
#include "file_for_patch.h"
#include "dirDiffPatch/dir_patch/dir_patch.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct IHPatchDirListener {
    IDirPatchListener base;
    void*       listenerImport;
    hpatch_BOOL (*patchBegin) (struct IHPatchDirListener* listener,TDirPatcher* dirPatcher);
    hpatch_BOOL (*patchFinish)(struct IHPatchDirListener* listener,hpatch_BOOL isPatchSuccess);
} IHPatchDirListener;


//IDirPatchListener
static hpatch_BOOL _makeNewDir(IDirPatchListener* listener,const char* newDir){
    return hpatch_makeNewDir(newDir);
}
static hpatch_BOOL _copySameFile(IDirPatchListener* listener,const char* oldFileName,
                                 const char* newFileName,hpatch_ICopyDataListener* copyListener){
    return TDirPatcher_copyFile(oldFileName,newFileName,copyListener);
}
static hpatch_BOOL _openNewFile(IDirPatchListener* listener,hpatch_TFileStreamOutput*  out_curNewFile,
                                const char* newFileName,hpatch_StreamPos_t newFileSize){
    return hpatch_TFileStreamOutput_open(out_curNewFile,newFileName,newFileSize);
}
static hpatch_BOOL _closeNewFile(IDirPatchListener* listener,hpatch_TFileStreamOutput* curNewFile){
    return hpatch_TFileStreamOutput_close(curNewFile);
}
//IHPatchDirListener
static hpatch_BOOL _dirPatchBegin(IHPatchDirListener* listener,TDirPatcher* dirPatcher){
    listener->listenerImport=dirPatcher;
    return hpatch_TRUE;
}
static hpatch_BOOL _dirPatchFinish(IHPatchDirListener* listener,hpatch_BOOL isPatchSuccess){
    TDirPatcher* dirPatcher=(TDirPatcher*)listener->listenerImport;
    {//ExecuteFile
        size_t i;
        size_t count=TDirPatcher_getNewExecuteFileCount(dirPatcher);
        for (i=0; i<count; ++i) {
            const char* executeFileName=TDirPatcher_getNewExecuteFileByIndex(dirPatcher,i);
            if (!hpatch_setIsExecuteFile(executeFileName)){
                printf("WARNING: can't set Execute tag to new file \"");
                hpatch_printPath_utf8(executeFileName); printf("\"\n");
            }
        }
    }
    return hpatch_TRUE;
}

static IHPatchDirListener defaultPatchDirlistener={{0,_makeNewDir,_copySameFile,_openNewFile,_closeNewFile},
                                                    0,_dirPatchBegin,_dirPatchFinish};

    

//IDirPatchListener
static hpatch_BOOL _tempDir_copySameFile(IDirPatchListener* listener,const char* oldFileName,
                                         const char* newFileName,hpatch_ICopyDataListener* copyListener){
    //checksum same file
    //not copy now
    if (copyListener==0) return hpatch_TRUE;
    return TDirPatcher_readFile(oldFileName,copyListener);
}
//IHPatchDirListener
static hpatch_BOOL _tempDirPatchBegin(IHPatchDirListener* self,TDirPatcher* dirPatcher){
    self->listenerImport=dirPatcher;
    assert(dirPatcher->dirDiffInfo.oldPathIsDir&&dirPatcher->dirDiffInfo.newPathIsDir);
    return hpatch_TRUE;
}
    
    static hpatch_BOOL _isPathNotExist(const char* pathName){
        hpatch_TPathType type;
        if (pathName==0) return hpatch_FALSE;
        if (!hpatch_getPathStat(pathName,&type,0)) return hpatch_FALSE;
        return (kPathType_notExist==type);
    }
    static hpatch_BOOL _tryRemovePath(const char* pathName){
        if (pathName==0) return hpatch_TRUE;
        if (_isPathNotExist(pathName)) return hpatch_TRUE;
        if (hpatch_getIsDirName(pathName))
            return hpatch_removeDir(pathName);
        else
            return hpatch_removeFile(pathName);
    }
static hpatch_BOOL _tempDirPatchFinish(IHPatchDirListener* self,hpatch_BOOL isPatchSuccess){
    hpatch_BOOL  result=hpatch_TRUE;
    TDirPatcher* dirPatcher=(TDirPatcher*)self->listenerImport;
    size_t       i;
    hpatch_BOOL  isInitSameRefError=isPatchSuccess?(!TDirPatcher_initOldSameRefCount(dirPatcher)):hpatch_FALSE;
    if (isInitSameRefError)
        result=hpatch_FALSE;
    if (isPatchSuccess && (!isInitSameRefError)){
        //move(+ some must copy) same to newTempDir from oldDir;
        for (i=dirPatcher->dirDiffHead.sameFilePairCount; i>0; --i) {
            size_t sameIndex=i-1;
            const char* oldPath;
            const char* newPath=TDirPatcher_getNewPathBySameIndex(dirPatcher,sameIndex);
            if (newPath==0) { result=hpatch_FALSE; continue; }
            oldPath=TDirPatcher_getOldPathBySameIndex(dirPatcher,sameIndex);
            if (oldPath==0) { result=hpatch_FALSE; continue; }
            if (TDirPatcher_oldSameRefCount(dirPatcher,sameIndex)>1){//copy old to new
                if (!TDirPatcher_copyFile(oldPath,newPath,0)){
                    result=hpatch_FALSE;
                    fprintf(stderr,"can't copy new file to newTempDir from same old file \"");
                    hpatch_printStdErrPath_utf8(newPath); fprintf(stderr,"\"  ERROR!\n");
                }
            }else{
                if (!hpatch_moveFile(oldPath,newPath)){//move old to new
                    result=hpatch_FALSE;
                    fprintf(stderr,"can't move new file to newTempDir from same old file \"");
                    hpatch_printStdErrPath_utf8(newPath); fprintf(stderr,"\"  ERROR!\n");
                }
            }
            TDirPatcher_decOldSameRefCount(dirPatcher,sameIndex);
        }
        TDirPatcher_finishOldSameRefCount(dirPatcher);
        
        //delete file in oldPathList; //WARNING
        //delete dir in oldPathList; //not check
        for (i=dirPatcher->dirDiffHead.oldPathCount; i>0; --i) {
            size_t oldPathIndex=i-1;
            const char* oldPath=TDirPatcher_getOldPathByIndex(dirPatcher,oldPathIndex);
            if (oldPath==0) continue;
            if (!hpatch_getIsDirName(oldPath)){
                if (!_tryRemovePath(oldPath)){
                    printf("WARNING: can't remove old file \"");
                    hpatch_printPath_utf8(oldPath); printf("\"\n");
                }
            }else{
                hpatch_removeDir(oldPath);
            }
        }
        
        //move all files and dir in newTempDir to oldDir;
        for (i=0; i<dirPatcher->dirDiffHead.newPathCount; ++i) {//make dir to old
            size_t newPathIndex=i;
            const char* newPath=TDirPatcher_getNewPathByIndex(dirPatcher,newPathIndex);
            if (newPath==0) { result=hpatch_FALSE; continue; }
            if (hpatch_getIsDirName(newPath)){
                const char* oldDir=TDirPatcher_getOldPathByNewPath(dirPatcher,newPath);
                if (oldDir==0) { result=hpatch_FALSE; continue; }
                if (!hpatch_makeNewDir(oldDir)) { result=hpatch_FALSE; continue; }
            }
        }
        for (i=dirPatcher->dirDiffHead.newPathCount; i>0; --i) {//move files to old and remove dir
            size_t newPathIndex=i-1;
            const char* newPath=TDirPatcher_getNewPathByIndex(dirPatcher,newPathIndex);
            if (newPath==0) { result=hpatch_FALSE; continue; }
            if (hpatch_getIsDirName(newPath)){
                hpatch_removeDir(newPath);
            }else{
                const char* oldPath=TDirPatcher_getOldPathByNewPath(dirPatcher,newPath);
                if (oldPath==0) { result=hpatch_FALSE; continue; }
                hpatch_removeFile(oldPath);//overwrite
                if (!hpatch_moveFile(newPath,oldPath)){//move new to old
                    result=hpatch_FALSE;
                    fprintf(stderr,"can't move new file to oldDirectory \"");
                    hpatch_printStdErrPath_utf8(newPath); fprintf(stderr,"\"  ERROR!\n");
                    continue;
                }
            }
        }
        {//ExecuteFile
            size_t i;
            size_t count=TDirPatcher_getNewExecuteFileCount(dirPatcher);
            for (i=0; i<count; ++i) {
                const char* executeFileName_new=TDirPatcher_getNewExecuteFileByIndex(dirPatcher,i);
                const char* executeFileName=TDirPatcher_getOldPathByNewPath(dirPatcher,executeFileName_new);
                if (!hpatch_setIsExecuteFile(executeFileName)){
                    printf("WARNING: can't set Execute tag to new file \"");
                    hpatch_printPath_utf8(executeFileName); printf("\"\n");
                }
            }
        }
    }
    
    {   //remove all temp file and dir
        for (i=dirPatcher->dirDiffHead.newPathCount; i>0; --i) {
            size_t newPathIndex=i-1;
            const char* newPath=TDirPatcher_getNewPathByIndex(dirPatcher,newPathIndex);
            _tryRemovePath(newPath);
        }
        {//check remove newTempDir result
            const char* newTempDir=TDirPatcher_getNewPathRoot(dirPatcher);
            result=result && _isPathNotExist(newTempDir);
        }
    }
    return result;
}

    
// 1. patch new ref to newTempDir,
//    make new dir to newTempDir
//    checksum same file
// 2. if patch ok then  {
//        move(+ some must copy) same to newTempDir from oldDir;
//        delete file in oldPathList; //WARNING
//        delete dir in oldPathList; //not check
//        move all files and dir in newTempDir to oldDir;
//        delete newTempDir; }
//    if patch error then  {
//        delelte all in newTempDir;//not check
//        delete newTempDir; }
static IHPatchDirListener tempDirPatchListener={{&tempDirPatchListener,_makeNewDir,_tempDir_copySameFile,
                                                   _openNewFile,_closeNewFile},
                                                 0,_tempDirPatchBegin,_tempDirPatchFinish};
    
#ifdef __cplusplus
}
#endif
#endif
