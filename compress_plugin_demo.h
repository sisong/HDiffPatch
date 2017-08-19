//compress_plugin_demo.h
//  compress plugin demo for HDiffz
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
#ifndef HDiff_compress_plugin_demo_h
#define HDiff_compress_plugin_demo_h
//compress plugin demo:
//  hdiff_TCompress zlibCompressPlugin;
//  hdiff_TCompress bz2CompressPlugin;
//  hdiff_TCompress lzmaCompressPlugin;

#include <assert.h>
#include "libHDiffPatch/HDiff/diff.h"

#define kCompressBufSize (1024*16)
#define kCompressFailResult 0
#define _compress_error_return(_errAt) {  \
    result=kCompressFailResult; \
    if (!errAt) errAt=_errAt;   \
    goto clear; }

#ifdef  _CompressPlugin_zlib
#include "zlib.h" // http://zlib.net/  https://github.com/madler/zlib
    static const char*  _zlib_stream_compressType(const hdiff_TStreamCompress* compressPlugin){
        static const char* kCompressType="zlib";
        return kCompressType;
    }
    static const char*  _zlib_compressType(const hdiff_TCompress* compressPlugin){
        return _zlib_stream_compressType(0);
    }
    static size_t  _zlib_maxCompressedSize(const hdiff_TCompress* compressPlugin,size_t dataSize){
        return dataSize*5/4+16*1024;
    }
    hpatch_StreamPos_t _zlib_compress_stream(const hdiff_TStreamCompress* compressPlugin,
                                             const hdiff_TStreamOutput* out_code,
                                             const hdiff_TStreamInput*  in_data){
        hpatch_StreamPos_t result=0;
        const char*        errAt="";
        unsigned char*     _temp_buf=0;
        z_stream           s;
        const unsigned char kWindowBits=16+MAX_WBITS;
        unsigned char*     code_buf,* data_buf;
        int                is_eof=0;
        int                is_stream_end=0;
        hpatch_StreamPos_t readFromPos=0;
        hpatch_StreamPos_t writeToPos=0;
        memset(&s,0,sizeof(s));
        
        _temp_buf=(unsigned char*)malloc(kCompressBufSize*2);
        if (!_temp_buf) _compress_error_return("memory alloc");
        code_buf=_temp_buf;
        data_buf=_temp_buf+kCompressBufSize;
        code_buf[0]=kWindowBits; //out kWindowBits
        s.next_out = (Bytef*)(code_buf+1);
        s.avail_out = kCompressBufSize-1;
        if (Z_OK!=deflateInit2(&s, Z_BEST_COMPRESSION,Z_DEFLATED,
                               kWindowBits,MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY))
            _compress_error_return("deflateInit2()");
        while (true) {
            if ((s.avail_out<kCompressBufSize)|is_stream_end){
                size_t writeLen=kCompressBufSize-s.avail_out;
                if (writeLen>0){
                    if (writeLen!=out_code->write(out_code->streamHandle,writeToPos,
                                                  code_buf,code_buf+writeLen))
                        _compress_error_return("out_code->write()");
                }
                writeToPos+=writeLen;
                result+=writeLen;
                s.next_out=(Bytef*)code_buf;
                s.avail_out=kCompressBufSize;
                if (is_stream_end)
                    break;//end loop
            }else{
                if (s.avail_in>0){
                    if (Z_OK!=deflate(&s,Z_NO_FLUSH)) _compress_error_return("deflate()");
                }else if (is_eof){
                    int ret=deflate(&s,Z_FINISH);
                    is_stream_end= (ret==Z_STREAM_END);
                    if ((ret!=Z_STREAM_END)&&(ret!=Z_OK)) _compress_error_return("deflate() Z_FINISH");
                }else{
                    size_t readLen=kCompressBufSize;
                    if (readFromPos+readLen>in_data->streamSize)
                        readLen=(size_t)(in_data->streamSize-readFromPos);
                    if (readLen==0){
                        is_eof=1;
                    }else{
                        if (readLen!=in_data->read(in_data->streamHandle,readFromPos,data_buf,data_buf+readLen))
                            _compress_error_return("in_data->read()");
                        readFromPos+=readLen;
                        s.next_in=data_buf;
                        s.avail_in=(uInt)readLen;
                    }
                }
            }
        }
    clear:
        if (_temp_buf) free(_temp_buf);
        if ((Z_OK!=deflateEnd(&s))&&(result!=kCompressFailResult))
            { result=kCompressFailResult; errAt="deflateEnd()"; }
        if (result==kCompressFailResult)
            printf("\n_zlib_compress_stream() %s fail!\n",errAt);
        return result;
        /*
        int is_eof=(in_part_data==0)&&(in_part_data==in_part_data_end);
        int is_stream_end=0;
        _zlib_stream_compress_t* self=(_zlib_stream_compress_t*)compressHandle;
        assert((in_part_data<in_part_data_end)||is_eof);
        self->c_stream.next_in = (Bytef*)in_part_data;
        self->c_stream.avail_in = (uInt)(in_part_data_end-in_part_data);
        while (true) {
            if (self->c_stream.avail_out==kCompressBufSize){
                if (is_eof){
                    int ret=deflate(&self->c_stream,Z_FINISH);
                    is_stream_end= (ret==Z_STREAM_END);
                    if ((ret!=Z_STREAM_END)&&(ret!=Z_OK))
                        _compress_error_return(-1,"_zlib_stream_compress_part() deflate() Z_FINISH error!");
                }else{
                    if (self->c_stream.avail_in>0){
                        int ret=deflate(&self->c_stream,Z_NO_FLUSH);
                        if (ret!=Z_OK)
                            _compress_error_return(-1,"_zlib_stream_compress_part() deflate() error!");
                    }
                }
            }
            long write_len=(long)(kCompressBufSize-self->c_stream.avail_out);
            if (write_len==0) break; //end loop
            long writed=self->out_code->write(self->out_code->streamHandle,self->writePos,
                                                  self->code_buf,self->code_buf+write_len);
            if(writed!=write_len)
                _compress_error_return(-1,"_zlib_stream_compress_part() write stream error!");
            self->writePos+=(size_t)writed;
            self->c_stream.next_out = (Bytef*)self->code_buf;
            self->c_stream.avail_out = kCompressBufSize;
            if (is_stream_end) break;  //end loop
        }
        return (long)(in_part_data_end-in_part_data);*/
    }
    static size_t  _zlib_compress(const hdiff_TCompress* compressPlugin,
                                  unsigned char* out_code,unsigned char* out_code_end,
                                  const unsigned char* data,const unsigned char* data_end){
        hdiff_TStreamOutput  codeStream;
        hdiff_TStreamInput   dataStream;
        mem_as_hStreamOutput(&codeStream,out_code,out_code_end);
        mem_as_hStreamInput(&dataStream,data,data_end);
        hpatch_StreamPos_t codeLen=_zlib_compress_stream(0,&codeStream,&dataStream);
        if (codeLen!=(size_t)codeLen) return kCompressFailResult;
        return (size_t)codeLen;
    }
    static hdiff_TCompress zlibCompressPlugin={_zlib_compressType,_zlib_maxCompressedSize,_zlib_compress};
    static hdiff_TStreamCompress zlibStreamCompressPlugin={_zlib_stream_compressType,_zlib_compress_stream};
#endif//_CompressPlugin_zlib
    
#ifdef  _CompressPlugin_bz2
#include "bzlib.h" // http://www.bzip.org/
    static const char*  _bz2_stream_compressType(const hdiff_TStreamCompress* compressPlugin){
        static const char* kCompressType="bz2";
        return kCompressType;
    }
    static const char*  _bz2_compressType(const hdiff_TCompress* compressPlugin){
        return _bz2_stream_compressType(0);
    }
    static size_t  _bz2_maxCompressedSize(const hdiff_TCompress* compressPlugin,size_t dataSize){
        return dataSize*5/4+16*1024;
    }
    static size_t  _bz2_compress(const hdiff_TCompress* compressPlugin,
                                 unsigned char* out_code,unsigned char* out_code_end,
                                 const unsigned char* data,const unsigned char* data_end){
        unsigned int destLen=(unsigned int)(out_code_end-out_code);
        assert(destLen==(out_code_end-out_code));
        int ret = BZ2_bzBuffToBuffCompress((char*)out_code,&destLen,
                                           (char *)data,(unsigned int)(data_end-data),
                                           9, 0, 0);
        if(ret != BZ_OK)
            _compress_error_return(0,"_bz2_compress() BZ2_bzBuffToBuffCompress() error!");
        return destLen;
    }

    struct _bz2_stream_compress_t{
        bz_stream                  c_stream;
        const hdiff_TStreamOutput* out_code;
        hpatch_StreamPos_t         writePos;
        unsigned char   code_buf[kCompressBufSize];
    };
    static hdiff_compressHandle _bz2_stream_compress_open(struct hdiff_TStreamCompress* compressPlugin,
                                                          const hdiff_TStreamOutput* out_code){
        _bz2_stream_compress_t* self=(_bz2_stream_compress_t*)malloc(sizeof(_bz2_stream_compress_t));
        if (!self) _compress_error_return(0,"_bz2_stream_compress_open() memory alloc error!");
        memset(self,0,sizeof(_bz2_stream_compress_t)-kCompressBufSize);
        self->out_code=out_code;
        self->writePos=0;
        self->c_stream.next_out = (char*)self->code_buf;
        self->c_stream.avail_out = kCompressBufSize;
        int ret = BZ2_bzCompressInit(&self->c_stream, 9, 0, 0);
        if(ret != BZ_OK){
            free(self); self=0;
            _compress_error_return(0,"_bz2_stream_compress_open() BZ2_bzCompressInit() error!");
        }
        return self;
    }
    long _bz2_stream_compress_part(const struct hdiff_TStreamCompress* compressPlugin,
                                   hdiff_compressHandle compressHandle,
                                   const unsigned char* in_part_data,const unsigned char* in_part_data_end){
        int is_eof=(in_part_data==0)&&(in_part_data==in_part_data_end);
        int is_stream_end=0;
        _bz2_stream_compress_t* self=(_bz2_stream_compress_t*)compressHandle;
        assert((in_part_data<in_part_data_end)||is_eof);
        self->c_stream.next_in = (char*)in_part_data;
        self->c_stream.avail_in = (unsigned int)(in_part_data_end-in_part_data);
        while (true) {
            if (self->c_stream.avail_out==kCompressBufSize){
                if (is_eof){
                    int ret=BZ2_bzCompress(&self->c_stream,BZ_FINISH);
                    is_stream_end= (ret==BZ_STREAM_END);
                    if ((ret!=BZ_STREAM_END)&&(ret!=BZ_FINISH_OK))
                        _compress_error_return(-1,"_bz2_stream_compress_part() BZ2_bzCompress() BZ_FINISH error!");
                }else{
                    if (self->c_stream.avail_in>0){
                        int ret=BZ2_bzCompress(&self->c_stream,BZ_RUN);
                        if (ret!=BZ_RUN_OK)
                            _compress_error_return(-1,"_bz2_stream_compress_part() BZ2_bzCompress() error!");
                    }
                }
            }
            long write_len=(long)(kCompressBufSize-self->c_stream.avail_out);
            if (write_len==0) break; //end loop
            long writed=self->out_code->write(self->out_code->streamHandle,self->writePos,
                                              self->code_buf,self->code_buf+write_len);
            if(writed!=write_len)
                _compress_error_return(-1,"_bz2_stream_compress_part() write stream error!");
            self->writePos+=(size_t)writed;
            self->c_stream.next_out = (char*)self->code_buf;
            self->c_stream.avail_out = kCompressBufSize;
            if (is_stream_end) break;  //end loop
        }
        return (long)(in_part_data_end-in_part_data);
    }
    static hpatch_StreamPos_t _bz2_stream_compress_close(struct hdiff_TStreamCompress* compressPlugin,
                                                         hdiff_compressHandle compressHandle){
        _bz2_stream_compress_t* self=(_bz2_stream_compress_t*)compressHandle;
        int is_error=((0-0)!=_bz2_stream_compress_part(compressPlugin,self,0,0));
        int ret = BZ2_bzCompressEnd(&self->c_stream);
        if (ret != BZ_OK) is_error=1;
        hpatch_StreamPos_t codeSize=self->writePos;
        if (self) free(self);
        return is_error?0:codeSize;
    }
    static hdiff_TCompress bz2CompressPlugin={_bz2_compressType,_bz2_maxCompressedSize,_bz2_compress};
    static hdiff_TStreamCompress bz2StreamCompressPlugin={_bz2_stream_compressType,_bz2_stream_compress_open,
                                                          _bz2_stream_compress_close,_bz2_stream_compress_part};
#endif//_CompressPlugin_bz2
    
#ifdef  _CompressPlugin_lzma
#include "../lzma/C/LzmaEnc.h" // http://www.7-zip.org/sdk.html
    static const char*  _lzma_stream_compressType(const hdiff_TStreamCompress* compressPlugin){
        static const char* kCompressType="lzma";
        return kCompressType;
    }
    static const char*  _lzma_compressType(const hdiff_TCompress* compressPlugin){
        return _lzma_stream_compressType(0);
    }
    static size_t  _lzma_maxCompressedSize(const hdiff_TCompress* compressPlugin,size_t dataSize){
        return dataSize*5/4+16*1024;
    }
    
    static void * __lzma_enc_Alloc(ISzAllocPtr p, size_t size){
        return malloc(size);
    }
    static void __lzma_enc_Free(ISzAllocPtr p, void *address){
        free(address);
    }

    struct _lzma_stream_compress_t{
        //lzma_stream                    c_stream;
        const hdiff_TStreamOutput* out_code;
        hpatch_StreamPos_t         writePos;
        unsigned char   code_buf[kCompressBufSize];
    };
    static hdiff_compressHandle _lzma_stream_compress_open(struct hdiff_TStreamCompress* compressPlugin,
                                                            const hdiff_TStreamOutput* out_code){
        /*_zlib_stream_compress_t* self=(_zlib_stream_compress_t*)malloc(sizeof(_zlib_stream_compress_t));
        if (!self) _compress_error_return(0,"_zlib_stream_compress_open() memory alloc error!");
        memset(self,0,sizeof(_zlib_stream_compress_t)-kCompressBufSize);
        self->out_code=out_code;
        self->writePos=0;
        const unsigned char kWindowBits=16+MAX_WBITS;
        self->code_buf[0]=kWindowBits; //out kWindowBits
        self->c_stream.next_out = (Bytef*)(self->code_buf+1);
        self->c_stream.avail_out = kCompressBufSize-1;
        int ret = deflateInit2(&self->c_stream, Z_BEST_COMPRESSION,Z_DEFLATED,
                               kWindowBits,MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
        if(ret != Z_OK){
            free(self); self=0;
            _compress_error_return(0,"_zlib_stream_compress_open() deflateInit2() error!");
        }
        return self;*/
        return 0;
    }
    long _lzma_stream_compress_part(const struct hdiff_TStreamCompress* compressPlugin,
                                    hdiff_compressHandle compressHandle,
                                    const unsigned char* in_part_data,const unsigned char* in_part_data_end){
        /*int is_eof=(in_part_data==0)&&(in_part_data==in_part_data_end);
        int is_stream_end=0;
        _zlib_stream_compress_t* self=(_zlib_stream_compress_t*)compressHandle;
        assert((in_part_data<in_part_data_end)||is_eof);
        self->c_stream.next_in = (Bytef*)in_part_data;
        self->c_stream.avail_in = (uInt)(in_part_data_end-in_part_data);
        while (true) {
            if (self->c_stream.avail_out==kCompressBufSize){
                if (is_eof){
                    int ret=deflate(&self->c_stream,Z_FINISH);
                    is_stream_end= (ret==Z_STREAM_END);
                    if ((ret!=Z_STREAM_END)&&(ret!=Z_OK))
                        _compress_error_return(-1,"_zlib_stream_compress_part() deflate() Z_FINISH error!");
                }else{
                    if (self->c_stream.avail_in>0){
                        int ret=deflate(&self->c_stream,Z_NO_FLUSH);
                        if (ret!=Z_OK)
                            _compress_error_return(-1,"_zlib_stream_compress_part() deflate() error!");
                    }
                }
            }
            long write_len=(long)(kCompressBufSize-self->c_stream.avail_out);
            if (write_len==0) break; //end loop
            long writed=self->out_code->write(self->out_code->streamHandle,self->writePos,
                                              self->code_buf,self->code_buf+write_len);
            if(writed!=write_len)
                _compress_error_return(-1,"_zlib_stream_compress_part() write stream error!");
            self->writePos+=(size_t)writed;
            self->c_stream.next_out = (Bytef*)self->code_buf;
            self->c_stream.avail_out = kCompressBufSize;
            if (is_stream_end) break;  //end loop
        }
        return (long)(in_part_data_end-in_part_data);*/
        return -1;
    }
    static hpatch_StreamPos_t _lzma_stream_compress_close(struct hdiff_TStreamCompress* compressPlugin,
                                                          hdiff_compressHandle compressHandle){
        /*_zlib_stream_compress_t* self=(_zlib_stream_compress_t*)compressHandle;
        int is_error=((0-0)!=_zlib_stream_compress_part(compressPlugin,self,0,0));
        int ret = deflateEnd(&self->c_stream);
        if (ret != Z_OK) is_error=1;
        hpatch_StreamPos_t codeSize=self->writePos;
        if (self) free(self);
        return is_error?0:codeSize;*/
        return 0;
    }
    static size_t  _lzma_compress(const hdiff_TCompress* compressPlugin,
                                  unsigned char* out_code,unsigned char* out_code_end,
                                  const unsigned char* data,const unsigned char* data_end){
        hdiff_TStreamOutput  streamOutput;
        mem_as_hStreamOutput(&streamOutput,out_code,out_code_end);
        hdiff_compressHandle compresser=_lzma_stream_compress_open(0,&streamOutput);
        if (!compresser) return 0;
        while (data<data_end) {
            const size_t kMaxPartLen=1<<20;
            size_t part_len=data_end-data;
            if (part_len>kMaxPartLen) part_len=kMaxPartLen;
            if (part_len!=_lzma_stream_compress_part(0,compresser,data,data+part_len)) return 0;
            data+=part_len;
        }
        hpatch_StreamPos_t codeLen=_lzma_stream_compress_close(0,compresser);
        if (codeLen!=(size_t)codeLen) return 0;
        return (size_t)codeLen;
    }

    static size_t  __lzma_compress(const hdiff_TCompress* compressPlugin,
                                  unsigned char* out_code,unsigned char* out_code_end,
                                  const unsigned char* data,const unsigned char* data_end){
        assert(out_code<out_code_end);
        ISzAlloc alloc={__lzma_enc_Alloc,__lzma_enc_Free};
        CLzmaEncHandle p = LzmaEnc_Create(&alloc);
        if (!p){
            std::cout <<"\nLzmaEnc_Create error\n";
            return 0;
        }
        CLzmaEncProps props;
        LzmaEncProps_Init(&props);
        props.level=9;
        props.dictSize=1<<22; //4M; patch_decompress() decompress need 4*4MB memroy
        LzmaEncProps_Normalize(&props);
        SRes res= LzmaEnc_SetProps(p,&props);
        if (res != SZ_OK){
            std::cout <<"\nLzmaEnc_SetProps error\n";
            return 0;
        }
        //save propsSize+props
        SizeT destLen=(SizeT)(out_code_end-out_code);
        assert(destLen==(size_t)(out_code_end-out_code));
        SizeT propsSize=destLen-1;
        res = LzmaEnc_WriteProperties(p,out_code+1,&propsSize);
        if ((res!=SZ_OK)||(propsSize>=256)){
            std::cout <<"\nLzmaEnc_WriteProperties error\n";
            return 0;
        }
        out_code[0]=(unsigned char)propsSize;
        out_code+=1+propsSize;
        destLen-=1+propsSize;
        
        res = LzmaEnc_MemEncode(p,out_code,&destLen,data,data_end-data,0,0,&alloc,&alloc);
        LzmaEnc_Destroy(p, &alloc, &alloc);
        if (res != SZ_OK){
            std::cout <<"\nLzmaEnc_MemEncode error\n";
            return 0;
        }
        return 1+propsSize+destLen;
    }
    static hdiff_TCompress lzmaCompressPlugin={_lzma_compressType,_lzma_maxCompressedSize,_lzma_compress};
    static hdiff_TStreamCompress lzmaStreamCompressPlugin={_lzma_stream_compressType,_lzma_stream_compress_open,
                                                           _lzma_stream_compress_close,_lzma_stream_compress_part};
#endif//_CompressPlugin_lzma

#endif
