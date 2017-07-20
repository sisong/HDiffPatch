//diffz_demo.cpp
// diff with compress plugin
//
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

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "libHDiffPatch/HDiff/diff.h"
#include "libHDiffPatch/HPatch/patch.h"
typedef unsigned char   TByte;

void readFile(std::vector<TByte>& data,const char* fileName){
    std::ifstream file(fileName, std::ios::in | std::ios::binary | std::ios::ate);
    std::streampos file_length=file.tellg();
    file.seekg(0,std::ios::beg);
    size_t needRead=(size_t)file_length;
    if ((file_length<0)||((std::streamsize)needRead!=(std::streamsize)file_length)) {
        file.close();
        exit(1);
    }
    data.resize(needRead);
    file.read((char*)&data[0], needRead);
    std::streamsize readed=file.gcount();
    file.close();
    if ((std::streamsize)needRead!=readed)  exit(1);
}

void writeFile(const std::vector<TByte>& data,const char* fileName){
    std::ofstream file(fileName, std::ios::out | std::ios::binary | std::ios::trunc);
    file.write((const char*)&data[0], data.size());
    file.close();
}

//===== select compress plugin =====
//#define _CompressPlugin_no
//#define _CompressPlugin_zlib
#define _CompressPlugin_bz2
//#define _CompressPlugin_lzma


#ifdef __cplusplus
extern "C" {
#endif

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
                                  const unsigned char* data,const unsigned char* data_end,
                                  unsigned char* out_code,unsigned char* out_code_end){
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
    hdiff_TCompress zlibCompressPlugin={_zlib_compressType,_zlib_maxCompressedSize,_zlib_compress};
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
                                 const unsigned char* data,const unsigned char* data_end,
                                 unsigned char* out_code,unsigned char* out_code_end){
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
    hdiff_TCompress bz2CompressPlugin={_bz2_compressType,_bz2_maxCompressedSize,_bz2_compress};
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
                                  const unsigned char* data,const unsigned char* data_end,
                                  unsigned char* out_code,unsigned char* out_code_end){
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
    hdiff_TCompress lzmaCompressPlugin={_lzma_compressType,_lzma_maxCompressedSize,_lzma_compress};
#endif//_CompressPlugin_lzma
    
#ifdef __cplusplus
}
#endif




int main(int argc, const char * argv[]){
    clock_t time0=clock();
    if (argc!=4) {
        std::cout<<"diffz command line parameter:\n oldFileName newFileName outDiffFileName\n";
        return 0;
    }
    const char* oldFileName=argv[1];
    const char* newFileName=argv[2];
    const char* outDiffFileName=argv[3];
    std::cout<<"old:\"" <<oldFileName<< "\"\nnew:\""<<newFileName<<"\"\nout:\""<<outDiffFileName<<"\"\n";
    
    std::vector<TByte> oldData; readFile(oldData,oldFileName);
    std::vector<TByte> newData; readFile(newData,newFileName);
    const size_t oldDataSize=oldData.size();
    const size_t newDataSize=newData.size();
    
    
#ifdef  _CompressPlugin_no
    const hdiff_TCompress* compressPlugin=hdiff_kNocompressPlugin;
#endif
#ifdef  _CompressPlugin_zlib
    const hdiff_TCompress* compressPlugin=&zlibCompressPlugin;
#endif
#ifdef  _CompressPlugin_bz2
    const hdiff_TCompress* compressPlugin=&bz2CompressPlugin;
#endif
#ifdef  _CompressPlugin_lzma
    const hdiff_TCompress* compressPlugin=&lzmaCompressPlugin;
#endif

    std::vector<TByte> diffData;
    TByte* newData0=newData.empty()?0:&newData[0];
    const TByte* oldData0=oldData.empty()?0:&oldData[0];
    clock_t time1=clock();
    create_compress_diff(newData0,newData0+newDataSize,oldData0,oldData0+oldDataSize,
                         diffData,compressPlugin);
    clock_t time2=clock();
    /*if (!check_diffz(newData_begin,newData_begin+newDataSize,oldData_begin,oldData_begin+oldDataSize, &diffData[0]+kNewDataSize, &diffData[0]+diffData.size(),?)){
        std::cout<<"  patch check diffz data error!!!\n";
        exit(2);
    }else{
        std::cout<<"  patch check diffz data ok!\n";
    }*/
    writeFile(diffData,outDiffFileName);
    clock_t time3=clock();
    std::cout<<"  out diffz file ok!\n";
    std::cout<<"oldDataSize : "<<oldDataSize<<"\nnewDataSize : "<<newDataSize<<"\ndiffDataSize: "<<diffData.size()<<"\n";
    std::cout<<"\ndiffz   time:"<<(time2-time1)*(1000.0/CLOCKS_PER_SEC)<<" ms\n";
    std::cout<<"all run time:"<<(time3-time0)*(1000.0/CLOCKS_PER_SEC)<<" ms\n";
    
    return 0;
}

