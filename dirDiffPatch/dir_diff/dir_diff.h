// dir_diff.h
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
#ifndef hdiff_dir_diff_h
#define hdiff_dir_diff_h
#include <string>
#include <vector>
#include "../../libHDiffPatch/HDiff/diff_types.h"

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
    virtual void diffFileList(std::vector<std::string>& oldList,std::vector<std::string>& newList){}
    virtual void refInfo(size_t sameFilePairCount,size_t refOldFileCount,size_t refNewFileCount,
                         hpatch_StreamPos_t refOldFileSize,hpatch_StreamPos_t refNewFileSize){}
    virtual void runHDiffBegin(){}
    virtual void runHDiffEnd(hpatch_StreamPos_t diffDataSize){}
    virtual void externData(std::vector<unsigned char>& out_externData){}
    virtual void externDataPosInDiffStream(hpatch_StreamPos_t externDataPos){}
};

void dir_diff(IDirDiffListener* listener,const std::string& oldPatch,const std::string& newPatch,
              const hpatch_TStreamOutput* outDiffStream,bool isLoadAll,size_t matchValue,
              hdiff_TStreamCompress* streamCompressPlugin,hdiff_TCompress* compressPlugin);


void resave_compressed_dirdiff(const hpatch_TStreamInput*  in_diff,
                               hpatch_TDecompress*         decompressPlugin,
                               const hpatch_TStreamOutput* out_diff,
                               hdiff_TStreamCompress*      compressPlugin);

#endif //hdiff_dir_diff_h
