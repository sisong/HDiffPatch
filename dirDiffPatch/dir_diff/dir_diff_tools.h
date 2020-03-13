// dir_diff_tools.h
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
#ifndef hdiff_dir_diff_tools_h
#define hdiff_dir_diff_tools_h
#include "dir_diff.h"
#include <algorithm> //sort
#include "../../libHDiffPatch/HDiff/private_diff/pack_uint.h"
#include "../../libHDiffPatch/HDiff/private_diff/mem_buf.h"
#include "../../libHDiffPatch/HPatch/checksum_plugin.h"
#include "../../libHDiffPatch/HDiff/diff_types.h"
#include "../../file_for_patch.h"

#define check(value,info) do { if (!(value)) { throw std::runtime_error(info); } } while(0)
#define checkv(value)     do { check(value,"check "#value" error!"); } while(0)

#if (_IS_NEED_DIR_DIFF_PATCH)
#include "../dir_patch/ref_stream.h"
#include "../dir_patch/res_handle_limit.h"
#include "file_for_dirDiff.h"
#endif

namespace hdiff_private{

template <class TVector>
static inline void swapClear(TVector& v){ TVector _t; v.swap(_t); }

static inline bool isDirName(const std::string& path_utf8){
    return 0!=hpatch_getIsDirName(path_utf8.c_str());
}

hpatch_StreamPos_t getFileSize(const std::string& fileName);

inline static void writeStream(const hpatch_TStreamOutput* out_stream,hpatch_StreamPos_t& outPos,
                               const TByte* buf_begin,const TByte* buf_end){
    checkv(out_stream->write(out_stream,outPos,buf_begin,buf_end));
    outPos+=(size_t)(buf_end-buf_begin);
}
inline static void writeStream(const hpatch_TStreamOutput* out_stream,hpatch_StreamPos_t& outPos,
                               const TByte* buf,size_t byteSize){
    writeStream(out_stream,outPos,buf,buf+byteSize);
}
inline static void writeStream(const hpatch_TStreamOutput* out_stream,hpatch_StreamPos_t& outPos,
                               const std::vector<TByte>& buf){
    writeStream(out_stream,outPos,buf.data(),buf.data()+buf.size());
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

struct CFileStreamOutput:public hpatch_TFileStreamOutput{
    inline CFileStreamOutput(){ hpatch_TFileStreamOutput_init(this); }
    inline void open(const std::string& fileName,hpatch_StreamPos_t max_file_length){
        assert(this->base.streamImport==0);
        check(hpatch_TFileStreamOutput_open(this,fileName.c_str(),max_file_length),
              "write file \""+fileName+"\" error!"); }
    inline CFileStreamOutput(const std::string& fileName,hpatch_StreamPos_t max_file_length){
        hpatch_TFileStreamOutput_init(this); open(fileName,max_file_length); }
    inline void closeFile() { check(hpatch_TFileStreamOutput_close(this),"close file error!"); }
    inline ~CFileStreamOutput(){ closeFile(); }
};

struct CChecksum{
    inline explicit CChecksum(hpatch_TChecksum* checksumPlugin,bool autoBegin=true)
    :_checksumPlugin(checksumPlugin),_handle(0){
        if (checksumPlugin){
            _handle=checksumPlugin->open(checksumPlugin);
            checkv(_handle!=0);
            if (autoBegin) appendBegin();
        } }
    inline ~CChecksum(){ if (_handle) _checksumPlugin->close(_checksumPlugin,_handle); }
    inline void append(const unsigned char* data,const unsigned char* data_end){
        if (_handle) _checksumPlugin->append(_handle,data,data_end); }
    inline void append(const std::vector<TByte>& data){ append(data.data(),data.data()+data.size()); }
    inline void append(const hpatch_TStreamInput* data){ append(data,0,data->streamSize); }
    void append(const hpatch_TStreamInput* data,hpatch_StreamPos_t begin,hpatch_StreamPos_t end);
    inline void appendBegin(){ if (_handle) _checksumPlugin->begin(_handle); }
    inline void appendEnd(){
        if (_handle){
            checksum.resize(_checksumPlugin->checksumByteSize());
            _checksumPlugin->end(_handle,checksum.data(),checksum.data()+checksum.size());
        }
    }
    hpatch_TChecksum*       _checksumPlugin;
    hpatch_checksumHandle   _handle;
    std::vector<TByte>      checksum;
};
    
void pushTypes(std::vector<TByte>& out_data,const char* kTypeAndVersion,
               const char* compressPluginType,hpatch_TChecksum* checksumPlugin);

#if (_IS_NEED_DIR_DIFF_PATCH)

struct TOffsetStreamOutput:public hpatch_TStreamOutput{
    explicit TOffsetStreamOutput(const hpatch_TStreamOutput* base,hpatch_StreamPos_t offset);
    const hpatch_TStreamOutput* _base;
    hpatch_StreamPos_t          _offset;
    hpatch_StreamPos_t          outSize;
    static hpatch_BOOL _write(const hpatch_TStreamOutput* stream,const hpatch_StreamPos_t writeToPos,
                              const unsigned char* data,const unsigned char* data_end);
};
    
struct CFileResHandleLimit{
    CFileResHandleLimit(size_t _limitMaxOpenCount,size_t resCount);
    inline ~CFileResHandleLimit() { close(); }
    void addRes(const std::string& fileName,hpatch_StreamPos_t fileSize);
    void open();
    bool closeFileHandles();
    void close();
    
    struct CFile:public hpatch_TFileStreamInput{
        std::string  fileName;
    };
    hpatch_TResHandleLimit          limit;
    std::vector<CFile>              fileList;
    std::vector<hpatch_IResHandle>  resList;
    size_t                          limitMaxOpenCount;
    size_t                          curInsert;
    static hpatch_BOOL openRes(struct hpatch_IResHandle* res,hpatch_TStreamInput** out_stream);
    static hpatch_BOOL closeRes(struct hpatch_IResHandle* res,const hpatch_TStreamInput* stream);
};

struct CRefStream:public hpatch_TRefStream{
    inline CRefStream(){ hpatch_TRefStream_init(this); }
    void open(const hpatch_TStreamInput** refList,size_t refCount,size_t kAlignSize=1);
    inline ~CRefStream(){ hpatch_TRefStream_close(this); }
};

size_t pushNameList(std::vector<TByte>& out_data,const char* rootPath,
                    const std::string* nameList,size_t nameListSize);
inline static size_t pushNameList(std::vector<TByte>& out_data,const std::string& rootPath,
                                  const std::vector<std::string>& nameList){
    return pushNameList(out_data,rootPath.c_str(),nameList.data(),nameList.size()); }

void packList(std::vector<TByte>& out_data,const hpatch_StreamPos_t* list,size_t listSize);
inline static void packList(std::vector<TByte>& out_data,const std::vector<hpatch_StreamPos_t>& list){
    packList(out_data,list.data(),list.size()); }
void packIncList(std::vector<TByte>& out_data,const size_t* list,size_t listSize);
inline static void packIncList(std::vector<TByte>& out_data,const std::vector<size_t>& list){
    packIncList(out_data,list.data(),list.size()); }

#endif
}

#endif //hdiff_dir_diff_tools_h
