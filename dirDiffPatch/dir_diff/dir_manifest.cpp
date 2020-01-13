// dir_manifest.cpp
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
#include "dir_manifest.h"
#if (_IS_NEED_DIR_DIFF_PATCH)
#include "../../_atosize.h"
#include "dir_diff_tools.h"
using namespace hdiff_private;

struct CDir{
    inline CDir(const std::string& dir):handle(0){ handle=hdiff_dirOpenForRead(dir.c_str()); }
    inline ~CDir(){ hdiff_dirClose(handle); }
    hdiff_TDirHandle handle;
};
static void _getDirSubFileList(const std::string& dirPath,std::vector<std::string>& out_list,
                               IDirPathIgnore* filter,size_t rootPathNameLen){
    assert(!hdiff_private::isDirName(dirPath));
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
            assert(!hdiff_private::isDirName(subName));
            switch (type) {
                case kPathType_dir:{
                    assignDirTag(subName);
                    if (!filter->isNeedIgnore(subName,rootPathNameLen)){
                        subDirs.push_back(subName.substr(0,subName.size()-1)); //no '/'
                        out_list.push_back(subName); //add dir
                    }
                } break;
                case kPathType_file:{
                    if (!filter->isNeedIgnore(subName,rootPathNameLen))
                        out_list.push_back(subName); //add file
                } break;
                default:{
                    //nothing
                } break;
            }
        }
    }
    
    for (size_t i=0; i<subDirs.size(); ++i) {
        assert(!hdiff_private::isDirName(subDirs[i]));
        _getDirSubFileList(subDirs[i],out_list,filter,rootPathNameLen);
    }
}
void getDirAllPathList(const std::string& dirPath,std::vector<std::string>& out_list,
                       IDirPathIgnore* filter){
    assert(hdiff_private::isDirName(dirPath));
    out_list.push_back(dirPath);
    const std::string dirName(dirPath.c_str(),dirPath.c_str()+dirPath.size()-1); //without '/'
    _getDirSubFileList(dirName,out_list,filter,dirName.size());
    sortDirPathList(out_list);
}

void get_manifest(IDirPathIgnore* listener,const std::string& inputPath,TManifest& out_manifest){
    assert(listener!=0);
    std::vector<std::string>& pathList=out_manifest.pathList;
    pathList.clear();
    out_manifest.rootPath=inputPath;
    const bool inputIsDir=isDirName(inputPath);
    if (inputIsDir){
        getDirAllPathList(inputPath,pathList,listener);
        sortDirPathList(pathList);
    }else{
        pathList.push_back(inputPath);
    }
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

void save_manifest(const TManifest& manifest,
                   const hpatch_TStreamOutput* outManifest,hpatch_TChecksum* checksumPlugin){
    const std::vector<std::string>& pathList=manifest.pathList;
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
            CChecksum  fileChecksum(checksumPlugin);
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
        const char* savedPath=_toSavedPath(tempPath,pathName.c_str()+manifest.rootPath.size());
        pushCStr(out_data, savedPath);
        pushCStr(out_data, "\n");
    }
    check(outManifest->write(outManifest,0,out_data.data(),out_data.data()+out_data.size()),
          "write manifest data error!");
}

void save_manifest(IDirPathIgnore* listener,const std::string& inputPath,
                   const hpatch_TStreamOutput* outManifest,hpatch_TChecksum* checksumPlugin){
    TManifest manifest;
    get_manifest(listener,inputPath,manifest);
    save_manifest(manifest,outManifest,checksumPlugin);
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
    static void _hex2data(TByte* out_data,const char* hex_begin,size_t hexSize){
        for (size_t i=0;i<hexSize;i+=2){
            out_data[i>>1]=_hex2b(hex_begin[i]) | (_hex2b(hex_begin[i+1])<<4);
        }
    }
void load_manifest(TManifestSaved& out_manifest,const std::string& rootPath,
                   const hpatch_TStreamInput* manifestStream){
    out_manifest.pathList.clear();
    out_manifest.checksumList.clear();
    out_manifest.checksumType.clear();
    out_manifest.rootPath=rootPath;
    out_manifest.checksumByteSize=0;
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
        size_t cur_i=out_manifest.pathList.size();
        checkv(cur_i<pathCount);
        _pushPathBack(out_manifest.pathList,out_manifest.rootPath,pathBegin,posEnd);
        const size_t hexSize=(checksumEnd-pos);
        if (hexSize>0){
            size_t& checksumByteSize=out_manifest.checksumByteSize;
            if (checksumByteSize==0){
                checksumByteSize=hexSize/2; //ok get it
                out_manifest.checksumList.resize(pathCount*checksumByteSize,0);
            }
            checkv(checksumByteSize*2==hexSize);
            _hex2data(&out_manifest.checksumList[cur_i*checksumByteSize],pos,hexSize);
        }
    }
    check(out_manifest.pathList.size()==pathCount,"manifest path count error!");
}
void checksum_manifest(const TManifestSaved& manifest,hpatch_TChecksum* checksumPlugin){
    if (checksumPlugin==0){
        check(manifest.checksumType.empty(),
              "checksum_manifest checksumPlugin can't null, need checksumType: "+manifest.checksumType);
        return; //ok not need checksum
    }
    check(checksumPlugin->checksumType()==manifest.checksumType,
          "checksum_manifest checksumType: "+manifest.checksumType);
    size_t checksumByteSize=manifest.checksumByteSize;
    if (checksumByteSize>0)
        check(checksumByteSize==checksumPlugin->checksumByteSize(),"checksum_manifest checksumByteSize");
    
    for (size_t i=0; i<manifest.pathList.size(); ++i) {
        const std::string& pathName=manifest.pathList[i];
        hpatch_TPathType pathType;
        check(hpatch_getPathStat(pathName.c_str(),&pathType,0),"checksum_manifest find: "+pathName);
        if (isDirName(pathName)){
            check(pathType==kPathType_dir,"checksum_manifest dir: "+pathName);
        }else{
            check(pathType==kPathType_file,"checksum_manifest file: "+pathName);
            CChecksum fileChecksum(checksumPlugin);
            CFileStreamInput file(pathName);
            fileChecksum.append(&file.base);
            fileChecksum.appendEnd();
            const TByte* savedChecksum=&manifest.checksumList[i*checksumByteSize];
            check(0==memcmp(savedChecksum,fileChecksum.checksum.data(),checksumByteSize),
                  "checksum_manifest checksum: "+pathName);
        }
    }
}


#endif
