// dir_patch.h
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
#ifndef DirPatch_dir_patch_h
#define DirPatch_dir_patch_h
#include "dir_patch_types.h"
#ifdef __cplusplus
extern "C" {
#endif
    hpatch_BOOL getCompressedDiffInfoByFile(const char* diffFileName,hpatch_compressedDiffInfo *out_info);
    static hpatch_inline
    hpatch_BOOL getIsCompressedDiffFile(const char* diffFileName){
        hpatch_compressedDiffInfo diffInfo;
        if (!getCompressedDiffInfoByFile(diffFileName,&diffInfo)) return hpatch_FALSE;
        return hpatch_TRUE;
    }
#ifdef __cplusplus
}
#endif

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
    
    
typedef struct TDirDiffInfo{
    hpatch_BOOL                 isDirDiff;
    hpatch_BOOL                 newPathIsDir;
    hpatch_BOOL                 oldPathIsDir;
    hpatch_BOOL                 dirDataIsCompressed;
    //compressType saved in hdiffInfo
    hpatch_compressedDiffInfo   hdiffInfo;
    hpatch_StreamPos_t          externDataOffset;
    hpatch_StreamPos_t          externDataSize;
    hpatch_StreamPos_t          checksumOffset;
    size_t                      checksumByteSize;
    char                        checksumType[hpatch_kMaxPluginTypeLength+1]; //ascii cstring
} TDirDiffInfo;


hpatch_BOOL getDirDiffInfo(const hpatch_TStreamInput* diffFile,TDirDiffInfo* out_info);
hpatch_BOOL getDirDiffInfoByFile(const char* diffFileName,TDirDiffInfo* out_info,
                                 hpatch_StreamPos_t diffDataOffert,hpatch_StreamPos_t diffDataSize);
static hpatch_inline
hpatch_BOOL getIsDirDiffFile(const char* diffFileName){
     TDirDiffInfo dirDiffInfo;
    if (!getDirDiffInfoByFile(diffFileName,&dirDiffInfo,0,0)) return hpatch_FALSE;
    return dirDiffInfo.isDirDiff;
}

    typedef struct _TDirDiffHead {
        size_t              oldPathCount;
        size_t              oldPathSumSize;
        size_t              newPathCount;
        size_t              newPathSumSize;
        size_t              oldRefFileCount;
        size_t              newRefFileCount;
        size_t              sameFilePairCount;
        size_t              newExecuteCount;
        size_t              privateReservedDataSize;
        hpatch_StreamPos_t  sameFileSize;
        hpatch_StreamPos_t  typesEndPos;
        hpatch_StreamPos_t  privateExternDataOffset; //headDataEndPos
        hpatch_StreamPos_t  privateExternDataSize;
        hpatch_StreamPos_t  compressSizeBeginPos;
        hpatch_StreamPos_t  headDataOffset;
        hpatch_StreamPos_t  headDataSize;
        hpatch_StreamPos_t  headDataCompressedSize;
        hpatch_StreamPos_t  hdiffDataOffset;
        hpatch_StreamPos_t  hdiffDataSize;
    } _TDirDiffHead;
    
    struct hpatch_TFileStreamInput;
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
    
    typedef struct TDirPatchChecksumSet{
        hpatch_TChecksum*   checksumPlugin;
        hpatch_BOOL         isCheck_oldRefData;
        hpatch_BOOL         isCheck_newRefData;  
        hpatch_BOOL         isCheck_copyFileData;
        hpatch_BOOL         isCheck_dirDiffData;
    } TDirPatchChecksumSet;

typedef struct TDirPatcher{
    TDirDiffInfo                dirDiffInfo;
    _TDirDiffHead               dirDiffHead;
    const char* const *         oldUtf8PathList;
    const char* const *         newUtf8PathList;
    const size_t*               oldRefList;
    const size_t*               newRefList;
    const size_t*               newExecuteList;
    const hpatch_StreamPos_t*   newRefSizeList;
    const hpatch_TSameFilePair* dataSamePairList; //new map to old index
    hpatch_BOOL                 isDiffDataChecksumError;
    hpatch_BOOL                 isNewRefDataChecksumError;
    hpatch_BOOL                 isOldRefDataChecksumError;
    hpatch_BOOL                 isCopyDataChecksumError;
//private:
    hpatch_INewStreamListener   _newDirStreamListener;
    hpatch_TNewStream           _newDirStream;
    struct hpatch_TFileStreamOutput*   _curNewFile;
    char*                       _newRootDir;
    char*                       _newRootDir_end;
    char*                       _newRootDir_bufEnd;
    void*                       _pNewRefMem;
    
    hpatch_TRefStream           _oldRefStream;
    hpatch_TResHandleLimit      _resLimit;
    hpatch_IResHandle*          _resList;
    struct hpatch_TFileStreamInput*    _oldFileList;
    char*                       _oldRootDir;
    char*                       _oldRootDir_end;
    char*                       _oldRootDir_bufEnd;
    void*                       _pOldRefMem;
    
    hpatch_ICopyDataListener    _sameFileCopyListener;
    
    TDirPatchChecksumSet        _checksumSet;
    hpatch_checksumHandle       _newRefChecksumHandle;
    hpatch_checksumHandle       _sameFileChecksumHandle;
    unsigned char*              _pChecksumMem;
    
    IDirPatchListener*          _listener;
    hpatch_TDecompress*         _decompressPlugin;
    const hpatch_TStreamInput*  _dirDiffData;
    void*                       _pDiffDataMem;
    size_t*                     _pOldSameRefCount;
} TDirPatcher;

hpatch_inline
static void TDirPatcher_init(TDirPatcher* self)  { memset(self,0,sizeof(*self)); }
hpatch_BOOL TDirPatcher_open(TDirPatcher* self,const hpatch_TStreamInput* dirDiffData,
                             const TDirDiffInfo** out_dirDiffInfo);
//if checksumSet->isCheck_dirDiffData then result&=checksum(dirDiffData);
hpatch_BOOL TDirPatcher_checksum(TDirPatcher* self,const TDirPatchChecksumSet* checksumSet);
hpatch_BOOL TDirPatcher_loadDirData(TDirPatcher* self,hpatch_TDecompress* decompressPlugin,
                                    const char* oldPath_utf8,const char* newPath_utf8);

hpatch_BOOL TDirPatcher_openOldRefAsStream(TDirPatcher* self,size_t kMaxOpenFileNumber,
                                           const hpatch_TStreamInput** out_oldRefStream);
hpatch_BOOL TDirPatcher_openNewDirAsStream(TDirPatcher* self,IDirPatchListener* listener,
                                           const hpatch_TStreamOutput** out_newDirStream);
hpatch_BOOL TDirPatcher_patch(TDirPatcher* self,const hpatch_TStreamOutput* out_newData,
                              const hpatch_TStreamInput* oldData,
                              unsigned char* temp_cache,unsigned char* temp_cache_end);

static hpatch_inline
size_t      TDirPatcher_getNewExecuteFileCount(const TDirPatcher* self) {
                                               return self->dirDiffHead.newExecuteCount;  }
const char* TDirPatcher_getNewExecuteFileByIndex(TDirPatcher* self,size_t newExecuteIndex);

hpatch_BOOL TDirPatcher_closeOldRefStream(TDirPatcher* self);//for TDirPatcher_openOldRefAsStream
hpatch_BOOL TDirPatcher_closeNewDirStream(TDirPatcher* self);//for TDirPatcher_openNewDirAsStream
hpatch_BOOL TDirPatcher_close(TDirPatcher* self);

//after loadDirData
const char* TDirPatcher_getOldPathByIndex(TDirPatcher* self,size_t oldPathIndex);
const char* TDirPatcher_getOldRefPathByRefIndex(TDirPatcher* self,size_t oldRefIndex);
const char* TDirPatcher_getOldPathByNewPath(TDirPatcher* self,const char* newPath);
const char* TDirPatcher_getNewPathRoot(TDirPatcher* self);
const char* TDirPatcher_getNewPathByIndex(TDirPatcher* self,size_t newPathIndex);

hpatch_BOOL TDirPatcher_initOldSameRefCount(TDirPatcher* self);
void        TDirPatcher_finishOldSameRefCount(TDirPatcher* self);
const char* TDirPatcher_getOldPathBySameIndex(TDirPatcher* self,size_t sameIndex);
const char* TDirPatcher_getNewPathBySameIndex(TDirPatcher* self,size_t sameIndex);
size_t      TDirPatcher_oldSameRefCount(TDirPatcher* self,size_t sameIndex);
void        TDirPatcher_decOldSameRefCount(TDirPatcher* self,size_t sameIndex);


//can checksum oldRefFiles+oldCopyFiles by part of dirDiffData;
typedef struct TDirOldDataChecksum{
//private:
    TDirPatcher         _dirPatcher;
    unsigned char*      _partDiffData;
    unsigned char*      _partDiffData_cur;
    unsigned char*      _partDiffData_end;
    hpatch_TStreamInput _diffStream;
    hpatch_BOOL         _isOpened;
    hpatch_BOOL         _isAppendStoped;
} TDirOldDataChecksum;
hpatch_inline
static void TDirOldDataChecksum_init(TDirOldDataChecksum* self)  { memset(self,0,sizeof(*self)); }
//if dirDiffData_part==dirDiffData_part_end means dirDiffData finished
hpatch_BOOL TDirOldDataChecksum_append(TDirOldDataChecksum* self,unsigned char* dirDiffData_part,
                                       unsigned char* dirDiffData_part_end,hpatch_BOOL* out_isAppendContinue);
const char* TDirOldDataChecksum_getChecksumType(const TDirOldDataChecksum* self);
const char* TDirOldDataChecksum_getCompressType(const TDirOldDataChecksum* self);

//open read the needed old files,check file sum size,and checksum file data
hpatch_BOOL TDirOldDataChecksum_checksum(TDirOldDataChecksum* self,hpatch_TDecompress* decompressPlugin,
                                         hpatch_TChecksum* checksumPlugin,const char* oldPath_utf8);

hpatch_BOOL TDirOldDataChecksum_close(TDirOldDataChecksum* self);
    
#ifdef __cplusplus
}
#endif
#endif
#endif //DirPatch_dir_patch_h
