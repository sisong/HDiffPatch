//  dir_sync_make.cpp
//  sync_make
//  Created by housisong on 2019-10-01.
/*
 The MIT License (MIT)
 Copyright (c) 2019-2023 HouSisong
 
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
#include "dir_sync_make.h"
#if (_IS_NEED_DIR_DIFF_PATCH)
#include "../../dirDiffPatch/dir_diff/dir_diff_tools.h"
#include "sync_make_hash_clash.h"
#include "sync_info_make.h"
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
                               hsync_TDictCompress* compressPlugin,
                               hsync_THsynz* hsynzPlugin,size_t threadNum);

void create_dir_sync_data(IDirSyncListener*         listener,
                          const TManifest&          newManifest,
                          const char*               out_hsyni_file,
                          const char*               out_hsynz_file,
                          size_t                    kMaxOpenFileNumber,
                          hpatch_TChecksum*         strongChecksumPlugin,
                          hsync_TDictCompress*      compressPlugin,
                          hsync_THsynz*             hsynzPlugin,
                          uint32_t kSyncBlockSize,size_t kSafeHashClashBit,size_t threadNum){
    assert(listener!=0);
    assert((out_hsynz_file!=0)&&(strlen(out_hsynz_file)>0));
    assert(kMaxOpenFileNumber>=kMaxOpenFileNumber_limit_min);
    kMaxOpenFileNumber-=2; // for out_hsyni_file & out_hsynz_file
    const std::vector<std::string>& newList=newManifest.pathList;
    std::vector<hpatch_StreamPos_t> newSizeList;
    std::vector<size_t> newExecuteList; //for linux etc

    for (size_t newi=0; newi<newList.size(); ++newi) {
        if ((!isDirName(newList[newi]))&&(listener->isExecuteFile(newList[newi])))
            newExecuteList.push_back(newi);
    }
    getRefList(newList,newSizeList);

    TNewDataSyncInfo_dir     dirInfo={0};
    std::vector<hpatch_byte> dirInfoSavedData;
    dirInfo.dir_newPathCount=newList.size();
    dirInfo.dir_newNameList_isCString=hpatch_FALSE;
    dirInfo.dir_utf8NewNameList=newList.data();
    dirInfo.dir_utf8NewRootPath=newManifest.rootPath.c_str();
    dirInfo.dir_newSizeList=newSizeList.data();
    dirInfo.dir_newExecuteCount=newExecuteList.size();
    dirInfo.dir_newExecuteIndexList=newExecuteList.data();
    TNewDataSyncInfo_dirWithHead_saveTo(&dirInfo,dirInfoSavedData);

    CFileResHandleLimit resLimit(kMaxOpenFileNumber,newList.size()+1);
    hpatch_StreamPos_t maxNewSize=dirInfoSavedData.size();
    {
        resLimit.addBufRes(dirInfoSavedData.data(),dirInfoSavedData.size());
        for (size_t i=0; i<newSizeList.size(); ++i) {
            hpatch_StreamPos_t newSize=newSizeList[i];
            maxNewSize=(newSize>maxNewSize)?newSize:maxNewSize;
            resLimit.addRes(newList[i],newSize);
        }
    }
    if (kSyncBlockSize>maxNewSize){
        kSyncBlockSize=(uint32_t)maxNewSize;
        if (kSyncBlockSize<kSyncBlockSize_min)
            kSyncBlockSize=kSyncBlockSize_min;
    }
    
    resLimit.open();
    CRefStream newRefStream;
    const size_t kAlignSize=kSyncBlockSize;
    newRefStream.open(resLimit.limit.streamList,resLimit.limit.streamCount,kAlignSize);
    
    bool isSafeHashClash=getStrongForHashClash(kSafeHashClashBit,newRefStream.stream->streamSize,kSyncBlockSize,
                                               strongChecksumPlugin->checksumByteSize()*8);
    listener->syncRefInfo(newManifest.rootPath.c_str(),newList.size(),newRefStream.stream->streamSize,
                          kSyncBlockSize,isSafeHashClash);
    checkv(isSafeHashClash); //warning as error

    CNewDataSyncInfo  newDataSyncInfo(strongChecksumPlugin,compressPlugin,
                                      newRefStream.stream->streamSize,kSyncBlockSize,kSafeHashClashBit);
    newDataSyncInfo.isDirSyncInfo=hpatch_TRUE;
    newDataSyncInfo.dirInfo=dirInfo;
    newDataSyncInfo.dirInfoSavedSize=dirInfoSavedData.size();
    
    CFileStreamOutput out_newSyncInfo(out_hsyni_file,~(hpatch_StreamPos_t)0);
    CFileStreamOutput out_newSyncData(out_hsynz_file,~(hpatch_StreamPos_t)0);
    
    _private_create_sync_data(&newDataSyncInfo, newRefStream.stream,&out_newSyncInfo.base,
                              &out_newSyncData.base, compressPlugin,hsynzPlugin,threadNum);
}



#endif

