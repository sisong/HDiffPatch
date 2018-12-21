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
    
    virtual void filterFileList(std::vector<std::string>& oldList,std::vector<std::string>& newList){}
    
    virtual bool isNeedSavedOldList()const{ return true; }
};


void dir_diff(IDirDiffListener* listener,const char* _oldPatch,const char* _newPatch,
              const char* outDiffFileName,bool oldIsDir,bool newIsDir,bool isLoadAll,size_t matchValue,
              hdiff_TStreamCompress* streamCompressPlugin,hdiff_TCompress* compressPlugin,
              hpatch_TDecompress* decompressPlugin);


#endif //hdiff_dir_diff_h
