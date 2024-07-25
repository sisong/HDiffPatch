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
//  zlibDecompressPlugin;   // support all deflate encoding by zlib
//  ldefDecompressPlugin;   // optimized deompress speed for deflate encoding
//  bz2DecompressPlugin;
//  lzmaDecompressPlugin;
//  lzma2DecompressPlugin;
//  lz4DecompressPlugin;
//  zstdDecompressPlugin;
//  brotliDecompressPlugin;
//  lzhamDecompressPlugin;
//  tuzDecompressPlugin;

// _bz2DecompressPlugin_unsz  : support for bspatch_with_cache()
// _7zXZDecompressPlugin      : support for vcpatch_with_cache(), diffData created by "xdelta3 -S lzma ..."
// _7zXZDecompressPlugin_a    : support for vcpatch_with_cache(), diffData created by "hdiffz -VCD-compressLevel ..."
#include <stdlib.h> //malloc free
#include <stdio.h>  //fprintf
#include "libHDiffPatch/HPatch/patch_types.h"

#ifndef kDecompressBufSize
#   define kDecompressBufSize (1024*32)
#endif
#ifndef _IsNeedIncludeDefaultCompressHead
#   define _IsNeedIncludeDefaultCompressHead 1
#endif

#define _dec_memErr()       _hpatch_update_decError(decompressPlugin,hpatch_dec_mem_error)
#define _dec_memErr_rt()    do { _dec_memErr(); return 0; } while(0)
#define _dec_openErr_rt()   do { _hpatch_update_decError(decompressPlugin,hpatch_dec_open_error); return 0; } while(0)
#define _dec_close_check(value) { if (!(value)) { LOG_ERR("check "#value " ERROR!\n"); \
                                    result=hpatch_FALSE; _hpatch_update_decError(decompressPlugin,hpatch_dec_close_error); } }

#define _dec_onDecErr_rt()  do { if (!(self)->decError) (self)->decError=hpatch_dec_error;  return 0; } while(0)
#define _dec_onDecErr_up()  do { if ((self)->decError) _hpatch_update_decError(decompressPlugin,(self)->decError); } while(0)

static void* _dec_malloc(hpatch_size_t size) {
    void* result=malloc(size);
    if (!result) LOG_ERRNO(errno);
    return result;
}
#define __dec_Alloc_fun(_type_TDecompress,p,size) {  \
    void* result=_dec_malloc(size); \
    if (!result)    \
        ((_type_TDecompress*)p)->decError=hpatch_dec_mem_error;   \
    return result;  }

static void __dec_free(void* _, void* address){
    if (address) free(address); }

#ifdef  _CompressPlugin_zlib
#if (_IsNeedIncludeDefaultCompressHead)
#   include "zlib.h" // http://zlib.net/  https://github.com/madler/zlib
#endif
    typedef struct _zlib_TDecompress{
        hpatch_StreamPos_t code_begin;
        hpatch_StreamPos_t code_end;
        const struct hpatch_TStreamInput* codeStream;
        
        unsigned char*  dec_buf;
        size_t          dec_buf_size;
        z_stream        d_stream;
        signed char     windowBits;
        hpatch_dec_error_t  decError;
    } _zlib_TDecompress;
    static void * __zlib_dec_Alloc(void* p,uInt items,uInt size) 
        __dec_Alloc_fun(_zlib_TDecompress,p,((items)*(size_t)(size)))
    static hpatch_BOOL _zlib_is_can_open(const char* compressType){
        return (0==strcmp(compressType,"zlib"))||(0==strcmp(compressType,"pzlib"));
    }

    static _zlib_TDecompress*  _zlib_decompress_open_at(hpatch_TDecompress* decompressPlugin,
                                                        const hpatch_TStreamInput* codeStream,
                                                        hpatch_StreamPos_t code_begin,
                                                        hpatch_StreamPos_t code_end,
                                                        int  isSavedWindowBits,
                                                        _zlib_TDecompress* self,size_t _self_and_buf_size){
        int ret;
        signed char kWindowBits=-MAX_WBITS;
        assert(_self_and_buf_size>sizeof(_zlib_TDecompress));
        if (isSavedWindowBits){//load kWindowBits
            if (code_end-code_begin<1) _dec_openErr_rt();
            if (!codeStream->read(codeStream,code_begin,(unsigned char*)&kWindowBits,
                                  (unsigned char*)&kWindowBits+1)) return 0;
            ++code_begin;
        }
        
        memset(self,0,sizeof(_zlib_TDecompress));
        self->dec_buf=((unsigned char*)self)+sizeof(_zlib_TDecompress);
        self->dec_buf_size=_self_and_buf_size-sizeof(_zlib_TDecompress);
        self->codeStream=codeStream;
        self->code_begin=code_begin;
        self->code_end=code_end;
        self->windowBits=kWindowBits;
        self->d_stream.zalloc=__zlib_dec_Alloc;
        self->d_stream.zfree=__dec_free;
        self->d_stream.opaque=self;
        ret = inflateInit2(&self->d_stream,self->windowBits);
        if (ret!=Z_OK) { _dec_onDecErr_up(); _dec_openErr_rt(); }
        return self;
    }
    static hpatch_decompressHandle  _zlib_decompress_open(hpatch_TDecompress* decompressPlugin,
                                                          hpatch_StreamPos_t dataSize,
                                                          const hpatch_TStreamInput* codeStream,
                                                          hpatch_StreamPos_t code_begin,
                                                          hpatch_StreamPos_t code_end){
        _zlib_TDecompress* self=0;
        unsigned char* _mem_buf=(unsigned char*)_dec_malloc(sizeof(_zlib_TDecompress)+kDecompressBufSize);
        if (!_mem_buf) _dec_memErr_rt();
        self=_zlib_decompress_open_at(decompressPlugin,codeStream,code_begin,code_end,1,
                                      (_zlib_TDecompress*)_mem_buf,sizeof(_zlib_TDecompress)+kDecompressBufSize);
        if (!self)
            free(_mem_buf);
        return self;
    }
    static hpatch_decompressHandle  _zlib_decompress_open_deflate(hpatch_TDecompress* decompressPlugin,
                                                                  hpatch_StreamPos_t dataSize,
                                                                  const hpatch_TStreamInput* codeStream,
                                                                  hpatch_StreamPos_t code_begin,
                                                                  hpatch_StreamPos_t code_end){
        _zlib_TDecompress* self=0;
        unsigned char* _mem_buf=(unsigned char*)_dec_malloc(sizeof(_zlib_TDecompress)+kDecompressBufSize);
        if (!_mem_buf) _dec_memErr_rt();
        self=_zlib_decompress_open_at(decompressPlugin,codeStream,code_begin,code_end,0,
                                      (_zlib_TDecompress*)_mem_buf,sizeof(_zlib_TDecompress)+kDecompressBufSize);
        if (!self)
            free(_mem_buf);
        return self;
    }

    static _zlib_TDecompress*  _zlib_decompress_open_by(hpatch_TDecompress* decompressPlugin,
                                                        const hpatch_TStreamInput* codeStream,
                                                        hpatch_StreamPos_t code_begin,
                                                        hpatch_StreamPos_t code_end,
                                                        int  isSavedWindowBits,
                                                        unsigned char* _mem_buf,size_t _mem_buf_size){
        #define __MAX_TS(a,b)  ((a)>=(b)?(a):(b))
        const hpatch_size_t kZlibAlign=__MAX_TS(__MAX_TS(sizeof(hpatch_StreamPos_t),sizeof(void*)),sizeof(uLongf));
        #undef __MAX_TS
        unsigned char* _mem_buf_end=_mem_buf+_mem_buf_size;
        unsigned char* self_at=(unsigned char*)_hpatch_align_upper(_mem_buf,kZlibAlign);
        if (self_at>=_mem_buf_end) return 0;
        return _zlib_decompress_open_at(decompressPlugin,codeStream,code_begin,code_end,isSavedWindowBits,
                                        (_zlib_TDecompress*)self_at,_mem_buf_end-self_at);
    }
    static hpatch_BOOL _zlib_decompress_close_by(struct hpatch_TDecompress* decompressPlugin,
                                                 _zlib_TDecompress* self){
        hpatch_BOOL result=hpatch_TRUE;
        if (!self) return result;
        _dec_onDecErr_up();
        if (self->d_stream.state!=0){
            _dec_close_check(Z_OK==inflateEnd(&self->d_stream));
        }
        memset(self,0,sizeof(_zlib_TDecompress));
        return result;
    }
    
    static hpatch_BOOL _zlib_decompress_close(struct hpatch_TDecompress* decompressPlugin,
                                              hpatch_decompressHandle decompressHandle){
        _zlib_TDecompress* self=(_zlib_TDecompress*)decompressHandle;
        hpatch_BOOL result=_zlib_decompress_close_by(decompressPlugin,self);
        if (self) free(self);
        return result;
    }

        static hpatch_BOOL _zlib_reset_for_next_node(z_stream* d_stream){
            //backup
            Bytef*   next_out_back=d_stream->next_out;
            Bytef*   next_in_back=d_stream->next_in;
            unsigned int avail_out_back=d_stream->avail_out;
            unsigned int avail_in_back=d_stream->avail_in;
                //reset
            if (Z_OK!=inflateReset(d_stream)) return hpatch_FALSE;
                //restore
            d_stream->next_out=next_out_back;
            d_stream->next_in=next_in_back;
            d_stream->avail_out=avail_out_back;
            d_stream->avail_in=avail_in_back;
            return hpatch_TRUE;
        }
    static hpatch_BOOL __zlib_do_inflate(hpatch_decompressHandle decompressHandle){
        _zlib_TDecompress* self=(_zlib_TDecompress*)decompressHandle;
        uInt avail_out_back,avail_in_back;
        int ret;
        hpatch_StreamPos_t codeLen=(self->code_end - self->code_begin);
        if ((self->d_stream.avail_in==0)&&(codeLen>0)) {
            size_t readLen=self->dec_buf_size;
            if (readLen>codeLen) readLen=(size_t)codeLen;
            self->d_stream.next_in=self->dec_buf;
            if (!self->codeStream->read(self->codeStream,self->code_begin,self->dec_buf,
                                        self->dec_buf+readLen)) return hpatch_FALSE;//error;
            self->d_stream.avail_in=(uInt)readLen;
            self->code_begin+=readLen;
            codeLen-=readLen;
        }
        
        avail_out_back=self->d_stream.avail_out;
        avail_in_back=self->d_stream.avail_in;
        ret=inflate(&self->d_stream,Z_NO_FLUSH);
        if (ret==Z_OK){
            if ((self->d_stream.avail_in==avail_in_back)&&(self->d_stream.avail_out==avail_out_back))
                _dec_onDecErr_rt();//error;
        }else if (ret==Z_STREAM_END){
            if (self->d_stream.avail_in+codeLen>0){ //next compress node!
                if (!_zlib_reset_for_next_node(&self->d_stream))
                    _dec_onDecErr_rt();//error;
            }else{//all end
                if (self->d_stream.avail_out!=0)
                    _dec_onDecErr_rt();//error;
            }
        }else{
            _dec_onDecErr_rt();//error;
        }
        return hpatch_TRUE;
    }
    static hpatch_BOOL _zlib_decompress_part(hpatch_decompressHandle decompressHandle,
                                             unsigned char* out_part_data,unsigned char* out_part_data_end){
        _zlib_TDecompress* self=(_zlib_TDecompress*)decompressHandle;
        assert(out_part_data<=out_part_data_end);
        
        self->d_stream.next_out = out_part_data;
        self->d_stream.avail_out =(uInt)(out_part_data_end-out_part_data);
        while (self->d_stream.avail_out>0) {
            if (!__zlib_do_inflate(self))
                return hpatch_FALSE;//error;
        }
        return hpatch_TRUE;
    }
    static hpatch_inline int _zlib_is_decompress_finish(const hpatch_TDecompress* decompressPlugin,
                                                        hpatch_decompressHandle decompressHandle){
        _zlib_TDecompress* self=(_zlib_TDecompress*)decompressHandle;
        while (self->code_begin!=self->code_end){ //for end tag code
            unsigned char _empty=0;
            self->d_stream.next_out = &_empty;
            self->d_stream.avail_out=0;
            if (!__zlib_do_inflate(self))
                return hpatch_FALSE;//error;
        }
        return   (self->code_begin==self->code_end)
                &(self->d_stream.avail_in==0)
                &(self->d_stream.avail_out==0);
    }
    static hpatch_TDecompress zlibDecompressPlugin={_zlib_is_can_open,_zlib_decompress_open,
                                                    _zlib_decompress_close,_zlib_decompress_part};
    static hpatch_TDecompress zlibDecompressPlugin_deflate={_zlib_is_can_open,_zlib_decompress_open_deflate,
                                                    _zlib_decompress_close,_zlib_decompress_part};
#endif//_CompressPlugin_zlib


#ifdef  _CompressPlugin_ldef
#if (_IsNeedIncludeDefaultCompressHead)
#   include "libdeflate.h" // https://github.com/sisong/libdeflate/tree/stream-mt based on https://github.com/ebiggers/libdeflate
#   if (_CompressPlugin_ldef_is_use_zlib)
#       include "zlib.h" // https://github.com/sisong/zlib/tree/bit_pos_padding based on https://github.com/madler/zlib
#   endif
#endif
    static const size_t _de_ldef_kDictSize = 1024*32;
    static const size_t _de_ldef_kMaxBlockSize =1024*32*19; 
    // used _de_ldef_kMaxBlockSize*2 memory; optimized speed for
    //   libdefalte & zlib's deflate code ..., when theirs input deflate code compress block size<=_de_ldef_kMaxBlockSize/2;
    // if (_de_ldef_kMaxBlockSize>=compress block size>_de_ldef_kMaxBlockSize/2) speed will little slower;
    // if (compress block size>_de_ldef_kMaxBlockSize) && if (_CompressPlugin_ldef_is_use_zlib!=0),
    //   then swap to zlib decompressor & very slower (slower than zlib); && if (_CompressPlugin_ldef_is_use_zlib==0) will decompress fail. 
 
    typedef struct _ldef_TDecompress{
        hpatch_StreamPos_t code_begin;
        hpatch_StreamPos_t code_end;
        const struct hpatch_TStreamInput* codeStream;
        
        unsigned char*  data_buf;
        size_t          data_buf_size;
        unsigned char*  code_buf;
        size_t          code_buf_size;
        struct libdeflate_decompressor* d;
        hpatch_dec_error_t  decError;

        size_t      data_cur;
        size_t      out_cur;
        size_t      code_cur;
    #if (_CompressPlugin_ldef_is_use_zlib)
        hpatch_BOOL is_swap_to_zlib;
        z_stream    d_stream;
    #endif
    } _ldef_TDecompress;

  #if (_CompressPlugin_ldef_is_use_zlib)
    static hpatch_inline hpatch_BOOL _ldef_is_swap_to_zlib(_ldef_TDecompress* self){
        return self->is_swap_to_zlib;
    }
        static hpatch_BOOL _ldef_decompress_part(hpatch_decompressHandle decompressHandle,
                                                 unsigned char* out_part_data,unsigned char* out_part_data_end);
    static hpatch_BOOL _ldef_swap_from_zlib(_ldef_TDecompress* self){
        const size_t out_part_len=self->d_stream.avail_out;
        self->is_swap_to_zlib=hpatch_FALSE;
        libdeflate_deflate_decompress_block_reset(self->d);
        {
            unsigned long shift_v;
            unsigned int shift_bit;
            zlib_inflate_shift_value(&self->d_stream,&shift_v,&shift_bit);
            if (shift_bit>>3) _dec_onDecErr_rt();
            libdeflate_deflate_decompress_set_state(self->d,(shift_v<<3)|shift_bit);
        }
        assert(self->data_cur==_de_ldef_kDictSize);
        self->code_cur=self->code_buf_size-self->d_stream.avail_in;
        assert(self->d_stream.next_in==self->code_buf+self->code_cur);
        {
            uInt dict_size=0;
            if (Z_OK!=inflateGetDictionary(&self->d_stream,0,&dict_size)) _dec_onDecErr_rt();
            if (Z_OK!=inflateGetDictionary(&self->d_stream,self->data_buf+self->data_cur-dict_size,0)) _dec_onDecErr_rt();
        }
        if (out_part_len>0)
            return _ldef_decompress_part(self,self->d_stream.next_out,self->d_stream.next_out+out_part_len);
        else
            return hpatch_TRUE;
    }
    static hpatch_BOOL _ldef_decompress_part_by_zlib(_ldef_TDecompress* self,
                                                     unsigned char* out_part_data,size_t out_part_len){
        self->d_stream.next_out=out_part_data;
        self->d_stream.avail_out=(uInt)out_part_len;
        while (self->d_stream.avail_out){
            uInt avail_out_back,avail_in_back;
            int ret;
            hpatch_StreamPos_t codeLen=(self->code_end-self->code_begin);
            if ((self->d_stream.avail_in==0)&&(codeLen>0)) {
                size_t readLen=self->code_buf_size;
                if (readLen>codeLen) readLen=(size_t)codeLen;
                self->d_stream.next_in=self->code_buf+self->code_buf_size-readLen;
                if (!self->codeStream->read(self->codeStream,self->code_begin,self->d_stream.next_in,
                                            self->d_stream.next_in+readLen)) return hpatch_FALSE;//error;
                self->d_stream.avail_in=(uInt)readLen;
                self->code_begin+=readLen;
                codeLen-=readLen;
            }

            avail_out_back=self->d_stream.avail_out;
            avail_in_back=self->d_stream.avail_in;
            ret=inflate(&self->d_stream,Z_BLOCK);
            if (ret==Z_OK){
                if ((self->d_stream.avail_in==avail_in_back)&&(self->d_stream.avail_out==avail_out_back))
                    _dec_onDecErr_rt();//error;
                if (zlib_inflate_is_block_end(&self->d_stream)&&(!zlib_inflate_is_last_block_end(&self->d_stream)))
                    return _ldef_swap_from_zlib(self);
            }else if (ret==Z_STREAM_END){
                if (self->d_stream.avail_in+codeLen>0){ //next compress node!
                    zlib_inflate_set_shift_value(&self->d_stream,0,0);
                    return _ldef_swap_from_zlib(self);
                }else{//all end
                    if (self->d_stream.avail_out!=0)
                        _dec_onDecErr_rt();//error;
                }
            }else{
                _dec_onDecErr_rt();//error;
            }
        }
        return hpatch_TRUE;
    }
    static hpatch_BOOL _ldef_swap_to_zlib(_ldef_TDecompress* self,size_t dec_state,
                                          unsigned char* out_part_data,size_t out_part_len){
        self->is_swap_to_zlib=hpatch_TRUE;
        if (self->d_stream.state==0){
            if (Z_OK!=inflateInit2(&self->d_stream,-15)){
                if (!self->decError) self->decError=hpatch_dec_open_error;
                return hpatch_FALSE;
            }
        }else{
            if (Z_OK!=inflateReset(&self->d_stream)) _dec_onDecErr_rt();
        }
        zlib_inflate_set_shift_value(&self->d_stream,(uLong)(dec_state>>3),(uInt)(dec_state&((1<<3)-1)));
        if (Z_OK!=inflateSetDictionary(&self->d_stream,self->data_buf+self->data_cur-_de_ldef_kDictSize,(uInt)_de_ldef_kDictSize)) _dec_onDecErr_rt();
        self->d_stream.next_in=self->code_buf+self->code_cur;
        self->d_stream.avail_in=(uInt)(self->code_buf_size-self->code_cur);
        return _ldef_decompress_part_by_zlib(self,out_part_data,out_part_len);
    }
  #endif //_CompressPlugin_ldef_is_use_zlib

    static hpatch_BOOL _ldef_is_can_open(const char* compressType){
        return (0==strcmp(compressType,"zlib"))||(0==strcmp(compressType,"pzlib"));
    }

    static _ldef_TDecompress*  _ldef_decompress_open_at(hpatch_TDecompress* decompressPlugin,
                                                        const hpatch_TStreamInput* codeStream,
                                                        hpatch_StreamPos_t code_begin,hpatch_StreamPos_t code_end,
                                                        int  isSavedWindowBits,_ldef_TDecompress* self,
                                                        size_t data_buf_size,size_t code_buf_size){
        if (isSavedWindowBits){//load kWindowBits
            signed char kWindowBits=0;
            if (code_end-code_begin<1) _dec_openErr_rt();
            if (!codeStream->read(codeStream,code_begin,(unsigned char*)&kWindowBits,
                                  (unsigned char*)&kWindowBits+1)) return 0;
            ++code_begin;
            if (!((-15<=kWindowBits)&&(kWindowBits<0))) 
                _dec_openErr_rt(); //now, unsupported these window bits
        }
        
        memset(self,0,sizeof(_ldef_TDecompress));
        self->d=libdeflate_alloc_decompressor();
        if (!self->d) _dec_memErr_rt();
        self->data_buf=((unsigned char*)self)+sizeof(_ldef_TDecompress);
        self->data_buf_size=data_buf_size;
        self->code_buf=self->data_buf+data_buf_size;
        self->code_buf_size=code_buf_size;
        self->codeStream=codeStream;
        self->code_begin=code_begin;
        self->code_end=code_end;

        self->data_cur=_de_ldef_kDictSize; //empty
        self->out_cur=_de_ldef_kDictSize;
        self->code_cur=code_buf_size; //empty
        return self;
    }
   
    static hpatch_decompressHandle  _ldef_decompress_open(hpatch_TDecompress* decompressPlugin,
                                                          hpatch_StreamPos_t dataSize,
                                                          const hpatch_TStreamInput* codeStream,
                                                          hpatch_StreamPos_t code_begin,
                                                          hpatch_StreamPos_t code_end){
        _ldef_TDecompress* self=0;
        const hpatch_StreamPos_t in_size=code_end-code_begin;
        const size_t data_buf_size=_de_ldef_kDictSize+((_de_ldef_kMaxBlockSize<dataSize)?_de_ldef_kMaxBlockSize:(size_t)dataSize);
        const size_t code_buf_size=(_de_ldef_kMaxBlockSize<in_size)?_de_ldef_kMaxBlockSize:(size_t)in_size;
        size_t _mem_size=sizeof(_ldef_TDecompress)+data_buf_size+code_buf_size;
        unsigned char* _mem_buf=(unsigned char*)_dec_malloc(_mem_size);
        if (!_mem_buf) _dec_memErr_rt();

        self=_ldef_decompress_open_at(decompressPlugin,codeStream,code_begin,code_end,1,
                                      (_ldef_TDecompress*)_mem_buf,data_buf_size,code_buf_size);
        if (!self)
            free(_mem_buf);
        return self;
    }

    static hpatch_BOOL _ldef_decompress_close_by(struct hpatch_TDecompress* decompressPlugin,
                                                 _ldef_TDecompress* self){
        hpatch_BOOL result=hpatch_TRUE;
        if (!self) return result;
        _dec_onDecErr_up();
    #if (_CompressPlugin_ldef_is_use_zlib)
        if (self->d_stream.state!=0){
            _dec_close_check(Z_OK==inflateEnd(&self->d_stream));
        }
    #endif
        if (self->d!=0)
            libdeflate_free_decompressor(self->d);
        memset(self,0,sizeof(_ldef_TDecompress));
        return result;
    }
    
    static hpatch_BOOL _ldef_decompress_close(struct hpatch_TDecompress* decompressPlugin,
                                              hpatch_decompressHandle decompressHandle){
        _ldef_TDecompress* self=(_ldef_TDecompress*)decompressHandle;
        hpatch_BOOL result=_ldef_decompress_close_by(decompressPlugin,self);
        if (self) free(self);
        return result;
    }

    static hpatch_BOOL _ldef_decompress_part(hpatch_decompressHandle decompressHandle,
                                             unsigned char* out_part_data,unsigned char* out_part_data_end){
        _ldef_TDecompress* self=(_ldef_TDecompress*)decompressHandle;
        size_t out_part_len=out_part_data_end-out_part_data;
    #if (_CompressPlugin_ldef_is_use_zlib)
        if (_ldef_is_swap_to_zlib(self)&&out_part_len)
            return _ldef_decompress_part_by_zlib(self,out_part_data,out_part_len);
    #endif
        const size_t kDictSize=_de_ldef_kDictSize;
        //     [ ( dict ) |     dataBuf                 ]              [            codeBuf              ]
        //     ^              ^         ^               ^              ^                     ^           ^
        //  data_buf       out_cur   data_cur     data_buf_size     code_buf              code_cur code_buf_size
        while (out_part_len) {
            {//write data out
                size_t out_len=self->data_cur-self->out_cur;
                if (out_len){ //have data
                    out_len=(out_len<out_part_len)?out_len:out_part_len;
                    memcpy(out_part_data,self->data_buf+self->out_cur,out_len);
                    self->out_cur+=out_len;
                    out_part_data+=out_len;
                    out_part_len-=out_len;
                    continue;
                }
            }

            size_t kLimitDataSize=_de_ldef_kMaxBlockSize/2+kDictSize;
            size_t kLimitCodeSize=self->code_buf_size/2;
        __datas_prepare:
            {//read code in
                if (self->code_cur>kLimitCodeSize){
                    const hpatch_StreamPos_t _codeSize=self->code_end-self->code_begin;
                    size_t read_len=(self->code_cur<_codeSize)?self->code_cur:(size_t)_codeSize;
                    if (read_len){
                        size_t code_len=self->code_buf_size-self->code_cur;
                        unsigned char* pcode=self->code_buf+self->code_cur;
                        unsigned char* pdst=pcode-read_len;
                        memmove(pdst,pcode,code_len);
                        pdst+=code_len;
                        if (!self->codeStream->read(self->codeStream,self->code_begin,pdst,pdst+read_len))
                            return 0; //read error
                        self->code_begin+=read_len;
                        self->code_cur-=read_len;
                    }
                }
            }
            {//move dict
                if (self->data_cur>kLimitDataSize){
                    size_t move_offset=self->data_cur-kDictSize;
                    if (move_offset){
                        memmove(self->data_buf,self->data_buf+move_offset,kDictSize);
                        self->data_cur=kDictSize;
                        self->out_cur=kDictSize;
                    }
                }
            }
            {// decompress
                int    is_final_block_ret;
                size_t actual_in_nbytes_ret;
                size_t actual_out_nbytes_ret;
                const size_t dec_state=libdeflate_deflate_decompress_get_state(self->d);
                enum libdeflate_result ret=libdeflate_deflate_decompress_block(self->d,
                                                self->code_buf+self->code_cur,self->code_buf_size-self->code_cur,
                                                self->data_buf,self->data_cur,self->data_buf_size-self->data_cur,
                                                &actual_in_nbytes_ret,&actual_out_nbytes_ret,
                                                LIBDEFLATE_STOP_BY_ANY_BLOCK,&is_final_block_ret);
                if (ret!=LIBDEFLATE_SUCCESS){
                    if ((self->code_begin==self->code_end)&&(ret!=LIBDEFLATE_INSUFFICIENT_SPACE))
                        _dec_onDecErr_rt();
                    if ((self->data_cur>kDictSize)||((self->code_cur>0)&&(self->code_begin<self->code_end))){
                        kLimitDataSize=kDictSize;
                        kLimitCodeSize=0;
                        libdeflate_deflate_decompress_set_state(self->d,dec_state);
                        goto __datas_prepare; //retry by libdefalte
                    }
                  #if (_CompressPlugin_ldef_is_use_zlib)
                    return _ldef_swap_to_zlib(self,dec_state,out_part_data,out_part_len); //retry by zlib
                  #else
                    _dec_onDecErr_rt();
                  #endif
                }
                self->code_cur+=actual_in_nbytes_ret;
                self->data_cur+=actual_out_nbytes_ret;
                if (is_final_block_ret)
                    libdeflate_deflate_decompress_block_reset(self->d);
            }
        }
        return hpatch_TRUE;
    }
    static hpatch_TDecompress ldefDecompressPlugin={_ldef_is_can_open,_ldef_decompress_open,
                                                    _ldef_decompress_close,_ldef_decompress_part};
#endif//_CompressPlugin_ldef


#ifdef  _CompressPlugin_bz2
#if (_IsNeedIncludeDefaultCompressHead)
#   include "bzlib.h" // http://www.bzip.org/  https://github.com/sisong/bzip2
#endif
    typedef struct _bz2_TDecompress{
        const struct hpatch_TStreamInput* codeStream;
        hpatch_StreamPos_t code_begin;
        hpatch_StreamPos_t code_end;
        
        bz_stream       d_stream;
        hpatch_dec_error_t decError;
        unsigned char   dec_buf[kDecompressBufSize];
    } _bz2_TDecompress;
    static hpatch_BOOL _bz2_is_can_open(const char* compressType){
        return (0==strcmp(compressType,"bz2"))||(0==strcmp(compressType,"bzip2"))
             ||(0==strcmp(compressType,"pbz2"))||(0==strcmp(compressType,"pbzip2"));
    }
    static hpatch_decompressHandle  _bz2_open(struct hpatch_TDecompress* decompressPlugin,
                                               hpatch_StreamPos_t dataSize,
                                               const hpatch_TStreamInput* codeStream,
                                               hpatch_StreamPos_t code_begin,
                                               hpatch_StreamPos_t code_end){
        int ret;
        _bz2_TDecompress* self=(_bz2_TDecompress*)_dec_malloc(sizeof(_bz2_TDecompress));
        if (!self) _dec_memErr_rt();
        memset(self,0,sizeof(_bz2_TDecompress)-kDecompressBufSize);
        self->codeStream=codeStream;
        self->code_begin=code_begin;
        self->code_end=code_end;
        
        ret=BZ2_bzDecompressInit(&self->d_stream,0,0);
        if (ret!=BZ_OK){ free(self); _dec_openErr_rt(); }
        return self;
    }
    static hpatch_BOOL _bz2_close(struct hpatch_TDecompress* decompressPlugin,
                                  hpatch_decompressHandle decompressHandle){
        hpatch_BOOL result=hpatch_TRUE;
        _bz2_TDecompress* self=(_bz2_TDecompress*)decompressHandle;
        if (!self) return result;
        _dec_onDecErr_up();
        _dec_close_check(BZ_OK==BZ2_bzDecompressEnd(&self->d_stream));
        free(self);
        return result;
    }
    static hpatch_BOOL _bz2_reset_for_next_node(_bz2_TDecompress* self){
        //backup
        char*   next_out_back=self->d_stream.next_out;
        char*   next_in_back=self->d_stream.next_in;
        unsigned int avail_out_back=self->d_stream.avail_out;
        unsigned int avail_in_back=self->d_stream.avail_in;
        //reset
        if (BZ_OK!=BZ2_bzDecompressEnd(&self->d_stream)) _dec_onDecErr_rt();
        if (BZ_OK!=BZ2_bzDecompressInit(&self->d_stream,0,0)) _dec_onDecErr_rt();
        //restore
        self->d_stream.next_out=next_out_back;
        self->d_stream.next_in=next_in_back;
        self->d_stream.avail_out=avail_out_back;
        self->d_stream.avail_in=avail_in_back;
        return hpatch_TRUE;
    }

    static hpatch_BOOL _bz2_decompress_part_(hpatch_decompressHandle decompressHandle,
                                             unsigned char* out_part_data,unsigned char* out_part_data_end,
                                             hpatch_BOOL isMustOutData){
        _bz2_TDecompress* self=(_bz2_TDecompress*)decompressHandle;
        assert(out_part_data<=out_part_data_end);
        
        self->d_stream.next_out =(char*)out_part_data;
        self->d_stream.avail_out =(unsigned int)(out_part_data_end-out_part_data);
        while (self->d_stream.avail_out>0) {
            unsigned int avail_out_back,avail_in_back;
            int ret;
            hpatch_StreamPos_t codeLen=(self->code_end - self->code_begin);
            if ((self->d_stream.avail_in==0)&&(codeLen>0)) {
                size_t readLen=kDecompressBufSize;
                self->d_stream.next_in=(char*)self->dec_buf;
                if (readLen>codeLen) readLen=(size_t)codeLen;
                if (!self->codeStream->read(self->codeStream,self->code_begin,self->dec_buf,
                                            self->dec_buf+readLen)) return hpatch_FALSE;//error;
                self->d_stream.avail_in=(unsigned int)readLen;
                self->code_begin+=readLen;
                codeLen-=readLen;
            }
            
            avail_out_back=self->d_stream.avail_out;
            avail_in_back=self->d_stream.avail_in;
            ret=BZ2_bzDecompress(&self->d_stream);
            if (ret==BZ_OK){
                if ((self->d_stream.avail_in==avail_in_back)&&(self->d_stream.avail_out==avail_out_back))
                    _dec_onDecErr_rt();//error;
            }else if (ret==BZ_STREAM_END){
                if (self->d_stream.avail_in+codeLen>0){ //next compress node!
                    if (!_bz2_reset_for_next_node(self))
                        return hpatch_FALSE;//error;
                }else{//all end
                    if (self->d_stream.avail_out!=0){
                        if (isMustOutData){ //fill out 0
                            memset(self->d_stream.next_out,0,self->d_stream.avail_out);
                            self->d_stream.next_out+=self->d_stream.avail_out;
                            self->d_stream.avail_out=0;
                        }else{
                            _dec_onDecErr_rt();//error;
                        }
                    }
                }
            }else{
                _dec_onDecErr_rt();//error;
            }
        }
        return hpatch_TRUE;
    }
    static hpatch_BOOL _bz2_decompress_part(hpatch_decompressHandle decompressHandle,
                                            unsigned char* out_part_data,unsigned char* out_part_data_end){
        return _bz2_decompress_part_(decompressHandle,out_part_data,out_part_data_end,hpatch_FALSE);
    }
    static hpatch_BOOL _bz2_decompress_part_unsz(hpatch_decompressHandle decompressHandle,
                                                 unsigned char* out_part_data,unsigned char* out_part_data_end){
        return _bz2_decompress_part_(decompressHandle,out_part_data,out_part_data_end,hpatch_TRUE);
    }
    
    static hpatch_TDecompress bz2DecompressPlugin={_bz2_is_can_open,_bz2_open,
                                                   _bz2_close,_bz2_decompress_part};

    //unkown uncompress data size
    static hpatch_TDecompress _bz2DecompressPlugin_unsz={_bz2_is_can_open,_bz2_open,
                                                         _bz2_close,_bz2_decompress_part_unsz};
#endif//_CompressPlugin_bz2


#if (defined _CompressPlugin_lzma) || (defined _CompressPlugin_lzma2)
#if (_IsNeedIncludeDefaultCompressHead)
#   include "LzmaDec.h" // "lzma/C/LzmaDec.h" https://github.com/sisong/lzma
#   ifdef _CompressPlugin_lzma2
#       include "Lzma2Dec.h"
#   endif
#endif
#endif

#ifdef _CompressPlugin_lzma
    typedef struct _lzma_TDecompress{
        ISzAlloc           memAllocBase;
        const struct hpatch_TStreamInput* codeStream;
        hpatch_StreamPos_t code_begin;
        hpatch_StreamPos_t code_end;
        
        CLzmaDec        decEnv;
        SizeT           decCopyPos;
        SizeT           decReadPos;
        hpatch_dec_error_t decError;
        unsigned char   dec_buf[kDecompressBufSize];
    } _lzma_TDecompress;
    static void * __lzma1_dec_Alloc(ISzAllocPtr p, size_t size) 
        __dec_Alloc_fun(_lzma_TDecompress,p,size)

    static hpatch_BOOL _lzma_is_can_open(const char* compressType){
        return (0==strcmp(compressType,"lzma"));
    }
    static hpatch_decompressHandle  _lzma_open(hpatch_TDecompress* decompressPlugin,
                                               hpatch_StreamPos_t dataSize,
                                               const hpatch_TStreamInput* codeStream,
                                               hpatch_StreamPos_t code_begin,
                                               hpatch_StreamPos_t code_end){
        _lzma_TDecompress* self=0;
        SRes ret;
        unsigned char propsSize=0;
        unsigned char props[256];
        //load propsSize
        if (code_end-code_begin<1) _dec_openErr_rt();
        if (!codeStream->read(codeStream,code_begin,&propsSize,&propsSize+1)) return 0;
        ++code_begin;
        if (propsSize>(code_end-code_begin)) _dec_openErr_rt();
        //load props
        if (!codeStream->read(codeStream,code_begin,props,props+propsSize)) return 0;
        code_begin+=propsSize;

        self=(_lzma_TDecompress*)_dec_malloc(sizeof(_lzma_TDecompress));
        if (!self) _dec_memErr_rt();
        memset(self,0,sizeof(_lzma_TDecompress)-kDecompressBufSize);
        self->memAllocBase.Alloc=__lzma1_dec_Alloc;
        *((void**)&self->memAllocBase.Free)=(void*)__dec_free;
        self->codeStream=codeStream;
        self->code_begin=code_begin;
        self->code_end=code_end;
        
        self->decCopyPos=0;
        self->decReadPos=kDecompressBufSize;
        
        LzmaDec_Construct(&self->decEnv);
        ret=LzmaDec_Allocate(&self->decEnv,props,propsSize,&self->memAllocBase);
        if (ret!=SZ_OK){ _dec_onDecErr_up(); free(self); _dec_openErr_rt(); }
        LzmaDec_Init(&self->decEnv);
        return self;
    }
    static hpatch_BOOL _lzma_close(struct hpatch_TDecompress* decompressPlugin,
                                   hpatch_decompressHandle decompressHandle){
        _lzma_TDecompress* self=(_lzma_TDecompress*)decompressHandle;
        if (!self) return hpatch_TRUE;
        LzmaDec_Free(&self->decEnv,&self->memAllocBase);
        _dec_onDecErr_up();
        free(self);
        return hpatch_TRUE;
    }
    static hpatch_BOOL _lzma_decompress_part(hpatch_decompressHandle decompressHandle,
                                             unsigned char* out_part_data,unsigned char* out_part_data_end){
        _lzma_TDecompress* self=(_lzma_TDecompress*)decompressHandle;
        unsigned char* out_cur=out_part_data;
        assert(out_part_data<=out_part_data_end);
        while (out_cur<out_part_data_end){
            size_t copyLen=(self->decEnv.dicPos-self->decCopyPos);
            if (copyLen>0){
                if (copyLen>(size_t)(out_part_data_end-out_cur))
                    copyLen=(out_part_data_end-out_cur);
                memcpy(out_cur,self->decEnv.dic+self->decCopyPos,copyLen);
                out_cur+=copyLen;
                self->decCopyPos+=copyLen;
                if ((self->decEnv.dicPos==self->decEnv.dicBufSize)
                    &&(self->decEnv.dicPos==self->decCopyPos)){
                    self->decEnv.dicPos=0;
                    self->decCopyPos=0;
                }
            }else{
                ELzmaStatus status;
                SizeT inSize,dicPos_back;
                SRes res;
                hpatch_StreamPos_t codeLen=(self->code_end - self->code_begin);
                if ((self->decReadPos==kDecompressBufSize)&&(codeLen>0)) {
                    size_t readLen=kDecompressBufSize;
                    if (readLen>codeLen) readLen=(size_t)codeLen;
                    self->decReadPos=kDecompressBufSize-readLen;
                    if (!self->codeStream->read(self->codeStream,self->code_begin,self->dec_buf+self->decReadPos,
                                                self->dec_buf+self->decReadPos+readLen)) return hpatch_FALSE;//error;
                    self->code_begin+=readLen;
                }

                inSize=kDecompressBufSize-self->decReadPos;
                dicPos_back=self->decEnv.dicPos;
                res=LzmaDec_DecodeToDic(&self->decEnv,self->decEnv.dicBufSize,
                                        self->dec_buf+self->decReadPos,&inSize,LZMA_FINISH_ANY,&status);
                if(res==SZ_OK){
                    if ((inSize==0)&&(self->decEnv.dicPos==dicPos_back))
                        _dec_onDecErr_rt();//error;
                }else{
                    _dec_onDecErr_rt();//error;
                }
                self->decReadPos+=inSize;
            }
        }
        return hpatch_TRUE;
    }
    static hpatch_TDecompress lzmaDecompressPlugin={_lzma_is_can_open,_lzma_open,
                                                    _lzma_close,_lzma_decompress_part};
#endif//_CompressPlugin_lzma

#ifdef _CompressPlugin_lzma2
    typedef struct _lzma2_TDecompress{
        ISzAlloc           memAllocBase;
        const struct hpatch_TStreamInput* codeStream;
        hpatch_StreamPos_t code_begin;
        hpatch_StreamPos_t code_end;
        
        CLzma2Dec       decEnv;
        SizeT           decCopyPos;
        SizeT           decReadPos;
        hpatch_dec_error_t decError;
        unsigned char   dec_buf[kDecompressBufSize];
    } _lzma2_TDecompress;
    static void * __lzma2_dec_Alloc(ISzAllocPtr p, size_t size) 
        __dec_Alloc_fun(_lzma2_TDecompress,p,size)
    
    static hpatch_BOOL _lzma2_is_can_open(const char* compressType){
        return (0==strcmp(compressType,"lzma2"));
    }
    static hpatch_decompressHandle _lzma2_open(hpatch_TDecompress* decompressPlugin,
                                            hpatch_StreamPos_t dataSize,
                                            const hpatch_TStreamInput* codeStream,
                                            hpatch_StreamPos_t code_begin,
                                            hpatch_StreamPos_t code_end){
        _lzma2_TDecompress* self=0;
        SRes ret;
        unsigned char propsSize=0;
        //load propsSize
        if (code_end-code_begin<1) _dec_openErr_rt();
        if (!codeStream->read(codeStream,code_begin,&propsSize,&propsSize+1)) return 0;
        ++code_begin;
        
        self=(_lzma2_TDecompress*)_dec_malloc(sizeof(_lzma2_TDecompress));
        if (!self) _dec_memErr_rt();
        memset(self,0,sizeof(_lzma2_TDecompress)-kDecompressBufSize);
        self->memAllocBase.Alloc=__lzma2_dec_Alloc;
        *((void**)&self->memAllocBase.Free)=(void*)__dec_free;
        self->codeStream=codeStream;
        self->code_begin=code_begin;
        self->code_end=code_end;
        
        self->decCopyPos=0;
        self->decReadPos=kDecompressBufSize;
        
        Lzma2Dec_Construct(&self->decEnv);
        ret=Lzma2Dec_Allocate(&self->decEnv,propsSize,&self->memAllocBase);
        if (ret!=SZ_OK){ _dec_onDecErr_up(); free(self); _dec_openErr_rt(); }
        Lzma2Dec_Init(&self->decEnv);
        return self;
    }
    static hpatch_BOOL _lzma2_close(struct hpatch_TDecompress* decompressPlugin,
                                    hpatch_decompressHandle decompressHandle){
        _lzma2_TDecompress* self=(_lzma2_TDecompress*)decompressHandle;
        if (!self) return hpatch_TRUE;
        Lzma2Dec_Free(&self->decEnv,&self->memAllocBase);
        _dec_onDecErr_up();
        free(self);
        return hpatch_TRUE;
    }
    static hpatch_BOOL _lzma2_decompress_part(hpatch_decompressHandle decompressHandle,
                                            unsigned char* out_part_data,unsigned char* out_part_data_end){
        _lzma2_TDecompress* self=(_lzma2_TDecompress*)decompressHandle;
        unsigned char* out_cur=out_part_data;
        assert(out_part_data<=out_part_data_end);
        while (out_cur<out_part_data_end){
            size_t copyLen=(self->decEnv.decoder.dicPos-self->decCopyPos);
            if (copyLen>0){
                if (copyLen>(size_t)(out_part_data_end-out_cur))
                    copyLen=(out_part_data_end-out_cur);
                memcpy(out_cur,self->decEnv.decoder.dic+self->decCopyPos,copyLen);
                out_cur+=copyLen;
                self->decCopyPos+=copyLen;
                if ((self->decEnv.decoder.dicPos==self->decEnv.decoder.dicBufSize)
                    &&(self->decEnv.decoder.dicPos==self->decCopyPos)){
                    self->decEnv.decoder.dicPos=0;
                    self->decCopyPos=0;
                }
            }else{
                ELzmaStatus status;
                SizeT inSize,dicPos_back;
                SRes res;
                hpatch_StreamPos_t codeLen=(self->code_end - self->code_begin);
                if ((self->decReadPos==kDecompressBufSize)&&(codeLen>0)) {
                    size_t readLen=kDecompressBufSize;
                    if (readLen>codeLen) readLen=(size_t)codeLen;
                    self->decReadPos=kDecompressBufSize-readLen;
                    if (!self->codeStream->read(self->codeStream,self->code_begin,self->dec_buf+self->decReadPos,
                                                self->dec_buf+self->decReadPos+readLen)) return hpatch_FALSE;//error;
                    self->code_begin+=readLen;
                }
                
                inSize=kDecompressBufSize-self->decReadPos;
                dicPos_back=self->decEnv.decoder.dicPos;
                res=Lzma2Dec_DecodeToDic(&self->decEnv,self->decEnv.decoder.dicBufSize,
                                        self->dec_buf+self->decReadPos,&inSize,LZMA_FINISH_ANY,&status);
                if(res==SZ_OK){
                    if ((inSize==0)&&(self->decEnv.decoder.dicPos==dicPos_back))
                        _dec_onDecErr_rt();//error;
                }else{
                    _dec_onDecErr_rt();//error;
                }
                self->decReadPos+=inSize;
            }
        }
        return hpatch_TRUE;
    }
    static hpatch_TDecompress lzma2DecompressPlugin={_lzma2_is_can_open,_lzma2_open,
                                                    _lzma2_close,_lzma2_decompress_part};
#endif//_CompressPlugin_lzma2

#ifdef _CompressPlugin_7zXZ
#if (_IsNeedIncludeDefaultCompressHead)
#   include "Xz.h" // "lzma/C/Xz.h" https://github.com/sisong/lzma
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
    
    typedef struct _7zXZ_TDecompress{
        ISzAlloc           memAllocBase;
        const struct hpatch_TStreamInput* codeStream;
        hpatch_StreamPos_t code_begin;
        hpatch_StreamPos_t code_end;
        hpatch_TDecompress* decompressPlugin;
        hpatch_BOOL isResetState;
        
        CXzUnpacker     decEnv;
        SizeT           decCopyPos;
        SizeT           decReadPos;
        hpatch_dec_error_t decError;
        unsigned char   dec_buf[kDecompressBufSize];
    } _7zXZ_TDecompress;
    static void * __7zXZ_dec_Alloc(ISzAllocPtr p, size_t size) 
        __dec_Alloc_fun(_7zXZ_TDecompress,p,size)
    
    static hpatch_BOOL _7zXZ_is_can_open(const char* compressType){
        return (0==strcmp(compressType,"7zXZ"));
    }
    static void _7zXZ_open_at(_7zXZ_TDecompress* self, hpatch_TDecompress* decompressPlugin,hpatch_StreamPos_t dataSize,
                              const hpatch_TStreamInput* codeStream,hpatch_StreamPos_t code_begin,
                              hpatch_StreamPos_t code_end,hpatch_BOOL isResetState,hpatch_BOOL isParseHead){
        memset(self,0,sizeof(_7zXZ_TDecompress)-kDecompressBufSize);
        self->memAllocBase.Alloc=__7zXZ_dec_Alloc;
        *((void**)&self->memAllocBase.Free)=(void*)__dec_free;
        self->codeStream=codeStream;
        self->code_begin=code_begin;
        self->code_end=code_end;
        self->decompressPlugin=decompressPlugin;
        self->isResetState=isResetState;

        self->decCopyPos=0;
        self->decReadPos=kDecompressBufSize;
        
        XzUnpacker_Construct(&self->decEnv,&self->memAllocBase);
        XzUnpacker_Init(&self->decEnv);
        if (!isParseHead)
            self->decEnv.state=XZ_STATE_BLOCK_HEADER;
    }
    static void _7zXZ_close_at(hpatch_decompressHandle decompressHandle){
        _7zXZ_TDecompress* self=(_7zXZ_TDecompress*)decompressHandle;
        if (self){
            hpatch_TDecompress* decompressPlugin=self->decompressPlugin;
            XzUnpacker_Free(&self->decEnv);
            _dec_onDecErr_up();
        }
    }
    static hpatch_decompressHandle _7zXZ_open(hpatch_TDecompress* decompressPlugin,
                                              hpatch_StreamPos_t dataSize,
                                              const hpatch_TStreamInput* codeStream,
                                              hpatch_StreamPos_t code_begin,
                                              hpatch_StreamPos_t code_end){
        _7zXZ_TDecompress* self=0;
        
        self=(_7zXZ_TDecompress*)_dec_malloc(sizeof(_7zXZ_TDecompress));
        if (!self) _dec_memErr_rt();
        _7zXZ_open_at(self,decompressPlugin,dataSize,codeStream,
                      code_begin,code_end,hpatch_FALSE,hpatch_TRUE);
        return self;
    }
    static hpatch_decompressHandle _7zXZ_a_open(hpatch_TDecompress* decompressPlugin,
                                                hpatch_StreamPos_t dataSize,
                                                const hpatch_TStreamInput* codeStream,
                                                hpatch_StreamPos_t code_begin,
                                                hpatch_StreamPos_t code_end){
        _7zXZ_TDecompress* self=0;
        
        self=(_7zXZ_TDecompress*)_dec_malloc(sizeof(_7zXZ_TDecompress));
        if (!self) _dec_memErr_rt();
        _7zXZ_open_at(self,decompressPlugin,dataSize,codeStream,
                      code_begin,code_end,hpatch_TRUE,hpatch_TRUE);
        return self;
    }
    static hpatch_BOOL _7zXZ_close(struct hpatch_TDecompress* decompressPlugin,
                                    hpatch_decompressHandle decompressHandle){
        if (decompressHandle){
            _7zXZ_close_at(decompressHandle);
            free(decompressHandle);
        }
        return hpatch_TRUE;
    }
    static hpatch_BOOL _7zXZ_reset_code(hpatch_decompressHandle decompressHandle,
                                        hpatch_StreamPos_t dataSize,
                                        const struct hpatch_TStreamInput* codeStream,
                                        hpatch_StreamPos_t code_begin,
                                        hpatch_StreamPos_t code_end){
        _7zXZ_TDecompress* self=(_7zXZ_TDecompress*)decompressHandle;
        hpatch_BOOL isResetState=self->isResetState;
        if (isResetState){
            hpatch_TDecompress* decompressPlugin=self->decompressPlugin;
            _7zXZ_close_at(self);
            _7zXZ_open_at(self,decompressPlugin,dataSize,codeStream,
                          code_begin,code_end,isResetState,hpatch_FALSE);
        }else{
            self->codeStream=codeStream;
            self->code_begin=code_begin;
            self->code_end=code_end;
            self->decCopyPos=0;
            self->decReadPos=kDecompressBufSize;
        }
        return hpatch_TRUE;
    }
    static hpatch_BOOL _7zXZ_decompress_part(hpatch_decompressHandle decompressHandle,
                                            unsigned char* out_part_data,unsigned char* out_part_data_end){
        _7zXZ_TDecompress* self=(_7zXZ_TDecompress*)decompressHandle;
        unsigned char* out_cur=out_part_data;
        assert(out_part_data<=out_part_data_end);
        while (out_cur<out_part_data_end){
            ECoderStatus status;
            SizeT inSize;
            SizeT outSize=out_part_data_end-out_cur;
            SRes res;
            hpatch_StreamPos_t codeLen=(self->code_end-self->code_begin);
            if ((self->decReadPos==kDecompressBufSize)&&(codeLen>0)) {
                size_t readLen=kDecompressBufSize;
                if (readLen>codeLen) readLen=(size_t)codeLen;
                self->decReadPos=kDecompressBufSize-readLen;
                if (!self->codeStream->read(self->codeStream,self->code_begin,self->dec_buf+self->decReadPos,
                                            self->dec_buf+self->decReadPos+readLen)) return hpatch_FALSE;//error;
                self->code_begin+=readLen;
                codeLen-=readLen;
            }
            
            inSize=kDecompressBufSize-self->decReadPos;
            res=XzUnpacker_Code(&self->decEnv,out_cur,&outSize,self->dec_buf+self->decReadPos,&inSize,
                                codeLen==0,CODER_FINISH_ANY,&status);
            if(res==SZ_OK){
                if ((inSize==0)&&(outSize==0))
                    _dec_onDecErr_rt();//error;
            }else{
                _dec_onDecErr_rt();//error;
            }
            self->decReadPos+=inSize;
            out_cur+=outSize;
        }
        return hpatch_TRUE;
    }
    static hpatch_TDecompress _7zXZDecompressPlugin={_7zXZ_is_can_open,_7zXZ_open,
                                                      _7zXZ_close,_7zXZ_decompress_part,_7zXZ_reset_code};
    static hpatch_TDecompress _7zXZDecompressPlugin_a={_7zXZ_is_can_open,_7zXZ_a_open,
                                                        _7zXZ_close,_7zXZ_decompress_part,_7zXZ_reset_code};
#endif//_CompressPlugin_7zXZ


#if (defined(_CompressPlugin_lz4) || defined(_CompressPlugin_lz4hc))
#if (_IsNeedIncludeDefaultCompressHead)
#   include "lz4.h" // "lz4/lib/lz4.h" https://github.com/lz4/lz4
#endif
    typedef struct _lz4_TDecompress{
        const struct hpatch_TStreamInput* codeStream;
        hpatch_StreamPos_t code_begin;
        hpatch_StreamPos_t code_end;
        
        LZ4_streamDecode_t *s;
        int                kLz4CompressBufSize;
        int                code_buf_size;
        int                data_begin;
        int                data_end;
        hpatch_dec_error_t decError;
        unsigned char      buf[1];
    } _lz4_TDecompress;
    static hpatch_BOOL _lz4_is_can_open(const char* compressType){
        return (0==strcmp(compressType,"lz4"));
    }
    #define _lz4_read_len4(len,in_code,code_begin,code_end,__dec_err_rt) { \
        unsigned char _temp_buf4[4];  \
        if (4>code_end-code_begin) __dec_err_rt(); \
        if (!in_code->read(in_code,code_begin,_temp_buf4,_temp_buf4+4)) \
            return hpatch_FALSE; \
        len=_temp_buf4[0]|(_temp_buf4[1]<<8)|(_temp_buf4[2]<<16)|(_temp_buf4[3]<<24); \
        code_begin+=4; \
    }
    static hpatch_decompressHandle  _lz4_open(hpatch_TDecompress* decompressPlugin,
                                              hpatch_StreamPos_t dataSize,
                                              const hpatch_TStreamInput* codeStream,
                                              hpatch_StreamPos_t code_begin,
                                              hpatch_StreamPos_t code_end){
        const int kMaxLz4CompressBufSize=(1<<20)*64; //defence attack
        _lz4_TDecompress* self=0;
        int kLz4CompressBufSize=0;
        int code_buf_size=0;
        assert(code_begin<code_end);
        {//read kLz4CompressBufSize
            _lz4_read_len4(kLz4CompressBufSize,codeStream,code_begin,code_end,_dec_openErr_rt);
            if ((kLz4CompressBufSize<0)||(kLz4CompressBufSize>=kMaxLz4CompressBufSize)) _dec_openErr_rt();
            code_buf_size=LZ4_compressBound(kLz4CompressBufSize);
        }
        self=(_lz4_TDecompress*)_dec_malloc(sizeof(_lz4_TDecompress)+kLz4CompressBufSize+code_buf_size);
        if (!self) _dec_memErr_rt();
        memset(self,0,sizeof(_lz4_TDecompress));
        self->codeStream=codeStream;
        self->code_begin=code_begin;
        self->code_end=code_end;
        self->kLz4CompressBufSize=kLz4CompressBufSize;
        self->code_buf_size=code_buf_size;
        self->data_begin=0;
        self->data_end=0;
        
        self->s = LZ4_createStreamDecode();
        if (!self->s){ free(self); _dec_openErr_rt(); }
        return self;
    }
    static hpatch_BOOL _lz4_close(struct hpatch_TDecompress* decompressPlugin,
                                  hpatch_decompressHandle decompressHandle){
        hpatch_BOOL result=hpatch_TRUE;
        _lz4_TDecompress* self=(_lz4_TDecompress*)decompressHandle;
        if (!self) return result;
        _dec_onDecErr_up();
        _dec_close_check(0==LZ4_freeStreamDecode(self->s));
        free(self);
        return result;
    }
    static hpatch_BOOL _lz4_decompress_part(hpatch_decompressHandle decompressHandle,
                                            unsigned char* out_part_data,unsigned char* out_part_data_end){
        _lz4_TDecompress* self=(_lz4_TDecompress*)decompressHandle;
        unsigned char* data_buf=self->buf;
        unsigned char* code_buf=self->buf+self->kLz4CompressBufSize;

        while (out_part_data<out_part_data_end) {
            size_t dataLen=self->data_end-self->data_begin;
            if (dataLen>0){
                if (dataLen>(out_part_data_end-out_part_data))
                    dataLen=(out_part_data_end-out_part_data);
                memcpy(out_part_data,data_buf+self->data_begin,dataLen);
                out_part_data+=(size_t)dataLen;
                self->data_begin+=dataLen;
            }else{
                int codeLen;
                _lz4_read_len4(codeLen,self->codeStream,self->code_begin,self->code_end,_dec_onDecErr_rt);
                if ((codeLen<=0)||(codeLen>self->code_buf_size)
                    ||((size_t)codeLen>(self->code_end-self->code_begin))) _dec_onDecErr_rt();
                if (!self->codeStream->read(self->codeStream,self->code_begin,
                                            code_buf,code_buf+codeLen)) return hpatch_FALSE;
                self->code_begin+=codeLen;
                self->data_begin=0;
                self->data_end=LZ4_decompress_safe_continue(self->s,(const char*)code_buf,(char*)data_buf,
                                                            codeLen,self->kLz4CompressBufSize);
                if (self->data_end<=0) _dec_onDecErr_rt();
            }
        }
        return hpatch_TRUE;
    }
    static hpatch_TDecompress lz4DecompressPlugin={_lz4_is_can_open,_lz4_open,
                                                   _lz4_close,_lz4_decompress_part};
#endif//_CompressPlugin_lz4 or _CompressPlugin_lz4hc

#ifdef  _CompressPlugin_zstd
#if (_IsNeedIncludeDefaultCompressHead)
//#   define ZSTD_STATIC_LINKING_ONLY //for ZSTD_customMem
#   include "zstd.h" // "zstd/lib/zstd.h" https://github.com/sisong/zstd
#endif
    typedef struct _zstd_TDecompress{
        const struct hpatch_TStreamInput* codeStream;
        hpatch_StreamPos_t code_begin;
        hpatch_StreamPos_t code_end;
        
        ZSTD_inBuffer      s_input;
        ZSTD_outBuffer     s_output;
        size_t             data_begin;
        ZSTD_DStream*      s;
        hpatch_dec_error_t decError;
        unsigned char      buf[1];
    } _zstd_TDecompress;
    #ifdef ZSTD_STATIC_LINKING_ONLY
    static void* __ZSTD_alloc(void* opaque, size_t size)
        __dec_Alloc_fun(_zstd_TDecompress,opaque,size)
    #endif
    static hpatch_BOOL _zstd_is_can_open(const char* compressType){
        return (0==strcmp(compressType,"zstd"));
    }
    static hpatch_decompressHandle  _zstd_open(hpatch_TDecompress* decompressPlugin,
                                               hpatch_StreamPos_t dataSize,
                                               const hpatch_TStreamInput* codeStream,
                                               hpatch_StreamPos_t code_begin,
                                               hpatch_StreamPos_t code_end){
        _zstd_TDecompress* self=0;
        size_t  ret;
        size_t _input_size=ZSTD_DStreamInSize();
        size_t _output_size=ZSTD_DStreamOutSize();
        self=(_zstd_TDecompress*)_dec_malloc(sizeof(_zstd_TDecompress)+_input_size+_output_size);
        if (!self) _dec_memErr_rt();
        memset(self,0,sizeof(_zstd_TDecompress));
        self->codeStream=codeStream;
        self->code_begin=code_begin;
        self->code_end=code_end;
        self->s_input.src=self->buf;
        self->s_input.size=_input_size;
        self->s_input.pos=_input_size;
        self->s_output.dst=self->buf+_input_size;
        self->s_output.size=_output_size;
        self->s_output.pos=0;
        self->data_begin=0;
        #ifdef ZSTD_STATIC_LINKING_ONLY
        {
            ZSTD_customMem customMem={__ZSTD_alloc,__dec_free,self};
            self->s=ZSTD_createDStream_advanced(customMem);
        }
        #else
            self->s=ZSTD_createDStream();
        #endif
        if (!self->s){ _dec_onDecErr_up(); free(self); _dec_openErr_rt(); }
        ret=ZSTD_initDStream(self->s);
        if (ZSTD_isError(ret)) { ZSTD_freeDStream(self->s); _dec_onDecErr_up(); free(self); _dec_openErr_rt(); }
        #define _ZSTD_WINDOWLOG_MAX 30
        ret=ZSTD_DCtx_setParameter(self->s,ZSTD_d_windowLogMax,_ZSTD_WINDOWLOG_MAX);
        //if (ZSTD_isError(ret)) { printf("WARNING: ZSTD_DCtx_setMaxWindowSize() error!"); }
        return self;
    }
    static hpatch_BOOL _zstd_close(struct hpatch_TDecompress* decompressPlugin,
                                   hpatch_decompressHandle decompressHandle){
        hpatch_BOOL result=hpatch_TRUE;
        _zstd_TDecompress* self=(_zstd_TDecompress*)decompressHandle;
        if (!self) return result;
        _dec_onDecErr_up();
        _dec_close_check(0==ZSTD_freeDStream(self->s));
        free(self);
        return result;
    }
    static hpatch_BOOL _zstd_decompress_part(hpatch_decompressHandle decompressHandle,
                                             unsigned char* out_part_data,unsigned char* out_part_data_end){
        _zstd_TDecompress* self=(_zstd_TDecompress*)decompressHandle;
        while (out_part_data<out_part_data_end) {
            size_t dataLen=(self->s_output.pos-self->data_begin);
            if (dataLen>0){
                if (dataLen>(size_t)(out_part_data_end-out_part_data))
                    dataLen=(out_part_data_end-out_part_data);
                memcpy(out_part_data,(const unsigned char*)self->s_output.dst+self->data_begin,dataLen);
                out_part_data+=dataLen;
                self->data_begin+=dataLen;
            }else{
                size_t ret;
                if (self->s_input.pos==self->s_input.size) {
                    self->s_input.pos=0;
                    if (self->s_input.size>self->code_end-self->code_begin)
                        self->s_input.size=(size_t)(self->code_end-self->code_begin);

                    if (self->s_input.size>0){
                        if (!self->codeStream->read(self->codeStream,self->code_begin,(unsigned char*)self->s_input.src,
                                                    (unsigned char*)self->s_input.src+self->s_input.size))
                            return hpatch_FALSE;
                        self->code_begin+=self->s_input.size;
                    }
                }
                self->s_output.pos=0;
                self->data_begin=0;
                ret=ZSTD_decompressStream(self->s,&self->s_output,&self->s_input);
                if (ZSTD_isError(ret)) _dec_onDecErr_rt();
                if (self->s_output.pos==self->data_begin) _dec_onDecErr_rt();
            }
        }
        return hpatch_TRUE;
    }
    static hpatch_TDecompress zstdDecompressPlugin={_zstd_is_can_open,_zstd_open,
                                                    _zstd_close,_zstd_decompress_part};
#endif//_CompressPlugin_zstd


#ifdef  _CompressPlugin_brotli
#if (_IsNeedIncludeDefaultCompressHead)
#   include "brotli/decode.h" // "brotli/c/include/brotli/decode.h" https://github.com/google/brotli
#endif
    typedef struct _brotli_TDecompress{
        const struct hpatch_TStreamInput* codeStream;
        hpatch_StreamPos_t code_begin;
        hpatch_StreamPos_t code_end;
        
        unsigned char*        input;
        unsigned char*        output;
        size_t                available_in;
        size_t                available_out;
        const unsigned char*  next_in;
        unsigned char*        next_out;
        unsigned char*        data_begin;
        BrotliDecoderState* s;
        hpatch_dec_error_t  decError;
        unsigned char       buf[1];
    } _brotli_TDecompress;
    static hpatch_BOOL _brotli_is_can_open(const char* compressType){
        return (0==strcmp(compressType,"brotli"));
    }
    static hpatch_decompressHandle  _brotli_open(hpatch_TDecompress* decompressPlugin,
                                                 hpatch_StreamPos_t dataSize,
                                                 const hpatch_TStreamInput* codeStream,
                                                 hpatch_StreamPos_t code_begin,
                                                 hpatch_StreamPos_t code_end){
        const size_t kBufSize=kDecompressBufSize;
        _brotli_TDecompress* self=0;
        assert(code_begin<code_end);
        self=(_brotli_TDecompress*)_dec_malloc(sizeof(_brotli_TDecompress)+kBufSize*2);
        if (!self) _dec_memErr_rt();
        memset(self,0,sizeof(_brotli_TDecompress));
        self->codeStream=codeStream;
        self->code_begin=code_begin;
        self->input=self->buf;
        self->output=self->buf+kBufSize;
        self->code_end=code_end;
        self->available_in = 0;
        self->next_in = 0;
        self->available_out = (self->output-self->input);
        self->next_out  =self->output;
        self->data_begin=self->output;
        
        self->s = BrotliDecoderCreateInstance(0,0,0);
        if (!self->s){ free(self); _dec_openErr_rt(); }
        if (!BrotliDecoderSetParameter(self->s, BROTLI_DECODER_PARAM_LARGE_WINDOW, 1u))
            { BrotliDecoderDestroyInstance(self->s); free(self); _dec_openErr_rt(); }
        return self;
    }
    static hpatch_BOOL _brotli_close(struct hpatch_TDecompress* decompressPlugin,
                                     hpatch_decompressHandle decompressHandle){
        _brotli_TDecompress* self=(_brotli_TDecompress*)decompressHandle;
        if (!self) return hpatch_TRUE;
        _dec_onDecErr_up();
        BrotliDecoderDestroyInstance(self->s);
        free(self);
        return hpatch_TRUE;
    }
    static hpatch_BOOL _brotli_decompress_part(hpatch_decompressHandle decompressHandle,
                                               unsigned char* out_part_data,unsigned char* out_part_data_end){
        _brotli_TDecompress* self=(_brotli_TDecompress*)decompressHandle;
        while (out_part_data<out_part_data_end) {
            size_t dataLen=(self->next_out-self->data_begin);
            if (dataLen>0){
                if (dataLen>(size_t)(out_part_data_end-out_part_data))
                    dataLen=(out_part_data_end-out_part_data);
                memcpy(out_part_data,self->data_begin,dataLen);
                out_part_data+=dataLen;
                self->data_begin+=dataLen;
            }else{
                BrotliDecoderResult ret;
                if (self->available_in==0) {
                    self->available_in=(self->output-self->input);
                    if (self->available_in>self->code_end-self->code_begin)
                        self->available_in=(size_t)(self->code_end-self->code_begin);
                    if (self->available_in>0){
                        if (!self->codeStream->read(self->codeStream,self->code_begin,(unsigned char*)self->input,
                                                    self->input+self->available_in))
                            return hpatch_FALSE;
                        self->code_begin+=self->available_in;
                    }
                    self->next_in=self->input;
                }
                self->available_out = (self->output-self->input);
                self->next_out  =self->output;
                self->data_begin=self->output;
                ret=BrotliDecoderDecompressStream(self->s,&self->available_in,&self->next_in,
                                                  &self->available_out,&self->next_out, 0);
                switch (ret){
                    case BROTLI_DECODER_RESULT_SUCCESS:
                    case BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT: {
                        if (self->next_out==self->data_begin) _dec_onDecErr_rt();
                    } break;  
                    case BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT: {
                        if (self->code_end==self->code_begin) _dec_onDecErr_rt();
                    } break;            
                    default:
                        _dec_onDecErr_rt();
                }
            }
        }
        return hpatch_TRUE;
    }
    static hpatch_TDecompress brotliDecompressPlugin={_brotli_is_can_open,_brotli_open,
                                                      _brotli_close,_brotli_decompress_part};
#endif//_CompressPlugin_brotli


#ifdef  _CompressPlugin_lzham
#if (_IsNeedIncludeDefaultCompressHead)
#   include "lzham.h" // "lzham_codec/include/lzham.h" https://github.com/richgel999/lzham_codec
#endif
    typedef struct _lzham_TDecompress{
        const struct hpatch_TStreamInput* codeStream;
        hpatch_StreamPos_t code_begin;
        hpatch_StreamPos_t code_end;
        
        unsigned char*        input;
        unsigned char*        output;
        size_t                available_in;
        size_t                available_out;
        const unsigned char*  next_in;
        unsigned char*        next_out;
        unsigned char*        data_begin;
        lzham_decompress_state_ptr s;
        hpatch_dec_error_t    decError;
        unsigned char       buf[1];
    } _lzham_TDecompress;
    static hpatch_BOOL _lzham_is_can_open(const char* compressType){
        return (0==strcmp(compressType,"lzham"));
    }
    static hpatch_decompressHandle  _lzham_open(hpatch_TDecompress* decompressPlugin,
                                                hpatch_StreamPos_t dataSize,
                                                const hpatch_TStreamInput* codeStream,
                                                hpatch_StreamPos_t code_begin,
                                                hpatch_StreamPos_t code_end){
        const size_t kBufSize=kDecompressBufSize;
        lzham_decompress_params params;
        unsigned char  dict_bits;
        _lzham_TDecompress* self=0;
        assert(code_begin<code_end);
        {//load head
            if (code_end-code_begin<1) _dec_openErr_rt();
            if (!codeStream->read(codeStream,code_begin,&dict_bits,(&dict_bits)+1))
                return 0;
            ++code_begin;
        }

        self=(_lzham_TDecompress*)_dec_malloc(sizeof(_lzham_TDecompress)+kBufSize*2);
        if (!self) _dec_memErr_rt();
        memset(self,0,sizeof(_lzham_TDecompress));
        self->codeStream=codeStream;
        self->code_begin=code_begin;
        self->input=self->buf;
        self->output=self->buf+kBufSize;
        self->code_end=code_end;
        self->available_in = 0;
        self->next_in = 0;
        self->available_out = (self->output-self->input);
        self->next_out  =self->output;
        self->data_begin=self->output;

        memset(&params, 0, sizeof(params));
        params.m_struct_size = sizeof(params);
        params.m_dict_size_log2 = dict_bits;

        self->s = lzham_decompress_init(&params);
        if (!self->s){ free(self); _dec_openErr_rt(); }

        return self;
    }
    static hpatch_BOOL _lzham_close(struct hpatch_TDecompress* decompressPlugin,
                                    hpatch_decompressHandle decompressHandle){
        _lzham_TDecompress* self=(_lzham_TDecompress*)decompressHandle;
        if (!self) return hpatch_TRUE;
        _dec_onDecErr_up();
        lzham_decompress_deinit(self->s);
        free(self);
        return hpatch_TRUE;
    }
    static hpatch_BOOL _lzham_decompress_part(hpatch_decompressHandle decompressHandle,
                                               unsigned char* out_part_data,unsigned char* out_part_data_end){
        _lzham_TDecompress* self=(_lzham_TDecompress*)decompressHandle;
        while (out_part_data<out_part_data_end) {
            size_t dataLen=(self->next_out-self->data_begin);
            if (dataLen>0){
                if (dataLen>(size_t)(out_part_data_end-out_part_data))
                    dataLen=(out_part_data_end-out_part_data);
                memcpy(out_part_data,self->data_begin,dataLen);
                out_part_data+=dataLen;
                self->data_begin+=dataLen;
            }else{
                lzham_decompress_status_t ret;
                if (self->available_in==0) {
                    self->available_in=(self->output-self->input);
                    if (self->available_in>self->code_end-self->code_begin)
                        self->available_in=(size_t)(self->code_end-self->code_begin);
                    if (self->available_in>0){
                        if (!self->codeStream->read(self->codeStream,self->code_begin,(unsigned char*)self->input,
                                                    self->input+self->available_in))
                            return hpatch_FALSE;
                        self->code_begin+=self->available_in;
                    }
                    self->next_in=self->input;
                }
                {
                    size_t available_in_back=self->available_in;
                    self->available_out = (self->output-self->input);
                    ret=lzham_decompress(self->s,self->next_in,&self->available_in,
                                         self->output,&self->available_out,(self->code_begin==self->code_end));
                    self->next_out=self->output+self->available_out;
                    self->next_in+=self->available_in;
                    self->available_in=available_in_back-self->available_in;
                    self->available_out=(self->output-self->input) - self->available_out;
                    self->data_begin=self->output;
                }
                switch (ret){
                    case LZHAM_DECOMP_STATUS_SUCCESS:
                    case LZHAM_DECOMP_STATUS_HAS_MORE_OUTPUT:
                    case LZHAM_DECOMP_STATUS_NOT_FINISHED: {
                        if (self->next_out==self->data_begin) _dec_onDecErr_rt();
                    } break;
                    case LZHAM_DECOMP_STATUS_NEEDS_MORE_INPUT: {
                        if (self->code_end==self->code_begin) _dec_onDecErr_rt();
                    } break;            
                    default:
                        _dec_onDecErr_rt();
                }
            }
        }
        return hpatch_TRUE;
    }
    static hpatch_TDecompress lzhamDecompressPlugin={_lzham_is_can_open,_lzham_open,
                                                     _lzham_close,_lzham_decompress_part};
#endif//_CompressPlugin_lzham


#ifdef _CompressPlugin_tuz
#if (_IsNeedIncludeDefaultCompressHead)
#   include "tuz_dec.h" // "tinyuz/decompress/tuz_dec.h" https://github.com/sisong/tinyuz
#endif
    typedef struct _tuz_TDecompress{
        const struct hpatch_TStreamInput* codeStream;
        hpatch_StreamPos_t code_begin;
        hpatch_StreamPos_t code_end;
        tuz_byte*   dec_mem;
        tuz_TStream s;
        hpatch_dec_error_t decError;
    } _tuz_TDecompress;

    static tuz_BOOL _tuz_TDecompress_read_code(tuz_TInputStreamHandle listener,
                                               tuz_byte* out_code,tuz_size_t* code_size){
        _tuz_TDecompress* self=(_tuz_TDecompress*)listener;
        tuz_size_t r_size=*code_size;
        hpatch_StreamPos_t s_size=self->code_end-self->code_begin;
        if (r_size>s_size){
            r_size=(tuz_size_t)s_size;
            *code_size=r_size;
        }
        if (!self->codeStream->read(self->codeStream,self->code_begin,
                                    out_code,out_code+r_size)) return tuz_FALSE;
        self->code_begin+=r_size;
        return tuz_TRUE;
    }

    static hpatch_BOOL _tuz_is_can_open(const char* compressType){
        return (0==strcmp(compressType,"tuz"));
    }
    static hpatch_decompressHandle  _tuz_open(hpatch_TDecompress* decompressPlugin,
                                              hpatch_StreamPos_t dataSize,
                                              const hpatch_TStreamInput* codeStream,
                                              hpatch_StreamPos_t code_begin,
                                              hpatch_StreamPos_t code_end){
        tuz_size_t dictSize;
        _tuz_TDecompress* self=0;
        self=(_tuz_TDecompress*)_dec_malloc(sizeof(_tuz_TDecompress));
        if (!self) _dec_memErr_rt();
        self->dec_mem=0;
        self->codeStream=codeStream;
        self->code_begin=code_begin;
        self->code_end=code_end;
        self->decError=hpatch_dec_ok;
        dictSize=tuz_TStream_read_dict_size(self,_tuz_TDecompress_read_code);
        if (((tuz_size_t)(dictSize-1))>=tuz_kMaxOfDictSize) { free(self); _dec_openErr_rt(); }
        self->dec_mem=(tuz_byte*)_dec_malloc(dictSize+kDecompressBufSize);
        if (self->dec_mem==0){ free(self); _dec_memErr_rt(); }
        if (tuz_OK!=tuz_TStream_open(&self->s,self,_tuz_TDecompress_read_code,
                                     self->dec_mem,dictSize,kDecompressBufSize)){
            free(self->dec_mem); free(self); _dec_openErr_rt(); }
        return self;
    }
    static hpatch_BOOL _tuz_close(struct hpatch_TDecompress* decompressPlugin,
                                  hpatch_decompressHandle decompressHandle){
        _tuz_TDecompress* self=(_tuz_TDecompress*)decompressHandle;
        if (!self) return hpatch_TRUE;
        _dec_onDecErr_up();
        if (self->dec_mem) free(self->dec_mem);
        free(self);
        return hpatch_TRUE;
    }
    static hpatch_BOOL _tuz_decompress_part(hpatch_decompressHandle decompressHandle,
                                            unsigned char* out_part_data,unsigned char* out_part_data_end){
        tuz_TResult ret;
        _tuz_TDecompress* self=(_tuz_TDecompress*)decompressHandle;
        size_t  out_size=out_part_data_end-out_part_data;
        tuz_size_t data_size=(tuz_size_t)out_size;
        assert(data_size==out_size);
        ret=tuz_TStream_decompress_partial(&self->s,out_part_data,&data_size);
        if (!((ret<=tuz_STREAM_END)&&(data_size==out_size)))
            _dec_onDecErr_rt();
        return hpatch_TRUE;
    }
    static hpatch_TDecompress tuzDecompressPlugin={_tuz_is_can_open,_tuz_open,
                                                   _tuz_close,_tuz_decompress_part};
#endif//_CompressPlugin_tuz

#endif
