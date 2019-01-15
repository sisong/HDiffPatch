// dir_diff.cpp
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
#include "dir_diff.h"
#include <algorithm> //sort
#include <map>
#include <set>
#include "../../libHDiffPatch/HDiff/private_diff/mem_buf.h"
#include "../../libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.h"
#include "../../libHDiffPatch/HDiff/private_diff/pack_uint.h"
#include "../../libHDiffPatch/HDiff/diff.h"
#include "../../file_for_patch.h"
#include "../file_for_dirDiff.h"
#include "../dir_patch/ref_stream.h"
#include "../dir_patch/dir_patch.h"
#include "../dir_patch/res_handle_limit.h"
using namespace hdiff_private;

static const char* kVersionType="DirDiff19&";

#define kFileIOBufSize      (1024*64)
#define check(value,info) if (!(value)) throw std::runtime_error(info);

#define hash_value_t                uint64_t
#define hash_begin(ph)              { (*(ph))=ADLER_INITIAL; }
#define hash_append(ph,pvalues,n)   { (*(ph))=fast_adler64_append(*(ph),pvalues,n); }
#define hash_end(ph)                {}


void assignDirTag(std::string& dir_utf8){
    if (dir_utf8.empty()||(dir_utf8[dir_utf8.size()-1]!=kPatch_dirSeparator))
        dir_utf8.push_back(kPatch_dirSeparator);
}

static inline bool isDirName(const std::string& path_utf8){
    return 0!=getIsDirName(path_utf8.c_str());
}

static void formatDirTagForSave(std::string& path_utf8){
    if (kPatch_dirSeparator==kPatch_dirSeparator_saved) return;
    for (size_t i=0;i<path_utf8.size();++i){
        if (path_utf8[i]!=kPatch_dirSeparator)
            continue;
        else
            path_utf8[i]=kPatch_dirSeparator_saved;
    }
}

bool IDirFilter::pathIsEndWith(const std::string& pathName,const char* testEndTag){
    size_t nameSize=pathName.size();
    size_t testSize=strlen(testEndTag);
    if (nameSize<testSize) return false;
    return 0==memcmp(pathName.c_str()+(nameSize-testSize),testEndTag,testSize);
}
bool IDirFilter::pathNameIs(const std::string& pathName_utf8,const char* testPathName){ //without dir path
    if (!pathIsEndWith(pathName_utf8,testPathName)) return false;
    size_t nameSize=pathName_utf8.size();
    size_t testSize=strlen(testPathName);
    return (nameSize==testSize) || (pathName_utf8[nameSize-testSize-1]==kPatch_dirSeparator);
}

struct CFileStreamInput:public TFileStreamInput{
    inline CFileStreamInput(){ TFileStreamInput_init(this); }
    inline void open(const std::string& fileName){
        assert(this->base.streamImport==0);
        check(TFileStreamInput_open(this,fileName.c_str()),"open file \""+fileName+"\" error!"); }
    inline CFileStreamInput(const std::string& fileName){
        TFileStreamInput_init(this); open(fileName); }
    inline ~CFileStreamInput(){
        check(TFileStreamInput_close(this),"close file error!"); }
};

struct CRefStream:public TRefStream{
    inline CRefStream(){ TRefStream_init(this); }
    inline void open(const hpatch_TStreamInput** refList,size_t refCount){
        check(TRefStream_open(this,refList,refCount),"TRefStream_open() refList error!");
    }
    inline ~CRefStream(){ TRefStream_close(this); }
};

struct TOffsetStreamOutput:public hpatch_TStreamOutput{
    inline explicit TOffsetStreamOutput(const hpatch_TStreamOutput* base,hpatch_StreamPos_t offset)
    :_base(base),_offset(offset),outSize(0){
        assert(offset<=base->streamSize);
        this->streamImport=this;
        this->streamSize=base->streamSize-offset;
        this->write=_write;
    }
    const hpatch_TStreamOutput* _base;
    hpatch_StreamPos_t          _offset;
    hpatch_StreamPos_t          outSize;
    static hpatch_BOOL _write(const hpatch_TStreamOutput* stream,const hpatch_StreamPos_t writeToPos,
                       const unsigned char* data,const unsigned char* data_end){
        TOffsetStreamOutput* self=(TOffsetStreamOutput*)stream->streamImport;
        hpatch_StreamPos_t newSize=writeToPos+(data_end-data);
        if (newSize>self->outSize) self->outSize=newSize;
        return self->_base->write(self->_base,self->_offset+writeToPos,data,data_end);
    }
};

template <class TVector>
static inline void clearVector(TVector& v){ TVector _t; v.swap(_t); }


struct CDir{
    inline CDir(const char* dir):handle(0){ handle=dirOpenForRead(dir); }
    inline ~CDir(){ dirClose(handle); }
    TDirHandle handle;
};

void getDirFileList(const std::string& dirPath,std::vector<std::string>& out_list,IDirFilter* filter){
    assert(isDirName(dirPath));
    CDir dir(dirPath.c_str());
    check((dir.handle!=0),"dirOpenForRead \""+dirPath+"\" error!");
    out_list.push_back(dirPath); //add dir
    while (true) {
        TPathType  type;
        const char* path=0;
        check(dirNext(dir.handle,&type,&path),"dirNext \""+dirPath+"\" error!");
        if (path==0) break; //finish
        if ((0==strcmp(path,""))||(0==strcmp(path,"."))||(0==strcmp(path,"..")))
            continue;
        std::string subName(dirPath+path);
        assert(!isDirName(subName));
        if (type==kPathType_dir){
            assignDirTag(subName);
            if (!filter->isNeedFilter(subName)){
                getDirFileList(subName,out_list,filter);
            }
        }else{// if (type==kPathType_file){
            if (!filter->isNeedFilter(subName)){
                out_list.push_back(subName); //add file
            }
        }
    }
}

static hash_value_t getFileHash(const std::string& fileName,hpatch_StreamPos_t* out_fileSize){
    CFileStreamInput f(fileName);
    TAutoMem  mem(kFileIOBufSize);
    hash_value_t result;
    hash_begin(&result);
    *out_fileSize=f.base.streamSize;
    for (hpatch_StreamPos_t pos=0; pos<f.base.streamSize;) {
        size_t readLen=kFileIOBufSize;
        if (pos+readLen>f.base.streamSize)
            readLen=(size_t)(f.base.streamSize-pos);
        check(f.base.read(&f.base,pos,mem.data(),mem.data()+readLen),
              "read file \""+fileName+"\" error!");
        hash_append(&result,mem.data(),readLen);
        pos+=readLen;
    }
    hash_end(&result);
    return result;
}

static bool fileData_isSame(const std::string& file_x,const std::string& file_y){
    CFileStreamInput f_x(file_x);
    CFileStreamInput f_y(file_y);
    if (f_x.base.streamSize!=f_y.base.streamSize)
        return false;
    TAutoMem  mem(kFileIOBufSize*2);
    for (hpatch_StreamPos_t pos=0; pos<f_x.base.streamSize;) {
        size_t readLen=kFileIOBufSize;
        if (pos+readLen>f_x.base.streamSize)
            readLen=(size_t)(f_x.base.streamSize-pos);
        check(f_x.base.read(&f_x.base,pos,mem.data(),mem.data()+readLen),
              "read file \""+file_x+"\" error!");
        check(f_y.base.read(&f_y.base,pos,mem.data()+readLen,mem.data()+readLen*2),
              "read file \""+file_y+"\" error!");
        if (0!=memcmp(mem.data(),mem.data()+readLen,readLen))
            return false;
        pos+=readLen;
    }
    return true;
}

void sortDirFileList(std::vector<std::string>& fileList){
    std::sort(fileList.begin(),fileList.end());
}

static void pushIncList(std::vector<TByte>& out_data,const std::vector<size_t>& list){
    size_t backValue=~(size_t)0;
    for (size_t i=0;i<list.size();++i){
        size_t curValue=list[i];
        assert(curValue>=(size_t)(backValue+1));
        packUInt(out_data,(size_t)(curValue-(size_t)(backValue+1)));
        backValue=curValue;
    }
}

static void pushList(std::vector<TByte>& out_data,const std::vector<hpatch_StreamPos_t>& list){
    for (size_t i=0;i<list.size();++i){
        packUInt(out_data,list[i]);
    }
}

static void pushSamePairList(std::vector<TByte>& out_data,const std::vector<TSameFileIndexPair>& pairs){
    size_t backPairNew=~(size_t)0;
    size_t backPairOld=~(size_t)0;
    for (size_t i=0;i<pairs.size();++i){
        size_t curNewValue=pairs[i].newIndex;
        assert(curNewValue>=(size_t)(backPairNew+1));
        packUInt(out_data,(size_t)(curNewValue-(size_t)(backPairNew+1)));
        backPairNew=curNewValue;
        
        size_t curOldValue=pairs[i].oldIndex;
        if (curOldValue>=(size_t)(backPairOld+1))
            packUIntWithTag(out_data,(size_t)(curOldValue-(size_t)(backPairOld+1)),0,1);
        else
            packUIntWithTag(out_data,(size_t)((size_t)(backPairOld+1)-curOldValue),1,1);
        backPairOld=curOldValue;
    }
}

static size_t pushNameList(std::vector<TByte>& out_data,const std::string& rootPath,
                           const std::vector<std::string>& nameList,IDirDiffListener* listener){
    const size_t rootLen=rootPath.size();
    std::string utf8;
    size_t outSize=0;
    for (size_t i=0;i<nameList.size();++i){
        const std::string& name=nameList[i];
        const size_t nameSize=name.size();
        assert(nameSize>=rootLen);
        assert(0==memcmp(name.data(),rootPath.data(),rootLen));
        const char* subName=name.c_str()+rootLen;
        const char* subNameEnd=subName+(nameSize-rootLen);
        utf8.assign(subName,subNameEnd);
        formatDirTagForSave(utf8);
        size_t writeLen=utf8.size()+1; // '\0'
        out_data.insert(out_data.end(),utf8.c_str(),utf8.c_str()+writeLen);
        outSize+=writeLen;
    }
    return outSize;
}


struct _TCmp_byHit {
    inline _TCmp_byHit(const char* _newPath,const std::vector<std::string>& _oldList,
                       size_t _oldRootPathSize,const std::vector<size_t>& _oldHitList)
    :newPath(_newPath),oldList(_oldList),oldRootPathSize(_oldRootPathSize),oldHitList(_oldHitList){}
    inline bool operator()(size_t ix,size_t iy)const{
        size_t vx=ToValue(ix);
        size_t vy=ToValue(iy);
        return vx>vy;
    }
    size_t ToValue(size_t index)const{
        const std::string& oldName=oldList[index];
        bool  isEqPath=(0==strcmp(newPath,oldName.c_str()+oldRootPathSize));
        size_t hitCount=oldHitList[index];
        
        const size_t kMaxValue=(size_t)(-1);
        if (hitCount==0){
            if (isEqPath) return kMaxValue;
            else return kMaxValue-1;
        }else{
            if (isEqPath) return kMaxValue-2;
            else return hitCount;
        }
    }
    const char* newPath;
    const std::vector<std::string>& oldList;
    const size_t oldRootPathSize;
    const std::vector<size_t>& oldHitList;
};

static void getRefList(const std::string& oldRootPath,const std::string& newRootPath,
                       const std::vector<std::string>& oldList,const std::vector<std::string>& newList,
                       std::vector<hpatch_StreamPos_t>& out_oldSizeList,
                       std::vector<hpatch_StreamPos_t>& out_newSizeList,
                       std::vector<TSameFileIndexPair>& out_dataSamePairList,
                       std::vector<size_t>& out_oldRefList,std::vector<size_t>& out_newRefList){
    typedef std::multimap<hash_value_t,size_t> TMap;
    TMap hashMap;
    std::set<size_t> oldRefSet;
    out_oldSizeList.assign(oldList.size(),0);
    out_newSizeList.assign(newList.size(),0);
    for (size_t i=0; i<oldList.size(); ++i) {
        const std::string& fileName=oldList[i];
        if (isDirName(fileName)) continue;
        hpatch_StreamPos_t fileSize=0;
        hash_value_t hash=getFileHash(fileName,&fileSize);
        out_oldSizeList[i]=fileSize;
        if (fileSize==0) continue;
        hashMap.insert(TMap::value_type(hash,i));
        oldRefSet.insert(i);
    }
    out_dataSamePairList.clear();
    out_newRefList.clear();
    std::vector<size_t> oldHitList(oldList.size(),0);
    for (size_t newi=0; newi<newList.size(); ++newi){
        const std::string& fileName=newList[newi];
        if (isDirName(fileName)) continue;
        hpatch_StreamPos_t fileSize=0;
        hash_value_t hash=getFileHash(fileName,&fileSize);
        out_newSizeList[newi]=fileSize;
        if (fileSize==0) continue;
        
        bool isFoundSame=false;
        size_t oldIndex=(size_t)(-1);
        std::pair<TMap::const_iterator,TMap::const_iterator> range=hashMap.equal_range(hash);
        std::vector<size_t> oldHashIndexs;
        for (;range.first!=range.second;++range.first)
            oldHashIndexs.push_back(range.first->second);
        const char* newPath=fileName.c_str()+newRootPath.size();
        if (oldHashIndexs.size()>1)
            std::sort(oldHashIndexs.begin(),oldHashIndexs.end(),
                      _TCmp_byHit(newPath,oldList,oldRootPath.size(),oldHitList));
        for (size_t oldi=0;oldi<oldHashIndexs.size();++oldi){
            size_t curOldIndex=oldHashIndexs[oldi];
            const std::string& oldName=oldList[curOldIndex];
            if (fileData_isSame(oldName,fileName)){
                isFoundSame=true;
                oldIndex=curOldIndex;
                break;
            }
        }
        
        if (isFoundSame){
            ++oldHitList[oldIndex];
            oldRefSet.erase(oldIndex);
            TSameFileIndexPair pair; pair.newIndex=newi; pair.oldIndex=oldIndex;
            out_dataSamePairList.push_back(pair);
        }else{
            out_newRefList.push_back(newi);
        }
    }
    out_oldRefList.assign(oldRefSet.begin(),oldRefSet.end());
    std::sort(out_oldRefList.begin(),out_oldRefList.end());
}

struct CFileResHandleLimit{
    inline CFileResHandleLimit(){ TResHandleLimit_init(&limit); }
    inline ~CFileResHandleLimit() { close(); }
    void open(size_t limitMaxOpenCount,std::vector<IResHandle>& _resList){
        assert(fileList.empty());
        fileList.resize(_resList.size());
        memset(fileList.data(),0,sizeof(CFile)*_resList.size());
        for (size_t i=0;i<_resList.size();++i){
            fileList[i].fileName=(const char*)_resList[i].resImport;
            _resList[i].resImport=&fileList[i];
        }
        check(TResHandleLimit_open(&limit,limitMaxOpenCount,_resList.data(),
                                   _resList.size()),"TResHandleLimit_open error!");
    }
    void close(){
        check(TResHandleLimit_close(&limit),"TResHandleLimit_close error!");
    }
    
    struct CFile:public TFileStreamInput{
        std::string  fileName;
    };
    TResHandleLimit     limit;
    std::vector<CFile>  fileList;
    static hpatch_BOOL openRes(struct IResHandle* res,hpatch_TStreamInput** out_stream){
        CFile* self=(CFile*)res->resImport;
        assert(self->m_file==0);
        check(TFileStreamInput_open(self,self->fileName.c_str()),"CFileResHandleLimit open file error!");
        *out_stream=&self->base;
        return hpatch_TRUE;
    }
    static hpatch_BOOL closeRes(struct IResHandle* res,const hpatch_TStreamInput* stream){
        CFile* self=(CFile*)res->resImport;
        assert(stream==&self->base);
        check(TFileStreamInput_close(self),"CFileResHandleLimit close file error!");
        return hpatch_TRUE;
    }
};

void dir_diff(IDirDiffListener* listener,const std::string& oldPath,const std::string& newPath,
              const hpatch_TStreamOutput* outDiffStream,bool isLoadAll,size_t matchValue,
              hdiff_TStreamCompress* streamCompressPlugin,hdiff_TCompress* compressPlugin,
              size_t kMaxOpenFileNumber){
    assert(listener!=0);
    assert(kMaxOpenFileNumber>=kMaxOpenFileNumber_limit_min);
    kMaxOpenFileNumber-=1; // for outDiffStream
    std::vector<std::string> oldList;
    std::vector<std::string> newList;
    const bool oldIsDir=isDirName(oldPath);
    const bool newIsDir=isDirName(newPath);
    {
        if (oldIsDir){
            getDirFileList(oldPath,oldList,listener);
            sortDirFileList(oldList);
        }else{
            oldList.push_back(oldPath);
        }
        if (newIsDir){
            getDirFileList(newPath,newList,listener);
            sortDirFileList(newList);
        }else{
            newList.push_back(newPath);
        }
        listener->diffPathList(oldList,newList);
    }
    
    std::vector<hpatch_StreamPos_t> oldSizeList;
    std::vector<hpatch_StreamPos_t> newSizeList;
    std::vector<TSameFileIndexPair> dataSamePairList;     //new map to same old
    std::vector<size_t> oldRefIList;
    std::vector<size_t> newRefIList;
    getRefList(oldPath,newPath,oldList,newList,
               oldSizeList,newSizeList,dataSamePairList,oldRefIList,newRefIList);
    std::vector<hpatch_StreamPos_t> newRefSizeList;
    std::vector<IResHandle>         resList;
    {
        resList.resize(oldRefIList.size()+newRefIList.size());
        IResHandle* res=resList.data();
        newRefSizeList.resize(newRefIList.size());
        for (size_t i=0; i<oldRefIList.size(); ++i) {
            size_t fi=oldRefIList[i];
            res->open=CFileResHandleLimit::openRes;
            res->close=CFileResHandleLimit::closeRes;
            res->resImport=(void*)oldList[fi].c_str();
            res->resStreamSize=oldSizeList[fi];
            ++res;
        }
        for (size_t i=0; i<newRefIList.size(); ++i) {
            size_t fi=newRefIList[i];
            res->open=CFileResHandleLimit::openRes;
            res->close=CFileResHandleLimit::closeRes;
            res->resImport=(void*)newList[fi].c_str();
            res->resStreamSize=newSizeList[fi];
            ++res;
            newRefSizeList[i]=newSizeList[fi];
        }
    }
    size_t sameFilePairCount=dataSamePairList.size();
    CFileResHandleLimit resLimit;
    resLimit.open(kMaxOpenFileNumber,resList);
    CRefStream oldRefStream;
    CRefStream newRefStream;
    oldRefStream.open(resLimit.limit.streamList,oldRefIList.size());
    newRefStream.open(resLimit.limit.streamList+oldRefIList.size(),newRefIList.size());
    listener->diffRefInfo(oldList.size(),newList.size(), sameFilePairCount,
                          oldRefIList.size(),newRefIList.size(),
                          oldRefStream.stream->streamSize,newRefStream.stream->streamSize);
    
    //serialize headData
    std::vector<TByte> headData;
    size_t oldPathSumSize=pushNameList(headData,oldPath,oldList,listener);
    size_t newPathSumSize=pushNameList(headData,newPath,newList,listener);
    pushIncList(headData,oldRefIList);
    pushIncList(headData,newRefIList);
    pushList(headData,newRefSizeList);
    pushSamePairList(headData,dataSamePairList);
    std::vector<TByte> headCode;
    if (compressPlugin){
        headCode.resize(compressPlugin->maxCompressedSize(compressPlugin,headData.size()));
        size_t codeSize=compressPlugin->compress(compressPlugin,headCode.data(),headCode.data()+headCode.size(),
                                                 headData.data(),headData.data()+headData.size());
        if ((0<codeSize)&&(codeSize<headData.size()))
            headCode.resize(codeSize);//compress ok
        else
            clearVector(headCode); //compress error or give up
    }
    
    //serialize  dir diff data
    std::vector<TByte> out_data;
    {//type version
        pushBack(out_data,(const TByte*)kVersionType,(const TByte*)kVersionType+strlen(kVersionType));
    }
    {//compressType
        const char* compressType="";
        if (compressPlugin)
            compressType=compressPlugin->compressType(compressPlugin);
        size_t compressTypeLen=strlen(compressType);
        check(compressTypeLen<=hpatch_kMaxCompressTypeLength,"compressTypeLen error!");
        pushBack(out_data,(const TByte*)compressType,(const TByte*)compressType+compressTypeLen+1); //'\0'
    }
    //head info
    const TByte kPatchModel=0;
    packUInt(out_data,kPatchModel);
    packUInt(out_data,oldIsDir?1:0);
    packUInt(out_data,newIsDir?1:0);
    packUInt(out_data,oldList.size());          clearVector(oldList);
    packUInt(out_data,newList.size());          clearVector(newList);
    packUInt(out_data,oldPathSumSize);
    packUInt(out_data,newPathSumSize);
    packUInt(out_data,oldRefIList.size());      clearVector(oldRefIList);
    packUInt(out_data,newRefIList.size());      clearVector(newRefIList);
    packUInt(out_data,sameFilePairCount);       clearVector(dataSamePairList);
    packUInt(out_data,headData.size());
    packUInt(out_data,headCode.size());
    //externData size
    std::vector<TByte> externData;
    listener->externData(externData);
    packUInt(out_data,externData.size());
    
    //begin write
    hpatch_StreamPos_t writeToPos=0;
    #define _pushv(v) { check(outDiffStream->write(outDiffStream,writeToPos,v.data(),v.data()+v.size()), \
                              "write diff data " #v " error!"); \
                        writeToPos+=v.size();  clearVector(v); }
    _pushv(out_data);
    if (headCode.size()>0){
        _pushv(headCode);
    }else{
        _pushv(headData);
    }
    listener->externDataPosInDiffStream(writeToPos);
    _pushv(externData);

    //diff data
    listener->runHDiffBegin();
    hpatch_StreamPos_t diffDataSize=0;
    if (isLoadAll){
        hpatch_StreamPos_t memSize=newRefStream.stream->streamSize+oldRefStream.stream->streamSize;
        check(memSize==(size_t)memSize,"alloc size overflow error!");
        TAutoMem  mem((size_t)memSize);
        TByte* newData=mem.data();
        TByte* oldData=mem.data()+newRefStream.stream->streamSize;
        check(newRefStream.stream->read(newRefStream.stream,0,newData,
                                        newData+newRefStream.stream->streamSize),"read new file error!");
        check(oldRefStream.stream->read(oldRefStream.stream,0,oldData,
                                        oldData+oldRefStream.stream->streamSize),"read old file error!");
        resLimit.close(); //close files
        std::vector<unsigned char> out_diff;
        create_compressed_diff(newData,newData+newRefStream.stream->streamSize,
                               oldData,oldData+oldRefStream.stream->streamSize,
                               out_diff,compressPlugin,(int)matchValue);
        diffDataSize=out_diff.size();
        _pushv(out_diff);
    }else{
        TOffsetStreamOutput ofStream(outDiffStream,writeToPos);
        create_compressed_diff_stream(newRefStream.stream,oldRefStream.stream,&ofStream,
                                      streamCompressPlugin,matchValue);
        diffDataSize=ofStream.outSize;
    }
    listener->runHDiffEnd(diffDataSize);
    #undef _pushv
}


void resave_compressed_dirdiff(const hpatch_TStreamInput*  in_diff,
                               hpatch_TDecompress*         decompressPlugin,
                               const hpatch_TStreamOutput* out_diff,
                               hdiff_TStreamCompress*      compressPlugin){
    //todo:
    
}
