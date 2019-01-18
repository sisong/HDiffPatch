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

    #define _fileError_return { self->fileError=hpatch_TRUE; return hpatch_FALSE; }

    static hpatch_BOOL _read_file(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                  TByte* out_data,TByte* out_data_end){
        size_t readLen;
        TFileStreamInput* self=(TFileStreamInput*)stream->streamImport;
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

hpatch_BOOL TFileStreamInput_open(TFileStreamInput* self,const char* fileName_utf8){
    assert(self->m_file==0);
    if (self->m_file) return hpatch_FALSE;
    if (!_import_fileOpenRead(fileName_utf8,&self->m_file,&self->base.streamSize)) return hpatch_FALSE;
    
    self->base.streamImport=self;
    self->base.read=_read_file;
    self->m_fpos=0;
    self->m_offset=0;
    self->fileError=hpatch_FALSE;
    return hpatch_TRUE;
}

void TFileStreamInput_setOffset(TFileStreamInput* self,size_t offset){
    assert(self->m_offset==0);
    assert(self->base.streamSize>=offset);
    self->m_offset=offset;
    self->base.streamSize-=offset;
}

hpatch_BOOL TFileStreamInput_close(TFileStreamInput* self){
    return _import_fileClose(&self->m_file);
}


    static hpatch_BOOL _write_file(const hpatch_TStreamOutput* stream,hpatch_StreamPos_t writeToPos,
                                   const TByte* data,const TByte* data_end){
        size_t writeLen;
        TFileStreamOutput* self=(TFileStreamOutput*)stream->streamImport;
        assert(data<=data_end);
        assert(self->m_offset==0);
        writeLen=(size_t)(data_end-data);
        if (writeLen==0) return hpatch_TRUE;
        if ((writeLen>self->base.streamSize)
            ||(writeToPos>self->base.streamSize-writeLen)) _fileError_return;
        if (writeToPos!=self->m_pos){
            if (self->is_random_out){
                if (!_import_fileSeek64(self->m_file,writeToPos,SEEK_SET)) _fileError_return;
                self->m_pos=writeToPos;
            }else{
                _fileError_return;
            }
        }
        if (!_import_fileWrite(self->m_file,data,data+writeLen)) _fileError_return;
        self->m_pos=writeToPos+writeLen;
        self->out_length=(self->out_length>=self->m_pos)?self->out_length:self->m_pos;
        return hpatch_TRUE;
    }
hpatch_BOOL TFileStreamOutput_open(TFileStreamOutput* self,const char* fileName_utf8,
                                          hpatch_StreamPos_t max_file_length){
    assert(self->m_file==0);
    if (self->m_file) return hpatch_FALSE;
    if (!_import_fileOpenCreateOrReWrite(fileName_utf8,&self->m_file)) return hpatch_FALSE;
    
    self->base.streamImport=self;
    self->base.streamSize=max_file_length;
    self->base.write=_write_file;
    *(void**)(&self->base.read_writed)=_read_file; //TFileStreamOutput is A TFileStreamInput !
    self->m_pos=0;
    self->m_offset=0;
    self->fileError=hpatch_FALSE;
    self->is_random_out=hpatch_FALSE;
    self->out_length=0;
    return hpatch_TRUE;
}

hpatch_BOOL TFileStreamOutput_flush(TFileStreamOutput* self){
    return import_fileFlush(self->m_file);
}

hpatch_BOOL TFileStreamOutput_close(TFileStreamOutput* self){
    return _import_fileClose(&self->m_file);
}
