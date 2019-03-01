//file_for_patch.c
// patch demo file tool
//
/*
 This is the HDiffPatch copyright.

 Copyright (c) 2012-2019 HouSisong All Rights Reserved.

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
#include "file_for_patch.h"
#include <sys/stat.h> //stat mkdir
#include <errno.h>    //errno

hpatch_BOOL _hpatch_getPathStat_noEndDirSeparator(const char* path_utf8,hpatch_TPathType* out_type,
                                                  hpatch_StreamPos_t* out_fileSize,size_t* out_st_mode){
#if (_IS_USED_WIN32_UTF8_WAPI)
    int            wsize;
    wchar_t        path_w[hpatch_kPathMaxSize];
    struct _stat64 s;
#else
#   ifdef _MSC_VER
    struct _stat64 s;
#   else
    struct stat  s;
#   endif
#endif
    
    int          rt;
    assert(out_type!=0);
    memset(&s,0,sizeof(s));
#if (_IS_USED_WIN32_UTF8_WAPI)
    wsize=_utf8FileName_to_w(path_utf8,path_w,hpatch_kPathMaxSize);
    if (wsize<=0) return hpatch_FALSE;
    rt = _wstat64(path_w,&s);
#else
#   ifdef _MSC_VER
    rt = _stat64(path_utf8,&s);
#   else
    rt = stat(path_utf8,&s);
#   endif
#endif
    
    if(rt!=0){
        if (errno==ENOENT){
            *out_type=kPathType_notExist;
            return hpatch_TRUE;
        }
        return hpatch_FALSE; //error
    }else if ((s.st_mode&S_IFMT)==S_IFREG){
        *out_type=kPathType_file;
        if (out_fileSize) *out_fileSize=s.st_size;
        if (out_st_mode) *out_st_mode=s.st_mode;
        return hpatch_TRUE;
    }else if ((s.st_mode&S_IFMT)==S_IFDIR){
        *out_type=kPathType_dir;
        if (out_fileSize) *out_fileSize=0;
        if (out_st_mode) *out_st_mode=s.st_mode;
        return hpatch_TRUE;
    }else{
        return hpatch_FALSE; //as error; unknow how to dispose
    }
}


hpatch_BOOL hpatch_getTempPathName(const char* path_utf8,char* out_tempPath_utf8,char* out_tempPath_end){
    //use tmpnam()?
#define _AddingLen 8
    size_t i;
    size_t len=strlen(path_utf8);
    if ((len>0)&&(path_utf8[len-1]==kPatch_dirSeparator)) --len; //without '/'
    if (len+(4+_AddingLen)>(size_t)(out_tempPath_end-out_tempPath_utf8)) return hpatch_FALSE;
    memcpy(out_tempPath_utf8,path_utf8,len);
    out_tempPath_utf8+=len;
    for (i=1; i<1000; ++i) {
        hpatch_TPathType tmpPathType;
        char adding[_AddingLen]={'0','0','0','.','t','m','p','\0'};
        adding[2]+=i%10;    adding[1]+=(i/10)%10;   adding[0]+=(i/100)%10;
        memcpy(out_tempPath_utf8,adding,sizeof(adding));
        if (!_hpatch_getPathStat_noEndDirSeparator(out_tempPath_utf8,&tmpPathType,0,0)) return hpatch_FALSE;
        if (tmpPathType==kPathType_notExist)
            return hpatch_TRUE; //ok
    }
    return hpatch_FALSE;
#undef _AddingLen
}


hpatch_BOOL hpatch_renamePath(const char* oldPath_utf8,const char* newPath_utf8){
    _path_noEndDirSeparator(oldPath,oldPath_utf8); {
        _path_noEndDirSeparator(newPath,newPath_utf8); {
#if (_IS_USED_WIN32_UTF8_WAPI)
            int     wsize;
            wchar_t oldPath_w[hpatch_kPathMaxSize];
            wchar_t newPath_w[hpatch_kPathMaxSize];
            wsize=_utf8FileName_to_w(oldPath,oldPath_w,hpatch_kPathMaxSize);
            if (wsize<=0) return hpatch_FALSE;
            wsize=_utf8FileName_to_w(newPath,newPath_w,hpatch_kPathMaxSize);
            if (wsize<=0) return hpatch_FALSE;
            return 0==_wrename(oldPath_w,newPath_w);
#else
            return 0==rename(oldPath,newPath);
#endif
        }
    }
}

hpatch_BOOL hpatch_removeFile(const char* fileName_utf8){
    _path_noEndDirSeparator(fileName,fileName_utf8);{
#if (_IS_USED_WIN32_UTF8_WAPI)
        int     wsize;
        wchar_t path_w[hpatch_kPathMaxSize];
        wsize=_utf8FileName_to_w(fileName,path_w,hpatch_kPathMaxSize);
        if (wsize<=0) return hpatch_FALSE;
        return 0==_wremove(path_w);
#else
        return 0==remove(fileName);
#endif
    }
}

#if (_IS_NEED_DIR_DIFF_PATCH)

hpatch_BOOL hpatch_removeDir(const char* dirName_utf8){
    _path_noEndDirSeparator(dirName,dirName_utf8);{
#if (_IS_USED_WIN32_UTF8_WAPI)
        int     wsize;
        wchar_t path_w[hpatch_kPathMaxSize];
        wsize=_utf8FileName_to_w(dirName,path_w,hpatch_kPathMaxSize);
        if (wsize<=0) return hpatch_FALSE;
        return 0==_wrmdir(path_w);
#else
#   ifdef _MSC_VER
        return 0==_rmdir(dirName);
#   else
        return 0==rmdir(dirName);
#   endif
#endif
    }
}

hpatch_BOOL hpatch_moveFile(const char* oldPath_utf8,const char* newPath_utf8){
    return hpatch_renamePath(oldPath_utf8,newPath_utf8);
}

hpatch_BOOL hpatch_makeNewDir(const char* dirName_utf8){
    hpatch_TPathType type;
    _path_noEndDirSeparator(path,dirName_utf8);
    if (!_hpatch_getPathStat_noEndDirSeparator(path,&type,0,0)) return hpatch_FALSE; //error
    switch (type) {
        case kPathType_dir :{ return hpatch_TRUE; } break; //exist
        case kPathType_file:{ return hpatch_FALSE; } break; //error, not overwite
        case kPathType_notExist:{
#if (_IS_USED_WIN32_UTF8_WAPI)
            int     wsize;
            wchar_t path_w[hpatch_kPathMaxSize];
            wsize=_utf8FileName_to_w(path,path_w,hpatch_kPathMaxSize);
            if (wsize<=0) return hpatch_FALSE;
            return 0==_wmkdir(path_w);
#else
#   ifdef _MSC_VER
            return 0==_mkdir(path);
#   else
            const mode_t kDefalutMode=S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;//0755
            return 0==mkdir(path,kDefalutMode);
#   endif
#endif
        } break;
    }
    return hpatch_FALSE;
}

#endif //_IS_NEED_DIR_DIFF_PATCH

hpatch_BOOL hpatch_getIsExecuteFile(const char* fileName){
#ifdef _WIN32
    return hpatch_FALSE; // now, not need execute info
#else
    hpatch_TPathType type;
    size_t           st_mode=0;
    if (!_hpatch_getPathStat_noEndDirSeparator(fileName,&type,0,&st_mode)) return hpatch_FALSE;
    if (type!=kPathType_file) return hpatch_FALSE;// now, not need execute info
    return (((mode_t)st_mode&S_IXUSR)!=0);
#endif
}

hpatch_BOOL hpatch_setIsExecuteFile(const char* fileName){
#ifdef _WIN32
    return hpatch_TRUE; // now, not need execute info
#else
    hpatch_TPathType type;
    size_t           st_mode=0;
    if (!_hpatch_getPathStat_noEndDirSeparator(fileName,&type,0,&st_mode)) return hpatch_FALSE;
    return 0==chmod(fileName,(mode_t)st_mode|S_IXUSR|S_IXGRP|S_IXOTH);
#endif
}


hpatch_inline static
hpatch_BOOL _import_fileTell64(hpatch_FileHandle file,hpatch_StreamPos_t* out_pos){
#ifdef _MSC_VER
    __int64 fpos=_ftelli64(file);
#else
    off_t fpos=ftello(file);
#endif
    hpatch_BOOL result=(fpos>=0);
    if (result) *out_pos=fpos;
    return result;
}

hpatch_inline static
hpatch_BOOL _import_fileSeek64(hpatch_FileHandle file,hpatch_StreamPos_t seekPos,int whence){
#ifdef _MSC_VER
    int ret=_fseeki64(file,seekPos,whence);
#else
    off_t fpos=seekPos;
    if ((fpos<0)||((hpatch_StreamPos_t)fpos!=seekPos)) return hpatch_FALSE;
    int ret=fseeko(file,fpos,whence);
#endif
    return (ret==0);
}


hpatch_inline static
hpatch_BOOL _import_fileClose(hpatch_FileHandle* pfile){
    hpatch_FileHandle file=*pfile;
    if (file){
        *pfile=0;
        if (0!=fclose(file))
            return hpatch_FALSE;
    }
    return hpatch_TRUE;
}

hpatch_BOOL _import_fileRead(hpatch_FileHandle file,TByte* buf,TByte* buf_end){
    while (buf<buf_end) {
        size_t readLen=(size_t)(buf_end-buf);
        if (readLen>hpatch_kFileIOBestMaxSize) readLen=hpatch_kFileIOBestMaxSize;
        if (readLen!=fread(buf,1,readLen,file)) return hpatch_FALSE;
        buf+=readLen;
    }
    return buf==buf_end;
}

hpatch_BOOL _import_fileWrite(hpatch_FileHandle file,const TByte* data,const TByte* data_end){
    while (data<data_end) {
        size_t writeLen=(size_t)(data_end-data);
        if (writeLen>hpatch_kFileIOBestMaxSize) writeLen=hpatch_kFileIOBestMaxSize;
        if (writeLen!=fwrite(data,1,writeLen,file)) return hpatch_FALSE;
        data+=writeLen;
    }
    return data==data_end;
}

hpatch_inline static
hpatch_BOOL _import_fileFlush(hpatch_FileHandle writedFile){
    return (0==fflush(writedFile));
}

#if (_IS_USED_WIN32_UTF8_WAPI)
#   define _kFileReadMode  L"rb"
#   define _kFileWriteMode L"wb+"
#else
#   define _kFileReadMode  "rb"
#   define _kFileWriteMode "wb+"
#endif

#if (_IS_USED_WIN32_UTF8_WAPI)
static hpatch_FileHandle _import_fileOpenByMode(const char* fileName_utf8,const wchar_t* mode_w){
    wchar_t fileName_w[hpatch_kPathMaxSize];
    int wsize=_utf8FileName_to_w(fileName_utf8,fileName_w,hpatch_kPathMaxSize);
    if (wsize>0) {
# if (_MSC_VER>=1400) // VC2005
        hpatch_FileHandle file=0;
        errno_t err=_wfopen_s(&file,fileName_w,mode_w);
        return (err==0)?file:0;
# else
        return _wfopen(fileName_w,mode_w);
# endif
    }else{
        return 0;
    }
}
#else
hpatch_inline static
hpatch_FileHandle _import_fileOpenByMode(const char* fileName_utf8,const char* mode){
    return fopen(fileName_utf8,mode); }
#endif

#define _file_error(fileHandle){ \
    if (fileHandle) _import_fileClose(&fileHandle); \
    return hpatch_FALSE; \
}

hpatch_BOOL _import_fileOpenRead(const char* fileName_utf8,hpatch_FileHandle* out_fileHandle,
                                 hpatch_StreamPos_t* out_fileLength){
    hpatch_FileHandle file=0;
    assert(out_fileHandle!=0);
    if (out_fileHandle==0) _file_error(file);
    file=_import_fileOpenByMode(fileName_utf8,_kFileReadMode);
    if (file==0) _file_error(file);
    if (out_fileLength!=0){
        hpatch_StreamPos_t file_length=0;
        if (!_import_fileSeek64(file,0,SEEK_END)) _file_error(file);
        if (!_import_fileTell64(file,&file_length)) _file_error(file);
        if (!_import_fileSeek64(file,0,SEEK_SET)) _file_error(file);
        *out_fileLength=file_length;
    }
    *out_fileHandle=file;
    return hpatch_TRUE;
}

hpatch_BOOL _import_fileOpenCreateOrReWrite(const char* fileName_utf8,hpatch_FileHandle* out_fileHandle){
    hpatch_FileHandle file=0;
    assert(out_fileHandle!=0);
    if (out_fileHandle==0) _file_error(file);
    file=_import_fileOpenByMode(fileName_utf8,_kFileWriteMode);
    if (file==0) _file_error(file);
    *out_fileHandle=file;
    return hpatch_TRUE;
}

#undef _file_error


#define _fileError_return { self->fileError=hpatch_TRUE; return hpatch_FALSE; }

    static hpatch_BOOL _TFileStreamInput_read_file(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                                   TByte* out_data,TByte* out_data_end){
        size_t readLen;
        hpatch_TFileStreamInput* self=(hpatch_TFileStreamInput*)stream->streamImport;
        assert(out_data<=out_data_end);
        readLen=(size_t)(out_data_end-out_data);
        if (readLen==0) return hpatch_TRUE;
        if ((readLen>self->base.streamSize)
            ||(readFromPos>self->base.streamSize-readLen)) _fileError_return;
        if (self->m_fpos!=readFromPos+self->m_offset){
            if (!_import_fileSeek64(self->m_file,readFromPos+self->m_offset,SEEK_SET)) _fileError_return;
        }
        if (!_import_fileRead(self->m_file,out_data,out_data+readLen)) _fileError_return;
        self->m_fpos=readFromPos+self->m_offset+readLen;
        return hpatch_TRUE;
    }

hpatch_BOOL hpatch_TFileStreamInput_open(hpatch_TFileStreamInput* self,const char* fileName_utf8){
    assert(self->m_file==0);
    if (self->m_file) return hpatch_FALSE;
    if (!_import_fileOpenRead(fileName_utf8,&self->m_file,&self->base.streamSize)) return hpatch_FALSE;
    
    self->base.streamImport=self;
    self->base.read=_TFileStreamInput_read_file;
    self->m_fpos=0;
    self->m_offset=0;
    self->fileError=hpatch_FALSE;
    return hpatch_TRUE;
}

hpatch_BOOL hpatch_TFileStreamInput_setOffset(hpatch_TFileStreamInput* self,hpatch_StreamPos_t offset){
    if (self->base.streamSize<offset)
        return hpatch_FALSE;
    self->m_offset+=offset;
    self->base.streamSize-=offset;
    return hpatch_TRUE;
}

hpatch_BOOL hpatch_TFileStreamInput_close(hpatch_TFileStreamInput* self){
    return _import_fileClose(&self->m_file);
}


    static hpatch_BOOL _TFileStreamOutput_write_file(const hpatch_TStreamOutput* stream,hpatch_StreamPos_t writeToPos,
                                   const TByte* data,const TByte* data_end){
        size_t writeLen;
        hpatch_TFileStreamOutput* self=(hpatch_TFileStreamOutput*)stream->streamImport;
        assert(data<=data_end);
        assert(self->m_offset==0);
        writeLen=(size_t)(data_end-data);
        if (writeLen==0) return hpatch_TRUE;
        if ((writeLen>self->base.streamSize)
            ||(writeToPos>self->base.streamSize-writeLen)) _fileError_return;
        self->is_in_readModel=hpatch_FALSE;
        if (writeToPos!=self->m_fpos){
            if (self->is_random_out){
                if (!_import_fileSeek64(self->m_file,writeToPos,SEEK_SET)) _fileError_return;
                self->m_fpos=writeToPos;
            }else{
                _fileError_return;
            }
        }
        if (!_import_fileWrite(self->m_file,data,data+writeLen)) _fileError_return;
        self->m_fpos=writeToPos+writeLen;
        self->out_length=(self->out_length>=self->m_fpos)?self->out_length:self->m_fpos;
        return hpatch_TRUE;
    }
    static hpatch_BOOL _hpatch_TFileStreamOutput_read_file(const hpatch_TStreamOutput* stream,
                                                    hpatch_StreamPos_t readFromPos,
                                                    TByte* out_data,TByte* out_data_end){
         //hpatch_TFileStreamOutput is A hpatch_TFileStreamInput !
        hpatch_TFileStreamOutput* self=(hpatch_TFileStreamOutput*)stream->streamImport;
        const hpatch_TStreamInput* in_stream=(const hpatch_TStreamInput*)stream;
        if (!self->is_in_readModel){
            if (!hpatch_TFileStreamOutput_flush(self)) return hpatch_FALSE;
            self->is_in_readModel=hpatch_TRUE;
        }
        return _TFileStreamInput_read_file(in_stream,readFromPos,out_data,out_data_end);
    }
hpatch_BOOL hpatch_TFileStreamOutput_open(hpatch_TFileStreamOutput* self,const char* fileName_utf8,
                                          hpatch_StreamPos_t max_file_length){
    assert(self->m_file==0);
    if (self->m_file) return hpatch_FALSE;
    if (!_import_fileOpenCreateOrReWrite(fileName_utf8,&self->m_file)) return hpatch_FALSE;
    
    self->base.streamImport=self;
    self->base.streamSize=max_file_length;
    self->base.read_writed=_hpatch_TFileStreamOutput_read_file;
    self->base.write=_TFileStreamOutput_write_file;
    self->m_fpos=0;
    self->m_offset=0;
    self->fileError=hpatch_FALSE;
    self->is_in_readModel=hpatch_FALSE;
    self->is_random_out=hpatch_FALSE;
    self->out_length=0;
    return hpatch_TRUE;
}

hpatch_BOOL hpatch_TFileStreamOutput_flush(hpatch_TFileStreamOutput* self){
    return _import_fileFlush(self->m_file);
}

hpatch_BOOL hpatch_TFileStreamOutput_close(hpatch_TFileStreamOutput* self){
    return _import_fileClose(&self->m_file);
}
