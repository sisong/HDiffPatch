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
#include "../libHDiffPatch/HDiff/diff_types.h"

void assignDirTag(std::string& dir);
bool getDirFileList(const std::string& dir,std::vector<std::string>& out_list);
void sortDirFileList(std::vector<std::string>& fileList);

struct IDirDiffListener{
    inline explicit IDirDiffListener(){}
    virtual ~IDirDiffListener(){}
    
    virtual void filterFileList(std::vector<std::string>& newList,std::vector<std::string>& oldList){}
    virtual void refInfo(size_t sameFileCount,size_t refNewFileCount,size_t refOldFileCount,
                         hpatch_StreamPos_t refNewFileSize,hpatch_StreamPos_t refOldFileSize){}
    virtual void externData(std::vector<unsigned char>& out_externData){}
    virtual void externDataPosInOutStream(hpatch_StreamPos_t externDataPos){}
    virtual void file_name_to_utf8(const std::string& fileName,std::string& out_utf8){ out_utf8.assign(fileName); }
};

void dir_diff(IDirDiffListener* listener,const char* _oldPatch,const char* _newPatch,
              const hpatch_TStreamOutput* outDiffStream,bool oldIsDir,bool newIsDir,
              bool isLoadAll,size_t matchValue,
              hdiff_TStreamCompress* streamCompressPlugin,hdiff_TCompress* compressPlugin);


#endif //hdiff_dir_diff_h
