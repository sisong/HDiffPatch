// dir_diff.h
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
#ifndef hdiff_dir_diff_h
#define hdiff_dir_diff_h
#include "../dir_patch/dir_patch_types.h"

struct THDiffSets{
    hpatch_BOOL isDiffInMem;//or diff by stream
    hpatch_BOOL isSingleCompressedDiff;
    //diff in mem
    hpatch_BOOL isUseBigCacheMatch;
    hpatch_BOOL isCheckNotEqual;
    size_t matchScore;
    size_t patchStepMemSize;
    size_t matchBlockSize;
    size_t threadNum;
    size_t threadNumSearch_s;
};

#if (_IS_NEED_DIR_DIFF_PATCH)
#include "dir_manifest.h"


struct IDirDiffListener{
    virtual ~IDirDiffListener(){}
    virtual bool isExecuteFile(const std::string& fileName) { return false; }
    virtual void diffRefInfo(size_t oldPathCount,size_t newPathCount,size_t sameFilePairCount,
                             hpatch_StreamPos_t sameFileSize,size_t refOldFileCount,size_t refNewFileCount,
                             hpatch_StreamPos_t refOldFileSize,hpatch_StreamPos_t refNewFileSize){}
    virtual void runHDiffBegin(){}
    virtual void runHDiffEnd(hpatch_StreamPos_t diffDataSize){}
    virtual void externData(std::vector<unsigned char>& out_externData){}
    virtual void externDataPosInDiffStream(hpatch_StreamPos_t externDataPos,size_t externDataSize){}
};

void dir_diff(IDirDiffListener* listener,const TManifest& oldManifest,
              const TManifest& newManifest,const hpatch_TStreamOutput* outDiffStream,
              const hdiff_TCompress* compressPlugin,hpatch_TChecksum* checksumPlugin,
              const THDiffSets& hdiffSets,size_t kMaxOpenFileNumber);
bool check_dirdiff(IDirDiffListener* listener,const TManifest& oldManifest,const TManifest& newManifest,
                   const hpatch_TStreamInput* testDiffData,hpatch_TDecompress* decompressPlugin,
                   hpatch_TChecksum* checksumPlugin,size_t kMaxOpenFileNumber);

//check oldPath's data is same as that in dir_diff
hpatch_BOOL check_dirOldDataChecksum(const char* oldPath,hpatch_TStreamInput* diffData,
                                     hpatch_TDecompress* decompressPlugin,hpatch_TChecksum* checksumPlugin);

void resave_dirdiff(const hpatch_TStreamInput* in_diff,hpatch_TDecompress* decompressPlugin,
                    const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                    hpatch_TChecksum* checksumPlugin);

#endif
#endif //hdiff_dir_diff_h
