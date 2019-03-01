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
#if (_IS_NEED_DIR_DIFF_PATCH)
#include <string>
#include <vector>
#include "../../libHDiffPatch/HDiff/diff_types.h"
#include "../../libHDiffPatch/HPatch/checksum_plugin.h"

void assignDirTag(std::string& dir);
struct IDirPathIgnore;
void getDirAllPathList(const std::string& dir,std::vector<std::string>& out_list,
                       IDirPathIgnore* filter,bool pathIsInOld);
void sortDirPathList(std::vector<std::string>& fileList);

struct IDirPathIgnore{
    virtual ~IDirPathIgnore(){}
    virtual bool isNeedIgnore(const std::string& path,size_t rootPathNameLen,bool pathIsInOld) { return false; }
};

struct IDirDiffListener:public IDirPathIgnore{
    virtual ~IDirDiffListener(){}
    virtual bool isExecuteFile(const std::string& fileName) { return false; }
    virtual void diffPathList(const std::vector<std::string>& oldPathList,
                              const std::vector<std::string>& newPathList){}
    virtual void diffRefInfo(size_t oldPathCount,size_t newPathCount,size_t sameFilePairCount,
                             hpatch_StreamPos_t sameFileSize,size_t refOldFileCount,size_t refNewFileCount,
                             hpatch_StreamPos_t refOldFileSize,hpatch_StreamPos_t refNewFileSize){}
    virtual void runHDiffBegin(){}
    virtual void runHDiffEnd(hpatch_StreamPos_t diffDataSize){}
    virtual void externData(std::vector<unsigned char>& out_externData){}
    virtual void externDataPosInDiffStream(hpatch_StreamPos_t externDataPos,size_t externDataSize){}
};

void dir_diff(IDirDiffListener* listener,const std::string& oldPath,const std::string& newPath,
              const hpatch_TStreamOutput* outDiffStream,bool isLoadAll,size_t matchValue,
              const hdiff_TCompress* compressPlugin,hpatch_TChecksum* checksumPlugin,size_t kMaxOpenFileNumber);
bool check_dirdiff(IDirDiffListener* listener,const std::string& oldPath,const std::string& newPath,
                   const hpatch_TStreamInput* testDiffData,hpatch_TDecompress* decompressPlugin,
                   hpatch_TChecksum* checksumPlugin,size_t kMaxOpenFileNumber);

struct TManifest{
    std::string                 rootPath;
    std::vector<std::string>    pathList;
};
void manifest_diff(IDirDiffListener* listener,const TManifest& oldManifest,
                   const TManifest& newManifest,const hpatch_TStreamOutput* outDiffStream,
                   bool isLoadAll,size_t matchValue,const hdiff_TCompress* compressPlugin,
                   hpatch_TChecksum* checksumPlugin,size_t kMaxOpenFileNumber);
bool check_manifestdiff(IDirDiffListener* listener,const TManifest& oldManifest,const TManifest& newManifest,
                        const hpatch_TStreamInput* testDiffData,hpatch_TDecompress* decompressPlugin,
                        hpatch_TChecksum* checksumPlugin,size_t kMaxOpenFileNumber);
void save_manifest(IDirDiffListener* listener,const std::string& inputPath,
                   const hpatch_TStreamOutput* outManifest,hpatch_TChecksum* checksumPlugin);

//as api demo
hpatch_BOOL check_dirOldDataChecksum(const char* oldPath,hpatch_TStreamInput* diffData,
                                     hpatch_TDecompress *decompressPlugin,hpatch_TChecksum *checksumPlugin);

void resave_dirdiff(const hpatch_TStreamInput* in_diff,hpatch_TDecompress* decompressPlugin,
                    const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                    hpatch_TChecksum* checksumPlugin);


struct TManifestSaved:public TManifest{
    std::string                                 checksumType;
    std::vector<std::vector<unsigned char> >    checksumList;
};

void load_manifestFile(TManifestSaved& out_manifest,const std::string& rootPath,
                       const std::string& manifestFile);
void load_manifest(TManifestSaved& out_manifest,const std::string& rootPath,
                   const hpatch_TStreamInput* manifestStream);
void checksum_manifest(const TManifestSaved& manifest,hpatch_TChecksum* checksumPlugin);

#endif
#endif //hdiff_dir_diff_h
