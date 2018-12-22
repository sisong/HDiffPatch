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
#include "../file_for_patch.h"
#include "../libHDiffPatch/HDiff/private_diff/mem_buf.h"
#include "../libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.h"
#include "../libHDiffPatch/HDiff/private_diff/pack_uint.h"
#include "patch/ref_stream.h"
#include "../libHDiffPatch/HDiff/diff.h"
using namespace hdiff_private;

#define kFileIOBufSize      (64*1024)
#define kFileIOBestMaxSize  (1024*1024)
#define check(value,info) if (!(value)) throw new std::runtime_error(info);

#define hash_value_t                uint64_t
#define hash_begin(ph)              { (*(ph))=ADLER_INITIAL; }
#define hash_append(ph,pvalues,n)   { (*(ph))=fast_adler64_append(*(ph),pvalues,n); }
#define hash_end(ph)                {}

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
    :_base(base),_offset(offset){
        assert(offset<=base->streamSize);
        this->streamHandle=this;
        this->streamSize=base->streamSize-offset;
        this->write=_write;
    }
    const hpatch_TStreamOutput* _base;
    hpatch_StreamPos_t          _offset;
    static long _write(hpatch_TStreamOutputHandle streamHandle,const hpatch_StreamPos_t writeToPos,
                       const unsigned char* data,const unsigned char* data_end){
        TOffsetStreamOutput* self=(TOffsetStreamOutput*)streamHandle;
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

void dir_diff(IDirDiffListener* listener,const char* _oldPatch,const char* _newPatch,
              const hpatch_TStreamOutput* outDiffStream,bool oldIsDir,bool newIsDir,
              bool isLoadAll,size_t matchValue,
              hdiff_TStreamCompress* streamCompressPlugin,hdiff_TCompress* compressPlugin){
    assert(listener!=0);
    std::string newPatch(_newPatch);
    std::string oldPatch(_oldPatch);
    std::vector<std::string> newList;
    std::vector<std::string> oldList;
    {
        if (newIsDir){
            assignDirTag(newPatch);
            check(getDirFileList(newPatch,newList),"new dir getDirFileList() error!");
            sortDirFileList(newList);
        }
        if (oldIsDir){
            assignDirTag(oldPatch);
            check(getDirFileList(oldPatch,oldList),"old dir getDirFileList() error!");
            sortDirFileList(oldList);
        }
        listener->filterFileList(newList,oldList);
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
    std::vector<TByte> externData;
    listener->externData(externData);
    
    
    std::vector<TByte> headData;
    std::vector<TByte> headCode;
    //todo:
    
    //serialize  dir diff data
    std::vector<TByte> out_data;
    {//type version
        static const char* kVersionType="DirDiff19&";
        pushBack(out_data,(const TByte*)kVersionType,(const TByte*)kVersionType+strlen(kVersionType));
    }
    {//compressType
        const char* compressType=compressPlugin->compressType(compressPlugin);
        size_t compressTypeLen=strlen(compressType);
        check(compressTypeLen<=hpatch_kMaxCompressTypeLength,"compressTypeLen error!");
        pushBack(out_data,(const TByte*)compressType,(const TByte*)compressType+compressTypeLen+1); //'\0'
    }
    //head info
    const TByte kPatchModel=0;
    packUInt(out_data,kPatchModel);
    packUInt(out_data,newList.size());
    packUInt(out_data,newRefIList.size());
    packUInt(out_data,sameFileCount);
    packUInt(out_data,oldList.size());
    packUInt(out_data,oldRefIList.size());
    packUInt(out_data,externData.size());
    packUInt(out_data,headData.size());
    packUInt(out_data,headCode.size());
    
    hpatch_StreamPos_t writeToPos=0;
    writeAll(outDiffStream,writeToPos,out_data.data(),out_data.data()+out_data.size());
    writeToPos+=out_data.size();
    writeAll(outDiffStream,writeToPos,externData.data(),externData.data()+externData.size());
    writeToPos+=externData.size();
    if (headCode.size()>0){
        writeAll(outDiffStream,writeToPos,headCode.data(),headCode.data()+headCode.size());
        writeToPos+=headCode.size();
    }else{
        writeAll(outDiffStream,writeToPos,headData.data(),headData.data()+headData.size());
        writeToPos+=headData.size();
    }
    
    //diff data
    if (isLoadAll){
        TAutoMem  mem(oldRefStream.stream->streamSize+newRefStream.stream->streamSize);
        TByte* oldData=mem.data();
        TByte* newData=mem.data()+oldRefStream.stream->streamSize;
        check(readAll(oldRefStream.stream,0,oldData,
                      oldData+oldRefStream.stream->streamSize),"read old file error!");
        check(readAll(newRefStream.stream,0,newData,
                      newData+newRefStream.stream->streamSize),"read new file error!");
        std::vector<unsigned char> out_diff;
        create_compressed_diff(newData,newData+newRefStream.stream->streamSize,
                               oldData,oldData+oldRefStream.stream->streamSize,
                               out_diff,compressPlugin,(int)matchValue);
        check(writeAll(outDiffStream,writeToPos,out_diff.data(),
                       out_diff.data()+out_diff.size()),"write diff data error!");
    }else{
        TOffsetStreamOutput ofStream(outDiffStream,writeToPos);
        create_compressed_diff_stream(oldRefStream.stream,newRefStream.stream,&ofStream,
                                      streamCompressPlugin,matchValue);
    }
}
