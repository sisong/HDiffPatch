// new_dir_output.h
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
#ifndef DirPatch_new_dir_output_h
#define DirPatch_new_dir_output_h
#include "dir_patch_types.h"
#if (_IS_NEED_DIR_DIFF_PATCH)
#include "../../libHDiffPatch/HPatch/checksum_plugin.h"
#include "ref_stream.h"
#include "new_stream.h"
#include "res_handle_limit.h"
#ifdef __cplusplus
extern "C" {
#endif
    
    typedef struct hpatch_ICopyDataListener{
        void* listenerImport;
        void    (*copyedData)(struct hpatch_ICopyDataListener* listener,const unsigned char* data,
                              const unsigned char* dataEnd);
    } hpatch_ICopyDataListener;
    
hpatch_BOOL TDirPatcher_copyFile(const char* oldFileName_utf8,const char* newFileName_utf8,
                                 hpatch_ICopyDataListener* copyListener);
hpatch_BOOL TDirPatcher_readFile(const char* oldFileName_utf8,hpatch_ICopyDataListener* copyListener);


    struct hpatch_TFileStreamOutput;
    
    typedef struct IDirPatchListener{
        void*       listenerImport;
        hpatch_BOOL   (*makeNewDir)(struct IDirPatchListener* listener,const char* newDir);
        hpatch_BOOL (*copySameFile)(struct IDirPatchListener* listener,const char* oldFileName,
                                    const char* newFileName,hpatch_ICopyDataListener* copyListener);
        hpatch_BOOL  (*openNewFile)(struct IDirPatchListener* listener,struct hpatch_TFileStreamOutput*  out_curNewFile,
                                    const char* newFileName,hpatch_StreamPos_t newFileSize);
        hpatch_BOOL (*closeNewFile)(struct IDirPatchListener* listener,struct hpatch_TFileStreamOutput* curNewFile);
    } IDirPatchListener;
    
    
    typedef struct hpatch_IOldPathListener{
        void* listenerImport;
        const char* (*getOldPathByIndex)(struct hpatch_IOldPathListener* listener,size_t oldPathIndex);
    } hpatch_IOldPathListener;
    
    typedef struct TNewDirOutput{
        //input:
        const char* const *         newUtf8PathList;
        const size_t*               newRefList;
        const size_t*               newExecuteList;
        const hpatch_StreamPos_t*   newRefSizeList;
        const hpatch_TSameFilePair* dataSamePairList; //new map to old index
        hpatch_StreamPos_t          newDataSize;
        size_t                      newPathCount;
        size_t                      newRefFileCount;
        size_t                      newExecuteCount;
        size_t                      sameFilePairCount;
        size_t                      checksumByteSize;
        hpatch_BOOL                 isCheck_newRefData;
        hpatch_BOOL                 isCheck_copyFileData;
        //private input:
        hpatch_IOldPathListener     _oldPathListener;
        char*                       _newRootDir;
        char*                       _newRootDir_end;
        char*                       _newRootDir_bufEnd;
        const unsigned char*        _pChecksum_newRefData_copyFileData;
        unsigned char*              _pChecksum_temp;
        hpatch_TChecksum*           _checksumPlugin;
        //private:
        hpatch_ICopyDataListener    _sameFileCopyListener;
        hpatch_TNewStream           _newDirStream;
        IDirPatchListener*          _listener;
        hpatch_INewStreamListener   _newDirStreamListener;
        hpatch_checksumHandle       _newRefChecksumHandle;
        hpatch_checksumHandle       _sameFileChecksumHandle;
        hpatch_BOOL                 _isNewRefDataChecksumError;
        hpatch_BOOL                 _isCopyDataChecksumError;
        struct hpatch_TFileStreamOutput*   _curNewFile;
        void*                       _pmem;
    } TNewDirOutput;

hpatch_inline
static void TNewDirOutput_init(TNewDirOutput* self)  { memset(self,0,sizeof(*self)); }
hpatch_BOOL TNewDirOutput_openDir(TNewDirOutput* self,IDirPatchListener* listener,
                                  size_t kAlignSize,const hpatch_TStreamOutput** out_newDirStream);

hpatch_BOOL TNewDirOutput_closeNewDirHandles(TNewDirOutput* self);//for TNewDirOutput_openDir
hpatch_BOOL TNewDirOutput_close(TNewDirOutput* self);

const char* TNewDirOutput_getNewPathRoot(TNewDirOutput* self);
const char* TNewDirOutput_getNewPathByIndex(TNewDirOutput* self,size_t newPathIndex);
const char* TNewDirOutput_getNewExecuteFileByIndex(TNewDirOutput* self,size_t newExecuteIndex);
const char* TNewDirOutput_getNewPathByRefIndex(TNewDirOutput* self,size_t newRefIndex);
const char* TNewDirOutput_getOldPathBySameIndex(TNewDirOutput* self,size_t sameIndex);
const char* TNewDirOutput_getNewPathBySameIndex(TNewDirOutput* self,size_t sameIndex);

//ExecuteList
typedef const char* (*IDirPathList_getPathNameByIndex)(void* import,size_t index);
typedef struct IDirPathList{
    void*                   import;
    size_t                  pathCount;
    IDirPathList_getPathNameByIndex getPathNameByIndex;
} IDirPathList;

static hpatch_inline void TNewDirOutput_getExecuteList(TNewDirOutput* self,IDirPathList* out_executeList) {
    out_executeList->import=self;
    out_executeList->pathCount=self->newExecuteCount;
    out_executeList->getPathNameByIndex=(IDirPathList_getPathNameByIndex)TNewDirOutput_getNewExecuteFileByIndex;
}
static hpatch_inline void TNewDirOutput_getNewDirPathList(TNewDirOutput* self,IDirPathList* out_newPathList) {
    out_newPathList->import=self;
    out_newPathList->pathCount=self->newPathCount;
    out_newPathList->getPathNameByIndex=(IDirPathList_getPathNameByIndex)TNewDirOutput_getNewPathByIndex;
}

#ifdef __cplusplus
}
#endif
#endif
#endif //DirPatch_new_dir_output_h
