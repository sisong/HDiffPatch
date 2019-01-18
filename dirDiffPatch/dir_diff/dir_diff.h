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
#include <string>
#include <vector>
#include "../../libHDiffPatch/HDiff/diff_types.h"
#include "../../libHDiffPatch/HPatch/checksum_plugin.h"

void assignDirTag(std::string& dir);
struct IDirFilter;
void getDirFileList(const std::string& dir,std::vector<std::string>& out_list,IDirFilter* filter);
void sortDirFileList(std::vector<std::string>& fileList);

struct IDirFilter{
    virtual ~IDirFilter(){}
    static bool pathIsEndWith(const std::string& pathName,const char* testEndTag);
    static bool pathNameIs(const std::string& pathName,const char* testPathName);
    
    virtual bool isNeedFilter(const std::string& fileName) { return false; }
};

struct IDirDiffListener:public IDirFilter{
    virtual ~IDirDiffListener(){}
    virtual void diffPathList(std::vector<std::string>& oldPathList,std::vector<std::string>& newPathList){}
    virtual void diffRefInfo(size_t oldPathCount,size_t newPathCount,size_t sameFilePairCount,
                             size_t refOldFileCount,size_t refNewFileCount,
                             hpatch_StreamPos_t refOldFileSize,hpatch_StreamPos_t refNewFileSize){}
    virtual void runHDiffBegin(){}
    virtual void runHDiffEnd(hpatch_StreamPos_t diffDataSize){}
    virtual void externData(std::vector<unsigned char>& out_externData){}
    virtual void externDataPosInDiffStream(hpatch_StreamPos_t externDataPos){}
};

void dir_diff(IDirDiffListener* listener,const std::string& oldPath,const std::string& newPath,
              const hpatch_TStreamOutput* outDiffStream,bool isLoadAll,size_t matchValue,
              hdiff_TStreamCompress* streamCompressPlugin,hdiff_TCompress* compressPlugin,
              hpatch_TChecksum* checksumPlugin,size_t kMaxOpenFileNumber);

bool check_dirdiff(IDirDiffListener* listener,const std::string& oldPath,const std::string& newPath,
                   const hpatch_TStreamInput* testDiffData,hpatch_TDecompress* decompressPlugin,
                   hpatch_TChecksum* checksumPlugin,size_t kMaxOpenFileNumber);

void resave_dirdiff(const hpatch_TStreamInput* in_diff,hpatch_TDecompress* decompressPlugin,
                    const hpatch_TStreamOutput* out_diff,hdiff_TStreamCompress* compressPlugin,
                    hpatch_TChecksum* checksumPlugin);

#endif //hdiff_dir_diff_h
