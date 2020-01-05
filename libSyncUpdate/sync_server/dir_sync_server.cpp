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


void get_newManifest(IDirPathIgnore* filter,const char* newDir,TManifest& out_newManifest){
    assert(hpatch_getIsDirName(newDir));
    out_newManifest.rootPath=newDir;
    out_newManifest.pathList.clear();
    getDirAllPathList(out_newManifest.rootPath,out_newManifest.pathList,filter,false);
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
                          const char*               outNewSyncInfoFile,
                          const char*               outNewSyncDataFile,
                          const hdiff_TCompress*    compressPlugin,
                          hpatch_TChecksum*         strongChecksumPlugin,
                          size_t                    kMaxOpenFileNumber,
                          uint32_t kMatchBlockSize,size_t threadNum){
    const char* kDirSyncUpdateTypeVersion = "DirSyncUpdate19";
    assert(listener!=0);
    assert((outNewSyncDataFile!=0)&&(strlen(outNewSyncDataFile)>0));
    assert(kMaxOpenFileNumber>=kMaxOpenFileNumber_limit_min);
    kMaxOpenFileNumber-=2; // for outNewSyncInfoFile & outNewSyncDataFile
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
    checkv(!isMatchBlockSizeWarning);
    
    //serialize headData
    std::vector<TByte> buf;
    size_t newPathSumSize=pushNameList(buf,newManifest.rootPath,newList);
    packList(buf,newSizeList);
    packIncList(buf,newExecuteList);
    
    std::vector<TByte> head;
    {//head info
        pushTypes(head,kDirSyncUpdateTypeVersion,compressPlugin,strongChecksumPlugin);
        packUInt(head,newList.size());
        packUInt(head,newPathSumSize);
        packUInt(head,kAlignSize);
        packUInt(head,newRefStream.stream->streamSize); //same as syncInfo::newDataSize
        packUInt(head,newExecuteList.size());     swapClear(newExecuteList);
    }
    std::vector<TByte> privateExternData;//now empty
    {//reservedDataSize
        //head +
        packUInt(head,privateExternData.size());
    }
    std::vector<TByte> externData;
    {//externData size
        listener->externData(externData);
        //head +
        packUInt(head,externData.size());
    }
    {//compress buf
        std::vector<TByte> cmbuf;
        if (compressPlugin){
            cmbuf.resize((size_t)compressPlugin->maxCompressedSize(buf.size()));
            size_t compressedSize=hdiff_compress_mem(compressPlugin,cmbuf.data(),cmbuf.data()+cmbuf.size(),
                                                     buf.data(),buf.data()+buf.size());
            checkv(compressedSize>0);
            if (compressedSize>=buf.size()) compressedSize=0; //not compressed
            cmbuf.resize(compressedSize);
        }
        
        //head +
        packUInt(head,buf.size());
        packUInt(head,cmbuf.size());
        if (cmbuf.size()>0){
            swapClear(buf);
            buf.swap(cmbuf);
        }
    }
    {//dirSyncHeadSize
        hpatch_StreamPos_t dirSyncHeadSize= (hpatch_StreamPos_t)head.size() + sizeof(dirSyncHeadSize)
                                           + privateExternData.size() + externData.size() + buf.size()
                                           + kPartStrongChecksumByteSize;
        //head +
        pushUInt(head,dirSyncHeadSize);
        //end head info
    }
    
    //privateExtern data
    pushBack(head,privateExternData);   swapClear(privateExternData);
    //externData data
    listener->externDataPosInSyncInfoStream(head.size(),externData.size());
    pushBack(head,externData);          swapClear(externData);
    //headData data
    pushBack(head,buf);                 swapClear(buf);
    
    {//checksum dir head
        CChecksum checksumHead(strongChecksumPlugin);
        checksumHead.append(head);
        checksumHead.appendEnd();
        toSyncPartChecksum(checksumHead.checksum.data(),
                       checksumHead.checksum.data(),checksumHead.checksum.size());
        
        pushBack(head,checksumHead.checksum.data(),kPartStrongChecksumByteSize);
        //end head
    }
    
    CFileStreamOutput out_newSyncInfo(outNewSyncInfoFile,~(hpatch_StreamPos_t)0);
    hpatch_StreamPos_t writeToPos=0;
    writeStream(&out_newSyncInfo.base,writeToPos,head);     swapClear(head);
    TOffsetStreamOutput ofStream(&out_newSyncInfo.base,writeToPos);
    
    CFileStreamOutput out_newSyncData(outNewSyncDataFile,~(hpatch_StreamPos_t)0);
    
    create_sync_data(newRefStream.stream,&ofStream,&out_newSyncData.base,
                     compressPlugin,strongChecksumPlugin,kMatchBlockSize,threadNum);
}



#endif

