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
    

    static hpatch_BOOL _dirPatch_setIsExecuteFile(IDirPathList* executeList){
        hpatch_BOOL result=hpatch_TRUE;
        size_t i;
        for (i=0; i<executeList->pathCount; ++i) {
            const char* executeFileName=executeList->getPathNameByIndex(executeList->import,i);
            if (!hpatch_setIsExecuteFile(executeFileName)){
                result=hpatch_FALSE;
                printf("WARNING: can't set Execute tag to new file \"");
                hpatch_printPath_utf8(executeFileName); printf("\"\n");
            }
        }
        return result;
    }

static hpatch_BOOL _dirPatchFinish(IHPatchDirListener* listener,hpatch_BOOL isPatchSuccess){
    TDirPatcher* dirPatcher=(TDirPatcher*)listener->listenerImport;
    IDirPathList executeList;
    if (!isPatchSuccess) return hpatch_TRUE;
    TDirPatcher_getNewExecuteList(dirPatcher,&executeList);
    return _dirPatch_setIsExecuteFile(&executeList);
}

static IHPatchDirListener defaultPatchDirlistener={{0,_makeNewDir,_copySameFile,_openNewFile,_closeNewFile},
                                                    0,_dirPatchBegin,_dirPatchFinish};

    
    static hpatch_BOOL _tryRemovePath(const char* pathName){
        if (pathName==0) return hpatch_TRUE;
        if (hpatch_isPathNotExist(pathName)) return hpatch_TRUE;
        if (hpatch_getIsDirName(pathName))
            return hpatch_removeDir(pathName);
        else
            return hpatch_removeFile(pathName);
    }
    
    typedef const char* (*IDirPathMove_getDstPathBySrcPath)(void* importMove,const char* srcPath);
    typedef struct IDirPathMove{
        void*           importMove;
        IDirPathMove_getDstPathBySrcPath getDstPathBySrcPath;
        IDirPathList    srcPathList;
        IDirPathList    dstPathList;
    } IDirPathMove;
    
    static hpatch_BOOL _moveNewToOld(IDirPathMove* dirPathMove) {
        hpatch_BOOL result=hpatch_TRUE;
        IDirPathList*  dstPathList=&dirPathMove->dstPathList;
        IDirPathList*  srcPathList=&dirPathMove->srcPathList;
        size_t i;
        //delete file in dstPathList; //WARNING
        //delete dir in dstPathList; //not check
        for (i=dstPathList->pathCount; i>0; --i) {
            size_t dstPathIndex=i-1;
            const char* dstPath=dstPathList->getPathNameByIndex(dstPathList->import,dstPathIndex);
            if (dstPath==0) continue;
            if (!hpatch_getIsDirName(dstPath)){
                if (!_tryRemovePath(dstPath)){
                    printf("WARNING: can't remove old file \"");
                    hpatch_printPath_utf8(dstPath); printf("\"\n");
                }
            }else{
                hpatch_removeDir(dstPath);
            }
        }
        //move all files and dirs in srcDir to dstDir;
        for (i=0; i<srcPathList->pathCount; ++i) {//make dirs to dstDir
            size_t srcPathIndex=i;
            const char* srcPath=srcPathList->getPathNameByIndex(srcPathList->import,srcPathIndex);
            if (srcPath==0) { result=hpatch_FALSE; continue; }
            if (hpatch_getIsDirName(srcPath)){
                const char* dstDir=dirPathMove->getDstPathBySrcPath(dirPathMove->importMove,srcPath);
                if (dstDir==0) { result=hpatch_FALSE; continue; }
                if (!hpatch_makeNewDir(dstDir)) { result=hpatch_FALSE; continue; }
            }
        }
        for (i=srcPathList->pathCount; i>0; --i) {//move files to dstDir and remove dirs in srcDir
            size_t srcPathIndex=i-1;
            const char* srcPath=srcPathList->getPathNameByIndex(srcPathList->import,srcPathIndex);
            if (srcPath==0) { result=hpatch_FALSE; continue; }
            if (hpatch_getIsDirName(srcPath)){
                hpatch_removeDir(srcPath);
            }else{
                const char* dstPath=dirPathMove->getDstPathBySrcPath(dirPathMove->importMove,srcPath);
                if (dstPath==0) { result=hpatch_FALSE; continue; }
                hpatch_removeFile(dstPath);//overwrite
                if (!hpatch_moveFile(srcPath,dstPath)){//move src to dst
                    result=hpatch_FALSE;
                    fprintf(stderr,"can't move new file to oldDirectory \"");
                    hpatch_printStdErrPath_utf8(srcPath); fprintf(stderr,"\"  ERROR!\n");
                    continue;
                }
            }
        }
        return result;
    }
    
    static hpatch_BOOL deleteAllInPathList(IDirPathList* pathList) {
        hpatch_BOOL result=hpatch_TRUE;
        size_t i;
        for (i=pathList->pathCount; i>0; --i) {
            size_t pathIndex=i-1;
            const char* path=pathList->getPathNameByIndex(pathList->import,pathIndex);
            if (!_tryRemovePath(path))
                result=hpatch_FALSE;
        }
        return result;
    }

#ifndef _IS_NEED_tempDirPatchListener
#   define _IS_NEED_tempDirPatchListener 1
#endif

#if (_IS_NEED_tempDirPatchListener)
    
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
    
static hpatch_BOOL _tempDirPatchFinish(IHPatchDirListener* self,hpatch_BOOL isPatchSuccess){
    hpatch_BOOL  result=hpatch_TRUE;
    TDirPatcher* dirPatcher=(TDirPatcher*)self->listenerImport;
    size_t       i;
    hpatch_BOOL  isInitSameRefError=isPatchSuccess?(!TDirPatcher_initOldSameRefCount(dirPatcher)):hpatch_FALSE;
    if (isInitSameRefError){
        isPatchSuccess=hpatch_FALSE;
        result=hpatch_FALSE;
    }
    if (isPatchSuccess){
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
        
        {//move new to old:
            IDirPathMove dirPathMove;
            dirPathMove.importMove=dirPatcher;
            dirPathMove.getDstPathBySrcPath=(IDirPathMove_getDstPathBySrcPath)TDirPatcher_getOldPathByNewPath;
            TDirPatcher_getNewDirPathList(dirPatcher,&dirPathMove.srcPathList);
            TDirPatcher_getOldDirPathList(dirPatcher,&dirPathMove.dstPathList);
            if (!_moveNewToOld(&dirPathMove))
                result=hpatch_FALSE;
        }
    
        {//set execute tags in oldDir
            IDirPathList oldExecuteList;
            oldExecuteList.import=dirPatcher;
            oldExecuteList.pathCount=TDirPatcher_getNewExecuteFileCount(dirPatcher);
            oldExecuteList.getPathNameByIndex=
                    (IDirPathList_getPathNameByIndex)TDirPatcher_getOldExecuteFileByNewExecuteIndex;
            _dirPatch_setIsExecuteFile(&oldExecuteList);
        }
    }
    
    {//remove all temp file and dir
        IDirPathList newPathList;
        TDirPatcher_getNewDirPathList(dirPatcher,&newPathList);
        deleteAllInPathList(&newPathList);
    }
    {//check remove newTempDir result
        const char* newTempDir=TDirPatcher_getNewPathRoot(dirPatcher);
        if (!hpatch_isPathNotExist(newTempDir)){
            result=hpatch_FALSE;
            fprintf(stderr,"can't delete newTempDir \"");
            hpatch_printStdErrPath_utf8(newTempDir); fprintf(stderr,"\"  ERROR!\n");
        }
    }
    return result;
}

    
// 1. patch new ref to newTempDir,
//    make new dir to newTempDir
//    checksum same file
// 2. if patch ok then  {
//        move(+ some must copy) same to newTempDir from oldDir;
//        move new to old             {
//            delete file in oldPathList; //WARNING
//            delete dir in oldPathList; //not check
//            move all files and dir in newTempDir to oldDir; }
//        set execute tags in oldDir;
//        delete newTempDir; }
//    if patch error then  {
//        delelte all in newTempDir;//not check
//        delete newTempDir; }
static IHPatchDirListener tempDirPatchListener={{&tempDirPatchListener,_makeNewDir,_tempDir_copySameFile,
                                                   _openNewFile,_closeNewFile},
                                                 0,_tempDirPatchBegin,_tempDirPatchFinish};
    
#endif //_IS_NEED_tempDirPatchListener
#ifdef __cplusplus
}
#endif
#endif
