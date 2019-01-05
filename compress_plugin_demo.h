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
//  zlibCompressPlugin  zlibStreamCompressPlugin;
//  bz2CompressPlugin   bz2StreamCompressPlugin;
//  lzmaCompressPlugin  lzmaStreamCompressPlugin;
//  lz4CompressPlugin   lz4StreamCompressPlugin;
//  lz4hcCompressPlugin lz4hcStreamCompressPlugin;
//  zstdCompressPlugin  zstdStreamCompressPlugin;

#include "libHDiffPatch/HDiff/diff_types.h"

#define kCompressBufSize (1024*64)
#ifndef _IsNeedIncludeDefaultCompressHead
#   define _IsNeedIncludeDefaultCompressHead 1
#endif
#define kCompressFailResult 0
#define _compress_error_return(_errAt) do{ \
    result=kCompressFailResult;          \
    if (strlen(errAt)==0) errAt=_errAt;  \
    goto clear; } while(0)

#ifndef IS_NOTICE_compress_canceled
#   define IS_NOTICE_compress_canceled 1
#endif
#ifndef IS_REUSE_compress_handle
#   define IS_REUSE_compress_handle 0
#endif


#define _check_compress_result(result,outStream_isCanceled,_at,_errAt) \
    if ((result)==kCompressFailResult){      \
        if (outStream_isCanceled){    \
            if (IS_NOTICE_compress_canceled) \
                printf("  (NOTICE: " _at " is canceled, warning.)\n"); \
        }else{ \
            printf("  (NOTICE: " _at " is canceled, %s ERROR!)\n",_errAt); \
        } \
    }

#define _stream_out_code_write(out_code,isCanceled,writePos,buf,len) { \
    if (!out_code->write(out_code,writePos,buf,buf+len)){ \
        isCanceled=1;  \
        _compress_error_return("out_code->write()");\
    } \
    writePos+=len;  }

#define _def_fun_compress_by_compress_stream(_fun_compress_name,_compress_stream) \
static size_t _fun_compress_name(const hdiff_TCompress* compressPlugin, \
                                unsigned char* out_code,unsigned char* out_code_end,     \
                                const unsigned char* data,const unsigned char* data_end){\
    hdiff_TStreamOutput  codeStream; \
    hdiff_TStreamInput   dataStream; \
    mem_as_hStreamOutput(&codeStream,out_code,out_code_end); \
    mem_as_hStreamInput(&dataStream,data,data_end);          \
    hpatch_StreamPos_t codeLen=_compress_stream(0,&codeStream,&dataStream); \
    if (codeLen!=(size_t)codeLen) return kCompressFailResult; \
    return (size_t)codeLen; \
}

//todo *_compress_level(*_dictSize...) 可以单独控制，而不再是全局变量;

#ifdef  _CompressPlugin_zlib
#if (_IsNeedIncludeDefaultCompressHead)
#   include "zlib.h" // http://zlib.net/  https://github.com/madler/zlib
#endif
    static int zlib_compress_level=9; //1..9
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
    typedef struct _zlib_TCompress{
        hpatch_StreamPos_t curWritePos;
        const hpatch_TStreamOutput* out_code;
        unsigned char*  c_buf;
        size_t          c_buf_size;
        z_stream        c_stream;
    } _zlib_TCompress;

    static _zlib_TCompress*  _zlib_compress_open_by(const hdiff_TStreamCompress* compressPlugin,
                                                    const hpatch_TStreamOutput* out_code,
                                                    int  isNeedSaveWindowBits,
                                                    int  compress_level,
                                                    int  mem_level,
                                                    unsigned char* _mem_buf,size_t _mem_buf_size){
        _zlib_TCompress* self=0;
        const signed char  kWindowBits=isNeedSaveWindowBits?(16+MAX_WBITS):(-MAX_WBITS);
        
        self=(_zlib_TCompress*)_hpatch_align_upper(_mem_buf,sizeof(hpatch_StreamPos_t));
        assert((_mem_buf+_mem_buf_size)>((unsigned char*)self+sizeof(_zlib_TCompress)));
        _mem_buf_size=(_mem_buf+_mem_buf_size)-((unsigned char*)self+sizeof(_zlib_TCompress));
        _mem_buf=(unsigned char*)self+sizeof(_zlib_TCompress);
        
        memset(self,0,sizeof(_zlib_TCompress));
        self->c_buf=_mem_buf;
        self->c_buf_size=_mem_buf_size;
        self->out_code=out_code;
        self->curWritePos=0;
        
        self->c_stream.next_out = (Bytef*)_mem_buf;
        self->c_stream.avail_out = (uInt)_mem_buf_size;
        if (isNeedSaveWindowBits){//save kWindowBits
            self->c_stream.next_out[0]=kWindowBits;
            ++self->c_stream.next_out;
            --self->c_stream.avail_out;
        }

        if (Z_OK!=deflateInit2(&self->c_stream,compress_level,Z_DEFLATED,
                               kWindowBits,mem_level,Z_DEFAULT_STRATEGY))
            return 0;
        return self;
    }
    static int _zlib_compress_close_by(const hdiff_TStreamCompress* compressPlugin,_zlib_TCompress* self){
        int result=1;//true;
        if (!self) return result;
        if (self->c_buf!=0){
            int ret=deflateEnd(&self->c_stream);
            result=(Z_OK==ret);
        }
        memset(self,0,sizeof(_zlib_TCompress));
        return result;
    }
    static int _zlib_compress_stream_part(const hdiff_TStreamCompress* compressPlugin,
                                          _zlib_TCompress* self,
                                          const unsigned char* part_data,const unsigned char* part_data_end,
                                          int is_data_end,hpatch_StreamPos_t* curWritedPos,int* outStream_isCanceled){
        int                 result=1; //true
        const char*         errAt="";
        int                 is_stream_end=0;
        int                 is_eof=0;
        assert(part_data<part_data_end);
        self->c_stream.next_in=(Bytef*)part_data;
        self->c_stream.avail_in=(uInt)(part_data_end-part_data);
        while (1) {
            if ((self->c_stream.avail_out<self->c_buf_size)|is_stream_end){
                size_t writeLen=self->c_buf_size-self->c_stream.avail_out;
                if (writeLen>0){
                    _stream_out_code_write(self->out_code,*outStream_isCanceled,
                                           (*curWritedPos),self->c_buf,writeLen);
                }
                self->c_stream.next_out=(Bytef*)self->c_buf;
                self->c_stream.avail_out=(uInt)self->c_buf_size;
                if (is_stream_end)
                    break;//end loop
            }else{
                if (self->c_stream.avail_in>0){
                    if (Z_OK!=deflate(&self->c_stream,Z_NO_FLUSH)) _compress_error_return("deflate()");
                }else if (is_eof){
                    int ret=deflate(&self->c_stream,Z_FINISH);
                    is_stream_end= (ret==Z_STREAM_END);
                    if ((ret!=Z_STREAM_END)&&(ret!=Z_OK))
                        _compress_error_return("deflate() Z_FINISH");
                }else{
                    if (!is_data_end)
                        break;//part ok
                    else
                        is_eof=1;
                }
            }
        }
    clear:
        _check_compress_result(result,*outStream_isCanceled,"_zlib_compress_stream_part()",errAt);
        return result;
    }

    static hpatch_StreamPos_t _zlib_compress_stream(const hdiff_TStreamCompress* compressPlugin,
                                                    const hdiff_TStreamOutput* out_code,
                                                    const hdiff_TStreamInput*  in_data){
        hpatch_StreamPos_t result=0; //writedPos
        hpatch_StreamPos_t readFromPos=0;
        const char*        errAt="";
        int                 outStream_isCanceled=0;
        unsigned char*     _temp_buf=0;
        unsigned char*     data_buf=0;
        _zlib_TCompress*   self=0;
        _temp_buf=(unsigned char*)malloc(sizeof(_zlib_TCompress)+kCompressBufSize*2);
        if (!_temp_buf) _compress_error_return("memory alloc");
        self=_zlib_compress_open_by(compressPlugin,out_code,1,zlib_compress_level,MAX_MEM_LEVEL,
                                    _temp_buf,sizeof(_zlib_TCompress)+kCompressBufSize);
        data_buf=_temp_buf+sizeof(_zlib_TCompress)+kCompressBufSize;
        if (!self) _compress_error_return("deflateInit2()");
        while (readFromPos<in_data->streamSize) {
            size_t readLen=kCompressBufSize;
            if (readLen>(hpatch_StreamPos_t)(in_data->streamSize-readFromPos))
                readLen=(size_t)(in_data->streamSize-readFromPos);
            if (!in_data->read(in_data,readFromPos,data_buf,data_buf+readLen))
                _compress_error_return("in_data->read()");
            readFromPos+=readLen;
            if (!_zlib_compress_stream_part(compressPlugin,self,data_buf,data_buf+readLen,
                                            (readFromPos==in_data->streamSize),&result,&outStream_isCanceled))
                _compress_error_return("_zlib_compress_stream_part()");
        }
    clear:
        if (!_zlib_compress_close_by(compressPlugin,self))
            { result=kCompressFailResult; if (strlen(errAt)==0) errAt="deflateEnd()"; }
        _check_compress_result(result,outStream_isCanceled,"_zlib_compress_stream()",errAt);
        if (_temp_buf) free(_temp_buf);
        return result;
    }
    _def_fun_compress_by_compress_stream(_zlib_compress,_zlib_compress_stream)
    static hdiff_TCompress zlibCompressPlugin={_zlib_compressType,_zlib_maxCompressedSize,_zlib_compress};
    static hdiff_TStreamCompress zlibStreamCompressPlugin={_zlib_stream_compressType,_zlib_compress_stream};
#endif//_CompressPlugin_zlib
    
#ifdef  _CompressPlugin_bz2
#if (_IsNeedIncludeDefaultCompressHead)
#   include "bzlib.h" // http://www.bzip.org/
#endif
    static int bz2_compress_level=9; //1..9
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
    static hpatch_StreamPos_t _bz2_compress_stream(const hdiff_TStreamCompress* compressPlugin,
                                                   const hdiff_TStreamOutput* out_code,
                                                   const hdiff_TStreamInput*  in_data){
        hpatch_StreamPos_t result=0;
        const char*        errAt="";
        unsigned char*     _temp_buf=0;
        bz_stream          s;
        unsigned char*     code_buf,* data_buf;
        int                is_eof=0;
        int                is_stream_end=0;
        hpatch_StreamPos_t readFromPos=0;
        int                outStream_isCanceled=0;
        memset(&s,0,sizeof(s));
        
        _temp_buf=(unsigned char*)malloc(kCompressBufSize*2);
        if (!_temp_buf) _compress_error_return("memory alloc");
        code_buf=_temp_buf;
        data_buf=_temp_buf+kCompressBufSize;
        s.next_out = (char*)code_buf;
        s.avail_out = kCompressBufSize;
        if (BZ_OK!=BZ2_bzCompressInit(&s, bz2_compress_level, 0, 0))
            _compress_error_return("BZ2_bzCompressInit()");
        while (1) {
            if ((s.avail_out<kCompressBufSize)|is_stream_end){
                size_t writeLen=kCompressBufSize-s.avail_out;
                if (writeLen>0){
                    _stream_out_code_write(out_code,outStream_isCanceled,result,code_buf,writeLen);
                }
                s.next_out=(char*)code_buf;
                s.avail_out=kCompressBufSize;
                if (is_stream_end)
                    break;//end loop
            }else{
                if (s.avail_in>0){
                    if (BZ_RUN_OK!=BZ2_bzCompress(&s,BZ_RUN)) _compress_error_return("BZ2_bzCompress()");
                }else if (is_eof){
                    int ret=BZ2_bzCompress(&s,BZ_FINISH);
                    is_stream_end= (ret==BZ_STREAM_END);
                    if ((ret!=BZ_STREAM_END)&&(ret!=BZ_FINISH_OK))
                        _compress_error_return("BZ2_bzCompress() BZ_FINISH");
                }else{
                    size_t readLen=kCompressBufSize;
                    if (readFromPos+readLen>in_data->streamSize)
                        readLen=(size_t)(in_data->streamSize-readFromPos);
                    if (readLen==0){
                        is_eof=1;
                    }else{
                        if (!in_data->read(in_data,readFromPos,data_buf,data_buf+readLen))
                            _compress_error_return("in_data->read()");
                        readFromPos+=readLen;
                        s.next_in=(char*)data_buf;
                        s.avail_in=(unsigned int)readLen;
                    }
                }
            }
        }
    clear:
        if (BZ_OK!=BZ2_bzCompressEnd(&s))
            { result=kCompressFailResult; if (strlen(errAt)==0) errAt="BZ2_bzCompressEnd()"; }
        _check_compress_result(result,outStream_isCanceled,"_bz2_compress_stream()",errAt);
        if (_temp_buf) free(_temp_buf);
        return result;
    }
    _def_fun_compress_by_compress_stream(_bz2_compress,_bz2_compress_stream)
    static hdiff_TCompress bz2CompressPlugin={_bz2_compressType,_bz2_maxCompressedSize,_bz2_compress};
    static hdiff_TStreamCompress bz2StreamCompressPlugin={_bz2_stream_compressType,_bz2_compress_stream};
#endif//_CompressPlugin_bz2
    
#ifdef  _CompressPlugin_lzma
#if (_IsNeedIncludeDefaultCompressHead)
#   include "LzmaEnc.h" // "lzma/C/LzmaEnc.h" http://www.7-zip.org/sdk.html
#endif
    static int lzma_compress_level=7;//0..9
    static UInt32 lzma_dictSize=1<<22;  //patch decompress need 4*lzma_dictSize memroy
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

    struct __lzma_SeqOutStream_t{
        ISeqOutStream               base;
        const hdiff_TStreamOutput*  out_code;
        hpatch_StreamPos_t          writeToPos;
        int                         isCanceled;
    };
    static size_t __lzma_SeqOutStream_Write(const ISeqOutStream *p, const void *buf, size_t size){
        __lzma_SeqOutStream_t* self=(__lzma_SeqOutStream_t*)p;
        const unsigned char* pdata=(const unsigned char*)buf;
        if (size>0){
            if (!self->out_code->write(self->out_code,self->writeToPos,pdata,pdata+size)){
                self->isCanceled=1;
                return 0;
            }
        }
        self->writeToPos+=size;
        return size;
    }
    struct __lzma_SeqInStream_t{
        ISeqInStream                base;
        const hdiff_TStreamInput*   in_data;
        hpatch_StreamPos_t          readFromPos;
    };
    static SRes __lzma_SeqInStream_Read(const ISeqInStream *p, void *buf, size_t *size){
        __lzma_SeqInStream_t* self=(__lzma_SeqInStream_t*)p;
        size_t readLen=*size;
        if (readLen+self->readFromPos>self->in_data->streamSize)
            readLen=(size_t)(self->in_data->streamSize-self->readFromPos);
        if (readLen>0){
            unsigned char* pdata=(unsigned char*)buf;
            if (!self->in_data->read(self->in_data,self->readFromPos,pdata,pdata+readLen)){
                *size=0;
                return SZ_ERROR_READ;
            }
        }
        self->readFromPos+=readLen;
        *size=readLen;
        return SZ_OK;
    }

    static hpatch_StreamPos_t _lzma_compress_stream(const hdiff_TStreamCompress* compressPlugin,
                                                    const hdiff_TStreamOutput* out_code,
                                                    const hdiff_TStreamInput*  in_data){
        ISzAlloc alloc={__lzma_enc_Alloc,__lzma_enc_Free};
        struct __lzma_SeqOutStream_t outStream={{__lzma_SeqOutStream_Write},out_code,0,0};
        struct __lzma_SeqInStream_t  inStream={{__lzma_SeqInStream_Read},in_data,0};
        hpatch_StreamPos_t result=0;
        const char*        errAt="";
#if (IS_REUSE_compress_handle)
        static CLzmaEncHandle   s=0;
#else
        CLzmaEncHandle          s=0;
#endif
        CLzmaEncProps      props;
        unsigned char      properties_buf[LZMA_PROPS_SIZE+1];
        SizeT              properties_size=LZMA_PROPS_SIZE;
        SRes               ret;
        uint32_t           dictSize=lzma_dictSize;
        if (!s) s=LzmaEnc_Create(&alloc);
        if (!s) _compress_error_return("LzmaEnc_Create()");
        while ((dictSize >= in_data->streamSize*3)&&(dictSize>=4*1024*2))
            dictSize>>=1;
        LzmaEncProps_Init(&props);
        props.level=lzma_compress_level;
        props.dictSize=dictSize;
        LzmaEncProps_Normalize(&props);
        if (SZ_OK!=LzmaEnc_SetProps(s,&props)) _compress_error_return("LzmaEnc_SetProps()");
        if (IS_NOTICE_compress_canceled){
            printf("  (used one lzma dictSize: %d)\n",props.dictSize);
        }
        
        //save properties_size+properties
        assert(LZMA_PROPS_SIZE<256);
        if (SZ_OK!=LzmaEnc_WriteProperties(s,properties_buf+1,&properties_size))
            _compress_error_return("LzmaEnc_WriteProperties()");
        properties_buf[0]=(unsigned char)properties_size;
        
        if (1+properties_size!=__lzma_SeqOutStream_Write(&outStream.base,properties_buf,1+properties_size))
            _compress_error_return("out_code->write()");
        
        ret=LzmaEnc_Encode(s,&outStream.base,&inStream.base,0,&alloc,&alloc);
        if (SZ_OK==ret){
            result=outStream.writeToPos;
        }else{//fail
            if (ret==SZ_ERROR_READ)
                _compress_error_return("in_data->read()");
            else if (ret==SZ_ERROR_WRITE)
                _compress_error_return("out_code->write()");
            else
                _compress_error_return("LzmaEnc_Encode()");
        }
    clear:
#if (!IS_REUSE_compress_handle)
        if (s) LzmaEnc_Destroy(s,&alloc,&alloc);
#endif
        _check_compress_result(result,outStream.isCanceled,"_lzma_compress_stream()",errAt);
        return result;
    }
    _def_fun_compress_by_compress_stream(_lzma_compress,_lzma_compress_stream)
    static hdiff_TCompress lzmaCompressPlugin={_lzma_compressType,_lzma_maxCompressedSize,_lzma_compress};
    static hdiff_TStreamCompress lzmaStreamCompressPlugin={_lzma_stream_compressType,_lzma_compress_stream};
#endif//_CompressPlugin_lzma

#if (defined(_CompressPlugin_lz4) || defined(_CompressPlugin_lz4hc))
#if (_IsNeedIncludeDefaultCompressHead)
#   include "lz4.h"   // "lz4/lib/lz4.h" https://github.com/lz4/lz4
#   include "lz4hc.h"
#endif
    static int lz4hc_compress_level=11; //3..12
    static const char*  _lz4_stream_compressType(const hdiff_TStreamCompress* compressPlugin){
        static const char* kCompressType="lz4";
        return kCompressType;
    }
    static const char*  _lz4_compressType(const hdiff_TCompress* compressPlugin){
        return _lz4_stream_compressType(0);
    }
    static size_t  _lz4_maxCompressedSize(const hdiff_TCompress* compressPlugin,size_t dataSize){
        return dataSize*5/4+16*1024;
    }
    #define _lz4_write_len4(out_code,isCanceled,writePos,len) { \
        unsigned char _temp_buf4[4];  \
        _temp_buf4[0]=(unsigned char)(len);      \
        _temp_buf4[1]=(unsigned char)((len)>>8); \
        _temp_buf4[2]=(unsigned char)((len)>>16);\
        _temp_buf4[3]=(unsigned char)((len)>>24);\
        _stream_out_code_write(out_code,isCanceled,writePos,_temp_buf4,4); \
    }
    static hpatch_StreamPos_t _lz4_compress_stream(const hdiff_TStreamCompress* compressPlugin,
                                                   const hdiff_TStreamOutput* out_code,
                                                   const hdiff_TStreamInput*  in_data){
        hpatch_StreamPos_t  result=0;
        const char*         errAt="";
        unsigned char*      _temp_buf=0;
        unsigned char*      code_buf,* data_buf,* data_buf_back;
        LZ4_stream_t*       s=0;
        hpatch_StreamPos_t  readFromPos=0;
        int                 outStream_isCanceled=0;
        const int           kLZ4DefaultAcceleration=1;
        int  kLz4CompressBufSize =1024*510; //patch decompress need 4*2*0.5MB memroy
        int  code_buf_size;
        if ((size_t)kLz4CompressBufSize>in_data->streamSize)
            kLz4CompressBufSize=(int)in_data->streamSize;
        code_buf_size=LZ4_compressBound(kLz4CompressBufSize);
        
        _temp_buf=(unsigned char*)malloc(kLz4CompressBufSize*2+code_buf_size);
        if (!_temp_buf) _compress_error_return("memory alloc");
        data_buf_back=_temp_buf;
        data_buf=_temp_buf+kLz4CompressBufSize;
        code_buf=_temp_buf+kLz4CompressBufSize*2;
        
        s=LZ4_createStream();
        if (!s) _compress_error_return("LZ4_createStream()");
        //out kLz4CompressBufSize
        _lz4_write_len4(out_code,outStream_isCanceled,result,kLz4CompressBufSize);
        
        while (1) {
            int  codeLen;
            unsigned char*  _swap=data_buf;
            data_buf=data_buf_back;
            data_buf_back=_swap;
            int dataLen=kLz4CompressBufSize;
            if ((size_t)dataLen>(in_data->streamSize-readFromPos))
                dataLen=(int)(in_data->streamSize-readFromPos);
            if (dataLen==0)
                break;//finish
            if (!in_data->read(in_data,readFromPos,data_buf,data_buf+dataLen))
                _compress_error_return("in_data->read()");
            readFromPos+=dataLen;
            
            codeLen=LZ4_compress_fast_continue(s,(const char*)data_buf,(char*)code_buf,
                                               dataLen,code_buf_size,kLZ4DefaultAcceleration);
            if (codeLen<=0) _compress_error_return("LZ4_compress_fast_continue()");
            //out step codeLen
            _lz4_write_len4(out_code,outStream_isCanceled,result,codeLen);
            //out code
            _stream_out_code_write(out_code,outStream_isCanceled,result,code_buf,codeLen);
        }
    clear:
        if (0!=LZ4_freeStream(s))
            { result=kCompressFailResult; if (strlen(errAt)==0) errAt="LZ4_freeStream()"; }
        _check_compress_result(result,outStream_isCanceled,"_lz4_compress_stream()",errAt);
        if (_temp_buf) free(_temp_buf);
        return result;
    }

    static hpatch_StreamPos_t _lz4hc_compress_stream(const hdiff_TStreamCompress* compressPlugin,
                                                     const hdiff_TStreamOutput* out_code,
                                                     const hdiff_TStreamInput*  in_data){
        hpatch_StreamPos_t  result=0;
        const char*         errAt="";
        unsigned char*      _temp_buf=0;
        unsigned char*      code_buf,* data_buf,* data_buf_back;
        LZ4_streamHC_t*     s=0;
        hpatch_StreamPos_t  readFromPos=0;
        int                 outStream_isCanceled=0;
        int  kLz4CompressBufSize =1024*510; //patch decompress need 4*2*0.5MB memroy
        int  code_buf_size;
        if ((size_t)kLz4CompressBufSize>in_data->streamSize)
            kLz4CompressBufSize=(int)in_data->streamSize;
        code_buf_size=LZ4_compressBound(kLz4CompressBufSize);
        
        _temp_buf=(unsigned char*)malloc(kLz4CompressBufSize*2+code_buf_size);
        if (!_temp_buf) _compress_error_return("memory alloc");
        data_buf_back=_temp_buf;
        data_buf=_temp_buf+kLz4CompressBufSize;
        code_buf=_temp_buf+kLz4CompressBufSize*2;
        
        s=LZ4_createStreamHC();
        if (!s) _compress_error_return("LZ4_createStreamHC()");
        LZ4_resetStreamHC(s,lz4hc_compress_level);
        //out kLz4CompressBufSize
        _lz4_write_len4(out_code,outStream_isCanceled,result,kLz4CompressBufSize);
        
        while (1) {
            int  codeLen;
            unsigned char*  _swap=data_buf;
            data_buf=data_buf_back;
            data_buf_back=_swap;
            int dataLen=kLz4CompressBufSize;
            if ((size_t)dataLen>(in_data->streamSize-readFromPos))
                dataLen=(int)(in_data->streamSize-readFromPos);
            if (dataLen==0)
                break;//finish
            if (!in_data->read(in_data,readFromPos,data_buf,data_buf+dataLen))
                _compress_error_return("in_data->read()");
            readFromPos+=dataLen;
            
            codeLen=LZ4_compress_HC_continue(s,(const char*)data_buf,(char*)code_buf,
                                             dataLen,code_buf_size);
            if (codeLen<=0) _compress_error_return("LZ4_compress_HC_continue()");
            //out step codeLen
            _lz4_write_len4(out_code,outStream_isCanceled,result,codeLen);
            //out code
            _stream_out_code_write(out_code,outStream_isCanceled,result,code_buf,codeLen);
        }
    clear:
        if (0!=LZ4_freeStreamHC(s))
            { result=kCompressFailResult; if (strlen(errAt)==0) errAt="LZ4_freeStreamHC()"; }
        _check_compress_result(result,outStream_isCanceled,"_lz4hc_compress_stream()",errAt);
        if (_temp_buf) free(_temp_buf);
        return result;
    }

    _def_fun_compress_by_compress_stream(_lz4_compress,_lz4_compress_stream)
    _def_fun_compress_by_compress_stream(_lz4hc_compress,_lz4hc_compress_stream)
    static hdiff_TCompress lz4CompressPlugin   ={_lz4_compressType,_lz4_maxCompressedSize,_lz4_compress};
    static hdiff_TCompress lz4hcCompressPlugin ={_lz4_compressType,_lz4_maxCompressedSize,_lz4hc_compress};
    static hdiff_TStreamCompress lz4StreamCompressPlugin   ={_lz4_stream_compressType,_lz4_compress_stream};
    static hdiff_TStreamCompress lz4hcStreamCompressPlugin ={_lz4_stream_compressType,_lz4hc_compress_stream};
#endif//_CompressPlugin_lz4 or _CompressPlugin_lz4hc


#ifdef  _CompressPlugin_zstd
#if (_IsNeedIncludeDefaultCompressHead)
#   include "zstd.h" // "zstd/lib/zstd.h" https://github.com/facebook/zstd
#endif
    static int zstd_compress_level=20; //0..22
    static const char*  _zstd_stream_compressType(const hdiff_TStreamCompress* compressPlugin){
        static const char* kCompressType="zstd";
        return kCompressType;
    }
    static const char*  _zstd_compressType(const hdiff_TCompress* compressPlugin){
        return _zstd_stream_compressType(0);
    }
    static size_t  _zstd_maxCompressedSize(const hdiff_TCompress* compressPlugin,size_t dataSize){
        return ZSTD_compressBound(dataSize);
    }
    
    static hpatch_StreamPos_t _zstd_compress_stream(const hdiff_TStreamCompress* compressPlugin,
                                                    const hdiff_TStreamOutput* out_code,
                                                    const hdiff_TStreamInput*  in_data){
        hpatch_StreamPos_t  result=0;
        const char*         errAt="";
        unsigned char*      _temp_buf=0;
        ZSTD_inBuffer       s_input;
        ZSTD_outBuffer      s_output;
#if (IS_REUSE_compress_handle)
        static ZSTD_CStream*  s=0;
#else
        ZSTD_CStream*         s=0;
#endif
        hpatch_StreamPos_t  readFromPos=0;
        int                 outStream_isCanceled=0;
        size_t              ret;
        
        s_input.size=ZSTD_CStreamInSize();
        s_output.size=ZSTD_CStreamOutSize();
        _temp_buf=(unsigned char*)malloc(s_input.size+s_output.size);
        if (!_temp_buf) _compress_error_return("memory alloc");
        s_input.src=_temp_buf;
        s_output.dst=_temp_buf+s_input.size;
        
        if (!s) s=ZSTD_createCStream();
        if (!s) _compress_error_return("ZSTD_createCStream()");
        ret=ZSTD_initCStream(s,zstd_compress_level);
        if (ZSTD_isError(ret)) _compress_error_return("ZSTD_initCStream()");
        
        while (readFromPos<in_data->streamSize) {
            s_input.pos=0;
            if (s_input.size>(in_data->streamSize-readFromPos))
                s_input.size=(size_t)(in_data->streamSize-readFromPos);
            if (!in_data->read(in_data,readFromPos,(unsigned char*)s_input.src,
                               (unsigned char*)s_input.src+s_input.size))
                _compress_error_return("in_data->read()");
            readFromPos+=s_input.size;
            while (s_input.pos<s_input.size) {
                s_output.pos=0;
                ret=ZSTD_compressStream(s,&s_output,&s_input);
                if (ZSTD_isError(ret)) _compress_error_return("ZSTD_compressStream()");
                if (s_output.pos>0){
                    _stream_out_code_write(out_code,outStream_isCanceled,result,
                                           (const unsigned char*)s_output.dst,s_output.pos);
                }
            }
        }
        while (1) {
            s_output.pos=0;
            ret=ZSTD_endStream(s,&s_output);
            if (s_output.pos>0){
                _stream_out_code_write(out_code,outStream_isCanceled,result,
                                       (const unsigned char*)s_output.dst,s_output.pos);
            }
            if (ret==0) break;
        }
    clear:
#if (!IS_REUSE_compress_handle)
        if (0!=ZSTD_freeCStream(s))
        { result=kCompressFailResult; if (strlen(errAt)==0) errAt="ZSTD_freeCStream()"; }
#endif
        _check_compress_result(result,outStream_isCanceled,"_zstd_compress_stream()",errAt);
        if (_temp_buf) free(_temp_buf);
        return result;
    }
    _def_fun_compress_by_compress_stream(_zstd_compress,_zstd_compress_stream)
    static hdiff_TCompress zstdCompressPlugin={_zstd_compressType,_zstd_maxCompressedSize,_zstd_compress};
    static hdiff_TStreamCompress zstdStreamCompressPlugin={_zstd_stream_compressType,_zstd_compress_stream};
#endif//_CompressPlugin_zstd

#endif
