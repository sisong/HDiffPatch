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
//  zlibCompressPlugin      // zlib's deflate encoding
//  pzlibCompressPlugin     // compatible with zlib's deflate encoding
//  ldefCompressPlugin      // compatible with zlib's deflate encoding
//  pldefCompressPlugin     // compatible with zlib's deflate encoding
//  bz2CompressPlugin
//  pbz2CompressPlugin
//  lzmaCompressPlugin
//  lzma2CompressPlugin
//  lz4CompressPlugin
//  lz4hcCompressPlugin
//  zstdCompressPlugin
//  brotliCompressPlugin
//  lzhamCompressPlugin
//  tuzCompressPlugin

// _7zXZCompressPlugin : support for create_vcdiff(), compatible with "xdelta3 -S lzma ..."

#include "libHDiffPatch/HDiff/diff_types.h"
#include "compress_parallel.h"
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

#ifndef kDefaultCompressThreadNumber
#if (_IS_USED_MULTITHREAD)
#   define kDefaultCompressThreadNumber     4
#else
#   define kDefaultCompressThreadNumber     1
#endif
#endif

#ifndef kCompressBufSize
#   define kCompressBufSize (1024*32)
#endif
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

static
hpatch_StreamPos_t _default_maxCompressedSize(hpatch_StreamPos_t dataSize){
    hpatch_StreamPos_t result=dataSize+(dataSize>>3)+1024*2;
    assert(result>dataSize);
    return result;
}
static
int _default_setParallelThreadNumber(hdiff_TCompress* compressPlugin,int threadNum){
    return 1;
}

#ifdef  _CompressPlugin_zlib
#if (_IsNeedIncludeDefaultCompressHead)
#   include "zlib.h" // http://zlib.net/  https://github.com/madler/zlib
#endif
    typedef struct{
        hdiff_TCompress base;
        int             compress_level; //0..9
        int             mem_level;
        signed char     windowBits; // -9..-15
        hpatch_BOOL     isNeedSaveWindowBits;
        int             strategy;
    } TCompressPlugin_zlib;
    typedef struct _zlib_TCompress{
        const hpatch_TStreamOutput* out_code;
        unsigned char*  c_buf;
        size_t          c_buf_size;
        z_stream        c_stream;
    } _zlib_TCompress;

    static _zlib_TCompress*  _zlib_compress_open_at(const hdiff_TCompress* compressPlugin,
                                                    int compressLevel,int compressMemLevel,
                                                    const hpatch_TStreamOutput* out_code,
                                                    _zlib_TCompress* self,size_t _self_and_buf_size){
        const TCompressPlugin_zlib* plugin=(const TCompressPlugin_zlib*)compressPlugin;
        assert(_self_and_buf_size>sizeof(_zlib_TCompress));
        memset(self,0,sizeof(_zlib_TCompress));
        self->c_buf=((unsigned char*)self)+sizeof(_zlib_TCompress);;
        self->c_buf_size=_self_and_buf_size-sizeof(_zlib_TCompress);
        self->out_code=out_code;
        
        self->c_stream.next_out = (Bytef*)self->c_buf;
        self->c_stream.avail_out = (uInt)self->c_buf_size;
        if (Z_OK!=deflateInit2(&self->c_stream,compressLevel,Z_DEFLATED,
                               plugin->windowBits,compressMemLevel,plugin->strategy))
            return 0;
        return self;
    }
    static _zlib_TCompress*  _zlib_compress_open_by(const hdiff_TCompress* compressPlugin,
                                                    int compressLevel,int compressMemLevel,
                                                    const hpatch_TStreamOutput* out_code,
                                                    unsigned char* _mem_buf,size_t _mem_buf_size){
        #define __MAX_TS(a,b)  ((a)>=(b)?(a):(b))
        const hpatch_size_t kZlibAlign=__MAX_TS(__MAX_TS(sizeof(hpatch_StreamPos_t),sizeof(void*)),sizeof(uLongf));
        #undef __MAX_TS
        unsigned char* _mem_buf_end=_mem_buf+_mem_buf_size;
        unsigned char* self_at=(unsigned char*)_hpatch_align_upper(_mem_buf,kZlibAlign);
        if (self_at>=_mem_buf_end) return 0;
        return _zlib_compress_open_at(compressPlugin,compressLevel,compressMemLevel,out_code,
                                      (_zlib_TCompress*)self_at,_mem_buf_end-self_at);
    }
    static hpatch_BOOL _zlib_compress_close_by(const hdiff_TCompress* compressPlugin,_zlib_TCompress* self){
        hpatch_BOOL result=hpatch_TRUE;
        if (!self) return result;
        if (self->c_stream.state!=0){
            int ret=deflateEnd(&self->c_stream);
            result=(Z_OK==ret)|(Z_DATA_ERROR==ret);
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
                                             const hpatch_TStreamOutput* out_code,
                                             const hpatch_TStreamInput*  in_data){
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
            const unsigned char* pchar=(const unsigned char*)&plugin->windowBits;
            if (!out_code->write(out_code,0,pchar,pchar+1)) _compress_error_return("out_code->write()");
            ++result;
        }
        while (readFromPos<in_data->streamSize){
            size_t readLen=kCompressBufSize;
            if (readLen>(hpatch_StreamPos_t)(in_data->streamSize-readFromPos))
                readLen=(size_t)(in_data->streamSize-readFromPos);
            if (!in_data->read(in_data,readFromPos,data_buf,data_buf+readLen))
                _compress_error_return("in_data->read()");
            readFromPos+=readLen;
            if (!_zlib_compress_part(self,data_buf,data_buf+readLen,
                                     (readFromPos==in_data->streamSize),&result,&outStream_isCanceled))
                _compress_error_return("_zlib_compress_part()");
        }
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
            9,8,-MAX_WBITS,hpatch_TRUE,Z_DEFAULT_STRATEGY};
    
#   if (_IS_USED_MULTITHREAD)
    //pzlib
    typedef struct {
        TCompressPlugin_zlib base;
        int                  thread_num; // 1..
        hdiff_TParallelCompress pc;
    } TCompressPlugin_pzlib;
    static int _pzlib_setThreadNum(hdiff_TCompress* compressPlugin,int threadNum){
        TCompressPlugin_pzlib* plugin=(TCompressPlugin_pzlib*)compressPlugin;
        plugin->thread_num=threadNum;
        return threadNum;
    }
    static void _pzlib_closeBlockCompressor(hdiff_TParallelCompress* pc,
                                            hdiff_compressBlockHandle blockCompressor){
        z_stream* stream=(z_stream*)blockCompressor;
        if (!stream) return;
        if (stream->state!=0){
            int ret=deflateEnd(stream);
            assert((Z_OK==ret)|(Z_DATA_ERROR==ret));
        }
        free(stream);
    }
    static hdiff_compressBlockHandle _pzlib_openBlockCompressor(hdiff_TParallelCompress* pc){
        const TCompressPlugin_pzlib* plugin=(const TCompressPlugin_pzlib*)pc->import;
        z_stream* stream=(z_stream*)malloc(sizeof(z_stream));
        if (!stream) return 0;
        memset(stream,0,sizeof(z_stream));
        int err = deflateInit2(stream,plugin->base.compress_level,Z_DEFLATED,
                               plugin->base.windowBits,plugin->base.mem_level,Z_DEFAULT_STRATEGY);
        if (err!=Z_OK){//error
            _pzlib_closeBlockCompressor(pc,stream);
            return 0;
        }
        return stream;
    }
    static int _pzlib_compress2(Bytef* dest,uLongf* destLen,const unsigned char* block_data,
                                const unsigned char* block_dictEnd,const unsigned char* block_dataEnd,
                                int isEndBlock,z_stream* stream){
        int err;
        #define _check_zlib_err(_must_V) { if (err!=(_must_V)) goto _errorReturn; } 
        err=deflateReset(stream);
        _check_zlib_err(Z_OK);
        stream->next_in   = (Bytef*)block_dictEnd;
        stream->avail_in  = (uInt)(block_dataEnd-block_dictEnd);
        stream->next_out  = dest;
        stream->avail_out = (uInt)*destLen;
        stream->total_out = 0;
        if (block_data<block_dictEnd){
            err=deflateSetDictionary(stream,(Bytef*)block_data,(uInt)(block_dictEnd-block_data));
            _check_zlib_err(Z_OK);
        }
        if (!isEndBlock){
            int bits; 
            err = deflate(stream,Z_BLOCK);
            _check_zlib_err(Z_OK);
            // add enough empty blocks to get to a byte boundary
            err = deflatePending(stream,Z_NULL,&bits);
            _check_zlib_err(Z_OK);
            if (bits & 1){
                err = deflate(stream,Z_SYNC_FLUSH);
                _check_zlib_err(Z_OK);
            } else if (bits & 7) {
                do { // add static empty blocks
                    err = deflatePrime(stream, 10, 2);
                    _check_zlib_err(Z_OK);
                    err = deflatePending(stream,Z_NULL,&bits);
                    _check_zlib_err(Z_OK);
                } while (bits & 7);
                err = deflate(stream,Z_BLOCK);
                _check_zlib_err(Z_OK);
            }
        }else{
            err = deflate(stream,Z_FINISH);
            _check_zlib_err(Z_STREAM_END);
            err=Z_OK;
        }
        *destLen = stream->total_out;
        return err;
    _errorReturn:
        return err == Z_OK ? Z_BUF_ERROR : err;
        #undef _check_zlib_err
    }
    static
    size_t _pzlib_compressBlock(hdiff_TParallelCompress* pc,hdiff_compressBlockHandle blockCompressor,
                                hpatch_StreamPos_t blockIndex,hpatch_StreamPos_t blockCount,unsigned char* out_code,unsigned char* out_codeEnd,
                                const unsigned char* block_data,const unsigned char* block_dictEnd,const unsigned char* block_dataEnd){
        const TCompressPlugin_pzlib* plugin=(const TCompressPlugin_pzlib*)pc->import;
        const hpatch_BOOL isAdding=(blockIndex==0)&&(plugin->base.isNeedSaveWindowBits);
        if (isAdding){
            if (out_code>=out_codeEnd) return 0;//error;
            out_code[0]=plugin->base.windowBits;
            ++out_code;
        }
        uLongf codeLen=(uLongf)(out_codeEnd-out_code);
        if (Z_OK!=_pzlib_compress2(out_code,&codeLen,block_data,block_dictEnd,block_dataEnd,
                                   blockIndex+1==blockCount?1:0,(z_stream*)blockCompressor))
            return 0; //error
        return codeLen+(isAdding?1:0);
    }
    static hpatch_StreamPos_t _pzlib_compress(const hdiff_TCompress* compressPlugin,
                                              const hpatch_TStreamOutput* out_code,
                                              const hpatch_TStreamInput*  in_data){
        TCompressPlugin_pzlib* plugin=(TCompressPlugin_pzlib*)compressPlugin;
        const size_t blockSize=256*1024;
        if ((plugin->thread_num<=1)||(plugin->base.compress_level==0)
                ||(in_data->streamSize<blockSize*2)){ //same as "zlib"
            return _zlib_compress(compressPlugin,out_code,in_data);
        }else{
            int dictBits=plugin->base.windowBits;
            if (dictBits<0) dictBits=-dictBits;
            if (dictBits>15) dictBits-=16;
            if (dictBits<9) dictBits=9;
            else if (dictBits>15) dictBits=15;
            plugin->pc.import=plugin;
            return parallel_compress_blocks(&plugin->pc,plugin->thread_num,((size_t)1<<dictBits),blockSize,out_code,in_data);
        }
    }
    
    static const TCompressPlugin_pzlib pzlibCompressPlugin={
        { {_zlib_compressType,_default_maxCompressedSize,_pzlib_setThreadNum,_pzlib_compress},
            6,8,-MAX_WBITS,hpatch_TRUE,Z_DEFAULT_STRATEGY},
        kDefaultCompressThreadNumber ,{0,_default_maxCompressedSize,_pzlib_openBlockCompressor,
            _pzlib_closeBlockCompressor,_pzlib_compressBlock} };
#   endif // _IS_USED_MULTITHREAD
#endif//_CompressPlugin_zlib


#ifdef  _CompressPlugin_ldef
#if (_IsNeedIncludeDefaultCompressHead)
#   include "libdeflate.h" // "libdeflate/libdeflate.h" https://github.com/sisong/libdeflate/tree/stream-mt based on https://github.com/ebiggers/libdeflate
#endif
    static const signed char _ldef_kWindowBits=-15; //always as zlib's windowBits -15
    static const size_t _ldef_kDictSize=(1<<15);
    static const size_t _ldef_kMax_in_border_nbytes=258;//deflate max match len
    static size_t _ldef_getBestWorkStepSize(hpatch_StreamPos_t data_size,size_t compress_level,size_t thread_num){
        const size_t _kMinBaseStep=600000;
        size_t base_step_size=1024*1024*2;
        hpatch_StreamPos_t work_count=(data_size+base_step_size-1)/base_step_size;
        if (work_count>=thread_num) return base_step_size;
        size_t step_size=(size_t)((data_size+thread_num-1)/thread_num);
        const size_t minStep=_kMinBaseStep;
        step_size=(step_size>minStep)?step_size:minStep;
        return step_size;
    }
    typedef struct{
        hdiff_TCompress base;
        int             compress_level; //0..12
        hpatch_BOOL     isNeedSaveWindowBits;
    } TCompressPlugin_ldef;

    static hpatch_StreamPos_t _ldef_compress(const hdiff_TCompress* compressPlugin,
                                             const hpatch_TStreamOutput* out_code,
                                             const hpatch_TStreamInput*  in_data){
        const TCompressPlugin_ldef* plugin=(const TCompressPlugin_ldef*)compressPlugin;
        hpatch_StreamPos_t result=0; //writedPos
        hpatch_StreamPos_t readFromPos=0;
        const char*        errAt="";
        int                 outStream_isCanceled=0;
        unsigned char*     data_buf=0;
        unsigned char*     code_buf=0;
        struct libdeflate_compressor* c=0;
        size_t in_border_nbytes=0;
        const size_t in_step_size=_ldef_getBestWorkStepSize(in_data->streamSize,plugin->compress_level,1);
        const size_t _in_buf_size=_ldef_kDictSize+in_step_size+_ldef_kMax_in_border_nbytes;
        const size_t block_bound=libdeflate_deflate_compress_bound_block(in_step_size);

        data_buf=(unsigned char*)malloc(_in_buf_size+block_bound);
        if (!data_buf) _compress_error_return("memory alloc");
        code_buf=data_buf+_in_buf_size;

        c=libdeflate_alloc_compressor(plugin->compress_level);
        if (!c) _compress_error_return("libdeflate_alloc_compressor()");
        
        if (plugin->isNeedSaveWindowBits){//save deflate windowBits
            const unsigned char* pchar=(const unsigned char*)&_ldef_kWindowBits;
            if (!out_code->write(out_code,0,pchar,pchar+1)) _compress_error_return("out_code->write()");
            ++result;
        }
        while (readFromPos<in_data->streamSize){
            const size_t curDictSize=(readFromPos>0)?_ldef_kDictSize:0;
            size_t readLen=in_step_size;
            size_t codeLen;
            if (readLen>(hpatch_StreamPos_t)(in_data->streamSize-readFromPos))
                readLen=(size_t)(in_data->streamSize-readFromPos);
            {
                unsigned char* dst_buf=data_buf+curDictSize+in_border_nbytes;
                size_t cur_in_border_nbytes=_ldef_kMax_in_border_nbytes;
                if (readLen+cur_in_border_nbytes>(hpatch_StreamPos_t)(in_data->streamSize-readFromPos))
                    cur_in_border_nbytes=(size_t)(in_data->streamSize-readFromPos-readLen);
                if (!in_data->read(in_data,readFromPos+in_border_nbytes,dst_buf,
                                   dst_buf+readLen+cur_in_border_nbytes-in_border_nbytes))
                    _compress_error_return("in_data->read()");
                in_border_nbytes=cur_in_border_nbytes;
            }
            readFromPos+=readLen;

            codeLen=libdeflate_deflate_compress_block_continue(c,data_buf,curDictSize,readLen,in_border_nbytes,
                        (readFromPos==in_data->streamSize),code_buf,block_bound,hpatch_FALSE);
        
            if (codeLen>0){
                _stream_out_code_write(out_code,outStream_isCanceled,result,code_buf,codeLen);
            }else
                _compress_error_return("libdeflate_deflate_compress_block_continue()");
            if (readFromPos<in_data->streamSize){
                memmove(data_buf,data_buf+readLen+curDictSize-_ldef_kDictSize,_ldef_kDictSize+in_border_nbytes);
            }
        }
    clear:
        if (c) libdeflate_free_compressor(c);
        _check_compress_result(result,outStream_isCanceled,"_ldef_compress()",errAt);
        if (data_buf) free(data_buf);
        return result;
    }
    _def_fun_compressType(_ldef_compressType,"zlib");
    _def_fun_compressType(_ldef_compressTypeForDisplay,"ldef (zlib compatible)");
    static const TCompressPlugin_ldef ldefCompressPlugin={
        {_ldef_compressType,_default_maxCompressedSize,_default_setParallelThreadNumber,
         _ldef_compress,_ldef_compressTypeForDisplay}, 12,hpatch_TRUE};

#   if (_IS_USED_MULTITHREAD)
    //pldef
    typedef struct {
        TCompressPlugin_ldef base;
        int                  thread_num; // 1..
        hdiff_TParallelCompress pc;
    } TCompressPlugin_pldef;
    static int _pldef_setThreadNum(hdiff_TCompress* compressPlugin,int threadNum){
        TCompressPlugin_pldef* plugin=(TCompressPlugin_pldef*)compressPlugin;
        plugin->thread_num=threadNum;
        return threadNum;
    }
    static hdiff_compressBlockHandle _pldef_openBlockCompressor(hdiff_TParallelCompress* pc){
        const TCompressPlugin_pldef* plugin=(const TCompressPlugin_pldef*)pc->import;
        return libdeflate_alloc_compressor(plugin->base.compress_level);
    }
    static void _pldef_closeBlockCompressor(hdiff_TParallelCompress* pc,
                                            hdiff_compressBlockHandle blockCompressor){
        libdeflate_free_compressor((struct libdeflate_compressor*)blockCompressor);
    }
    static
    size_t _pldef_compressBlock(hdiff_TParallelCompress* pc,hdiff_compressBlockHandle blockCompressor,
                                hpatch_StreamPos_t blockIndex,hpatch_StreamPos_t blockCount,unsigned char* out_code,unsigned char* out_codeEnd,
                                const unsigned char* block_data,const unsigned char* block_dictEnd,const unsigned char* block_dataEnd){
        const TCompressPlugin_pldef* plugin=(const TCompressPlugin_pldef*)pc->import;
        struct libdeflate_compressor* c=(struct libdeflate_compressor*)blockCompressor;
        const hpatch_BOOL isAdding=(blockIndex==0)&&plugin->base.isNeedSaveWindowBits;
        if (isAdding){
            if (out_code>=out_codeEnd) return 0;//error;
            out_code[0]=_ldef_kWindowBits;
            ++out_code;
        }

        size_t codeLen=libdeflate_deflate_compress_block(c,block_data,block_dictEnd-block_data,block_dataEnd-block_dictEnd,
                                                         blockIndex+1==blockCount,out_code,out_codeEnd-out_code,1);
        if (codeLen==0)
            return 0; //error
        return codeLen+(isAdding?1:0);
    }
    static hpatch_StreamPos_t _pldef_compress(const hdiff_TCompress* compressPlugin,
                                              const hpatch_TStreamOutput* out_code,
                                              const hpatch_TStreamInput*  in_data){
        TCompressPlugin_pldef* plugin=(TCompressPlugin_pldef*)compressPlugin;
        const size_t blockSize=_ldef_getBestWorkStepSize(in_data->streamSize,plugin->base.compress_level,plugin->thread_num);
        if ((plugin->thread_num<=1)||(plugin->base.compress_level==0)
                ||(in_data->streamSize<blockSize*2)){ //same as "zlib"
            return _ldef_compress(compressPlugin,out_code,in_data);
        }else{
            plugin->pc.import=plugin;
            return parallel_compress_blocks(&plugin->pc,plugin->thread_num,_ldef_kDictSize,blockSize,out_code,in_data);
        }
    }
    
    
    static const TCompressPlugin_pldef pldefCompressPlugin={
        { {_ldef_compressType,_default_maxCompressedSize,_pldef_setThreadNum,
           _pldef_compress,_ldef_compressTypeForDisplay}, 9,hpatch_TRUE},
        kDefaultCompressThreadNumber ,{0,_default_maxCompressedSize,_pldef_openBlockCompressor,
            _pldef_closeBlockCompressor,_pldef_compressBlock} };
#   endif // _IS_USED_MULTITHREAD
#endif//_CompressPlugin_ldef


#ifdef  _CompressPlugin_bz2
#if (_IsNeedIncludeDefaultCompressHead)
#   include "bzlib.h" // http://www.bzip.org/  https://github.com/sisong/bzip2
#endif
    typedef struct{
        hdiff_TCompress base;
        int             compress_level; //0..9
    } TCompressPlugin_bz2;
    static hpatch_StreamPos_t _bz2_compress(const hdiff_TCompress* compressPlugin,
                                            const hpatch_TStreamOutput* out_code,
                                            const hpatch_TStreamInput*  in_data){
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
    typedef struct{
        TCompressPlugin_bz2     base;
        int                     thread_num; // 1..
        hdiff_TParallelCompress pc;
    } TCompressPlugin_pbz2;
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
                               hpatch_StreamPos_t blockIndex,hpatch_StreamPos_t blockCount,unsigned char* out_code,unsigned char* out_codeEnd,
                               const unsigned char* block_data,const unsigned char* block_dictEnd,const unsigned char* block_dataEnd){
        const TCompressPlugin_pbz2* plugin=(const TCompressPlugin_pbz2*)pc->import;
        unsigned int codeLen=(unsigned int)(out_codeEnd-out_code);
        if (BZ_OK!=BZ2_bzBuffToBuffCompress((char*)out_code,&codeLen,(char*)block_data,
                                            (unsigned int)(block_dataEnd-block_data),
                                            plugin->base.compress_level,0,0)) return 0; //error
        return codeLen;
    }
    static hpatch_StreamPos_t _pbz2_compress(const hdiff_TCompress* compressPlugin,
                                             const hpatch_TStreamOutput* out_code,
                                             const hpatch_TStreamInput*  in_data){
        TCompressPlugin_pbz2* plugin=(TCompressPlugin_pbz2*)compressPlugin;
        const size_t blockSize=plugin->base.compress_level*100000;
        if ((plugin->thread_num<=1)||(plugin->base.compress_level==0)
                ||(in_data->streamSize<blockSize*2)){ //same as "bz2"
            return _bz2_compress(compressPlugin,out_code,in_data);
        }else{
            plugin->pc.import=plugin;
            return parallel_compress_blocks(&plugin->pc,plugin->thread_num,0,blockSize,out_code,in_data);
        }
    }
    
    _def_fun_compressType(_pbz2_compressType,"pbz2");
    static const TCompressPlugin_pbz2 pbz2CompressPlugin={
        { {_pbz2_compressType,_default_maxCompressedSize,_pbz2_setThreadNum,_pbz2_compress}, 8},
        kDefaultCompressThreadNumber ,{0,_default_maxCompressedSize,_pbz2_openBlockCompressor,
            _pbz2_closeBlockCompressor,_pbz2_compressBlock} };
#   endif // _IS_USED_MULTITHREAD
#endif//_CompressPlugin_bz2


#if (defined _CompressPlugin_lzma)||(defined _CompressPlugin_lzma2)||(defined _CompressPlugin_7zXZ)
#if (_IsNeedIncludeDefaultCompressHead)
#   include "LzmaEnc.h" // "lzma/C/LzmaEnc.h" https://github.com/sisong/lzma
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
        const hpatch_TStreamOutput* out_code;
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
        const hpatch_TStreamInput*  in_data;
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
    typedef struct{
        hdiff_TCompress base;
        int             compress_level; //0..9
        UInt32          dict_size;      //patch decompress need 4?*lzma_dictSize memory
        int             thread_num;     //1..2
    } TCompressPlugin_lzma;
    static int _lzma_setThreadNumber(hdiff_TCompress* compressPlugin,int threadNum){
        TCompressPlugin_lzma* plugin=(TCompressPlugin_lzma*)compressPlugin;
        if (threadNum>2) threadNum=2;
        plugin->thread_num=threadNum;
        return threadNum;
    }
    static hpatch_StreamPos_t _lzma_compress(const hdiff_TCompress* compressPlugin,
                                             const hpatch_TStreamOutput* out_code,
                                             const hpatch_TStreamInput*  in_data){
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
        if (s) { LzmaEnc_Destroy(s,&__lzma_enc_alloc,&__lzma_enc_alloc); s=0; }
#endif
        _check_compress_result(result,outStream.isCanceled,"_lzma_compress()",errAt);
        return result;
    }
    _def_fun_compressType(_lzma_compressType,"lzma");
    static const TCompressPlugin_lzma lzmaCompressPlugin={
        {_lzma_compressType,_default_maxCompressedSize,_lzma_setThreadNumber,_lzma_compress},
        7,(1<<23),(kDefaultCompressThreadNumber>=2)?2:kDefaultCompressThreadNumber};
#endif//_CompressPlugin_lzma
    
#ifdef  _CompressPlugin_lzma2
#if (_IsNeedIncludeDefaultCompressHead)
#   include "MtCoder.h" // "lzma/C/MtCoder.h"   for MTCODER__THREADS_MAX
#endif
    struct TCompressPlugin_lzma2{
        hdiff_TCompress base;
        int             compress_level; //0..9
        UInt32          dict_size;      //patch decompress need 4?*lzma_dictSize memory
        int             thread_num;     //1..(64?)
    };
    static int _lzma2_setThreadNumber(hdiff_TCompress* compressPlugin,int threadNum){
        TCompressPlugin_lzma2* plugin=(TCompressPlugin_lzma2*)compressPlugin;
        if (threadNum>MTCODER_THREADS_MAX) threadNum=MTCODER_THREADS_MAX;
        plugin->thread_num=threadNum;
        return threadNum;
    }
    static hpatch_StreamPos_t _lzma2_compress(const hdiff_TCompress* compressPlugin,
                                              const hpatch_TStreamOutput* out_code,
                                              const hpatch_TStreamInput*  in_data){
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
        props.blockSize=LZMA2_ENC_PROPS_BLOCK_SIZE_AUTO;
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
        if (s) { Lzma2Enc_Destroy(s); s=0; }
#endif
        _check_compress_result(result,outStream.isCanceled,"_lzma2_compress()",errAt);
        return result;
    }
    _def_fun_compressType(_lzma2_compressType,"lzma2");
    static const TCompressPlugin_lzma2 lzma2CompressPlugin={
        {_lzma2_compressType,_default_maxCompressedSize,_lzma2_setThreadNumber,_lzma2_compress},
        7,(1<<23),kDefaultCompressThreadNumber};
#endif//_CompressPlugin_lzma2

#ifdef  _CompressPlugin_7zXZ
#if (_IsNeedIncludeDefaultCompressHead)
#   include "XzEnc.h" // "lzma/C/XzEnc.h" https://github.com/sisong/lzma
#   include "MtCoder.h" // "lzma/C/MtCoder.h"   for MTCODER__THREADS_MAX
#   include "7zCrc.h" // CrcGenerateTable()
#endif

#ifndef _init_CompressPlugin_7zXZ_DEF
#   define _init_CompressPlugin_7zXZ_DEF
    static int _init_CompressPlugin_7zXZ(){
        static hpatch_BOOL _isInit=hpatch_FALSE;
        if (!_isInit){
            CrcGenerateTable();
            _isInit=hpatch_TRUE;
        }
        return 0;
    }
#endif

    typedef struct{
        hdiff_TCompress base;
        int             compress_level; //0..9
        UInt32          dict_size;      //patch decompress need 3?*lzma_dictSize memory
        int             thread_num;     //1..(64?)
    } TCompressPlugin_7zXZ;
    static int _7zXZ_setThreadNumber(hdiff_TCompress* compressPlugin,int threadNum){
        TCompressPlugin_lzma2* plugin=(TCompressPlugin_lzma2*)compressPlugin;
        if (threadNum>MTCODER_THREADS_MAX) threadNum=MTCODER_THREADS_MAX;
        plugin->thread_num=threadNum;
        return threadNum;
    }
    
    typedef void* _7zXZ_compressHandle;
    static _7zXZ_compressHandle _7zXZ_compress_open(const hdiff_TCompress* compressPlugin){
        const TCompressPlugin_7zXZ* plugin=(const TCompressPlugin_7zXZ*)compressPlugin;
        _7zXZ_compressHandle result=0;
        const char*        errAt="";
#if (IS_REUSE_compress_handle)
        static CXzEncHandle  s=0;
#else
        CXzEncHandle         s=0;
#endif
        CXzProps           xzprops;
        hpatch_uint32_t    dictSize=plugin->dict_size;
        if (!s) s=XzEnc_Create(&__lzma_enc_alloc,&__lzma_enc_alloc);
        if (!s) _compress_error_return("XzEnc_Create()");
        XzProps_Init(&xzprops);
        xzprops.lzma2Props.lzmaProps.level=plugin->compress_level;
        xzprops.lzma2Props.lzmaProps.dictSize=dictSize;
        xzprops.lzma2Props.numTotalThreads=plugin->thread_num;
        Lzma2EncProps_Normalize(&xzprops.lzma2Props);
        xzprops.numTotalThreads=plugin->thread_num;
        xzprops.checkId=XZ_CHECK_NO;
        if (SZ_OK!=XzEnc_SetProps(s,&xzprops)) _compress_error_return("XzEnc_SetProps()");
        return s;
    clear:
#if (!IS_REUSE_compress_handle)
        if (s) { XzEnc_Destroy(s); s=0; }
#endif
        return result;
    }
    static hpatch_BOOL _7zXZ_compress_close(const hdiff_TCompress* compressPlugin,
                                     _7zXZ_compressHandle compressHandle){
#if (!IS_REUSE_compress_handle)
        CXzEncHandle s=(CXzEncHandle)compressHandle;
        if (s) { XzEnc_Destroy(s); s=0; }
#endif
        return hpatch_TRUE;
    }

    static hpatch_StreamPos_t _7zXZ_compress_encode(_7zXZ_compressHandle compressHandle,
                                                    const hpatch_TStreamOutput* out_code,
                                                    const hpatch_TStreamInput*  in_data,
                                                    hpatch_BOOL isWriteHead,hpatch_BOOL isWriteEnd){
        struct __lzma_SeqOutStream_t outStream={{__lzma_SeqOutStream_Write},out_code,0,0};
        struct __lzma_SeqInStream_t  inStream={{__lzma_SeqInStream_Read},in_data,0};
        hpatch_StreamPos_t result=0;
        const char*        errAt="";
        CXzEncHandle s=(CXzEncHandle)compressHandle;
        SRes               ret;
        XzEnc_SetDataSize(s,in_data->streamSize);
        
        ret=XzEnc_Encode_Part(s,&outStream.base,&inStream.base,0,isWriteHead,isWriteEnd);
        if (SZ_OK==ret){
            result=outStream.writeToPos;
        }else{//fail
            if (ret==SZ_ERROR_READ)
                _compress_error_return("in_data->read()");
            else if (ret==SZ_ERROR_WRITE)
                _compress_error_return("out_code->write()");
            else
                _compress_error_return("XzEnc_Encode_Part()");
        }
    clear:
        _check_compress_result(result,outStream.isCanceled,"_7zXZ_compress_encode()",errAt);
        return result;
    }

    static hpatch_StreamPos_t _7zXZ_compress(const hdiff_TCompress* compressPlugin,
                                             const hpatch_TStreamOutput* out_code,
                                             const hpatch_TStreamInput*  in_data){
        const TCompressPlugin_7zXZ* plugin=(const TCompressPlugin_7zXZ*)compressPlugin;
        hpatch_StreamPos_t result=0;
        const char*        errAt="";
        _7zXZ_compressHandle s=_7zXZ_compress_open(compressPlugin);
        if (!s) _compress_error_return("_7zXZ_compress_open()");
        result=_7zXZ_compress_encode(s,out_code,in_data,hpatch_TRUE,hpatch_TRUE);
        if (result==0) 
            _compress_error_return("_7zXZ_compress_encode()");
    clear:
        if (s) { _7zXZ_compress_close(compressPlugin,s); s=0; }
        return result;
    }
    _def_fun_compressType(_7zXZ_compressType,"7zXZ");
    static const TCompressPlugin_7zXZ _7zXZCompressPlugin={
        {_7zXZ_compressType,_default_maxCompressedSize,_7zXZ_setThreadNumber,_7zXZ_compress},
        7,(1<<23),kDefaultCompressThreadNumber};

#endif //_CompressPlugin_7zXZ

    
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
                                            const hpatch_TStreamOutput* out_code,
                                            const hpatch_TStreamInput*  in_data){
        const TCompressPlugin_lz4* plugin=(const TCompressPlugin_lz4*)compressPlugin;
        const int kLZ4DefaultAcceleration=50-(plugin->compress_level-1);
        hpatch_StreamPos_t  result=0;
        const char*         errAt="";
        unsigned char*      _temp_buf=0;
        unsigned char*      code_buf,* data_buf,* data_buf_back;
        LZ4_stream_t*       s=0;
        hpatch_StreamPos_t  readFromPos=0;
        int                 outStream_isCanceled=0;
        int  kLz4CompressBufSize =1024*510; //patch decompress need 4*2*0.5MB memory
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
                                              const hpatch_TStreamOutput* out_code,
                                              const hpatch_TStreamInput*  in_data){
        const TCompressPlugin_lz4hc* plugin=(const TCompressPlugin_lz4hc*)compressPlugin;
        hpatch_StreamPos_t  result=0;
        const char*         errAt="";
        unsigned char*      _temp_buf=0;
        unsigned char*      code_buf,* data_buf,* data_buf_back;
        LZ4_streamHC_t*     s=0;
        hpatch_StreamPos_t  readFromPos=0;
        int                 outStream_isCanceled=0;
        int  kLz4CompressBufSize =1024*510; //patch decompress need 4*2*0.5MB memory
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
#   include "zstd.h" // "zstd/lib/zstd.h" https://github.com/sisong/zstd
#endif
    struct TCompressPlugin_zstd{
        hdiff_TCompress base;
        int             compress_level; //0..22
        int             dict_bits;  // 10..(30 or 31)
        int             thread_num;     //1..(200?)
    };
    static int _zstd_setThreadNumber(hdiff_TCompress* compressPlugin,int threadNum){
        TCompressPlugin_zstd* plugin=(TCompressPlugin_zstd*)compressPlugin;
        #define ZSTDMT_NBWORKERS_MAX 200
        if (threadNum>ZSTDMT_NBWORKERS_MAX) threadNum=ZSTDMT_NBWORKERS_MAX;
        plugin->thread_num=threadNum;
        return threadNum;
    }
    static hpatch_StreamPos_t _zstd_compress(const hdiff_TCompress* compressPlugin,
                                             const hpatch_TStreamOutput* out_code,
                                             const hpatch_TStreamInput*  in_data){
        const TCompressPlugin_zstd* plugin=(const TCompressPlugin_zstd*)compressPlugin;
        hpatch_StreamPos_t  result=0;
        const char*         errAt="";
        unsigned char*      _temp_buf=0;
        ZSTD_inBuffer       s_input;
        ZSTD_outBuffer      s_output;
#if (IS_REUSE_compress_handle)
        static ZSTD_CCtx*   s=0;
#else
        ZSTD_CCtx*          s=0;
#endif
        hpatch_StreamPos_t  readFromPos=0;
        int                 outStream_isCanceled=0;
        int                 dict_bits;
        size_t              ret;

        s_input.size=ZSTD_CStreamInSize();
        s_output.size=ZSTD_CStreamOutSize();
        _temp_buf=(unsigned char*)malloc(s_input.size+s_output.size);
        if (!_temp_buf) _compress_error_return("memory alloc");
        s_input.src=_temp_buf;
        s_output.dst=_temp_buf+s_input.size;
        
        if (!s) s=ZSTD_createCCtx();
        else ZSTD_CCtx_reset(s,ZSTD_reset_session_only);
        if (!s) _compress_error_return("ZSTD_createCCtx()");
        ret=ZSTD_CCtx_setParameter(s,ZSTD_c_compressionLevel,plugin->compress_level);
        if (ZSTD_isError(ret)) _compress_error_return("ZSTD_CCtx_setParameter(,ZSTD_c_compressionLevel)");
        ZSTD_CCtx_setPledgedSrcSize(s,in_data->streamSize);
        #define _ZSTD_WINDOWLOG_MIN 10
        dict_bits=plugin->dict_bits;
        while (((((hpatch_StreamPos_t)1)<<(dict_bits-1)) >= in_data->streamSize)
                &&((dict_bits-1)>=_ZSTD_WINDOWLOG_MIN)) {
            --dict_bits;
        }
        ret=ZSTD_CCtx_setParameter(s,ZSTD_c_windowLog,dict_bits);
        if (ZSTD_isError(ret)) _compress_error_return("ZSTD_CCtx_setParameter(,ZSTD_c_windowLog)");
        if (plugin->thread_num>1){
            ret=ZSTD_CCtx_setParameter(s, ZSTD_c_nbWorkers,plugin->thread_num);
            //if (ZSTD_isError(ret)) printf("  (NOTICE: zstd unsupport multi-threading, warning.)\n");
        }

        for (;;){
            if (readFromPos<in_data->streamSize){
                s_input.pos=0;
                if (s_input.size>(in_data->streamSize-readFromPos))
                    s_input.size=(size_t)(in_data->streamSize-readFromPos);
                if (!in_data->read(in_data,readFromPos,(unsigned char*)s_input.src,
                                (unsigned char*)s_input.src+s_input.size))
                    _compress_error_return("in_data->read()");
                readFromPos+=s_input.size;
            }
            {
                const int lastChunk=(readFromPos==in_data->streamSize);
                const ZSTD_EndDirective mode=lastChunk?ZSTD_e_end:ZSTD_e_continue;
                int finished;
                do {
                    s_output.pos=0;
                    ret=ZSTD_compressStream2(s,&s_output,&s_input,mode);
                    if (ZSTD_isError(ret)) _compress_error_return("ZSTD_compressStream2()");
                    if (s_output.pos>0){
                        _stream_out_code_write(out_code,outStream_isCanceled,result,
                                            (const unsigned char*)s_output.dst,s_output.pos);
                    }
                    finished=lastChunk?(ret==0):(s_input.pos==s_input.size);
                } while (!finished);
                //if (s_input.pos!=s_input.size) _compress_error_return("Impossible: zstd only returns 0 when the input is completely consumed!");
                if (lastChunk)
                    break;
            }
        }
    clear:
#if (!IS_REUSE_compress_handle)
        if (0!=ZSTD_freeCCtx(s))
        { s=0; result=kCompressFailResult; if (strlen(errAt)==0) errAt="ZSTD_freeCStream()"; }
#endif
        _check_compress_result(result,outStream_isCanceled,"_zstd_compress()",errAt);
        if (_temp_buf) free(_temp_buf);
        return result;
    }
    _def_fun_compressType(_zstd_compressType,"zstd");
    static TCompressPlugin_zstd zstdCompressPlugin={
        {_zstd_compressType,_default_maxCompressedSize,_zstd_setThreadNumber,_zstd_compress},
        20,24,kDefaultCompressThreadNumber};
#endif//_CompressPlugin_zstd


#ifdef  _CompressPlugin_brotli
#if (_IsNeedIncludeDefaultCompressHead)
#   include "brotli/encode.h" // "brotli/c/include/brotli/encode.h" https://github.com/google/brotli
#endif
    struct TCompressPlugin_brotli{
        hdiff_TCompress base;
        int             compress_level; //0..11
        int             dict_bits;  // 10..30
    };
    static hpatch_StreamPos_t _brotli_compress(const hdiff_TCompress* compressPlugin,
                                               const hpatch_TStreamOutput* out_code,
                                               const hpatch_TStreamInput*  in_data){
        const TCompressPlugin_brotli* plugin=(const TCompressPlugin_brotli*)compressPlugin;
        hpatch_StreamPos_t  result=0;
        const char*         errAt="";
        BrotliEncoderState* s=0;
        hpatch_StreamPos_t  readFromPos=0;
        int                 outStream_isCanceled=0;
        uint8_t*        _temp_buf=0;
        const size_t    kBufSize=kCompressBufSize;
        uint8_t*        input;
        uint8_t*        output;
        size_t          available_in;
        size_t          available_out;
        const uint8_t*  next_in;
        uint8_t*        next_out;
        
        _temp_buf=(uint8_t*)malloc(kBufSize*2);
        if (!_temp_buf) _compress_error_return("memory alloc");
        input=_temp_buf;
        output=_temp_buf+kBufSize;
        available_in=0;
        available_out=kBufSize;
        next_out=output;
        next_in=input;

        if (!s) s=BrotliEncoderCreateInstance(0,0,0);
        if (!s) _compress_error_return("BrotliEncoderCreateInstance()");
        if (!BrotliEncoderSetParameter(s,BROTLI_PARAM_QUALITY,plugin->compress_level))
            _compress_error_return("BrotliEncoderSetParameter()");
        {
            uint32_t lgwin = plugin->dict_bits;
            #define BROTLI_WINDOW_GAP 16
            #define BROTLI_MAX_BACKWARD_LIMIT(W) (((hpatch_StreamPos_t)1 << (W)) - BROTLI_WINDOW_GAP)
            while ((BROTLI_MAX_BACKWARD_LIMIT(lgwin-1) >= in_data->streamSize)
                    &&((lgwin-1)>= BROTLI_MIN_WINDOW_BITS)) {
                --lgwin;
            }
            if (lgwin > BROTLI_MAX_WINDOW_BITS)
                BrotliEncoderSetParameter(s, BROTLI_PARAM_LARGE_WINDOW, 1u);
            BrotliEncoderSetParameter(s, BROTLI_PARAM_LGWIN, lgwin);
        }
        if (in_data->streamSize > 0) {
            uint32_t size_hint = in_data->streamSize < (1 << 30) ?
                (uint32_t)in_data->streamSize : (1u << 30);
            BrotliEncoderSetParameter(s, BROTLI_PARAM_SIZE_HINT, size_hint);
        }
        
        while (1) {
            int s_isFinished;
            if ((available_in==0)&&(readFromPos<in_data->streamSize)){
                available_in=kBufSize;
                if (available_in>(in_data->streamSize-readFromPos))
                    available_in=(size_t)(in_data->streamSize-readFromPos);
                if (!in_data->read(in_data,readFromPos,input,input+available_in))
                    _compress_error_return("in_data->read()");
                readFromPos+=available_in;
                next_in=input;
            }

            if (!BrotliEncoderCompressStream(s,
                (readFromPos==in_data->streamSize) ? BROTLI_OPERATION_FINISH : BROTLI_OPERATION_PROCESS,
                &available_in, &next_in, &available_out, &next_out, 0))
                    _compress_error_return("BrotliEncoderCompressStream()");

            s_isFinished=BrotliEncoderIsFinished(s);
            if ((available_out == 0)||s_isFinished) {                
                _stream_out_code_write(out_code,outStream_isCanceled,result,
                                       output,kBufSize-available_out);
                next_out=output;
                available_out=kBufSize;
            }

            if (s_isFinished)
                break;
        }
    clear:
        BrotliEncoderDestroyInstance(s);
        _check_compress_result(result,outStream_isCanceled,"_brotli_compress()",errAt);
        if (_temp_buf) free(_temp_buf);
        return result;
    }
    _def_fun_compressType(_brotli_compressType,"brotli");
    static TCompressPlugin_brotli brotliCompressPlugin={
        {_brotli_compressType,_default_maxCompressedSize,_default_setParallelThreadNumber,_brotli_compress}, 9,24};
#endif//_CompressPlugin_brotli


#ifdef  _CompressPlugin_lzham
#if (_IsNeedIncludeDefaultCompressHead)
#   include "lzham.h" // "lzham_codec/include/lzham.h" https://github.com/richgel999/lzham_codec
#endif
    struct TCompressPlugin_lzham{
        hdiff_TCompress base;
        int             compress_level; //0..5
        int             dict_bits;  // 15..(26 or 29)
        int             thread_num; //1..(64?)
    };

    static int _lzham_setParallelThreadNumber(hdiff_TCompress* compressPlugin,int threadNum){
        TCompressPlugin_lzham* plugin=(TCompressPlugin_lzham*)compressPlugin;
        if (threadNum>LZHAM_MAX_HELPER_THREADS) threadNum=LZHAM_MAX_HELPER_THREADS;
        plugin->thread_num=threadNum;
        return threadNum;
    }

    static hpatch_StreamPos_t _lzham_compress(const hdiff_TCompress* compressPlugin,
                                              const hpatch_TStreamOutput* out_code,
                                              const hpatch_TStreamInput*  in_data){
        const TCompressPlugin_lzham* plugin=(const TCompressPlugin_lzham*)compressPlugin;
        hpatch_StreamPos_t  result=0;
        const char*         errAt="";
        lzham_compress_params    params;
        unsigned char            dict_bits;
        lzham_compress_state_ptr s=0;
        hpatch_StreamPos_t  readFromPos=0;
        int                 outStream_isCanceled=0;
        unsigned char*      _temp_buf=0;
        const size_t        kBufSize=kCompressBufSize;
        unsigned char*          input;
        unsigned char*          output;
        size_t                  available_in;
        size_t                  available_out;
        const unsigned char*    next_in;
        unsigned char*          next_out;
        
        _temp_buf=(unsigned char*)malloc(kBufSize*2);
        if (!_temp_buf) _compress_error_return("memory alloc");
        input=_temp_buf;
        output=_temp_buf+kBufSize;
        available_in=0;
        available_out=kBufSize;
        next_out=output;
        next_in=input;

        memset(&params, 0, sizeof(params));
        params.m_struct_size=sizeof(params);
        dict_bits= (unsigned char)plugin->dict_bits;
         while (((((hpatch_StreamPos_t)1)<<(dict_bits-1)) >= in_data->streamSize)
                &&((dict_bits-1)>=LZHAM_MIN_DICT_SIZE_LOG2)) {
            --dict_bits;
        }
        params.m_dict_size_log2 = dict_bits;
        if (plugin->compress_level<=LZHAM_COMP_LEVEL_UBER){
            params.m_level=(lzham_compress_level)plugin->compress_level;
        }else{
            params.m_level = LZHAM_COMP_LEVEL_UBER;
            params.m_compress_flags |= LZHAM_COMP_FLAG_EXTREME_PARSING;
        }
        if (plugin->thread_num>1)
            params.m_max_helper_threads = plugin->thread_num;

        if (!s) s=lzham_compress_init(&params);
        if (!s) _compress_error_return("lzham_compress_init()");
                
        {//save head
            if (!out_code->write(out_code,0,(&dict_bits),(&dict_bits)+1))
                _compress_error_return("out_code->write()");
            ++result;
        }

        while (1) {
            lzham_compress_status_t ret;
            int s_isFinished;
            if ((available_in==0)&&(readFromPos<in_data->streamSize)){
                available_in=kBufSize;
                if (available_in>(in_data->streamSize-readFromPos))
                    available_in=(size_t)(in_data->streamSize-readFromPos);
                if (!in_data->read(in_data,readFromPos,input,input+available_in))
                    _compress_error_return("in_data->read()");
                readFromPos+=available_in;
                next_in=input;
            }

            {
                size_t available_in_back=available_in;
                size_t available_out_back=available_out;
                ret=lzham_compress2(s,next_in,&available_in,next_out,&available_out,
                                    (readFromPos==in_data->streamSize) ? LZHAM_FINISH : LZHAM_NO_FLUSH);
                next_out+=available_out;
                next_in+=available_in;
                available_in=available_in_back-available_in;
                available_out=available_out_back-available_out;
            }
            if (ret>=LZHAM_COMP_STATUS_FIRST_FAILURE_CODE)
                _compress_error_return("lzham_compress2()");

            s_isFinished=(ret==LZHAM_COMP_STATUS_SUCCESS);
            if ((ret==LZHAM_COMP_STATUS_HAS_MORE_OUTPUT)||(available_out == 0)||s_isFinished) {                
                _stream_out_code_write(out_code,outStream_isCanceled,result,
                                       output,kBufSize-available_out);
                next_out=output;
                available_out=kBufSize;
            }

            if (s_isFinished)
                break;
        }
    clear:
        lzham_compress_deinit(s);
        _check_compress_result(result,outStream_isCanceled,"_lzham_compress()",errAt);
        if (_temp_buf) free(_temp_buf);
        return result;
    }
    _def_fun_compressType(_lzham_compressType,"lzham");
    static TCompressPlugin_lzham lzhamCompressPlugin={
        {_lzham_compressType,_default_maxCompressedSize,_lzham_setParallelThreadNumber,_lzham_compress},
         LZHAM_COMP_LEVEL_BETTER,24,kDefaultCompressThreadNumber};
#endif//_CompressPlugin_lzham

#ifdef _CompressPlugin_tuz
#if (_IsNeedIncludeDefaultCompressHead)
#   include "tuz_enc.h" // "tinyuz/compress/tuz_enc.h" https://github.com/sisong/tinyuz
#endif
    typedef struct TCompressPlugin_tuz{
        hdiff_TCompress     base;
        tuz_TCompressProps  props;
    } TCompressPlugin_tuz;
    static int _tuz_setParallelThreadNumber(hdiff_TCompress* compressPlugin,int threadNum){
        TCompressPlugin_tuz* plugin=(TCompressPlugin_tuz*)compressPlugin;
        plugin->props.threadNum=threadNum;
        return threadNum;
    }
    static hpatch_StreamPos_t _tuz_compress(const hdiff_TCompress* compressPlugin,
                                            const hpatch_TStreamOutput* out_code,
                                            const hpatch_TStreamInput*  in_data){
        const TCompressPlugin_tuz* plugin=(const TCompressPlugin_tuz*)compressPlugin;
        tuz_TCompressProps props=plugin->props;
        if (props.dictSize>=in_data->streamSize)
            props.dictSize=(in_data->streamSize>1)?(tuz_size_t)(in_data->streamSize-1):1;
#       if (IS_NOTICE_compress_canceled)
        printf("  (used one tinyuz dictSize: %" PRIu64 "  (input data: %" PRIu64 "))\n",
               (hpatch_StreamPos_t)props.dictSize,in_data->streamSize);
#       endif
        return tuz_compress(out_code,in_data,&props);
    }
    _def_fun_compressType(_tuz_compressType,"tuz");
    static const TCompressPlugin_tuz tuzCompressPlugin={
        {_tuz_compressType,_default_maxCompressedSize,_tuz_setParallelThreadNumber,_tuz_compress},
            {(1<<24),tuz_kMaxOfMaxSaveLength,kDefaultCompressThreadNumber}};
#endif //_CompressPlugin_tuz

#ifdef __cplusplus
}
#endif
#endif
