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
#include "sync_info_server.h"
#include "../../dirDiffPatch/dir_diff/dir_diff_tools.h"
using namespace hdiff_private;
using namespace sync_private;

static void getRefList(const std::vector<std::string>& newList,
                       std::vector<hpatch_StreamPos_t>& out_newSizeList){
    out_newSizeList.assign(newList.size(),0);
    for (size_t newi=0; newi<newList.size(); ++newi){
        const std::string& fileName=newList[newi];
        if (isDirName(fileName)) continue;
        out_newSizeList[newi]=getFileSize(fileName);
    }
}

void _private_create_sync_data(TNewDataSyncInfo*           newSyncInfo,
                               const hpatch_TStreamInput*  newData,
                               const hpatch_TStreamOutput* out_newSyncInfo,
                               const hpatch_TStreamOutput* out_newSyncData,
                               const hdiff_TCompress* compressPlugin,size_t threadNum);

void create_dir_sync_data(IDirSyncListener*         listener,
                          const TManifest&          newManifest,
                          const char*               outNewSyncInfoFile,
                          const char*               outNewSyncDataFile,
                          const hdiff_TCompress*    compressPlugin,
                          hpatch_TChecksum*         strongChecksumPlugin,
                          size_t                    kMaxOpenFileNumber,
                          uint32_t kMatchBlockSize,size_t threadNum){
    assert(listener!=0);
    assert((outNewSyncDataFile!=0)&&(strlen(outNewSyncDataFile)>0));
    assert(kMaxOpenFileNumber>=kMaxOpenFileNumber_limit_min);
    kMaxOpenFileNumber-=2; // for outNewSyncInfoFile & outNewSyncDataFile
    const std::vector<std::string>& newList=newManifest.pathList;
    std::vector<hpatch_StreamPos_t> newSizeList;
    std::vector<size_t> newExecuteList; //for linux etc
    
    for (size_t newi=0; newi<newList.size(); ++newi) {
        if ((!isDirName(newList[newi]))&&(listener->isExecuteFile(newList[newi])))
            newExecuteList.push_back(newi);
    }

    getRefList(newList,newSizeList);
    CFileResHandleLimit resLimit(kMaxOpenFileNumber,newList.size());
    hpatch_StreamPos_t maxNewSize=0;
    {
        for (size_t i=0; i<newSizeList.size(); ++i) {
            hpatch_StreamPos_t newSize=newSizeList[i];
            maxNewSize=(newSize>maxNewSize)?newSize:maxNewSize;
            resLimit.addRes(newList[i],newSize);
        }
    }
    if (kMatchBlockSize>maxNewSize){
        kMatchBlockSize=(uint32_t)maxNewSize;
        if (kMatchBlockSize<kMatchBlockSize_min)
            kMatchBlockSize=kMatchBlockSize_min;
    }
    
    resLimit.open();
    CRefStream newRefStream;
    const size_t kAlignSize=kMatchBlockSize;
    newRefStream.open(resLimit.limit.streamList,newList.size(),kAlignSize);
    
    int hashClashBit=estimateHashClashBit(newRefStream.stream->streamSize,kMatchBlockSize);
    bool isMatchBlockSizeWarning=hashClashBit>kAllowMaxHashClashBit;
    listener->syncRefInfo(newList.size(),newRefStream.stream->streamSize,kMatchBlockSize,isMatchBlockSizeWarning);
    checkv(!isMatchBlockSizeWarning); //warning as error

    CNewDataSyncInfo  newDataSyncInfo(strongChecksumPlugin,compressPlugin,
                                      newRefStream.stream->streamSize,kMatchBlockSize);
    newDataSyncInfo.isDirSyncInfo=hpatch_TRUE;
    newDataSyncInfo.dir_newCount=newList.size();
    newDataSyncInfo.dir_newNameList_isCString=hpatch_FALSE;
    newDataSyncInfo.dir_utf8NewNameList=newList.data();
    newDataSyncInfo.dir_utf8RootPath=newManifest.rootPath.c_str();
    newDataSyncInfo.dir_newSizeList=newSizeList.data();
    newDataSyncInfo.dir_newExecuteCount=newExecuteList.size();
    newDataSyncInfo.dir_newExecuteIndexList=newExecuteList.data();
    
    std::vector<unsigned char> out_externData;
    listener->externData(out_externData);
    newDataSyncInfo.externData_begin=out_externData.data();
    newDataSyncInfo.externData_end=out_externData.data()+out_externData.size();
    
    CFileStreamOutput out_newSyncInfo(outNewSyncInfoFile,~(hpatch_StreamPos_t)0);
    CFileStreamOutput out_newSyncData(outNewSyncDataFile,~(hpatch_StreamPos_t)0);
    
    _private_create_sync_data(&newDataSyncInfo, newRefStream.stream,&out_newSyncInfo.base,
                              &out_newSyncData.base, compressPlugin,threadNum);
}



#endif

