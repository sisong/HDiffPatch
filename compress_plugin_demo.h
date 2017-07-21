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

#include "libHDiffPatch/HDiff/diff.h"

#ifdef  _CompressPlugin_zlib
#include "zlib.h"
    static const char*  _zlib_compressType(const hdiff_TCompress* compressPlugin){
        static const char* kCompressType="zlib";
        return kCompressType;
    }
    static size_t  _zlib_maxCompressedSize(const hdiff_TCompress* compressPlugin,size_t dataSize){
        return dataSize*5/4+16*1024;
    }
    static size_t  _zlib_compress(const hdiff_TCompress* compressPlugin,
                                  unsigned char* out_code,unsigned char* out_code_end,
                                  const unsigned char* data,const unsigned char* data_end){
        z_stream c_stream;
        c_stream.zalloc = (alloc_func)0;
        c_stream.zfree = (free_func)0;
        c_stream.opaque = (voidpf)0;
        c_stream.next_in = (Bytef*)data;
        c_stream.avail_in = (uInt)(data_end-data);
        assert(c_stream.avail_in==(data_end-data));
        c_stream.next_out = (Bytef*)out_code;
        c_stream.avail_out = (uInt)(out_code_end-out_code);
        assert(c_stream.avail_out==(out_code_end-out_code));
        int ret = deflateInit2(&c_stream, Z_BEST_COMPRESSION,Z_DEFLATED, 31,8, Z_DEFAULT_STRATEGY);
        if(ret != Z_OK){
            std::cout <<"\ndeflateInit2 error\n";
            return 0;
        }
        ret = deflate(&c_stream, Z_FINISH);
        if (ret != Z_STREAM_END){
            deflateEnd(&c_stream);
            std::cout <<"\nret != Z_STREAM_END err="<< ret <<std::endl;
            return 0;
        }
        
        uLong codeLen = c_stream.total_out;
        ret = deflateEnd(&c_stream);
        if (ret != Z_OK){
            std::cout <<"\ndeflateEnd error\n";
            return 0;
        }
        return codeLen;
    }
    static hdiff_TCompress zlibCompressPlugin={_zlib_compressType,_zlib_maxCompressedSize,_zlib_compress};
#endif//_CompressPlugin_zlib
    
#ifdef  _CompressPlugin_bz2
#include "bzlib.h"
    static const char*  _bz2_compressType(const hdiff_TCompress* compressPlugin){
        static const char* kCompressType="bz2";
        return kCompressType;
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
        if(ret != BZ_OK){
            std::cout <<"\nBZ2_bzBuffToBuffCompress error\n";
            return 0;
        }
        return destLen;
    }
    static hdiff_TCompress bz2CompressPlugin={_bz2_compressType,_bz2_maxCompressedSize,_bz2_compress};
#endif//_CompressPlugin_bz2
    
#ifdef  _CompressPlugin_lzma
#include "../lzma/C/LzmaEnc.h"
    static const char*  _lzma_compressType(const hdiff_TCompress* compressPlugin){
        static const char* kCompressType="lzma";
        return kCompressType;
    }
    static size_t  _lzma_maxCompressedSize(const hdiff_TCompress* compressPlugin,size_t dataSize){
        return dataSize*5/4+16*1024;
    }
    
    static void * __lzma_Alloc(ISzAllocPtr p, size_t size){
        return malloc(size);//new char[size];//
    }
    static void __lzma_Free(ISzAllocPtr p, void *address){
        free(address);      //delete [] (char*)address;//
    }
    static size_t  _lzma_compress(const hdiff_TCompress* compressPlugin,
                                  unsigned char* out_code,unsigned char* out_code_end,
                                  const unsigned char* data,const unsigned char* data_end){
        ISzAlloc alloc={__lzma_Alloc,__lzma_Free};
        CLzmaEncHandle p = LzmaEnc_Create(&alloc);
        if (!p){
            std::cout <<"\nLzmaEnc_Create error\n";
            return 0;
        }
        CLzmaEncProps props;
        LzmaEncProps_Init(&props);
        LzmaEncProps_Normalize(&props);
        props.level=7;
        props.dictSize=1<<22;
        SRes res= LzmaEnc_SetProps(p,&props);
        if (res != SZ_OK){
            std::cout <<"\nLzmaEnc_SetProps error\n";
            return 0;
        }
        size_t destLen=(out_code_end-out_code);
        res = LzmaEnc_MemEncode(p,out_code,&destLen,data,data_end-data,0,0,&alloc,&alloc);
        LzmaEnc_Destroy(p, &alloc, &alloc);
        if (res != SZ_OK){
            std::cout <<"\nLzmaEnc_MemEncode error\n";
            return 0;
        }
        return destLen;
    }
    static hdiff_TCompress lzmaCompressPlugin={_lzma_compressType,_lzma_maxCompressedSize,_lzma_compress};
#endif//_CompressPlugin_lzma

#endif
