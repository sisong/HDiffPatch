// dir_diff.cpp
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
#include "dir_diff.h"
#include <algorithm> //sort
#include <map>
#include <set>
#include "file_for_dir.h"
#include "../libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.h"

#define kFileIOBufSize  (64*1024)
#define _error(info) throw new std::runtime_error(info);

void assignDirTag(std::string& dir){
    if (dir.empty()||(dir.back()!=kPatch_dirTag))
        dir.push_back(kPatch_dirTag);
}

static inline bool isDirName(const std::string& path){
    return (!path.empty())&&(path.back()==kPatch_dirTag);
}

bool getDirFileList(const std::string& dir,std::vector<std::string>& out_list){
    assert(isDirName(dir));
    TDirHandle dirh=dirOpenForRead(dir.c_str());
    if (dirh==0) return false;
    bool isHaveSub=false;
    while (true) {
        TPathType  type;
        const char* path=dirNext(dirh,&type);
        if (path==0) break;
        if ((0==strcmp(path,""))||(0==strcmp(path,"."))||(0==strcmp(path,"..")))
            continue;
        isHaveSub=true;
        std::string subName(dir+path);
        if (type==kPathType_file){
            assert(subName.back()!=kPatch_dirTag);
            out_list.push_back(subName); //add file
        }else{// if (type==kPathType_dir){
            assignDirTag(subName);
            if (!getDirFileList(subName,out_list)) return false;
        }
    }
    if (!isHaveSub)
        out_list.push_back(dir); //add empty dir
    dirClose(dirh);
    return true;
}

#define hash_value_t uint64_t
#define get_hash     fast_adler64_start

static hash_value_t getFileHash(const std::string& fileName){
    //todo:
    return 0;
}

static bool fileData_isSame(const std::string& file_x,const std::string& file_y){
    //todo:
    return false;
}

void sortDirFileList(std::vector<std::string>& fileList){
    std::sort(fileList.begin(),fileList.end());
}

static void getRefList(const std::vector<std::string>& oldList,const std::vector<std::string>& newList,
                       std::vector<size_t>& out_samePairList,
                       std::vector<size_t>& out_oldRefList,std::vector<size_t>& out_newRefList){
    typedef std::multimap<hash_value_t,size_t> TMap;
    TMap hashMap;
    std::set<size_t> oldRefList;
    for (int i=0; i<oldList.size(); ++i) {
        const std::string& fileName=oldList[i];
        if (isDirName(fileName)) continue;
        hash_value_t hash=getFileHash(fileName);
        hashMap.insert(TMap::value_type(hash,i));
        oldRefList.insert(i);
    }
    out_samePairList.clear();
    out_newRefList.clear();
    for (int i=0; i<newList.size(); ++i){
        const std::string& fileName=newList[i];
        if (isDirName(fileName)) continue;
        hash_value_t hash=getFileHash(fileName);
        bool findSame=false;
        size_t oldIndex=(size_t)(-1);
        std::pair<TMap::const_iterator,TMap::const_iterator> range=hashMap.equal_range(hash);
        for (;range.first!=range.second;++range.first) {
            oldIndex=range.first->second;
            if (fileData_isSame(oldList[oldIndex],fileName)){
                findSame=true;
                break;
            }
        }
        if (findSame){
            oldRefList.erase(oldIndex);
            out_samePairList.push_back(i);
            out_samePairList.push_back(oldIndex);
        }else{
            out_newRefList.push_back(i);
        }
    }
    out_oldRefList.assign(oldRefList.begin(),oldRefList.end());
    std::sort(out_oldRefList.begin(),out_oldRefList.end());
}

void dir_diff(IDirDiffListener* listener,const char* _oldPatch,const char* _newPatch,
              const char* outDiffFileName,bool oldIsDir,bool newIsDir,bool isLoadAll,size_t matchValue,
              hdiff_TStreamCompress* streamCompressPlugin,hdiff_TCompress* compressPlugin,
              hpatch_TDecompress* decompressPlugin){
    assert(listener!=0);
    std::string oldPatch(_oldPatch);
    std::string newPatch(_newPatch);
    std::vector<std::string> oldList;
    std::vector<std::string> newList;
    if (oldIsDir){
        assignDirTag(oldPatch);
        if (!getDirFileList(oldPatch,oldList)) _error("search old dir error!");
        sortDirFileList(oldList);
    }
    if (newIsDir){
        assignDirTag(newPatch);
        if (!getDirFileList(newPatch,newList)) _error("search new dir error!");
        sortDirFileList(newList);
    }
    listener->filterFileList(oldList,newList);

    std::vector<size_t> samePairList; //new map to same old
    std::vector<size_t> oldRefList;
    std::vector<size_t> newRefList;
    getRefList(oldList,newList,samePairList,oldRefList,newRefList);
    
    //
}
