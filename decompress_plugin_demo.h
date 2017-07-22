//decompress_plugin_demo.h
//  decompress plugin demo for HDiffz\HPatchz
/*
 The MIT License (MIT)
 Copyright (c) 2012-2017 HouSisong
 
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
#ifndef HPatch_decompress_plugin_demo_h
#define HPatch_decompress_plugin_demo_h
//decompress plugin demo:
//  hpatch_TDeompress zlibDecompressPlugin;
//  hpatch_TDeompress bz2DecompressPlugin;
//  hpatch_TDeompress lzmaDecompressPlugin;

#include "libHDiffPatch/HPatch/patch.h"

static const int kDecompressBufSize =1024;

#ifdef  _CompressPlugin_zlib
#include "zlib.h"
    typedef struct _zlib_TDecompress{
        const struct hpatch_TStreamInput* codeStream;
        hpatch_StreamPos_t code_begin;
        hpatch_StreamPos_t code_end;
        
        z_stream        d_stream;
        unsigned char   dec_buf[kDecompressBufSize];
    } _zlib_TDecompress;
    static hpatch_decompressHandle  _zlib_open(struct hpatch_TDecompress* decompressPlugin,
                                               const char* compressType,
                                               hpatch_StreamPos_t dataSize,
                                               const struct hpatch_TStreamInput* codeStream,
                                               hpatch_StreamPos_t code_begin,
                                               hpatch_StreamPos_t code_end){
        if (0!=strcmp(compressType,"zlib")) return 0;
        //load kWindowBits
        if (code_begin>=code_end) return 0;
        unsigned char kWindowBits=0;
        if (1!=codeStream->read(codeStream->streamHandle,code_begin,
                                &kWindowBits,&kWindowBits+1)) return 0;
        ++code_begin;
        
        _zlib_TDecompress* self=(_zlib_TDecompress*)malloc(sizeof(_zlib_TDecompress));
        if (!self) return 0;
        memset(self,0,sizeof(_zlib_TDecompress)-kDecompressBufSize);
                self->codeStream=codeStream;
        self->code_begin=code_begin;
        self->code_end=code_end;
        
        int ret = inflateInit2(&self->d_stream,kWindowBits);
        if (ret!=Z_OK){ free(self); return 0; }
        return self;
    }
    static void _zlib_close(struct hpatch_TDecompress* decompressPlugin,
                            hpatch_decompressHandle decompressHandle){
        _zlib_TDecompress* self=(_zlib_TDecompress*)decompressHandle;
        int ret=inflateEnd(&self->d_stream);
        assert(ret==Z_OK);
        free(self);
    }
    static long _zlib_decompress_part(const struct hpatch_TDecompress* decompressPlugin,
                                      hpatch_decompressHandle decompressHandle,
                                      unsigned char* out_part_data,unsigned char* out_part_data_end){
        assert(out_part_data!=out_part_data_end);
        _zlib_TDecompress* self=(_zlib_TDecompress*)decompressHandle;
        
        self->d_stream.next_out = out_part_data;
        self->d_stream.avail_out =(uInt)(out_part_data_end-out_part_data);
        while (self->d_stream.avail_out>0) {
            long oldLen=self->d_stream.avail_in;
            long codeLen=(long)(self->code_end - self->code_begin);
            if ((oldLen<kDecompressBufSize)&&(codeLen>0)) {
                if (oldLen>0) memmove(self->dec_buf,self->d_stream.next_in,oldLen);
                self->d_stream.next_in=self->dec_buf;
                long readLen=kDecompressBufSize-oldLen;
                if (readLen>codeLen) readLen=codeLen;
                if (readLen<=0) return 0;//error;
                long readed=self->codeStream->read(self->codeStream->streamHandle,self->code_begin,
                                                   self->dec_buf+oldLen,
                                                   self->dec_buf+oldLen+readLen);
                if (readed!=readLen) return 0;//error;
                self->d_stream.avail_in=oldLen+readLen;
                self->code_begin+=readLen;
            }
            int ret = inflate(&self->d_stream,Z_PARTIAL_FLUSH);
            if ((ret!=Z_OK)&&(ret!=Z_STREAM_END))
                return 0;//error
        }
        return (long)(out_part_data_end-out_part_data);
    }
    static hpatch_TDecompress zlibDecompressPlugin={_zlib_open,_zlib_close,_zlib_decompress_part};
#endif//_CompressPlugin_zlib
    
#ifdef  _CompressPlugin_bz2
#include "bzlib.h"
    typedef struct _bz2_TDecompress{
        const struct hpatch_TStreamInput* codeStream;
        hpatch_StreamPos_t code_begin;
        hpatch_StreamPos_t code_end;
        
        bz_stream       d_stream;
        unsigned char   dec_buf[kDecompressBufSize];
    } _bz2_TDecompress;
    static hpatch_decompressHandle  _bz2_open(struct hpatch_TDecompress* decompressPlugin,
                                               const char* compressType,
                                               hpatch_StreamPos_t dataSize,
                                               const struct hpatch_TStreamInput* codeStream,
                                               hpatch_StreamPos_t code_begin,
                                               hpatch_StreamPos_t code_end){
        if (0!=strcmp(compressType,"bz2")) return 0;
        //load kWindowBits
        _bz2_TDecompress* self=(_bz2_TDecompress*)malloc(sizeof(_bz2_TDecompress));
        if (!self) return 0;
        memset(self,0,sizeof(_bz2_TDecompress)-kDecompressBufSize);
        self->codeStream=codeStream;
        self->code_begin=code_begin;
        self->code_end=code_end;
        
        int ret = BZ2_bzDecompressInit(&self->d_stream,0,0);
        if (ret!=BZ_OK){ free(self); return 0; }
        return self;
    }
    static void _bz2_close(struct hpatch_TDecompress* decompressPlugin,
                            hpatch_decompressHandle decompressHandle){
        _bz2_TDecompress* self=(_bz2_TDecompress*)decompressHandle;
        int ret=BZ2_bzDecompressEnd(&self->d_stream);
        assert(ret==BZ_OK);
        free(self);
    }
    static long _bz2_decompress_part(const struct hpatch_TDecompress* decompressPlugin,
                                      hpatch_decompressHandle decompressHandle,
                                      unsigned char* out_part_data,unsigned char* out_part_data_end){
        assert(out_part_data!=out_part_data_end);
        _bz2_TDecompress* self=(_bz2_TDecompress*)decompressHandle;
        
        self->d_stream.next_out =(char*)out_part_data;
        self->d_stream.avail_out =(unsigned int)(out_part_data_end-out_part_data);
        while (self->d_stream.avail_out>0) {
            long oldLen=self->d_stream.avail_in;
            long codeLen=(long)(self->code_end - self->code_begin);
            if ((oldLen<kDecompressBufSize)&&(codeLen>0)) {
                if (oldLen>0) memmove(self->dec_buf,self->d_stream.next_in,oldLen);
                self->d_stream.next_in=(char*)self->dec_buf;
                long readLen=kDecompressBufSize-oldLen;
                if (readLen>codeLen) readLen=codeLen;
                if (readLen<=0) return 0;//error;
                long readed=self->codeStream->read(self->codeStream->streamHandle,self->code_begin,
                                                   self->dec_buf+oldLen,
                                                   self->dec_buf+oldLen+readLen);
                if (readed!=readLen) return 0;//error;
                self->d_stream.avail_in=oldLen+readLen;
                self->code_begin+=readLen;
            }
            int ret = BZ2_bzDecompress(&self->d_stream);
            if ((ret!=BZ_OK)&&(ret!=BZ_STREAM_END))
                return 0;//error
        }
        return (long)(out_part_data_end-out_part_data);
    }
    static hpatch_TDecompress bz2DecompressPlugin={_bz2_open,_bz2_close,_bz2_decompress_part};
#endif//_CompressPlugin_bz2
    
#ifdef  _CompressPlugin_lzma
#include "../lzma/C/LzmaDec.h"
#endif//_CompressPlugin_lzma

#endif
