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
//  zlibCompressPlugin
//  pzlibCompressPlugin
//  bz2CompressPlugin
//  pbz2CompressPlugin
//  lzmaCompressPlugin
//  lzma2CompressPlugin
//  lz4CompressPlugin
//  lz4hcCompressPlugin
//  zstdCompressPlugin

#include "libHDiffPatch/HDiff/diff_types.h"
#include "compress_parallel.h"
#ifdef __cplusplus
extern "C" {
#endif

#if (_IS_USED_MULTITHREAD)
#   define kDefualtCompressThreadNumber     4
#else
#   define kDefualtCompressThreadNumber     1
#endif


#define kCompressBufSize (1024*32)
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

#define _def_fun_compressType(_fun_name,cstr)   \
static const char*  _fun_name(void){            \
    static const char* kCompressType=cstr;      \
    return kCompressType;                       \
}

hpatch_inline static
hpatch_StreamPos_t _default_maxCompressedSize(hpatch_StreamPos_t dataSize){
    hpatch_StreamPos_t result=dataSize+(dataSize>>3)+1024*2;
    assert(result>dataSize);
    return result;
}
hpatch_inline static
int _default_setParallelThreadNumber(hdiff_TCompress* compressPlugin,int threadNum){
    return 1;
}

#ifdef  _CompressPlugin_zlib
#if (_IsNeedIncludeDefaultCompressHead)
#   include "zlib.h" // http://zlib.net/  https://github.com/madler/zlib
#endif
    struct TCompressPlugin_zlib{
        hdiff_TCompress base;
        int             compress_level; //0..9
        int             mem_level;
        signed char     windowBits;
        hpatch_BOOL     isNeedSaveWindowBits;
    };
    typedef struct _zlib_TCompress{
        const hpatch_TStreamOutput* out_code;
        unsigned char*  c_buf;
        size_t          c_buf_size;
        z_stream        c_stream;
    } _zlib_TCompress;

    static _zlib_TCompress*  _zlib_compress_open_by(const hdiff_TCompress* compressPlugin,
                                                    int compressLevel,int compressMemLevel,
                                                    const hpatch_TStreamOutput* out_code,
                                                    unsigned char* _mem_buf,size_t _mem_buf_size){
        const TCompressPlugin_zlib* plugin=(const TCompressPlugin_zlib*)compressPlugin;
        _zlib_TCompress* self=0;
        
        self=(_zlib_TCompress*)_hpatch_align_upper(_mem_buf,sizeof(hpatch_StreamPos_t));
        assert((_mem_buf+_mem_buf_size)>((unsigned char*)self+sizeof(_zlib_TCompress)));
        _mem_buf_size=(_mem_buf+_mem_buf_size)-((unsigned char*)self+sizeof(_zlib_TCompress));
        _mem_buf=(unsigned char*)self+sizeof(_zlib_TCompress);
        
        memset(self,0,sizeof(_zlib_TCompress));
        self->c_buf=_mem_buf;
        self->c_buf_size=_mem_buf_size;
        self->out_code=out_code;
        
        self->c_stream.next_out = (Bytef*)_mem_buf;
        self->c_stream.avail_out = (uInt)_mem_buf_size;
        if (Z_OK!=deflateInit2(&self->c_stream,compressLevel,Z_DEFLATED,
                               plugin->windowBits,compressMemLevel,Z_DEFAULT_STRATEGY))
            return 0;
        return self;
    }
    static int _zlib_compress_close_by(const hdiff_TCompress* compressPlugin,_zlib_TCompress* self){
        int result=1;//true;
        if (!self) return result;
        if (self->c_stream.total_in!=0){
            int ret=deflateEnd(&self->c_stream);
            result=(Z_OK==ret);
        }
        memset(self,0,sizeof(_zlib_TCompress));
        return result;
    }
    static int _zlib_compress_part(_zlib_TCompress* self,
                                   const unsigned char* part_data,const unsigned char* part_data_end,
                                   int is_data_end,hpatch_StreamPos_t* curWritedPos,int* outStream_isCanceled){
        int                 result=1; //true
        const char*         errAt="";
        int                 is_stream_end=0;
        int                 is_eof=0;
        assert(part_data<=part_data_end);
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
        _check_compress_result(result,*outStream_isCanceled,"_zlib_compress_part()",errAt);
        return result;
    }

    static hpatch_StreamPos_t _zlib_compress(const hdiff_TCompress* compressPlugin,
                                             const hdiff_TStreamOutput* out_code,
                                             const hdiff_TStreamInput*  in_data){
        const TCompressPlugin_zlib* plugin=(const TCompressPlugin_zlib*)compressPlugin;
        hpatch_StreamPos_t result=0; //writedPos
        hpatch_StreamPos_t readFromPos=0;
        const char*        errAt="";
        int                 outStream_isCanceled=0;
        unsigned char*     _temp_buf=0;
        unsigned char*     data_buf=0;
        _zlib_TCompress*   self=0;
        _temp_buf=(unsigned char*)malloc(sizeof(_zlib_TCompress)+kCompressBufSize*2);
        if (!_temp_buf) _compress_error_return("memory alloc");
        self=_zlib_compress_open_by(compressPlugin,plugin->compress_level,plugin->mem_level,
                                    out_code,_temp_buf,sizeof(_zlib_TCompress)+kCompressBufSize);
        data_buf=_temp_buf+sizeof(_zlib_TCompress)+kCompressBufSize;
        if (!self) _compress_error_return("deflateInit2()");
        if (plugin->isNeedSaveWindowBits){
            unsigned char* pchar=(unsigned char*)&plugin->windowBits;
            if (!out_code->write(out_code,0,pchar,pchar+1)) _compress_error_return("out_code->write()");
            ++result;
        }
        do {
            size_t readLen=kCompressBufSize;
            if (readLen>(hpatch_StreamPos_t)(in_data->streamSize-readFromPos))
                readLen=(size_t)(in_data->streamSize-readFromPos);
            if (!in_data->read(in_data,readFromPos,data_buf,data_buf+readLen))
                _compress_error_return("in_data->read()");
            readFromPos+=readLen;
            if (!_zlib_compress_part(self,data_buf,data_buf+readLen,
                                     (readFromPos==in_data->streamSize),&result,&outStream_isCanceled))
                _compress_error_return("_zlib_compress_part()");
        } while (readFromPos<in_data->streamSize);
    clear:
        if (!_zlib_compress_close_by(compressPlugin,self))
            { result=kCompressFailResult; if (strlen(errAt)==0) errAt="deflateEnd()"; }
        _check_compress_result(result,outStream_isCanceled,"_zlib_compress()",errAt);
        if (_temp_buf) free(_temp_buf);
        return result;
    }
    _def_fun_compressType(_zlib_compressType,"zlib");
    static const TCompressPlugin_zlib zlibCompressPlugin={
        {_zlib_compressType,_default_maxCompressedSize,_default_setParallelThreadNumber,_zlib_compress},
            9,MAX_MEM_LEVEL,-MAX_WBITS,hpatch_TRUE};
    
#   if (_IS_USED_MULTITHREAD)
    //pzlib
    struct TCompressPlugin_pzlib{
        TCompressPlugin_zlib base;
        int                  thread_num; // 1..
        hdiff_TParallelCompress pc;
    };
    static int _pzlib_setThreadNum(hdiff_TCompress* compressPlugin,int threadNum){
        TCompressPlugin_pzlib* plugin=(TCompressPlugin_pzlib*)compressPlugin;
        plugin->thread_num=threadNum;
        return threadNum;
    }
    static hdiff_compressBlockHandle _pzlib_openBlockCompressor(hdiff_TParallelCompress* pc){
        return pc;
    }
    static void _pzlib_closeBlockCompressor(hdiff_TParallelCompress* pc,
                                            hdiff_compressBlockHandle blockCompressor){
        assert(blockCompressor==pc);
    }
    static int _pzlib_compress2(Bytef* dest,uLongf* destLen,const Bytef* source,uLong sourceLen,
                                int level,int mem_level,int windowBits){
        z_stream stream;
        int err;
        stream.next_in   = (Bytef *)source;
        stream.avail_in  = (uInt)sourceLen;
        stream.next_out  = dest;
        stream.avail_out = (uInt)*destLen;
        stream.zalloc = 0;
        stream.zfree  = 0;
        stream.opaque = 0;
        err = deflateInit2(&stream,level,Z_DEFLATED,windowBits,mem_level,Z_DEFAULT_STRATEGY);
        if (err != Z_OK) return err;
        err = deflate(&stream, Z_FINISH);
        if (err != Z_STREAM_END){
            deflateEnd(&stream);
            return err == Z_OK ? Z_BUF_ERROR : err;
        }
        *destLen = stream.total_out;
        err = deflateEnd(&stream);
        return err;
    }
    static
    size_t _pzlib_compressBlock(hdiff_TParallelCompress* pc,hdiff_compressBlockHandle blockCompressor,
                                hpatch_StreamPos_t blockIndex,unsigned char* out_code,unsigned char* out_codeEnd,
                                const unsigned char* block_data,const unsigned char* block_dataEnd){
        const TCompressPlugin_pzlib* plugin=(const TCompressPlugin_pzlib*)pc->import;
        hpatch_BOOL isAdding=(blockIndex==0)&&(plugin->base.isNeedSaveWindowBits);
        if (isAdding){
            if (out_code>=out_codeEnd) return 0;//error;
            out_code[0]=plugin->base.windowBits;
            ++out_code;
        }
        uLongf codeLen=(uLongf)(out_codeEnd-out_code);
        if (Z_OK!=_pzlib_compress2(out_code,&codeLen,block_data,(uLong)(block_dataEnd-block_data),
                                   plugin->base.compress_level,plugin->base.mem_level,plugin->base.windowBits))
            return 0; //error
        return codeLen+(isAdding?1:0);
    }
    static hpatch_StreamPos_t _pzlib_compress(const hdiff_TCompress* compressPlugin,
                                              const hdiff_TStreamOutput* out_code,
                                              const hdiff_TStreamInput*  in_data){
        TCompressPlugin_pzlib* plugin=(TCompressPlugin_pzlib*)compressPlugin;
        const size_t blockSize=(plugin->base.compress_level>1)?
                    1024*128*(plugin->base.compress_level-1):1024*128;
        if ((plugin->thread_num<=1)||(plugin->base.compress_level==0)
                ||(in_data->streamSize<blockSize*2)){ //same as "zlib"
            return _zlib_compress(compressPlugin,out_code,in_data);
        }else{
            plugin->pc.import=plugin;
            return parallel_compress_blocks(&plugin->pc,plugin->thread_num,blockSize,out_code,in_data);
        }
    }
    
    _def_fun_compressType(_pzlib_compressType,"pzlib");
    static const TCompressPlugin_pzlib pzlibCompressPlugin={
        { {_pzlib_compressType,_default_maxCompressedSize,_pzlib_setThreadNum,_pzlib_compress},
            6,MAX_MEM_LEVEL,-MAX_WBITS,hpatch_TRUE},
        kDefualtCompressThreadNumber ,{0,_default_maxCompressedSize,_pzlib_openBlockCompressor,
            _pzlib_closeBlockCompressor,_pzlib_compressBlock} };
#   endif // _IS_USED_MULTITHREAD
#endif//_CompressPlugin_zlib
    
#ifdef  _CompressPlugin_bz2
#if (_IsNeedIncludeDefaultCompressHead)
#   include "bzlib.h" // http://www.bzip.org/  https://github.com/sisong/bzip2
#endif
    struct TCompressPlugin_bz2{
        hdiff_TCompress base;
        int             compress_level; //0..9
    };
    static hpatch_StreamPos_t _bz2_compress(const hdiff_TCompress* compressPlugin,
                                            const hdiff_TStreamOutput* out_code,
                                            const hdiff_TStreamInput*  in_data){
        const TCompressPlugin_bz2* plugin=(const TCompressPlugin_bz2*)compressPlugin;
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
        if (BZ_OK!=BZ2_bzCompressInit(&s,plugin->compress_level, 0, 0))
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
        _check_compress_result(result,outStream_isCanceled,"_bz2_compress()",errAt);
        if (_temp_buf) free(_temp_buf);
        return result;
    }
    _def_fun_compressType(_bz2_compressType,"bz2");
    static const TCompressPlugin_bz2 bz2CompressPlugin={
        {_bz2_compressType,_default_maxCompressedSize,_default_setParallelThreadNumber,_bz2_compress}, 9};
    
#   if (_IS_USED_MULTITHREAD)
    //pbz2
    struct TCompressPlugin_pbz2{
        TCompressPlugin_bz2     base;
        int                     thread_num; // 1..
        hdiff_TParallelCompress pc;
    };
    static int _pbz2_setThreadNum(hdiff_TCompress* compressPlugin,int threadNum){
        TCompressPlugin_pbz2* plugin=(TCompressPlugin_pbz2*)compressPlugin;
        plugin->thread_num=threadNum;
        return threadNum;
    }
    static hdiff_compressBlockHandle _pbz2_openBlockCompressor(hdiff_TParallelCompress* pc){
        return pc;
    }
    static void _pbz2_closeBlockCompressor(hdiff_TParallelCompress* pc,
                                           hdiff_compressBlockHandle blockCompressor){
        assert(blockCompressor==pc);
    }
    static
    size_t _pbz2_compressBlock(hdiff_TParallelCompress* pc,hdiff_compressBlockHandle blockCompressor,
                               hpatch_StreamPos_t blockIndex,unsigned char* out_code,unsigned char* out_codeEnd,
                               const unsigned char* block_data,const unsigned char* block_dataEnd){
        const TCompressPlugin_pbz2* plugin=(const TCompressPlugin_pbz2*)pc->import;
        unsigned int codeLen=(unsigned int)(out_codeEnd-out_code);
        if (BZ_OK!=BZ2_bzBuffToBuffCompress((char*)out_code,&codeLen,(char*)block_data,
                                            (unsigned int)(block_dataEnd-block_data),
                                            plugin->base.compress_level,0,0)) return 0; //error
        return codeLen;
    }
    static hpatch_StreamPos_t _pbz2_compress(const hdiff_TCompress* compressPlugin,
                                             const hdiff_TStreamOutput* out_code,
                                             const hdiff_TStreamInput*  in_data){
        TCompressPlugin_pbz2* plugin=(TCompressPlugin_pbz2*)compressPlugin;
        const size_t blockSize=plugin->base.compress_level*100000;
        if ((plugin->thread_num<=1)||(plugin->base.compress_level==0)
                ||(in_data->streamSize<blockSize*2)){ //same as "bz2"
            return _bz2_compress(compressPlugin,out_code,in_data);
        }else{
            plugin->pc.import=plugin;
            return parallel_compress_blocks(&plugin->pc,plugin->thread_num,blockSize,out_code,in_data);
        }
    }
    
    _def_fun_compressType(_pbz2_compressType,"pbz2");
    static const TCompressPlugin_pbz2 pbz2CompressPlugin={
        { {_pbz2_compressType,_default_maxCompressedSize,_pbz2_setThreadNum,_pbz2_compress}, 8},
        kDefualtCompressThreadNumber ,{0,_default_maxCompressedSize,_pbz2_openBlockCompressor,
            _pbz2_closeBlockCompressor,_pbz2_compressBlock} };
#   endif // _IS_USED_MULTITHREAD
#endif//_CompressPlugin_bz2


#if (defined _CompressPlugin_lzma)||(defined _CompressPlugin_lzma2)
#if (_IsNeedIncludeDefaultCompressHead)
#   include "LzmaEnc.h" // "lzma/C/LzmaEnc.h" http://www.7-zip.org/sdk.html
//    https://github.com/sisong/lzma/tree/pthread  support multi-thread compile in macos and linux
#   ifdef _CompressPlugin_lzma2
#       include "Lzma2Enc.h"
#   endif
#endif
    static void * __lzma_enc_Alloc(ISzAllocPtr p, size_t size){
        return malloc(size);
    }
    static void __lzma_enc_Free(ISzAllocPtr p, void *address){
        free(address);
    }
    static ISzAlloc __lzma_enc_alloc={__lzma_enc_Alloc,__lzma_enc_Free};
    
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
#endif
    
#ifdef  _CompressPlugin_lzma
    struct TCompressPlugin_lzma{
        hdiff_TCompress base;
        int             compress_level; //0..9
        UInt32          dict_size;      //patch decompress need 4*lzma_dictSize memroy
        int             thread_num;     //1..2
    };
    static int _lzma_setThreadNumber(hdiff_TCompress* compressPlugin,int threadNum){
        TCompressPlugin_lzma* plugin=(TCompressPlugin_lzma*)compressPlugin;
        if (threadNum>2) threadNum=2;
        plugin->thread_num=threadNum;
        return threadNum;
    }
    static hpatch_StreamPos_t _lzma_compress(const hdiff_TCompress* compressPlugin,
                                             const hdiff_TStreamOutput* out_code,
                                             const hdiff_TStreamInput*  in_data){
        const TCompressPlugin_lzma* plugin=(const TCompressPlugin_lzma*)compressPlugin;
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
        hpatch_uint32_t    dictSize=plugin->dict_size;
        if (!s) s=LzmaEnc_Create(&__lzma_enc_alloc);
        if (!s) _compress_error_return("LzmaEnc_Create()");
        LzmaEncProps_Init(&props);
        props.level=plugin->compress_level;
        props.dictSize=dictSize;
        props.reduceSize=in_data->streamSize;
        props.numThreads=plugin->thread_num;
        LzmaEncProps_Normalize(&props);
        if (SZ_OK!=LzmaEnc_SetProps(s,&props)) _compress_error_return("LzmaEnc_SetProps()");
#       if (IS_NOTICE_compress_canceled)
        printf("  (used one lzma dictSize: %" PRIu64 "  (input data: %" PRIu64 "))\n",
               (hpatch_StreamPos_t)props.dictSize,in_data->streamSize);
#       endif
        
        //save properties_size+properties
        assert(LZMA_PROPS_SIZE<256);
        if (SZ_OK!=LzmaEnc_WriteProperties(s,properties_buf+1,&properties_size))
            _compress_error_return("LzmaEnc_WriteProperties()");
        properties_buf[0]=(unsigned char)properties_size;
        
        if (1+properties_size!=__lzma_SeqOutStream_Write(&outStream.base,properties_buf,1+properties_size))
            _compress_error_return("out_code->write()");
        
        ret=LzmaEnc_Encode(s,&outStream.base,&inStream.base,0,&__lzma_enc_alloc,&__lzma_enc_alloc);
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
        if (s) LzmaEnc_Destroy(s,&__lzma_enc_alloc,&__lzma_enc_alloc);
#endif
        _check_compress_result(result,outStream.isCanceled,"_lzma_compress()",errAt);
        return result;
    }
    _def_fun_compressType(_lzma_compressType,"lzma");
    static const TCompressPlugin_lzma lzmaCompressPlugin={
        {_lzma_compressType,_default_maxCompressedSize,_lzma_setThreadNumber,_lzma_compress},
        7,(1<<23),(kDefualtCompressThreadNumber>=2)?2:kDefualtCompressThreadNumber};
#endif//_CompressPlugin_lzma
    
#ifdef  _CompressPlugin_lzma2
#   include "MtCoder.h" // // "lzma/C/MtCoder.h"   for MTCODER__THREADS_MAX
    struct TCompressPlugin_lzma2{
        hdiff_TCompress base;
        int             compress_level; //0..9
        UInt32          dict_size;      //patch decompress need 4*lzma_dictSize memroy
        int             thread_num;     //1..(64?)
    };
    static int _lzma2_setThreadNumber(hdiff_TCompress* compressPlugin,int threadNum){
        TCompressPlugin_lzma2* plugin=(TCompressPlugin_lzma2*)compressPlugin;
        if (threadNum>MTCODER__THREADS_MAX) threadNum=MTCODER__THREADS_MAX;
        plugin->thread_num=threadNum;
        return threadNum;
    }
    static hpatch_StreamPos_t _lzma2_compress(const hdiff_TCompress* compressPlugin,
                                              const hdiff_TStreamOutput* out_code,
                                              const hdiff_TStreamInput*  in_data){
        const TCompressPlugin_lzma2* plugin=(const TCompressPlugin_lzma2*)compressPlugin;
        struct __lzma_SeqOutStream_t outStream={{__lzma_SeqOutStream_Write},out_code,0,0};
        struct __lzma_SeqInStream_t  inStream={{__lzma_SeqInStream_Read},in_data,0};
        hpatch_StreamPos_t result=0;
        const char*        errAt="";
#if (IS_REUSE_compress_handle)
        static CLzma2EncHandle  s=0;
#else
        CLzma2EncHandle         s=0;
#endif
        CLzma2EncProps     props;
        Byte               properties_size=0;
        SRes               ret;
        hpatch_uint32_t    dictSize=plugin->dict_size;
        if (!s) s=Lzma2Enc_Create(&__lzma_enc_alloc,&__lzma_enc_alloc);
        if (!s) _compress_error_return("LzmaEnc_Create()");
        Lzma2EncProps_Init(&props);
        props.lzmaProps.level=plugin->compress_level;
        props.lzmaProps.dictSize=dictSize;
        props.lzmaProps.reduceSize=in_data->streamSize;
        props.blockSize=LZMA2_ENC_PROPS__BLOCK_SIZE__AUTO;
        props.numTotalThreads=plugin->thread_num;
        Lzma2EncProps_Normalize(&props);
        if (SZ_OK!=Lzma2Enc_SetProps(s,&props)) _compress_error_return("Lzma2Enc_SetProps()");
#       if (IS_NOTICE_compress_canceled)
        printf("  (used one lzma2 dictSize: %" PRIu64 "  (input data: %" PRIu64 "))\n",
               (hpatch_StreamPos_t)props.lzmaProps.dictSize,in_data->streamSize);
#       endif
        
        //save properties_size+properties
        assert(LZMA_PROPS_SIZE<256);
        properties_size=Lzma2Enc_WriteProperties(s);
        
        if (1!=__lzma_SeqOutStream_Write(&outStream.base,&properties_size,1))
            _compress_error_return("out_code->write()");
        
        ret=Lzma2Enc_Encode2(s,&outStream.base,0,0,&inStream.base,0,0,0);
        if (SZ_OK==ret){
            result=outStream.writeToPos;
        }else{//fail
            if (ret==SZ_ERROR_READ)
                _compress_error_return("in_data->read()");
            else if (ret==SZ_ERROR_WRITE)
                _compress_error_return("out_code->write()");
            else
                _compress_error_return("Lzma2Enc_Encode2()");
        }
    clear:
#if (!IS_REUSE_compress_handle)
        if (s) Lzma2Enc_Destroy(s);
#endif
        _check_compress_result(result,outStream.isCanceled,"_lzma2_compress()",errAt);
        return result;
    }
    _def_fun_compressType(_lzma2_compressType,"lzma2");
    static const TCompressPlugin_lzma2 lzma2CompressPlugin={
        {_lzma2_compressType,_default_maxCompressedSize,_lzma2_setThreadNumber,_lzma2_compress},
        7,(1<<23),kDefualtCompressThreadNumber};
#endif//_CompressPlugin_lzma2

    
#if (defined(_CompressPlugin_lz4) || defined(_CompressPlugin_lz4hc))
#if (_IsNeedIncludeDefaultCompressHead)
#   include "lz4.h"   // "lz4/lib/lz4.h" https://github.com/lz4/lz4
#   include "lz4hc.h"
#endif
    struct TCompressPlugin_lz4{
        hdiff_TCompress base;
        int             compress_level; //1..50 default 50
    };
    struct TCompressPlugin_lz4hc{
        hdiff_TCompress base;
        int             compress_level; //3..12
    };

    #define _lz4_write_len4(out_code,isCanceled,writePos,len) { \
        unsigned char _temp_buf4[4];  \
        _temp_buf4[0]=(unsigned char)(len);      \
        _temp_buf4[1]=(unsigned char)((len)>>8); \
        _temp_buf4[2]=(unsigned char)((len)>>16);\
        _temp_buf4[3]=(unsigned char)((len)>>24);\
        _stream_out_code_write(out_code,isCanceled,writePos,_temp_buf4,4); \
    }
    static hpatch_StreamPos_t _lz4_compress(const hdiff_TCompress* compressPlugin,
                                            const hdiff_TStreamOutput* out_code,
                                            const hdiff_TStreamInput*  in_data){
        const TCompressPlugin_lz4* plugin=(const TCompressPlugin_lz4*)compressPlugin;
        const int kLZ4DefaultAcceleration=50-(plugin->compress_level-1);
        hpatch_StreamPos_t  result=0;
        const char*         errAt="";
        unsigned char*      _temp_buf=0;
        unsigned char*      code_buf,* data_buf,* data_buf_back;
        LZ4_stream_t*       s=0;
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
        _check_compress_result(result,outStream_isCanceled,"_lz4_compress()",errAt);
        if (_temp_buf) free(_temp_buf);
        return result;
    }

    static hpatch_StreamPos_t _lz4hc_compress(const hdiff_TCompress* compressPlugin,
                                              const hdiff_TStreamOutput* out_code,
                                              const hdiff_TStreamInput*  in_data){
        const TCompressPlugin_lz4hc* plugin=(const TCompressPlugin_lz4hc*)compressPlugin;
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
        LZ4_resetStreamHC(s,plugin->compress_level);
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
        _check_compress_result(result,outStream_isCanceled,"_lz4hc_compress()",errAt);
        if (_temp_buf) free(_temp_buf);
        return result;
    }

    _def_fun_compressType(_lz4_compressType,"lz4");
    static TCompressPlugin_lz4 lz4CompressPlugin={
        {_lz4_compressType,_default_maxCompressedSize,_default_setParallelThreadNumber,_lz4_compress}, 50};
    static TCompressPlugin_lz4hc lz4hcCompressPlugin ={
        {_lz4_compressType,_default_maxCompressedSize,_default_setParallelThreadNumber,_lz4hc_compress}, 11};
#endif//_CompressPlugin_lz4 or _CompressPlugin_lz4hc


#ifdef  _CompressPlugin_zstd
#if (_IsNeedIncludeDefaultCompressHead)
#   include "zstd.h" // "zstd/lib/zstd.h" https://github.com/facebook/zstd
#endif
    struct TCompressPlugin_zstd{
        hdiff_TCompress base;
        int             compress_level; //0..22
    };
    static hpatch_StreamPos_t _zstd_compress(const hdiff_TCompress* compressPlugin,
                                             const hdiff_TStreamOutput* out_code,
                                             const hdiff_TStreamInput*  in_data){
        const TCompressPlugin_zstd* plugin=(const TCompressPlugin_zstd*)compressPlugin;
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
        ret=ZSTD_initCStream(s,plugin->compress_level);
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
        _check_compress_result(result,outStream_isCanceled,"_zstd_compress()",errAt);
        if (_temp_buf) free(_temp_buf);
        return result;
    }
    _def_fun_compressType(_zstd_compressType,"zstd");
    static TCompressPlugin_zstd zstdCompressPlugin={
        {_zstd_compressType,_default_maxCompressedSize,_default_setParallelThreadNumber,_zstd_compress}, 20};
#endif//_CompressPlugin_zstd

#ifdef __cplusplus
}
#endif
#endif
