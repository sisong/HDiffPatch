// dir_patch.h
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
#ifndef DirPatch_dir_patch_h
#define DirPatch_dir_patch_h
#include "../../libHDiffPatch/HPatch/patch_types.h"
#ifdef __cplusplus
extern "C" {
#endif

hpatch_inline static hpatch_BOOL isAsciiString(const char* str,const char* strEnd){
    for (;str<strEnd;++str) {
        if ((*(const unsigned char*)str)<0x80)
            continue;
        else
            return hpatch_FALSE;
    }
    return hpatch_TRUE;
}
    
typedef struct TDirDiffInfo{
    hpatch_BOOL                 isDirDiff;
    hpatch_BOOL                 newPathIsDir;
    hpatch_BOOL                 oldPathIsDir;
    hpatch_BOOL                 dirDataIsCompressed;
    hpatch_StreamPos_t          externDataOffset;
    hpatch_StreamPos_t          externDataSize;
    hpatch_compressedDiffInfo   hdiffInfo;
} TDirDiffInfo;


hpatch_BOOL getDirDiffInfo(const hpatch_TStreamInput* diffFile,TDirDiffInfo* out_info);
hpatch_BOOL getDirDiffInfoByFile(const char* diffFileName,TDirDiffInfo* out_info);
hpatch_inline static hpatch_BOOL getIsDirDiffFile(const char* diffFileName,hpatch_BOOL* out_isDirDiffFile){
    TDirDiffInfo info;
    hpatch_BOOL result=getDirDiffInfoByFile(diffFileName,&info);
    if (result) *out_isDirDiffFile=info.isDirDiff;
    return result;
}

    typedef struct _TDirDiffHead {
        hpatch_StreamPos_t  oldPathCount;
        hpatch_StreamPos_t  newPathCount;
        hpatch_StreamPos_t  oldPathSumSize;
        hpatch_StreamPos_t  newPathSumSize;
        hpatch_StreamPos_t  oldRefFileCount;
        hpatch_StreamPos_t  newRefFileCount;
        hpatch_StreamPos_t  sameFilePairCount;
        hpatch_StreamPos_t  headDataOffset;
        hpatch_StreamPos_t  headDataSize;
        hpatch_StreamPos_t  headDataCompressedSize;
        hpatch_StreamPos_t  hdiffDataOffset;
        hpatch_StreamPos_t  hdiffDataSize;
    } _TDirDiffHead;

typedef struct TDirPatcher{
    TDirDiffInfo                dirDiffInfo;
    _TDirDiffHead               dirDiffHead;
    const char* const *         oldUtf8PathList;
    const char* const *         newUtf8PathList;
    const size_t*               oldRefList;
    const size_t*               newRefList;
    const size_t*               dataSamePairList; //new map to old index
//private:
    const hpatch_TStreamInput*  _dirDiffData;
    hpatch_TDecompress*         _decompressPlugin;
    void*                       _pmem;
} TDirPatcher;

hpatch_inline
static void     TDirPatcher_init(TDirPatcher* self)  { memset(self,0,sizeof(*self)); }
hpatch_BOOL     TDirPatcher_open(TDirPatcher* self,const hpatch_TStreamInput* dirDiffData);
    
hpatch_BOOL     TDirPatcher_loadDirData(TDirPatcher* self,hpatch_TDecompress* decompressPlugin);

hpatch_BOOL     TDirPatcher_patch(TDirPatcher* self,const hpatch_TStreamOutput* out_newData,
                                      const hpatch_TStreamInput* oldData,
                                      unsigned char* temp_cache,unsigned char* temp_cache_end);

void            TDirPatcher_close(TDirPatcher* self);


#ifdef __cplusplus
}
#endif
#endif //DirPatch_dir_patch_h
