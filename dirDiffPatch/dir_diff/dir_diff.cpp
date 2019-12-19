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
#if (_IS_NEED_DIR_DIFF_PATCH)
#include <stdio.h>
#include <algorithm> //sort
#include <map>
#include <set>
#include "../../libHDiffPatch/HDiff/private_diff/mem_buf.h"
#include "../../libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.h"
#include "../../libHDiffPatch/HDiff/private_diff/limit_mem_diff/stream_serialize.h"
#include "../../libHDiffPatch/HDiff/private_diff/pack_uint.h"
#include "../../libHDiffPatch/HDiff/diff.h"
#include "../../file_for_patch.h"
#include "file_for_dirDiff.h"
#include "../dir_patch/ref_stream.h"
#include "../dir_patch/dir_patch.h"
#include "../dir_patch/dir_patch_private.h"
#include "../dir_patch/res_handle_limit.h"
#include "../../_atosize.h"
using namespace hdiff_private;

static const char* kDirDiffVersionType= "HDIFF19";

static std::string  cmp_hash_type    =  "fadler64";
#define cmp_hash_value_t                uint64_t
#define cmp_hash_begin(ph)              { (*(ph))=ADLER_INITIAL; }
#define cmp_hash_append(ph,pvalues,n)   { (*(ph))=fast_adler64_append(*(ph),pvalues,n); }
#define cmp_hash_end(ph)                {}
#define cmp_hash_combine(ph,rightHash,rightLen) { (*(ph))=fast_adler64_by_combine(*(ph),rightHash,rightLen); }

#define kFileIOBufSize      (1024*64)
#define check(value,info) { if (!(value)) { throw std::runtime_error(info); } }
#define checkv(value)     check(value,"check "#value" error!")

void assignDirTag(std::string& dir_utf8){
    if (dir_utf8.empty()||(dir_utf8[dir_utf8.size()-1]!=kPatch_dirSeparator))
        dir_utf8.push_back(kPatch_dirSeparator);
}

static inline bool isDirName(const std::string& path_utf8){
    return 0!=hpatch_getIsDirName(path_utf8.c_str());
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

struct CFileStreamInput:public hpatch_TFileStreamInput{
    inline CFileStreamInput(){ hpatch_TFileStreamInput_init(this); }
    inline void open(const std::string& fileName){
        assert(this->base.streamImport==0);
        check(hpatch_TFileStreamInput_open(this,fileName.c_str()),"open file \""+fileName+"\" error!"); }
    inline CFileStreamInput(const std::string& fileName){
        hpatch_TFileStreamInput_init(this); open(fileName); }
    inline void closeFile() { check(hpatch_TFileStreamInput_close(this),"close file error!"); }
    inline ~CFileStreamInput(){ closeFile(); }
};

struct CRefStream:public hpatch_TRefStream{
    inline CRefStream(){ hpatch_TRefStream_init(this); }
    inline void open(const hpatch_TStreamInput** refList,size_t refCount){
        check(hpatch_TRefStream_open(this,refList,refCount),"TRefStream_open() refList error!");
    }
    inline ~CRefStream(){ hpatch_TRefStream_close(this); }
};

struct TOffsetStreamOutput:public hpatch_TStreamOutput{
    inline explicit TOffsetStreamOutput(const hpatch_TStreamOutput* base,hpatch_StreamPos_t offset)
    :_base(base),_offset(offset),outSize(0){
        assert(offset<=base->streamSize);
        this->streamImport=this;
        this->streamSize=base->streamSize-offset;
        this->read_writed=0;
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
    inline CDir(const std::string& dir):handle(0){ handle=hdiff_dirOpenForRead(dir.c_str()); }
    inline ~CDir(){ hdiff_dirClose(handle); }
    hdiff_TDirHandle handle;
};

void _getDirSubFileList(const std::string& dirPath,std::vector<std::string>& out_list,
                        IDirPathIgnore* filter,size_t rootPathNameLen,bool pathIsInOld){
    assert(!isDirName(dirPath));
    std::vector<std::string> subDirs;
    {//serach cur dir
        CDir dir(dirPath);
        check((dir.handle!=0),"hdiff_dirOpenForRead \""+dirPath+"\" error!");
        while (true) {
            hpatch_TPathType  type;
            const char* path=0;
            check(hdiff_dirNext(dir.handle,&type,&path),"hdiff_dirNext \""+dirPath+"\" error!");
            if (path==0) break; //finish
            if ((0==strcmp(path,""))||(0==strcmp(path,"."))||(0==strcmp(path,"..")))
                continue;
            std::string subName(dirPath+kPatch_dirSeparator+path);
            assert(!isDirName(subName));
            switch (type) {
                case kPathType_dir:{
                    assignDirTag(subName);
                    if (!filter->isNeedIgnore(subName,rootPathNameLen,pathIsInOld)){
                        subDirs.push_back(subName.substr(0,subName.size()-1)); //no '/'
                        out_list.push_back(subName); //add dir
                    }
                } break;
                case kPathType_file:{
                    if (!filter->isNeedIgnore(subName,rootPathNameLen,pathIsInOld))
                        out_list.push_back(subName); //add file
                } break;
                default:{
                    //nothing
                } break;
            }
        }
    }
    
    for (size_t i=0; i<subDirs.size(); ++i) {
        assert(!isDirName(subDirs[i]));
        _getDirSubFileList(subDirs[i],out_list,filter,rootPathNameLen,pathIsInOld);
    }
}

void getDirAllPathList(const std::string& dirPath,std::vector<std::string>& out_list,
                       IDirPathIgnore* filter,bool pathIsInOld){
    assert(isDirName(dirPath));
    out_list.push_back(dirPath);
    const std::string dirName(dirPath.c_str(),dirPath.c_str()+dirPath.size()-1); //without '/'
    _getDirSubFileList(dirName,out_list,filter,dirName.size(),pathIsInOld);
}


static cmp_hash_value_t getStreamHash(const hpatch_TStreamInput* stream,const std::string& errorTag){
    TAutoMem  mem(kFileIOBufSize);
    cmp_hash_value_t result;
    cmp_hash_begin(&result);
    for (hpatch_StreamPos_t pos=0; pos<stream->streamSize;) {
        size_t readLen=kFileIOBufSize;
        if (pos+readLen>stream->streamSize)
            readLen=(size_t)(stream->streamSize-pos);
        check(stream->read(stream,pos,mem.data(),mem.data()+readLen),
              "read \""+errorTag+"\" error!");
        cmp_hash_append(&result,mem.data(),readLen);
        pos+=readLen;
    }
    cmp_hash_end(&result);
    return result;
}

static cmp_hash_value_t getFileHash(const std::string& fileName,hpatch_StreamPos_t* out_fileSize){
    CFileStreamInput f(fileName);
    *out_fileSize=f.base.streamSize;
    return getStreamHash(&f.base,fileName);
}

static hpatch_StreamPos_t getFileSize(const std::string& fileName){
    hpatch_TPathType   type;
    hpatch_StreamPos_t fileSize;
    checkv(hpatch_getPathStat(fileName.c_str(),&type,&fileSize));
    checkv(type==kPathType_file);
    return fileSize;
}

static bool fileData_isSame(const std::string& file_x,const std::string& file_y,
                            hpatch_ICopyDataListener* copyListener=0){
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
        if (copyListener)
            copyListener->copyedData(copyListener,mem.data(),mem.data()+readLen);
        pos+=readLen;
    }
    return true;
}

void sortDirPathList(std::vector<std::string>& fileList){
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

static void pushSamePairList(std::vector<TByte>& out_data,const std::vector<hpatch_TSameFilePair>& pairs){
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
        
        const size_t kMaxValue=~(size_t)0;
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
                       std::vector<hpatch_TSameFilePair>& out_dataSamePairList,
                       std::vector<size_t>& out_oldRefList,std::vector<size_t>& out_newRefList,
                       bool isNeedOutHashs,
                       std::vector<cmp_hash_value_t>& out_oldHashList,
                       std::vector<cmp_hash_value_t>& out_newHashList){
    typedef std::multimap<cmp_hash_value_t,size_t> TMap;
    TMap hashMap;
    std::set<size_t> oldRefSet;
    const bool isFileSizeAsHash =(!isNeedOutHashs) && ((oldList.size()*newList.size())<=16);
    out_oldSizeList.assign(oldList.size(),0);
    out_newSizeList.assign(newList.size(),0);
    if (isNeedOutHashs){
        out_oldHashList.assign(oldList.size(),0);
        out_newHashList.assign(newList.size(),0);
    }
    for (size_t oldi=0; oldi<oldList.size(); ++oldi) {
        const std::string& fileName=oldList[oldi];
        if (isDirName(fileName)) continue;
        hpatch_StreamPos_t fileSize=0;
        cmp_hash_value_t hash;
        if (isFileSizeAsHash){
            fileSize=getFileSize(fileName);
            hash=fileSize;
        }else{
            if ((fileName.empty())&&(oldList.size()==1)){ // isOldPathInputEmpty
                hpatch_TStreamInput emptyStream;
                mem_as_hStreamInput(&emptyStream,0,0);
                hash=getStreamHash(&emptyStream,"isOldPathInputEmpty, as empty file");
                fileSize=0;
            }else{
                hash=getFileHash(fileName,&fileSize);
            }
        }
        out_oldSizeList[oldi]=fileSize;
        if (isNeedOutHashs) out_oldHashList[oldi]=hash;
        if (fileSize==0) continue;
        hashMap.insert(TMap::value_type(hash,oldi));
        oldRefSet.insert(oldi);
    }
    out_dataSamePairList.clear();
    out_newRefList.clear();
    std::vector<size_t> oldHitList(oldList.size(),0);
    for (size_t newi=0; newi<newList.size(); ++newi){
        const std::string& fileName=newList[newi];
        if (isDirName(fileName)) continue;
        hpatch_StreamPos_t fileSize=0;
        cmp_hash_value_t hash;
        if (isFileSizeAsHash){
            fileSize=getFileSize(fileName);
            hash=fileSize;
        }else{
            hash=getFileHash(fileName,&fileSize);
        }
        out_newSizeList[newi]=fileSize;
        if (isNeedOutHashs) out_newHashList[newi]=hash;
        if (fileSize==0) continue;
        
        bool isFoundSame=false;
        size_t oldIndex=~(size_t)0;
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
            hpatch_TSameFilePair pair; pair.newIndex=newi; pair.oldIndex=oldIndex;
            out_dataSamePairList.push_back(pair);
        }else{
            out_newRefList.push_back(newi);
        }
    }
    out_oldRefList.assign(oldRefSet.begin(),oldRefSet.end());
    std::sort(out_oldRefList.begin(),out_oldRefList.end());
}

struct CFileResHandleLimit{
    inline CFileResHandleLimit(size_t _limitMaxOpenCount,size_t resCount)
    :limitMaxOpenCount(_limitMaxOpenCount),curInsert(0){
        hpatch_TResHandleLimit_init(&limit);
        resList.resize(resCount);
        memset(resList.data(),0,sizeof(hpatch_IResHandle)*resCount);
        fileList.resize(resCount);
        for(size_t i=0;i<resCount;++i)
            hpatch_TFileStreamInput_init(&fileList[i]);
    }
    inline ~CFileResHandleLimit() { close(); }
    void addRes(const std::string& fileName,hpatch_StreamPos_t fileSize){
        assert(curInsert<resList.size());
        fileList[curInsert].fileName=fileName;
        hpatch_IResHandle* res=&resList[curInsert];
        res->open=CFileResHandleLimit::openRes;
        res->close=CFileResHandleLimit::closeRes;
        res->resImport=&fileList[curInsert];
        res->resStreamSize=fileSize;
        ++curInsert;
    }
    void open(){
        assert(curInsert==resList.size());
        check(hpatch_TResHandleLimit_open(&limit,limitMaxOpenCount,resList.data(),
                                          resList.size()),"TResHandleLimit_open error!");
    }
    void close(){
        check(hpatch_TResHandleLimit_close(&limit),"TResHandleLimit_close error!");
    }
    
    struct CFile:public hpatch_TFileStreamInput{
        std::string  fileName;
    };
    hpatch_TResHandleLimit          limit;
    std::vector<CFile>              fileList;
    std::vector<hpatch_IResHandle>  resList;
    size_t                          limitMaxOpenCount;
    size_t                          curInsert;
    static hpatch_BOOL openRes(struct hpatch_IResHandle* res,hpatch_TStreamInput** out_stream){
        CFile* self=(CFile*)res->resImport;
        assert(self->m_file==0);
        check(hpatch_TFileStreamInput_open(self,self->fileName.c_str()),"CFileResHandleLimit open file error!");
        *out_stream=&self->base;
        return hpatch_TRUE;
    }
    static hpatch_BOOL closeRes(struct hpatch_IResHandle* res,const hpatch_TStreamInput* stream){
        CFile* self=(CFile*)res->resImport;
        assert(stream==&self->base);
        check(hpatch_TFileStreamInput_close(self),"CFileResHandleLimit close file error!");
        return hpatch_TRUE;
    }
};

struct CChecksum{
    inline explicit CChecksum(hpatch_TChecksum* checksumPlugin,bool isCanUseCombine)
    :_checksumPlugin(checksumPlugin),_handle(0),_isCanUseCombine(isCanUseCombine) {
        if (checksumPlugin){
            _handle=checksumPlugin->open(checksumPlugin);
            checkv(_handle!=0);
            checksumPlugin->begin(_handle);
            if (_isCanUseCombine) cmp_hash_begin(&_combineHash);
        } }
    inline ~CChecksum(){
        if (_handle) _checksumPlugin->close(_checksumPlugin,_handle); }
    inline void append(const unsigned char* data,const unsigned char* data_end){
        if (_handle) _checksumPlugin->append(_handle,data,data_end); }
    inline void append(const std::vector<TByte>& data){ append(data.data(),data.data()+data.size()); }
    inline void append(const hpatch_TStreamInput* data){ append(data,0,data->streamSize); }
    void append(const hpatch_TStreamInput* data,hpatch_StreamPos_t begin,hpatch_StreamPos_t end){
        if (!_handle) return;
        TAutoMem buf(kFileIOBufSize);
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
        }
        if (_isCanUseCombine){
            cmp_hash_end(&_combineHash);
            assert(sizeof(cmp_hash_value_t)==_checksumPlugin->checksumByteSize());
            checksum.resize(sizeof(cmp_hash_value_t));
            cmp_hash_value_t hash=_combineHash;
            for (size_t i=0;i<sizeof(cmp_hash_value_t);++i){
                checksum[i]=(TByte)hash;
                hash>>=8;
            }
        }
    }
    inline void combine(cmp_hash_value_t rightHash,uint64_t rightLen){
        cmp_hash_combine(&_combineHash,rightHash,rightLen);
    }
    hpatch_TChecksum*       _checksumPlugin;
    hpatch_checksumHandle   _handle;
    const bool              _isCanUseCombine;
    cmp_hash_value_t        _combineHash;
    std::vector<TByte>      checksum;
};

static void _outType(std::vector<TByte>& out_data,const hdiff_TCompress* compressPlugin,
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
              const hdiff_TCompress* compressPlugin,hpatch_TChecksum* checksumPlugin,size_t kMaxOpenFileNumber){
    TManifest oldManifest;
    TManifest newManifest;
    oldManifest.rootPath=oldPath;
    newManifest.rootPath=newPath;
    if (isDirName(oldManifest.rootPath)){
        getDirAllPathList(oldManifest.rootPath,oldManifest.pathList,listener,true);
        sortDirPathList(oldManifest.pathList);
    }else{
        oldManifest.pathList.push_back(oldManifest.rootPath);
    }
    if (isDirName(newManifest.rootPath)){
        getDirAllPathList(newManifest.rootPath,newManifest.pathList,listener,false);
        sortDirPathList(newManifest.pathList);
    }else{
        newManifest.pathList.push_back(newManifest.rootPath);
    }
    manifest_diff(listener,oldManifest,newManifest,outDiffStream,isLoadAll,matchValue,
                  compressPlugin,checksumPlugin,kMaxOpenFileNumber);
}

void manifest_diff(IDirDiffListener* listener,const TManifest& oldManifest,
                   const TManifest& newManifest,const hpatch_TStreamOutput* outDiffStream,
                   bool isLoadAll,size_t matchValue,const hdiff_TCompress* compressPlugin,
                   hpatch_TChecksum* checksumPlugin,size_t kMaxOpenFileNumber){
    assert(listener!=0);
    assert(kMaxOpenFileNumber>=kMaxOpenFileNumber_limit_min);
    if ((checksumPlugin)&&(!isLoadAll)){
        check((outDiffStream->read_writed!=0),
              "for update checksum, outDiffStream->read_writed can't null error!");
    }
    kMaxOpenFileNumber-=1; // for outDiffStream
    const std::vector<std::string>& oldList=oldManifest.pathList;
    const std::vector<std::string>& newList=newManifest.pathList;
    listener->diffPathList(oldList,newList);
    const bool oldIsDir=isDirName(oldManifest.rootPath);
    const bool newIsDir=isDirName(newManifest.rootPath);
    
    std::vector<hpatch_StreamPos_t> oldSizeList;
    std::vector<hpatch_StreamPos_t> newSizeList;
    std::vector<hpatch_TSameFilePair> dataSamePairList;     //new map to same old
    std::vector<size_t> oldRefIList;
    std::vector<size_t> newRefIList;
    std::vector<size_t> newExecuteList; //for linux etc
    
    for (size_t newi=0; newi<newList.size(); ++newi) {
        if ((!isDirName(newList[newi]))&&(listener->isExecuteFile(newList[newi])))
            newExecuteList.push_back(newi);
    }
    
    const bool isCachedHashs=(checksumPlugin!=0)&&(checksumPlugin->checksumType()==cmp_hash_type);
    if (isCachedHashs) checkv(sizeof(cmp_hash_value_t)==checksumPlugin->checksumByteSize());
    std::vector<cmp_hash_value_t> oldHashList;
    std::vector<cmp_hash_value_t> newHashList;
    getRefList(oldManifest.rootPath,newManifest.rootPath,oldList,newList,
               oldSizeList,newSizeList,dataSamePairList,oldRefIList,newRefIList,
               isCachedHashs,oldHashList,newHashList);
    std::vector<hpatch_StreamPos_t> newRefSizeList;
    CFileResHandleLimit resLimit(kMaxOpenFileNumber,oldRefIList.size()+newRefIList.size());
    {
        for (size_t i=0; i<oldRefIList.size(); ++i) {
            size_t fi=oldRefIList[i];
            resLimit.addRes(oldList[fi],oldSizeList[fi]);
        }
        newRefSizeList.resize(newRefIList.size());
        for (size_t i=0; i<newRefIList.size(); ++i) {
            size_t fi=newRefIList[i];
            newRefSizeList[i]=newSizeList[fi];
            resLimit.addRes(newList[fi],newSizeList[fi]);
        }
    }
    resLimit.open();
    CRefStream oldRefStream;
    CRefStream newRefStream;
    oldRefStream.open(resLimit.limit.streamList,oldRefIList.size());
    newRefStream.open(resLimit.limit.streamList+oldRefIList.size(),newRefIList.size());
    
    //checksum
    const size_t checksumByteSize=(checksumPlugin==0)?0:checksumPlugin->checksumByteSize();
    //oldRefChecksum
    CChecksum oldRefChecksum(checksumPlugin,isCachedHashs);
    if (isCachedHashs){
        for (size_t i=0; i<oldRefIList.size(); ++i) {
            size_t fi=oldRefIList[i];
            oldRefChecksum.combine(oldHashList[fi],oldSizeList[fi]);
        }
    }else{
        oldRefChecksum.append(oldRefStream.stream);
    }
    oldRefChecksum.appendEnd();
    //newRefChecksum
    CChecksum newRefChecksum(checksumPlugin,isCachedHashs);
    if (isCachedHashs){
        for (size_t i=0; i<newRefIList.size(); ++i) {
            size_t fi=newRefIList[i];
            newRefChecksum.combine(newHashList[fi],newSizeList[fi]);
        }
    }else{
        newRefChecksum.append(newRefStream.stream);
    }
    newRefChecksum.appendEnd();
    //sameFileChecksum
    CChecksum sameFileChecksum(checksumPlugin,isCachedHashs);
    hpatch_StreamPos_t sameFileSize=0;
    for (size_t i=0; i<dataSamePairList.size(); ++i) {
        size_t newi=dataSamePairList[i].newIndex;
        if (isCachedHashs){
            hpatch_StreamPos_t fileSize=newSizeList[newi];
            sameFileChecksum.combine(newHashList[newi],fileSize);
            sameFileSize+=fileSize;
        }else{
            CFileStreamInput file(newList[newi]);
            assert(file.base.streamSize==newSizeList[newi]);
            sameFileChecksum.append(&file.base);
            sameFileSize+=file.base.streamSize;
        }
    }
    sameFileChecksum.appendEnd();
    
    listener->diffRefInfo(oldList.size(),newList.size(),dataSamePairList.size(),
                          sameFileSize,oldRefIList.size(),newRefIList.size(),
                          oldRefStream.stream->streamSize,newRefStream.stream->streamSize);

    //serialize headData
    std::vector<TByte> headData;
    size_t oldPathSumSize=pushNameList(headData,oldManifest.rootPath,oldList,listener);
    size_t newPathSumSize=pushNameList(headData,newManifest.rootPath,newList,listener);
    pushIncList(headData,oldRefIList);
    pushIncList(headData,newRefIList);
    pushList(headData,newRefSizeList);
    pushSamePairList(headData,dataSamePairList);
    pushIncList(headData,newExecuteList);
    std::vector<TByte> privateReservedData;//now empty
    headData.insert(headData.end(),privateReservedData.begin(),privateReservedData.end());
    std::vector<TByte> headCode;
    if (compressPlugin){
        headCode.resize((size_t)compressPlugin->maxCompressedSize(headData.size()));
        size_t codeSize=hdiff_compress_mem(compressPlugin,headCode.data(),headCode.data()+headCode.size(),
                                           headData.data(),headData.data()+headData.size());
        if ((0<codeSize)&&(codeSize<headData.size()))
            headCode.resize(codeSize);//compress ok
        else
            clearVector(headCode); //compress error or give up
    }
    
    //serialize  dir diff data
    std::vector<TByte> out_data;
    _outType(out_data,compressPlugin,checksumPlugin);
    //head info
    packUInt(out_data,oldIsDir?1:0);
    packUInt(out_data,newIsDir?1:0);
    packUInt(out_data,oldList.size());
    packUInt(out_data,oldPathSumSize);
    packUInt(out_data,newList.size());
    packUInt(out_data,newPathSumSize);
    packUInt(out_data,oldRefIList.size());      clearVector(oldRefIList);
    packUInt(out_data,oldRefStream.stream->streamSize); //same as hdiffz::oldDataSize
    packUInt(out_data,newRefIList.size());      clearVector(newRefIList);
    packUInt(out_data,newRefStream.stream->streamSize); //same as hdiffz::newDataSize
    packUInt(out_data,dataSamePairList.size()); clearVector(dataSamePairList);
    packUInt(out_data,sameFileSize);
    packUInt(out_data,newExecuteList.size());   clearVector(newExecuteList);
    packUInt(out_data,privateReservedData.size()); clearVector(privateReservedData);
    std::vector<TByte> privateExternData;//now empty
    //privateExtern size
    packUInt(out_data,privateExternData.size());
    //externData size
    std::vector<TByte> externData;
    listener->externData(externData);
    packUInt(out_data,externData.size());
    //headData size
    packUInt(out_data,headData.size());
    packUInt(out_data,headCode.size());
    //other checksum info
    packUInt(out_data,checksumByteSize);
    if (checksumByteSize>0){
        pushBack(out_data,oldRefChecksum.checksum);
        pushBack(out_data,newRefChecksum.checksum);
        pushBack(out_data,sameFileChecksum.checksum);
    }
    //checksum(dirdiff)
    CChecksum diffChecksum(checksumPlugin,false);
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
    //privateExtern data
    _pushv(privateExternData);
    //externData
    listener->externDataPosInDiffStream(writeToPos,externData.size());
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
        std::vector<TByte> out_diff;
        create_compressed_diff(newData,newData+newRefStream.stream->streamSize,
                               oldData,oldData+oldRefStream.stream->streamSize,
                               out_diff,compressPlugin,(int)matchValue);
        diffDataSize=out_diff.size();
        _pushv(out_diff);
    }else{
        TOffsetStreamOutput ofStream(outDiffStream,writeToPos);
        create_compressed_diff_stream(newRefStream.stream,oldRefStream.stream,&ofStream,
                                      compressPlugin,matchValue);
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



    static std::string _i2a(size_t ivalue){
        const int kMaxNumCharSize =32;
        std::string result;
        result.resize(kMaxNumCharSize);
#ifdef _MSC_VER
        int len=sprintf_s(&result[0],kMaxNumCharSize,"%" PRIu64,(hpatch_StreamPos_t)ivalue);
#else
        int len=sprintf(&result[0],"%" PRIu64,(hpatch_StreamPos_t)ivalue);
#endif
        result.resize(len);
        return result;
    }
    static const char* _toSavedPath(std::string& tempPath,const char* path){
        if (kPatch_dirSeparator_saved==kPatch_dirSeparator)
            return path;
        tempPath.assign(path);
        for (size_t i=0; i<tempPath.size(); ++i) {
            if (tempPath[i]==kPatch_dirSeparator)
                tempPath[i]=kPatch_dirSeparator_saved;
        }
        return tempPath.c_str();
    }
static std::string kChecksumTypeTag="Checksum_Type:";
static std::string kPathCountTag="Path_Count:";
static std::string kPathTag="Path:";
void save_manifest(IDirDiffListener* listener,const std::string& inputPath,
                   const hpatch_TStreamOutput* outManifest,hpatch_TChecksum* checksumPlugin){
    assert(listener!=0);
    std::vector<std::string> pathList;
    const bool inputIsDir=isDirName(inputPath);
    if (inputIsDir){
        getDirAllPathList(inputPath,pathList,listener,false);
        sortDirPathList(pathList);
    }else{
        pathList.push_back(inputPath);
    }
    
    std::vector<TByte> out_data;
    {//head
        pushCStr(out_data,"HDiff_Manifest_Version:1.0\n");
        pushCStr(out_data,kChecksumTypeTag.c_str());
        if (checksumPlugin)
            pushCStr(out_data,checksumPlugin->checksumType());
        pushCStr(out_data, "\n");
        pushString(out_data, kPathCountTag+_i2a(pathList.size())+"\n");
        pushCStr(out_data, "\n");
    }
    //path list
    std::string tempPath;
    for (size_t i=0; i<pathList.size(); ++i) {
        const std::string& pathName=pathList[i];
        pushCStr(out_data, "Path:");
        if ((!isDirName(pathName))&&(checksumPlugin!=0)){//checksum file
            CChecksum fileChecksum(checksumPlugin,false);
            CFileStreamInput file(pathName);
            fileChecksum.append(&file.base);
            fileChecksum.appendEnd();
            //to hex
            const std::vector<TByte>& datas=fileChecksum.checksum;
            checkv(datas.size()==checksumPlugin->checksumByteSize());
            std::string hexs(datas.size()*2,' ');
            static const char _i2h[]="0123456789abcdef";
            for (size_t h=0; h<datas.size(); ++h) {
                TByte d=datas[h];
                hexs[h*2+0]=_i2h[d&0xF];
                hexs[h*2+1]=_i2h[d>>4];
            }
            pushCStr(out_data, hexs.c_str());
            pushCStr(out_data, ":");
        }
        const char* savedPath=_toSavedPath(tempPath,pathName.c_str()+inputPath.size());
        pushCStr(out_data, savedPath);
        pushCStr(out_data, "\n");
    }
    check(outManifest->write(outManifest,0,out_data.data(),out_data.data()+out_data.size()),
          "write manifest data error!");
}

void load_manifestFile(TManifestSaved& out_manifest,const std::string& rootPath,
                   const std::string& manifestFile){
    CFileStreamInput mf(manifestFile);
    load_manifest(out_manifest,rootPath,&mf.base);
}


    static void _fromSavedPath(char* begin,char* end){
        if (kPatch_dirSeparator_saved==kPatch_dirSeparator)
            return;
        for (;begin<end;++begin){
            if ((*begin)==kPatch_dirSeparator_saved)
                *begin=kPatch_dirSeparator;
        }
    }
    static void _pushPathBack(std::vector<std::string>& pathList,const std::string& rootPath,
                      const char* begin,const char* end){
        pathList.push_back(std::string());
        std::string& path=pathList.back();
        path.append(rootPath);
        if (begin<end){
            path.append(begin,end);
            char* pload=&path[0];
            _fromSavedPath(pload+rootPath.size(),pload+path.size());
        }
    }
    static TByte _hex2b(char c){
        switch (c) {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9': {
                return c-'0';
            } break;
            case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': {
                return c-('a'-10);
            } break;
            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': {
                return c-('A'-10);
            } break;
            default:
                check(false,"input char not hex,can't convert to Byte error! "+std::string(1,c));
        }
    }
    static void _pushChecksumBack(std::vector<std::vector<TByte> >& checksumList,
                                  const char* begin,const char* end){
        checksumList.push_back(std::vector<TByte>());
        std::vector<TByte>& data=checksumList.back();
        size_t hexSize=end-begin;
        checkv((hexSize&1)==0);
        data.resize(hexSize>>1);
        for (size_t i=0;i<hexSize;i+=2){
            data[i>>1]=_hex2b(begin[i]) | (_hex2b(begin[i+1])<<4);
        }
    }
void load_manifest(TManifestSaved& out_manifest,const std::string& rootPath,
                   const hpatch_TStreamInput* manifestStream){
    out_manifest.pathList.clear();
    out_manifest.checksumList.clear();
    out_manifest.checksumType.clear();
    out_manifest.rootPath=rootPath;
    //rootPath type
    hpatch_TPathType rootPathType;
    checkv(hpatch_getPathStat(rootPath.c_str(),&rootPathType,0));
    checkv(rootPathType!=kPathType_notExist);
    if (rootPathType==kPathType_dir)
        assignDirTag(out_manifest.rootPath);
    //read file
    TAutoMem buf((size_t)manifestStream->streamSize);
    checkv(buf.size()==manifestStream->streamSize);
    checkv(manifestStream->read(manifestStream,0,buf.data(),buf.data_end()));
    const char* cur=(const char*)buf.data();
    const char* const end=(const char*)buf.data_end();
    //get checksumType
    static const std::string kPosChecksumTypeTag="\n"+kChecksumTypeTag;
    const char* pos=std::search(cur,end,kPosChecksumTypeTag.begin(),kPosChecksumTypeTag.end());
    const char * posEnd;
    bool isHaveChecksum=false;
    if (pos!=end){
        pos+=kPosChecksumTypeTag.size();
        posEnd=std::find(pos,end,'\n');
        cur=posEnd;
        //while ((pos<posEnd)&&isspace(pos[0])) ++pos;
        //while ((pos<posEnd)&&isspace(posEnd[-1])) --posEnd;
        if (posEnd[-1]=='\r') --posEnd;
        isHaveChecksum=pos<posEnd;
        out_manifest.checksumType.append(pos,posEnd);
    }
    //get path count
    size_t pathCount=0;
    {
        static const std::string kPosPathCountTag="\n"+kPathCountTag;
        pos=std::search(cur,end,kPosPathCountTag.begin(),kPosPathCountTag.end());
        check(pos!=end,"not found path count in manifest error!");
        pos+=kPosPathCountTag.size();
        posEnd=std::find(pos,end,'\n');
        cur=posEnd;
        if (posEnd[-1]=='\r') --posEnd;
        check(a_to_size(pos,posEnd-pos,&pathCount),
              "can't convert input string to number error! "+std::string(pos,posEnd));
        out_manifest.pathList.reserve(pathCount);
        out_manifest.checksumList.reserve(pathCount);
    }
    //pathList
    static const std::string kPosPathTag="\n"+kPathTag;
    while (true) {
        pos=std::search(cur,end,kPosPathTag.begin(),kPosPathTag.end());
        if (out_manifest.pathList.empty()){//rootPath
            check(pos!=end,"not found root path in manifest error!");
        }else{
            if (pos==end) break; //ok finish
        }
        pos+=kPosPathTag.size();
        posEnd=std::find(pos,end,'\n');
        cur=posEnd;
        if (posEnd[-1]=='\r') --posEnd;
        const char* checksumEnd=pos;
        const char* pathBegin=pos;
        if (out_manifest.pathList.empty()){//rootPath
            if (isHaveChecksum&&(rootPathType==kPathType_file)){
                checkv(posEnd[-1]==':');
                checksumEnd=posEnd-1;
                checkv(pos<checksumEnd);
                pathBegin=posEnd;
            }
            checkv(pathBegin==posEnd);
        }else{
            bool pathIsDir=(posEnd[-1]==kPatch_dirSeparator_saved);
            if (isHaveChecksum&&(!pathIsDir)){
                checksumEnd=std::find(pos,posEnd,':');
                checkv(checksumEnd!=posEnd);
                checkv(pos<checksumEnd);
                pathBegin=checksumEnd+1;
            }
            checkv(pathBegin<posEnd);
        }
        _pushPathBack(out_manifest.pathList,out_manifest.rootPath,pathBegin,posEnd);
        _pushChecksumBack(out_manifest.checksumList,pos,checksumEnd);
    }
    check(out_manifest.pathList.size()==pathCount,"manifest path count error!");
}
void checksum_manifest(const TManifestSaved& manifest,hpatch_TChecksum* checksumPlugin){
    if (checksumPlugin==0){
        check(manifest.checksumType.empty(),"checksum_manifest checksumType: "+manifest.checksumType);
        return;
    }
    check(checksumPlugin->checksumType()==manifest.checksumType,
          "checksum_manifest checksumType: "+manifest.checksumType);
    
    for (size_t i=0; i<manifest.pathList.size(); ++i) {
        const std::string& pathName=manifest.pathList[i];
        hpatch_TPathType pathType;
        check(hpatch_getPathStat(pathName.c_str(),&pathType,0),"checksum_manifest find: "+pathName);
        if (isDirName(pathName)){
            check(pathType==kPathType_dir,"checksum_manifest dir: "+pathName);
        }else{
            check(pathType==kPathType_file,"checksum_manifest file: "+pathName);
            const std::vector<TByte>& datas=manifest.checksumList[i];
            CChecksum fileChecksum(checksumPlugin,false);
            CFileStreamInput file(pathName);
            fileChecksum.append(&file.base);
            fileChecksum.appendEnd();
            check(datas==fileChecksum.checksum,"checksum_manifest checksum: "+pathName);
        }
    }
}



#define _test(value) { if (!(value)) { fprintf(stderr,"DirPatch check "#value" error!\n");  return hpatch_FALSE; } }

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
            path.resize(path.size()-1);
        while ((!path.empty())&&(path[path.size()-1]!=kPatch_dirSeparator)) {
            path.resize(path.size()-1);
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
    static hpatch_BOOL _copySameFile(IDirPatchListener* listener,const char* _oldFileName,
                                     const char* _newFileName,hpatch_ICopyDataListener* copyListener){
        CDirPatchListener* self=(CDirPatchListener*)listener->listenerImport;
        std::string oldFileName(_oldFileName);
        std::string newFileName(_newFileName);
        _test(self->isParentDirExists(newFileName));
        _test(self->_oldSet.find(oldFileName)!=self->_oldSet.end());
        _test(self->_newSet.find(newFileName)!=self->_newSet.end());
        _test(fileData_isSame(oldFileName,newFileName,copyListener));
        self->_newSet.erase(newFileName);
        return hpatch_TRUE;
    }
    
    struct _TCheckOutNewDataStream:public hpatch_TStreamOutput{
        explicit _TCheckOutNewDataStream(const char* _newFileName,
                                         TByte* _buf,size_t _bufSize)
        :newData(0),writedLen(0),buf(_buf),bufSize(_bufSize),newFileName(_newFileName){
            hpatch_TFileStreamInput_init(&newFile);
            if (hpatch_TFileStreamInput_open(&newFile,newFileName.c_str())){
                newData=&newFile.base;
                streamSize=newData->streamSize;
            }else{
                streamSize=~(hpatch_StreamPos_t)0;
            }
            streamImport=this;
            read_writed=0;
            write=_write_check;
        }
        ~_TCheckOutNewDataStream(){  hpatch_TFileStreamInput_close(&newFile); }
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
        hpatch_TFileStreamInput            newFile;
        std::string                 newFileName;
    };
    
    TAutoMem _buf;
    static hpatch_BOOL _openNewFile(IDirPatchListener* listener,hpatch_TFileStreamOutput*  out_curNewFile,
                             const char* newFileName,hpatch_StreamPos_t newFileSize){
        CDirPatchListener* self=(CDirPatchListener*)listener->listenerImport;
        _TCheckOutNewDataStream* newFile=new _TCheckOutNewDataStream(newFileName,self->_buf.data(),
                                                                     self->_buf.size());
        out_curNewFile->base=*newFile;
        _test(newFile->isOpenOk());
        _test(self->_newSet.find(newFile->newFileName)!=self->_newSet.end());
        return true;
    }
    static hpatch_BOOL _closeNewFile(IDirPatchListener* listener,hpatch_TFileStreamOutput* curNewFile){
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
    TManifest oldManifest;
    TManifest newManifest;
    oldManifest.rootPath=oldPath;
    newManifest.rootPath=newPath;
    if (isDirName(oldManifest.rootPath)){
        getDirAllPathList(oldManifest.rootPath,oldManifest.pathList,listener,true);
        sortDirPathList(oldManifest.pathList);
    }else{
        oldManifest.pathList.push_back(oldManifest.rootPath);
    }
    if (isDirName(newManifest.rootPath)){
        getDirAllPathList(newManifest.rootPath,newManifest.pathList,listener,false);
        sortDirPathList(newManifest.pathList);
    }else{
        newManifest.pathList.push_back(newManifest.rootPath);
    }
    return check_manifestdiff(listener,oldManifest,newManifest,testDiffData,
                              decompressPlugin,checksumPlugin,kMaxOpenFileNumber);
}

bool check_manifestdiff(IDirDiffListener* listener,const TManifest& oldManifest,const TManifest& newManifest,
                        const hpatch_TStreamInput* testDiffData,hpatch_TDecompress* decompressPlugin,
                        hpatch_TChecksum* checksumPlugin,size_t kMaxOpenFileNumber){
    bool     result=true;
    assert(kMaxOpenFileNumber>=kMaxOpenFileNumber_limit_min);
    const std::vector<std::string>& oldList=oldManifest.pathList;
    const std::vector<std::string>& newList=newManifest.pathList;
    listener->diffPathList(oldList,newList);
    
    CDirPatchListener    patchListener(newManifest.rootPath,oldList,newList);
    CDirPatcher          dirPatcher;
    const TDirDiffInfo*  dirDiffInfo=0;
    TDirPatchChecksumSet    checksumSet={checksumPlugin,hpatch_TRUE,hpatch_TRUE,hpatch_TRUE,hpatch_TRUE};
    TAutoMem             p_temp_mem(kFileIOBufSize*4);
    TByte*               temp_cache=p_temp_mem.data();
    size_t               temp_cache_size=p_temp_mem.size();
    const hpatch_TStreamInput*  oldStream=0;
    const hpatch_TStreamOutput* newStream=0;
    {//dir diff info
        hpatch_TPathType    oldType;
        if (oldManifest.rootPath.empty()){ // isOldPathInputEmpty
            oldType=kPathType_file; //as empty file
        }else{
            _test(hpatch_getPathStat(oldManifest.rootPath.c_str(),&oldType,0));
            _test(oldType!=kPathType_notExist);
        }
        _test(TDirPatcher_open(&dirPatcher,testDiffData,&dirDiffInfo));
        _test(dirDiffInfo->isDirDiff);
        if (dirDiffInfo->oldPathIsDir){
            _test(kPathType_dir==oldType);
        }else{
            _test(kPathType_file==oldType);
        }
    }
    if (checksumPlugin)
        _test(TDirPatcher_checksum(&dirPatcher,&checksumSet));
    _test(TDirPatcher_loadDirData(&dirPatcher,decompressPlugin,
                                  oldManifest.rootPath.c_str(),newManifest.rootPath.c_str()));
    _test(TDirPatcher_openOldRefAsStream(&dirPatcher,kMaxOpenFileNumber,&oldStream));
    _test(TDirPatcher_openNewDirAsStream(&dirPatcher,&patchListener,&newStream));
    _test(TDirPatcher_patch(&dirPatcher,newStream,oldStream,temp_cache,temp_cache+temp_cache_size));
    _test(TDirPatcher_closeNewDirStream(&dirPatcher));
    _test(TDirPatcher_closeOldRefStream(&dirPatcher));
    _test(patchListener.isNewOk());
    return result;
}
#undef _test


#define _check(value) { if (!(value)) { fprintf(stderr,"dirOldDataChecksum check "#value" error!\n"); \
                            result= hpatch_FALSE; goto clear; } }
hpatch_BOOL check_dirOldDataChecksum(const char* oldPath,hpatch_TStreamInput* diffData,
                                     hpatch_TDecompress *decompressPlugin,hpatch_TChecksum *checksumPlugin){
    hpatch_BOOL  result=hpatch_TRUE;
    hpatch_BOOL         isAppendContinue=hpatch_FALSE;
    hpatch_StreamPos_t  readPos=0;
    const char*         savedCompressType=0;
    TDirOldDataChecksum oldCheck;
    TDirOldDataChecksum_init(&oldCheck);
    while (hpatch_TRUE) {
        TByte buf[1024*2];
        size_t dataLen=sizeof(buf);
        if (readPos+dataLen>diffData->streamSize)
            dataLen=(size_t)(diffData->streamSize-readPos);
        if (dataLen>0){
            _check(diffData->read(diffData,readPos,buf,buf+dataLen));
        }
        readPos+=dataLen;
        _check(TDirOldDataChecksum_append(&oldCheck,buf,buf+dataLen,&isAppendContinue));
        if (!isAppendContinue)
            break;
        else
            _check(dataLen>0);
    }
    _check(0==strcmp((checksumPlugin?checksumPlugin->checksumType():""),
                     TDirOldDataChecksum_getChecksumType(&oldCheck)));
    savedCompressType=TDirOldDataChecksum_getCompressType(&oldCheck);
    _check(((decompressPlugin==0)&&(strlen(savedCompressType)==0))||
           decompressPlugin->is_can_open(savedCompressType));
    _check(TDirOldDataChecksum_checksum(&oldCheck,decompressPlugin,checksumPlugin,oldPath));
clear:
    if (!TDirOldDataChecksum_close(&oldCheck)) result=hpatch_FALSE;
    return result;
}
#undef _check

void resave_dirdiff(const hpatch_TStreamInput* in_diff,hpatch_TDecompress* decompressPlugin,
                    const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                    hpatch_TChecksum* checksumPlugin){
    _TDirDiffHead       head;
    TDirDiffInfo        diffInfo;
    assert(in_diff!=0);
    assert(in_diff->read!=0);
    assert(out_diff!=0);
    assert(out_diff->write!=0);
    if (checksumPlugin){
        check((out_diff->read_writed!=0),
              "for update checksum, out_diff->read_writed can't null error!");
    }
    {//read head
        check(read_dirdiff_head(&diffInfo,&head,in_diff),
              "resave_dirdiff() read_dirdiff_head() error!");
        assert(0==strcmp(diffInfo.checksumType,(checksumPlugin?checksumPlugin->checksumType():"")));
        assert(diffInfo.checksumByteSize==(checksumPlugin?checksumPlugin->checksumByteSize():0));
        int compressedCount=diffInfo.hdiffInfo.compressedCount+((head.headDataCompressedSize)?1:0);
        check((decompressPlugin!=0)||(compressedCount<=0),
              "resave_dirdiff() decompressPlugin null error!");
        if ((decompressPlugin)&&(compressedCount>0)){
            check(decompressPlugin->is_can_open(diffInfo.hdiffInfo.compressType),
                  "resave_dirdiff() decompressPlugin cannot open compressed data error!");
        }
    }
    const size_t checksumByteSize=diffInfo.checksumByteSize;
    TPlaceholder diffChecksumPlaceholder(0,0);
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
        //headDataSize
        outDiff.packUInt(head.headDataSize);
        TPlaceholder compress_headData_sizePos=
            outDiff.packUInt_pos(compressPlugin?head.headDataSize:0);//headDataCompressedSize
        {//resave checksum
            outDiff.packUInt(checksumByteSize);
            if (checksumByteSize>0){
                diffChecksumPlaceholder.pos=outDiff.getWritedPos()+checksumByteSize*3;
                diffChecksumPlaceholder.pos_end=diffChecksumPlaceholder.pos+checksumByteSize;
                TStreamClip clip(in_diff,diffInfo.checksumOffset,
                                 diffInfo.checksumOffset+checksumByteSize*4);
                outDiff.pushStream(&clip);
            }
        }
        {//resave headData
            bool isCompressed=(head.headDataCompressedSize>0);
            hpatch_StreamPos_t bufSize=isCompressed?head.headDataCompressedSize:head.headDataSize;
            TStreamClip clip(in_diff,head.headDataOffset,head.headDataOffset+bufSize,
                             (isCompressed?decompressPlugin:0),head.headDataSize);
            outDiff.pushStream(&clip,compressPlugin,compress_headData_sizePos);
        }
        if (head.privateExternDataSize>0){//resave privateExternData
            TStreamClip clip(in_diff,head.privateExternDataOffset,
                             head.privateExternDataOffset+head.privateExternDataSize);
            outDiff.pushStream(&clip);
        }
        if (diffInfo.externDataSize>0){//resave externData
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
        writeToPos+=ofStream.outSize;
    }
    if (checksumByteSize>0){// update dirdiff checksum
        CChecksum    diffChecksum(checksumPlugin,false);
        assert(out_diff->read_writed!=0);
        diffChecksum.append((const hpatch_TStreamInput*)out_diff,0,diffChecksumPlaceholder.pos);
        diffChecksum.append((const hpatch_TStreamInput*)out_diff,diffChecksumPlaceholder.pos_end,writeToPos);
        diffChecksum.appendEnd();
        const std::vector<TByte>& v=diffChecksum.checksum;
        assert(diffChecksumPlaceholder.size()==v.size());
        check(out_diff->write(out_diff,diffChecksumPlaceholder.pos,v.data(),v.data()+v.size()),
              "write diff data checksum error!");
    }
}

#endif
