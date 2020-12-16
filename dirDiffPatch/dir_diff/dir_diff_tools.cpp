// dir_diff_tools.cpp
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
#include "dir_diff_tools.h"

namespace hdiff_private{

    hpatch_StreamPos_t getFileSize(const std::string& fileName){
        hpatch_TPathType   type;
        hpatch_StreamPos_t fileSize;
        checkv(hpatch_getPathStat(fileName.c_str(),&type,&fileSize));
        checkv(type==kPathType_file);
        return fileSize;
    }
    
    
    void CChecksum::append(const hpatch_TStreamInput* data,hpatch_StreamPos_t begin,hpatch_StreamPos_t end){
        if (!_handle) return;
        TAutoMem buf(hpatch_kFileIOBufBetterSize);
        while (begin<end){
            size_t len=buf.size();
            if (len>(end-begin))
                len=(size_t)(end-begin);
            checkv(data->read(data,begin,buf.data(),buf.data()+len));
            append(buf.data(),buf.data()+len);
            begin+=len;
        }
    }
    
    void pushTypes(std::vector<TByte>& out_data,const char* kTypeAndVersion,
                   const char* compressPluginType,hpatch_TChecksum* checksumPlugin){
        //type version
        pushCStr(out_data,kTypeAndVersion);
        pushCStr(out_data,"&");
        {//compressType
            const char* compressType="";
            if (compressPluginType)
                compressType=compressPluginType;
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
    
#if (_IS_NEED_DIR_DIFF_PATCH)
    
    TOffsetStreamOutput::TOffsetStreamOutput(const hpatch_TStreamOutput* base,hpatch_StreamPos_t offset)
    :_base(base),_offset(offset),outSize(0){
        assert(offset<=base->streamSize);
        this->streamImport=this;
        this->streamSize=base->streamSize-offset;
        this->read_writed=0;
        this->write=_write;
    }
    hpatch_BOOL TOffsetStreamOutput::_write(const hpatch_TStreamOutput* stream,const hpatch_StreamPos_t writeToPos,
                                            const unsigned char* data,const unsigned char* data_end){
        TOffsetStreamOutput* self=(TOffsetStreamOutput*)stream->streamImport;
        hpatch_StreamPos_t newSize=writeToPos+(data_end-data);
        if (newSize>self->outSize) self->outSize=newSize;
        return self->_base->write(self->_base,self->_offset+writeToPos,data,data_end);
    }

    void CRefStream::open(const hpatch_TStreamInput** refList,size_t refCount,size_t kAlignSize){
        check(hpatch_TRefStream_open(this,refList,refCount,kAlignSize),"TRefStream_open() refList error!");
    }
    void packIncList(std::vector<TByte>& out_data,const size_t* list,size_t listSize){
        size_t backValue=~(size_t)0;
        for (size_t i=0;i<listSize;++i){
            size_t curValue=list[i];
            assert(curValue>=(size_t)(backValue+1));
            packUInt(out_data,(size_t)(curValue-(size_t)(backValue+1)));
            backValue=curValue;
        }
    }
    
    void packList(std::vector<TByte>& out_data,const hpatch_StreamPos_t* list,size_t listSize){
        for (size_t i=0;i<listSize;++i){
            packUInt(out_data,list[i]);
        }
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
    
    size_t pushNameList(std::vector<TByte>& out_data,const char* rootPath,
                        const std::string* nameList,size_t nameListSize){
        const size_t rootLen=strlen(rootPath);
        std::string utf8;
        size_t outSize=0;
        for (size_t i=0;i<nameListSize;++i){
            const std::string& name=nameList[i];
            const size_t nameSize=name.size();
            assert(nameSize>=rootLen);
            assert(0==memcmp(name.data(),rootPath,rootLen));
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

    
    CFileResHandleLimit::CFileResHandleLimit(size_t _limitMaxOpenCount,size_t resCount)
    :limitMaxOpenCount(_limitMaxOpenCount),curInsert(0){
        hpatch_TResHandleLimit_init(&limit);
        resList.resize(resCount);
        memset(resList.data(),0,sizeof(hpatch_IResHandle)*resCount);
        fileList.resize(resCount);
        for(size_t i=0;i<resCount;++i)
            hpatch_TFileStreamInput_init(&fileList[i]);
    }
    
    void CFileResHandleLimit::addRes(const std::string& fileName,hpatch_StreamPos_t fileSize){
        assert(curInsert<resList.size());
        fileList[curInsert].fileName=fileName;
        hpatch_IResHandle* res=&resList[curInsert];
        res->open=CFileResHandleLimit::openRes;
        res->close=CFileResHandleLimit::closeRes;
        res->resImport=&fileList[curInsert];
        res->resStreamSize=fileSize;
        ++curInsert;
    }
    void CFileResHandleLimit::open(){
        assert(curInsert==resList.size());
        check(hpatch_TResHandleLimit_open(&limit,limitMaxOpenCount,resList.data(),
                                          resList.size()),"TResHandleLimit_open error!");
    }
    bool CFileResHandleLimit::closeFileHandles(){
        return hpatch_TResHandleLimit_closeFileHandles(&limit)!=0;
    }
    void CFileResHandleLimit::close(){
        check(hpatch_TResHandleLimit_close(&limit),"TResHandleLimit_close error!");
    }
    
    hpatch_BOOL CFileResHandleLimit::openRes(struct hpatch_IResHandle* res,hpatch_TStreamInput** out_stream){
        CFile* self=(CFile*)res->resImport;
        assert(self->m_file==0);
        check(hpatch_TFileStreamInput_open(self,self->fileName.c_str()),"CFileResHandleLimit open file error!");
        *out_stream=&self->base;
        return hpatch_TRUE;
    }
    hpatch_BOOL CFileResHandleLimit::closeRes(struct hpatch_IResHandle* res,const hpatch_TStreamInput* stream){
        CFile* self=(CFile*)res->resImport;
        assert(stream==&self->base);
        check(hpatch_TFileStreamInput_close(self),"CFileResHandleLimit close file error!");
        return hpatch_TRUE;
    }
    
#endif
}
