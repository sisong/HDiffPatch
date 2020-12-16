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
#include <map>
#include <set>
#include "../../libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.h"
#include "../../libHDiffPatch/HDiff/private_diff/limit_mem_diff/stream_serialize.h"
#include "../../libHDiffPatch/HDiff/diff.h"
#include "../dir_patch/dir_patch.h"
#include "../dir_patch/dir_patch_private.h"
#include "dir_diff_tools.h"
using namespace hdiff_private;

static const char* kDirDiffVersionType= "HDIFF19";

static std::string  cmp_hash_type    =  "fadler64";
#define cmp_hash_value_t                uint64_t
#define cmp_hash_begin(ph)              { (*(ph))=ADLER_INITIAL; }
#define cmp_hash_append(ph,pvalues,n)   { (*(ph))=fast_adler64_append(*(ph),pvalues,n); }
#define cmp_hash_end(ph)                {}
#define cmp_hash_combine(ph,rightHash,rightLen) { (*(ph))=fast_adler64_by_combine(*(ph),rightHash,rightLen); }

static cmp_hash_value_t getStreamHash(const hpatch_TStreamInput* stream,const std::string& errorTag){
    TAutoMem  mem(hpatch_kFileIOBufBetterSize);
    cmp_hash_value_t result;
    cmp_hash_begin(&result);
    for (hpatch_StreamPos_t pos=0; pos<stream->streamSize;) {
        size_t readLen=hpatch_kFileIOBufBetterSize;
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


static bool fileData_isSame(const std::string& file_x,const std::string& file_y,
                            hpatch_ICopyDataListener* copyListener=0){
    CFileStreamInput f_x(file_x);
    CFileStreamInput f_y(file_y);
    if (f_x.base.streamSize!=f_y.base.streamSize)
        return false;
    TAutoMem  mem(hpatch_kFileIOBufBetterSize*2);
    for (hpatch_StreamPos_t pos=0; pos<f_x.base.streamSize;) {
        size_t readLen=hpatch_kFileIOBufBetterSize;
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

static void packSamePairList(std::vector<TByte>& out_data,const std::vector<hpatch_TSameFilePair>& pairs){
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

struct CChecksumCombine:public CChecksum{
    inline explicit CChecksumCombine(hpatch_TChecksum* checksumPlugin,bool isCanUseCombine)
    :CChecksum(checksumPlugin),_isCanUseCombine(isCanUseCombine) {
        if (checksumPlugin&&_isCanUseCombine) cmp_hash_begin(&_combineHash); }
    inline void appendEnd(){
        CChecksum::appendEnd();
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
    const bool              _isCanUseCombine;
    cmp_hash_value_t        _combineHash;
};

void dir_diff(IDirDiffListener* listener,const TManifest& oldManifest,
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
    CChecksumCombine oldRefChecksum(checksumPlugin,isCachedHashs);
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
    CChecksumCombine newRefChecksum(checksumPlugin,isCachedHashs);
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
    CChecksumCombine sameFileChecksum(checksumPlugin,isCachedHashs);
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
    size_t oldPathSumSize=pushNameList(headData,oldManifest.rootPath,oldList);
    size_t newPathSumSize=pushNameList(headData,newManifest.rootPath,newList);
    packIncList(headData,oldRefIList);
    packIncList(headData,newRefIList);
    packList(headData,newRefSizeList);
    packSamePairList(headData,dataSamePairList);
    packIncList(headData,newExecuteList);
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
            swapClear(headCode); //compress error or give up
    }
    
    //serialize  dir diff data
    std::vector<TByte> out_data;
    pushTypes(out_data,kDirDiffVersionType,compressPlugin?compressPlugin->compressType():0,checksumPlugin);
    //head info
    packUInt(out_data,oldIsDir?1:0);
    packUInt(out_data,newIsDir?1:0);
    packUInt(out_data,oldList.size());
    packUInt(out_data,oldPathSumSize);
    packUInt(out_data,newList.size());
    packUInt(out_data,newPathSumSize);
    packUInt(out_data,oldRefIList.size());      swapClear(oldRefIList);
    packUInt(out_data,oldRefStream.stream->streamSize); //same as hdiffz::oldDataSize
    packUInt(out_data,newRefIList.size());      swapClear(newRefIList);
    packUInt(out_data,newRefStream.stream->streamSize); //same as hdiffz::newDataSize
    packUInt(out_data,dataSamePairList.size()); swapClear(dataSamePairList);
    packUInt(out_data,sameFileSize);
    packUInt(out_data,newExecuteList.size());   swapClear(newExecuteList);
    packUInt(out_data,privateReservedData.size()); swapClear(privateReservedData);
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
                        writeToPos+=v.size();  swapClear(v); }
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

#define _test(value) { if (!(value)) { fprintf(stderr,"DirPatch check "#value" error!\n");  return hpatch_FALSE; } }

struct CDirPatchListener:public IDirPatchListener{
    explicit CDirPatchListener(const std::string& newRootDir,
                               const std::vector<std::string>& oldList,
                               const std::vector<std::string>& newList)
    :_oldSet(oldList.begin(),oldList.end()),_newSet(newList.begin(),newList.end()),
    _buf(hpatch_kFileIOBufBetterSize){
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

bool check_dirdiff(IDirDiffListener* listener,const TManifest& oldManifest,const TManifest& newManifest,
                   const hpatch_TStreamInput* testDiffData,hpatch_TDecompress* decompressPlugin,
                   hpatch_TChecksum* checksumPlugin,size_t kMaxOpenFileNumber){
    bool     result=true;
    assert(kMaxOpenFileNumber>=kMaxOpenFileNumber_limit_min);
    const std::vector<std::string>& oldList=oldManifest.pathList;
    const std::vector<std::string>& newList=newManifest.pathList;
    
    CDirPatchListener    patchListener(newManifest.rootPath,oldList,newList);
    CDirPatcher          dirPatcher;
    const TDirDiffInfo*  dirDiffInfo=0;
    TDirPatchChecksumSet    checksumSet={checksumPlugin,hpatch_TRUE,hpatch_TRUE,hpatch_TRUE,hpatch_TRUE};
    TAutoMem             p_temp_mem(hpatch_kFileIOBufBetterSize*4);
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
            pushTypes(out_type,kDirDiffVersionType,compressPlugin?compressPlugin->compressType():0,checksumPlugin);
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
        CChecksum diffChecksum(checksumPlugin);
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
