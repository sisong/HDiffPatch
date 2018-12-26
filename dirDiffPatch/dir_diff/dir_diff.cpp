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
#include "../file_for_dir.h"
#include "../../file_for_patch.h"
#include "../../libHDiffPatch/HDiff/private_diff/mem_buf.h"
#include "../../libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.h"
#include "../../libHDiffPatch/HDiff/private_diff/pack_uint.h"
#include "../../libHDiffPatch/HDiff/diff.h"
#include "../dir_patch/ref_stream.h"
using namespace hdiff_private;

static const char* kVersionType="DirDiff19&";

#define kFileIOBufSize      (64*1024)
#define kFileIOBestMaxSize  (1024*1024)
#define check(value,info) if (!(value)) throw new std::runtime_error(info);

#define hash_value_t                uint64_t
#define hash_begin(ph)              { (*(ph))=ADLER_INITIAL; }
#define hash_append(ph,pvalues,n)   { (*(ph))=fast_adler64_append(*(ph),pvalues,n); }
#define hash_end(ph)                {}


bool IDirFilter::pathIsEndWith(const std::string& pathName,const char* testEndTag){
    size_t nameSize=pathName.size();
    size_t testSize=strlen(testEndTag);
    if (nameSize<testSize) return false;
    return 0==memcmp(pathName.c_str()+(nameSize-testSize),testEndTag,testSize);
}
bool IDirFilter::pathIs(const std::string& pathName,const char* testPathName){ //without dir path
    if (!pathIsEndWith(pathName,testPathName)) return false;
    size_t nameSize=pathName.size();
    size_t testSize=strlen(testPathName);
    return (nameSize==testSize) || (pathName[nameSize-testSize-1]==kPatch_dirTag);
}

struct CFileStreamInput:public TFileStreamInput{
    inline CFileStreamInput(){ TFileStreamInput_init(this); }
    inline void open(const std::string& fileName){
        assert(this->base.streamHandle==0);
        check(TFileStreamInput_open(this,fileName.c_str()),"open file \""+fileName+"\" error!"); }
    inline CFileStreamInput(const std::string& fileName){
        TFileStreamInput_init(this); open(fileName); }
    inline ~CFileStreamInput(){
        check(TFileStreamInput_close(this),"close file error!"); }
};

struct CRefStream:public RefStream{
    inline CRefStream(){ RefStream_init(this); }
    inline void open(const hpatch_TStreamInput** refList,size_t refCount){
        check(RefStream_open(this,refList,refCount),"RefStream_open() refList error!");
    }
    inline ~CRefStream(){ RefStream_close(this); }
};

struct TOffsetStreamOutput:public hpatch_TStreamOutput{
    inline explicit TOffsetStreamOutput(const hpatch_TStreamOutput* base,hpatch_StreamPos_t offset)
    :_base(base),_offset(offset),outSize(0){
        assert(offset<=base->streamSize);
        this->streamHandle=this;
        this->streamSize=base->streamSize-offset;
        this->write=_write;
    }
    const hpatch_TStreamOutput* _base;
    hpatch_StreamPos_t          _offset;
    hpatch_StreamPos_t          outSize;
    static long _write(hpatch_TStreamOutputHandle streamHandle,const hpatch_StreamPos_t writeToPos,
                       const unsigned char* data,const unsigned char* data_end){
        TOffsetStreamOutput* self=(TOffsetStreamOutput*)streamHandle;
        hpatch_StreamPos_t newSize=writeToPos+(data_end-data);
        if (newSize>self->outSize) self->outSize=newSize;
        return self->_base->write(self->_base->streamHandle,self->_offset+writeToPos,data,data_end);
    }
};

bool readAll(const hpatch_TStreamInput* stream,hpatch_StreamPos_t pos,
             TByte* dst,TByte* dstEnd){
    while (dst<dstEnd) {
        size_t readLen=dstEnd-dst;
        if (readLen>kFileIOBestMaxSize) readLen=kFileIOBestMaxSize;
        if ((long)readLen!=stream->read(stream->streamHandle,pos,dst,dst+readLen))
            return false;
        dst+=readLen;
        pos+=readLen;
    }
    return true;
}
bool writeAll(const hpatch_TStreamOutput* stream,hpatch_StreamPos_t pos,
              const TByte* src,const TByte* srcEnd){
    while (src<srcEnd) {
        size_t writeLen=srcEnd-src;
        if (writeLen>kFileIOBestMaxSize) writeLen=kFileIOBestMaxSize;
        if ((long)writeLen!=stream->write(stream->streamHandle,pos,src,src+writeLen))
            return false;
        src+=writeLen;
        pos+=writeLen;
    }
    return true;
}

void assignDirTag(std::string& dir){
    if (dir.empty()||(dir.back()!=kPatch_dirTag))
        dir.push_back(kPatch_dirTag);
}

bool isDirName(const std::string& path){
    return (!path.empty())&&(path.back()==kPatch_dirTag);
}

template <class TVector>
static inline void clearVector(TVector& v){ TVector _t; v.swap(_t); }

bool getDirFileList(const std::string& dir,std::vector<std::string>& out_list,IDirFilter* filter){
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
        std::string subName(dir+path);
        if (type==kPathType_file){
            assert(subName.back()!=kPatch_dirTag);
            if (!filter->isNeedFilter(subName)){
                isHaveSub=true;
                out_list.push_back(subName); //add file
            }
        }else{// if (type==kPathType_dir){
            assignDirTag(subName);
            if (!filter->isNeedFilter(subName)){
                isHaveSub=true;
                if (!getDirFileList(subName,out_list,filter)) return false;
            }
        }
    }
    if (!isHaveSub)
        out_list.push_back(dir); //add empty dir
    dirClose(dirh);
    return true;
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
            readLen=f.base.streamSize-pos;
        check((long)readLen==f.base.read(&f.base.streamHandle,pos,mem.data(),mem.data()+readLen),
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
            readLen=f_x.base.streamSize-pos;
        check((long)readLen==f_x.base.read(&f_x.base.streamHandle,pos,mem.data(),mem.data()+readLen),
              "read file \""+file_x+"\" error!");
        check((long)readLen==f_y.base.read(&f_y.base.streamHandle,pos,mem.data()+readLen,mem.data()+readLen*2),
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
        packUInt(out_data,(size_t)(curValue-(size_t)(backValue+1)));
        backValue=curValue;
    }
}

static void pushSamePairList(std::vector<TByte>& out_data,const std::vector<size_t>& pairs){
    size_t backPairNew=~(size_t)0;
    size_t backPairOld=~(size_t)0;
    assert(pairs.size()==pairs.size()/2*2);
    for (size_t i=0;i<pairs.size();i+=2){
        size_t curNewValue=pairs[i];
        packUInt(out_data,(size_t)(curNewValue-(size_t)(backPairNew+1)));
        backPairNew=curNewValue;
        
        size_t curOldValue=pairs[i+1];
        if (curOldValue>=(size_t)(backPairOld+1))
            packUIntWithTag(out_data,(size_t)(curOldValue-(size_t)(backPairOld+1)),0,1);
        else
            packUIntWithTag(out_data,(size_t)((size_t)(backPairOld+1)-curOldValue),1,1);
        backPairOld=curOldValue;
    }
}

static bool isAsciiString(const char* str,const char* strEnd){
    for (;str<strEnd;++str) {
        if (isascii(*str))
            continue;
        else
            return false;
    }
    return true;
}

static void pushNameList(std::vector<TByte>& out_data,const std::string& rootPatch,
                         const std::vector<std::string>& nameList,IDirDiffListener* listener){
    const size_t rootLen=rootPatch.size();
    std::string temp;
    std::string utf8;
    std::string last_utf8="";
    for (size_t i=0;i<nameList.size();++i){
        const std::string& name=nameList[i];
        const size_t nameSize=name.size();
        assert(nameSize>=rootLen);
        assert(0==memcmp(name.data(),rootPatch.data(),rootLen));
        const char* subName=name.c_str()+rootLen;
        const char* subNameEnd=subName+(nameSize-rootLen);
        if (isAsciiString(subName,subNameEnd)){
            utf8.assign(subName,subNameEnd);
        }else{
            temp.assign(subName,subNameEnd);
            listener->file_name_to_utf8(temp,utf8);
        }
        size_t eqLen=0;
        while ((eqLen<utf8.size())&&(utf8[eqLen]==last_utf8[eqLen]))
               ++eqLen;
        packUInt(out_data,eqLen);
        out_data.insert(out_data.end(),utf8.c_str()+eqLen,utf8.c_str()+utf8.size()+1); // '\0'
        last_utf8.swap(utf8);
    }
}

static void getRefList(const std::vector<std::string>& newList,const std::vector<std::string>& oldList,
                       std::vector<size_t>& out_samePairList,
                       std::vector<size_t>& out_newRefList,std::vector<size_t>& out_oldRefList){
    typedef std::multimap<hash_value_t,size_t> TMap;
    TMap hashMap;
    std::set<size_t> oldRefList;
    for (int i=0; i<oldList.size(); ++i) {
        const std::string& fileName=oldList[i];
        if (isDirName(fileName)) continue;
        hpatch_StreamPos_t fileSize=0;
        hash_value_t hash=getFileHash(fileName,&fileSize);
        if (fileSize==0) continue;
        hashMap.insert(TMap::value_type(hash,i));
        oldRefList.insert(i);
    }
    out_samePairList.clear();
    out_newRefList.clear();
    for (int i=0; i<newList.size(); ++i){
        const std::string& fileName=newList[i];
        if (isDirName(fileName)) continue;
        hpatch_StreamPos_t fileSize=0;
        hash_value_t hash=getFileHash(fileName,&fileSize);
        if (fileSize==0) continue;
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

void dir_diff(IDirDiffListener* listener,const std::string& oldPatch,const std::string& newPatch,
              const hpatch_TStreamOutput* outDiffStream,bool isLoadAll,size_t matchValue,
              hdiff_TStreamCompress* streamCompressPlugin,hdiff_TCompress* compressPlugin){
    assert(listener!=0);
    std::vector<std::string> newList;
    std::vector<std::string> oldList;
    const bool newIsDir=isDirName(newPatch);
    const bool oldIsDir=isDirName(oldPatch);
    {
        if (newIsDir){
            check(getDirFileList(newPatch,newList,listener),"new dir getDirFileList() error!");
            sortDirFileList(newList);
        }else{
            newList.push_back(newPatch);
        }
        if (oldIsDir){
            check(getDirFileList(oldPatch,oldList,listener),"old dir getDirFileList() error!");
            sortDirFileList(oldList);
        }else{
            oldList.push_back(oldPatch);
        }
        listener->diffFileList(newList,oldList);
    }

    std::vector<size_t> samePairList; //new map to same old
    std::vector<const hpatch_TStreamInput*> newRefSList;
    std::vector<const hpatch_TStreamInput*> oldRefSList;
    std::vector<size_t> newRefIList;
    std::vector<size_t> oldRefIList;
    std::vector<CFileStreamInput> _newRefList;
    std::vector<CFileStreamInput> _oldRefList;
    {
        getRefList(newList,oldList,samePairList,newRefIList,oldRefIList);
        _newRefList.resize(newRefIList.size());
        _oldRefList.resize(oldRefIList.size());
        oldRefSList.resize(oldRefIList.size());
        newRefSList.resize(newRefIList.size());
        for (size_t i=0; i<oldRefIList.size(); ++i) {
            _oldRefList[i].open(oldList[oldRefIList[i]]);
            oldRefSList[i]=&_oldRefList[i].base;
        }
        for (size_t i=0; i<newRefIList.size(); ++i) {
            _newRefList[i].open(newList[newRefIList[i]]);
            newRefSList[i]=&_newRefList[i].base;
        }
    }
    size_t sameFileCount=samePairList.size()/2;
    CRefStream newRefStream;
    CRefStream oldRefStream;
    newRefStream.open(newRefSList.data(),newRefSList.size());
    oldRefStream.open(oldRefSList.data(),oldRefSList.size());
    listener->refInfo(sameFileCount,newRefIList.size(),oldRefIList.size(),
                      newRefStream.stream->streamSize,oldRefStream.stream->streamSize);
    
    std::vector<TByte> headData;
    pushNameList(headData,newPatch,newList,listener);
    pushNameList(headData,oldPatch,oldList,listener);
    pushSamePairList(headData,samePairList);
    pushIncList(headData,newRefIList);
    pushIncList(headData,oldRefIList);
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
    packUInt(out_data,newIsDir?1:0);
    packUInt(out_data,oldIsDir?1:0);
    packUInt(out_data,newList.size());      clearVector(newList);
    packUInt(out_data,oldList.size());      clearVector(oldList);
    packUInt(out_data,sameFileCount);       clearVector(samePairList);
    packUInt(out_data,newRefIList.size());  clearVector(newRefIList);
    packUInt(out_data,oldRefIList.size());  clearVector(oldRefIList);
    packUInt(out_data,newRefStream.stream->streamSize);
    packUInt(out_data,oldRefStream.stream->streamSize);
    packUInt(out_data,headData.size());
    packUInt(out_data,headCode.size());
    //externData size
    std::vector<TByte> externData;
    listener->externData(externData);
    packUInt(out_data,externData.size());
    
    hpatch_StreamPos_t writeToPos=0;
    #define _pushv(v){  check(writeAll(outDiffStream,writeToPos,v.data(),v.data()+v.size()), \
                              "write diff data " #v " error!"); \
                        writeToPos+=v.size();  clearVector(out_data); }
    _pushv(out_data);
    if (headCode.size()>0){
        _pushv(headCode);
        clearVector(headData);
    }else{
        _pushv(headData);
        clearVector(headCode);
    }
    listener->externDataPosInOutStream(writeToPos);
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
        check(readAll(newRefStream.stream,0,newData,
                      newData+newRefStream.stream->streamSize),"read new file error!");
        _newRefList.clear();//close files
        check(readAll(oldRefStream.stream,0,oldData,
                      oldData+oldRefStream.stream->streamSize),"read old file error!");
        _oldRefList.clear();//close files
        std::vector<unsigned char> out_diff;
        create_compressed_diff(newData,newData+newRefStream.stream->streamSize,
                               oldData,oldData+oldRefStream.stream->streamSize,
                               out_diff,compressPlugin,(int)matchValue);
        diffDataSize=out_diff.size();
        _pushv(out_diff);
    }else{
        TOffsetStreamOutput ofStream(outDiffStream,writeToPos);
        create_compressed_diff_stream(oldRefStream.stream,newRefStream.stream,&ofStream,
                                      streamCompressPlugin,matchValue);
        diffDataSize=ofStream.outSize;
    }
    listener->runHDiffEnd(diffDataSize);
}


void resave_compressed_dirdiff(const hpatch_TStreamInput*  in_diff,
                               hpatch_TDecompress*         decompressPlugin,
                               const hpatch_TStreamOutput* out_diff,
                               hdiff_TStreamCompress*      compressPlugin){
    //todo:
    
}
