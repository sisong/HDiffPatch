//  dir_sync_server.cpp
//  sync_server
//  Created by housisong on 2019-10-01.
/*
 The MIT License (MIT)
 Copyright (c) 2019-2019 HouSisong
 
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
#include "dir_sync_server.h"
#if (_IS_NEED_DIR_DIFF_PATCH)
#include "../../dirDiffPatch/dir_diff/dir_diff_tools.h"
using namespace hdiff_private;

void create_dir_sync_data(IDirSyncListener*         listener,
                          const char*               newDataDir,
                          const char*               out_newSyncInfoFile,
                          const char*               out_newSyncDataFile,
                          const hdiff_TCompress*    compressPlugin,
                          hpatch_TChecksum*         strongChecksumPlugin,
                          size_t                    kMaxOpenFileNumber,
                          uint32_t kMatchBlockSize,size_t threadNum){
    TManifest newManifest;
    newManifest.rootPath=newDataDir;
    assert(hpatch_getIsDirName(newManifest.rootPath.c_str()));
    getDirAllPathList(newManifest.rootPath,newManifest.pathList,listener);
    sortDirPathList(newManifest.pathList);
    
    create_dir_sync_data(listener,newManifest,out_newSyncInfoFile,out_newSyncDataFile,
                         compressPlugin,strongChecksumPlugin,kMaxOpenFileNumber,kMatchBlockSize,threadNum);
}


static void getRefList(const std::string& newRootPath,const std::vector<std::string>& newList,
                       std::vector<hpatch_StreamPos_t>& out_newSizeList){
    out_newSizeList.assign(newList.size(),0);
    for (size_t newi=0; newi<newList.size(); ++newi){
        const std::string& fileName=newList[newi];
        if (isDirName(fileName)) continue;
        out_newSizeList[newi]=getFileSize(fileName);
    }
}


void create_dir_sync_data(IDirSyncListener*         listener,
                          const TManifest&          newManifest,
                          const char*               out_newSyncInfoFile,
                          const char*               out_newSyncDataFile,
                          const hdiff_TCompress*    compressPlugin,
                          hpatch_TChecksum*         strongChecksumPlugin,
                          size_t                    kMaxOpenFileNumber,
                          uint32_t kMatchBlockSize,size_t threadNum){
    assert(listener!=0);
    assert(kMaxOpenFileNumber>=kMaxOpenFileNumber_limit_min);
    kMaxOpenFileNumber-=2; // for out_newSyncInfoFile & out_newSyncDataFile
    const std::vector<std::string>& newList=newManifest.pathList;
    listener->syncPathList(newList);
    
    std::vector<hpatch_StreamPos_t> newSizeList;
    std::vector<size_t> newExecuteList; //for linux etc
    
    for (size_t newi=0; newi<newList.size(); ++newi) {
        if ((!isDirName(newList[newi]))&&(listener->isExecuteFile(newList[newi])))
            newExecuteList.push_back(newi);
    }

    getRefList(newManifest.rootPath,newList,newSizeList);
    CFileResHandleLimit resLimit(kMaxOpenFileNumber,newList.size());
    {
        for (size_t i=0; i<newSizeList.size(); ++i) {
            resLimit.addRes(newList[i],newSizeList[i]);
        }
    }
    resLimit.open();
    CRefStream newRefStream;
    newRefStream.open(resLimit.limit.streamList,newList.size());
    
    listener->syncRefInfo(newList.size(),newRefStream.stream->streamSize,kMatchBlockSize);
}



#endif

