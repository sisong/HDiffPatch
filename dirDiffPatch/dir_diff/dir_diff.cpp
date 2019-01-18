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
#include "../../libHDiffPatch/HDiff/private_diff/limit_mem_diff/stream_serialize.h"
#include "../../libHDiffPatch/HDiff/private_diff/pack_uint.h"
#include "../../libHDiffPatch/HDiff/diff.h"
#include "../../file_for_patch.h"
#include "../file_for_dirDiff.h"
#include "../dir_patch/ref_stream.h"
#include "../dir_patch/dir_patch.h"
#include "../dir_patch/dir_patch_private.h"
#include "../dir_patch/res_handle_limit.h"
using namespace hdiff_private;

static const char* kDirDiffVersionType="DirDiff19";

#define kFileIOBufSize      (1024*64)
#define check(value,info) { if (!(value)) { throw std::runtime_error(info); } }
#define checkv(value)     check(value,#value" error!")

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

struct CChecksum{
    inline explicit CChecksum(hpatch_TChecksum* checksumPlugin)
    :_checksumPlugin(checksumPlugin),_handle(0) {
        if (checksumPlugin){
            _handle=checksumPlugin->open(checksumPlugin);
            checkv(_handle!=0);
            checksumPlugin->begin(_handle);
        } }
    inline ~CChecksum(){
        if (_handle) _checksumPlugin->close(_checksumPlugin,_handle); }
    inline void append(const unsigned char* data,const unsigned char* data_end){
        if (_handle) _checksumPlugin->append(_handle,data,data_end); }
    inline void append(const std::vector<TByte>& data){ append(data.data(),data.data()+data.size()); }
    inline void append(const hpatch_TStreamInput* data){ append(data,0,data->streamSize); }
    void append(const hpatch_TStreamInput* data,hpatch_StreamPos_t begin,hpatch_StreamPos_t end){
        if (!_handle) return;
        TAutoMem buf(1024*32);
        while (begin<end){
            size_t len=buf.size();
            if (len>(end-begin))
                len=(size_t)(end-begin);
            checkv(data->read(data,begin,buf.data(),buf.data()+len));
            append(buf.data(),buf.data()+len);
            begin+=len;
        }
    }
    inline void appendEnd(){
        if (_handle){
            checksum.resize(_checksumPlugin->checksumByteSize());
            _checksumPlugin->end(_handle,checksum.data(),checksum.data()+checksum.size());
        } }
    hpatch_TChecksum*       _checksumPlugin;
    hpatch_checksumHandle   _handle;
    std::vector<TByte>      checksum;
};

static void _outType(std::vector<TByte>& out_data,hdiff_TStreamCompress* compressPlugin,
                     hpatch_TChecksum* checksumPlugin){
    //type version
    pushCStr(out_data,kDirDiffVersionType);
    pushCStr(out_data,"&");
    {//compressType
        const char* compressType="";
        if (compressPlugin)
            compressType=compressPlugin->compressType();
        size_t compressTypeLen=strlen(compressType);
        check(compressTypeLen<=hpatch_kMaxPluginTypeLength,"compressTypeLen error!");
        check(0==strchr(compressType,'&'), "compressType cannot contain '&'");
        pushCStr(out_data,compressType);
    }
    pushCStr(out_data,"&");
    {//checksumType
        const char* checksumType="";
        if (checksumPlugin)
            checksumType=checksumPlugin->checksumType();
        size_t checksumTypeLen=strlen(checksumType);
        check(checksumTypeLen<=hpatch_kMaxPluginTypeLength,"checksumTypeLen error!");
        check(0==strchr(checksumType,'&'), "checksumType cannot contain '&'");
        pushCStr(out_data,checksumType);
    }
    const TByte _cstrEndTag='\0';//c string end tag
    pushBack(out_data,&_cstrEndTag,(&_cstrEndTag)+1);
}

void dir_diff(IDirDiffListener* listener,const std::string& oldPath,const std::string& newPath,
              const hpatch_TStreamOutput* outDiffStream,bool isLoadAll,size_t matchValue,
              hdiff_TStreamCompress* streamCompressPlugin,hdiff_TCompress* compressPlugin,
              hpatch_TChecksum* checksumPlugin,size_t kMaxOpenFileNumber){
    assert(listener!=0);
    assert(kMaxOpenFileNumber>=kMaxOpenFileNumber_limit_min);
    if ((checksumPlugin)&&(!isLoadAll)){
        check((outDiffStream->read_writed!=0),
              "for update checksum, outDiffStream->read_writed can't null error!");
    }
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
    CFileResHandleLimit resLimit;
    resLimit.open(kMaxOpenFileNumber,resList);
    CRefStream oldRefStream;
    CRefStream newRefStream;
    oldRefStream.open(resLimit.limit.streamList,oldRefIList.size());
    newRefStream.open(resLimit.limit.streamList+oldRefIList.size(),newRefIList.size());
    listener->diffRefInfo(oldList.size(),newList.size(),dataSamePairList.size(),
                          oldRefIList.size(),newRefIList.size(),
                          oldRefStream.stream->streamSize,newRefStream.stream->streamSize);
    
    //checksum
    const size_t checksumByteSize=(checksumPlugin==0)?0:checksumPlugin->checksumByteSize();
    CChecksum oldRefChecksum(checksumPlugin);
    CChecksum newRefChecksum(checksumPlugin);
    CChecksum sameFileChecksum(checksumPlugin);
    oldRefChecksum.append(oldRefStream.stream);
    oldRefChecksum.appendEnd();
    newRefChecksum.append(newRefStream.stream);
    newRefChecksum.appendEnd();
    for (size_t i=0; i<dataSamePairList.size(); ++i) {
        CFileStreamInput file(newList[dataSamePairList[i].newIndex]);
        sameFileChecksum.append(&file.base);
    }
    sameFileChecksum.appendEnd();

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
    _outType(out_data,streamCompressPlugin,checksumPlugin);
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
    packUInt(out_data,dataSamePairList.size()); clearVector(dataSamePairList);
    //externData size
    std::vector<TByte> externData;
    listener->externData(externData);
    packUInt(out_data,externData.size());
    //headData info
    packUInt(out_data,headData.size());
    packUInt(out_data,headCode.size());
    //checksum info
    packUInt(out_data,checksumByteSize);
    if (checksumByteSize>0){
        pushBack(out_data,oldRefChecksum.checksum);
        pushBack(out_data,newRefChecksum.checksum);
        pushBack(out_data,sameFileChecksum.checksum);
    }
    CChecksum diffChecksum(checksumPlugin);
    TPlaceholder diffChecksumPlaceholder(out_data.size(),out_data.size()+checksumByteSize);
    if (checksumByteSize>0){
        diffChecksum.append(out_data);
        out_data.insert(out_data.end(),checksumByteSize,0);//placeholder
    }
    
    //begin write
    hpatch_StreamPos_t writeToPos=0;
    #define _pushv(v) { if ((checksumByteSize>0)&&(writeToPos>=diffChecksumPlaceholder.pos_end))\
                            diffChecksum.append(v); \
                        check(outDiffStream->write(outDiffStream,writeToPos,v.data(),v.data()+v.size()), \
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
        if (checksumByteSize>0){
            assert(outDiffStream->read_writed!=0);
            diffChecksum.append((const hpatch_TStreamInput*)outDiffStream,
                                writeToPos,writeToPos+diffDataSize);
        }
    }
    listener->runHDiffEnd(diffDataSize);
    if (checksumByteSize>0){ //update diffChecksum
        diffChecksum.appendEnd();
        const std::vector<TByte>& v=diffChecksum.checksum;
        assert(diffChecksumPlaceholder.size()==v.size());
        check(outDiffStream->write(outDiffStream,diffChecksumPlaceholder.pos,v.data(),v.data()+v.size()),
              "write diff data checksum error!");
    }
    #undef _pushv
}


#define _test(v) { if (!(v)) { fprintf(stderr,"DirPatch test "#v" error!\n");  return hpatch_FALSE; } }

struct CDirPatchListener:public IDirPatchListener{
    explicit CDirPatchListener(const std::string& newRootDir,
                               const std::vector<std::string>& oldList,
                               const std::vector<std::string>& newList)
    :_oldSet(oldList.begin(),oldList.end()),_newSet(newList.begin(),newList.end()),
    _buf(kFileIOBufSize){
        _dirSet.insert(getParentDir(newRootDir));
        this->listenerImport=this;
        this->makeNewDir=_makeNewDir;
        this->copySameFile=_copySameFile;
        this->openNewFile=_openNewFile;
        this->closeNewFile=_closeNewFile;
    }
    bool isNewOk()const{ return _newSet.empty(); }
    std::set<std::string>   _oldSet;
    std::set<std::string>   _newSet;
    std::set<std::string>   _dirSet;
    static inline std::string getParentDir(const std::string& _path){
        std::string path(_path);
        if (path.empty()) return path;
        if (path[path.size()-1]==kPatch_dirSeparator)
            path.pop_back();
        while ((!path.empty())&&(path[path.size()-1]!=kPatch_dirSeparator)) {
            path.pop_back();
        }
        return path;
    }
    inline bool isParentDirExists(const std::string& _path)const{
        std::string path=getParentDir(_path);
        if (path.empty()) return true;
        return _dirSet.find(path)!=_dirSet.end();
    }
    
    static hpatch_BOOL _makeNewDir(IDirPatchListener* listener,const char* _newDir){
        CDirPatchListener* self=(CDirPatchListener*)listener->listenerImport;
        std::string newDir(_newDir);
        _test(self->isParentDirExists(newDir));
        self->_dirSet.insert(newDir);
        self->_newSet.erase(newDir);
        return hpatch_TRUE;
    }
    static hpatch_BOOL _copySameFile(IDirPatchListener* listener,const char* _oldFileName,const char* _newFileName){
        CDirPatchListener* self=(CDirPatchListener*)listener->listenerImport;
        std::string oldFileName(_oldFileName);
        std::string newFileName(_newFileName);
        _test(self->isParentDirExists(newFileName));
        _test(self->_oldSet.find(oldFileName)!=self->_oldSet.end());
        _test(self->_newSet.find(newFileName)!=self->_newSet.end());
        _test(fileData_isSame(oldFileName,newFileName));
        self->_newSet.erase(newFileName);
        return hpatch_TRUE;
    }
    
    struct _TCheckOutNewDataStream:public hpatch_TStreamOutput{
        explicit _TCheckOutNewDataStream(const char* _newFileName,
                                         TByte* _buf,size_t _bufSize)
        :newData(0),writedLen(0),buf(_buf),bufSize(_bufSize),newFileName(_newFileName){
            TFileStreamInput_init(&newFile);
            if (TFileStreamInput_open(&newFile,newFileName.c_str())){
                newData=&newFile.base;
                streamSize=newData->streamSize;
            }else{
                streamSize=(hpatch_StreamPos_t)(-1);
            }
            streamImport=this;
            write=_write_check;
        }
        ~_TCheckOutNewDataStream(){  TFileStreamInput_close(&newFile); }
        static hpatch_BOOL _write_check(const hpatch_TStreamOutput* stream,hpatch_StreamPos_t writeToPos,
                                        const unsigned char* data,const unsigned char* data_end){
            _TCheckOutNewDataStream* self=(_TCheckOutNewDataStream*)stream->streamImport;
            _test(self->isOpenOk());
            _test(self->writedLen==writeToPos);
            self->writedLen+=(size_t)(data_end-data);
            _test(self->writedLen<=self->streamSize);
            
            hpatch_StreamPos_t readPos=writeToPos;
            while (data<data_end) {
                size_t readLen=(size_t)(data_end-data);
                if (readLen>self->bufSize) readLen=self->bufSize;
                _test(self->newData->read(self->newData,readPos,self->buf,self->buf+readLen));
                _test(0==memcmp(data,self->buf,readLen));
                data+=readLen;
                readPos+=readLen;
            }
            return hpatch_TRUE;
        }
        bool isOpenOk()const{ return (newData!=0); };
        bool isWriteOk()const{ return (!newFile.fileError)&&(writedLen==newData->streamSize); }
        const hpatch_TStreamInput*  newData;
        hpatch_StreamPos_t          writedLen;
        TByte*                      buf;
        size_t                      bufSize;
        TFileStreamInput            newFile;
        std::string                 newFileName;
    };
    
    TAutoMem _buf;
    static hpatch_BOOL _openNewFile(IDirPatchListener* listener,TFileStreamOutput*  out_curNewFile,
                             const char* newFileName,hpatch_StreamPos_t newFileSize){
        CDirPatchListener* self=(CDirPatchListener*)listener->listenerImport;
        _TCheckOutNewDataStream* newFile=new _TCheckOutNewDataStream(newFileName,self->_buf.data(),
                                                                     self->_buf.size());
        out_curNewFile->base=*newFile;
        _test(newFile->isOpenOk());
        _test(self->_newSet.find(newFile->newFileName)!=self->_newSet.end());
        return true;
    }
    static hpatch_BOOL _closeNewFile(IDirPatchListener* listener,TFileStreamOutput* curNewFile){
        CDirPatchListener* self=(CDirPatchListener*)listener->listenerImport;
        if (curNewFile==0) return true;
        _TCheckOutNewDataStream* newFile=(_TCheckOutNewDataStream*)curNewFile->base.streamImport;
        if (newFile==0) return true;
        _test(newFile->isWriteOk());
        _test(self->_newSet.find(newFile->newFileName)!=self->_newSet.end());
        self->_newSet.erase(newFile->newFileName);
        delete newFile;
        return true;
    }
};

struct CDirPatcher:public TDirPatcher{
    CDirPatcher(){ TDirPatcher_init(this); }
    ~CDirPatcher() { TDirPatcher_close(this); }
};

bool check_dirdiff(IDirDiffListener* listener,const std::string& oldPath,const std::string& newPath,
                   const hpatch_TStreamInput* testDiffData,hpatch_TDecompress* decompressPlugin,
                   hpatch_TChecksum* checksumPlugin,size_t kMaxOpenFileNumber){
    bool     result=true;
    assert(kMaxOpenFileNumber>=kMaxOpenFileNumber_limit_min);
    std::vector<std::string> oldList;
    std::vector<std::string> newList;
    {
        if (isDirName(oldPath)){
            getDirFileList(oldPath,oldList,listener);
            sortDirFileList(oldList);
        }else{
            oldList.push_back(oldPath);
        }
        if (isDirName(newPath)){
            getDirFileList(newPath,newList,listener);
            sortDirFileList(newList);
        }else{
            newList.push_back(newPath);
        }
        listener->diffPathList(oldList,newList);
    }
    
    CDirPatchListener    patchListener(newPath,oldList,newList);
    CDirPatcher          dirPatcher;
    const TDirDiffInfo*  dirDiffInfo=0;
    TPatchChecksumSet    checksumSet={checksumPlugin,hpatch_TRUE,hpatch_TRUE,hpatch_TRUE,hpatch_TRUE};
    TAutoMem             p_temp_mem(kFileIOBufSize*5);
    TByte*               temp_cache=p_temp_mem.data();
    size_t               temp_cache_size=p_temp_mem.size();
    const hpatch_TStreamInput*  oldStream=0;
    const hpatch_TStreamOutput* newStream=0;
    {//dir diff info
        TPathType    oldType;
        _test(getPathTypeByName(oldPath.c_str(),&oldType,0));
        _test(TDirPatcher_open(&dirPatcher,testDiffData,&dirDiffInfo));
        _test(dirDiffInfo->isDirDiff);
        if (dirDiffInfo->oldPathIsDir){
            _test(kPathType_dir==oldType);
        }else{
            _test(kPathType_dir!=oldType);
        }
    }
    _test(TDirPatcher_checksum(&dirPatcher,&checksumSet));
    _test(TDirPatcher_loadDirData(&dirPatcher,decompressPlugin));
    _test(TDirPatcher_openOldRefAsStream(&dirPatcher,oldPath.c_str(),kMaxOpenFileNumber,&oldStream));
    _test(TDirPatcher_openNewDirAsStream(&dirPatcher,newPath.c_str(),&patchListener,&newStream));
    _test(TDirPatcher_patch(&dirPatcher,newStream,oldStream,temp_cache,temp_cache+temp_cache_size));
    _test(TDirPatcher_closeNewDirStream(&dirPatcher));
    _test(TDirPatcher_closeOldRefStream(&dirPatcher));
    _test(patchListener.isNewOk());
    return result;
}
#undef _test


void resave_dirdiff(const hpatch_TStreamInput* in_diff,hpatch_TDecompress* decompressPlugin,
                    const hpatch_TStreamOutput* out_diff,hdiff_TStreamCompress* compressPlugin,
                    hpatch_TChecksum* checksumPlugin){
    _TDirDiffHead       head;
    TDirDiffInfo        diffInfo;
    assert(in_diff!=0);
    assert(in_diff->read!=0);
    assert(out_diff!=0);
    assert(out_diff->write!=0);
    
    {//read head
        check(read_dirdiff_head(&diffInfo,&head,in_diff),
              "resave_dirdiff() read_dirdiff_head() error!");
        int compressedCount=diffInfo.hdiffInfo.compressedCount+((head.headDataCompressedSize)?1:0);
        check((decompressPlugin!=0)||(compressedCount<=0),
              "resave_dirdiff() decompressPlugin null error!");
        if ((decompressPlugin)&&(compressedCount>0)){
            check(decompressPlugin->is_can_open(diffInfo.hdiffInfo.compressType),
                  "resave_dirdiff() decompressPlugin cannot open compressed data error!");
        }
    }
    hpatch_StreamPos_t writeToPos=0;
    {//save head
        TDiffStream outDiff(out_diff);
        {//type
            std::vector<TByte> out_type;
            _outType(out_type,compressPlugin,checksumPlugin);
            outDiff.pushBack(out_type.data(),out_type.size());
        }
        {//copy other
            TStreamClip clip(in_diff,head.typesEndPos,head.compressSizeBeginPos);
            outDiff.pushStream(&clip);
        }
        //headData
        outDiff.packUInt(head.headDataSize);
        TPlaceholder compress_headData_sizePos=
        outDiff.packUInt_pos(compressPlugin?head.headDataSize:0);//headDataCompressedSize
        {//resave headData
            bool isCompressed=(head.headDataCompressedSize>0);
            hpatch_StreamPos_t bufSize=isCompressed?head.headDataCompressedSize:head.headDataSize;
            TStreamClip clip(in_diff,head.headDataOffset,head.headDataOffset+bufSize,
                             (isCompressed?decompressPlugin:0),head.headDataSize);
            outDiff.pushStream(&clip,compressPlugin,compress_headData_sizePos);
        }
        {//save externData
            TStreamClip clip(in_diff,diffInfo.externDataOffset,
                             diffInfo.externDataOffset+diffInfo.externDataSize);
            outDiff.pushStream(&clip);
        }
        writeToPos=outDiff.getWritedPos();
    }
    {//resave hdiffData
        TOffsetStreamOutput ofStream(out_diff,writeToPos);
        TStreamClip clip(in_diff,head.hdiffDataOffset,head.hdiffDataOffset+head.hdiffDataSize);
        resave_compressed_diff(&clip,decompressPlugin,&ofStream,compressPlugin);
    }
}
