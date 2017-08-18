//hdiffz.cpp
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
#include <exception>//std::exception
#include "libHDiffPatch/HDiff/diff.h"
#include "libHDiffPatch/HPatch/patch.h"
typedef unsigned char   TByte;

#define _IS_USE_FILE_STREAM_LIMIT_MEMORY  //ON: TODO:

//===== select compress plugin =====
//#define _CompressPlugin_no
#define _CompressPlugin_zlib
//#define _CompressPlugin_bz2
//#define _CompressPlugin_lzma


#ifndef _CompressPlugin_no
#   include "compress_plugin_demo.h"
#   include "decompress_plugin_demo.h"
#endif

#ifdef _IS_USE_FILE_STREAM_LIMIT_MEMORY
#   include "file_for_patch.h"

#define _error_return(info){ \
    if (strlen(info)>0)      \
        printf("\n  %s\n",(info)); \
    exitCode=1; \
    goto clear; \
}

#define _check_error(is_error,errorInfo){ \
    if (is_error){  \
        exitCode=1; \
        printf("\n  %s\n",(errorInfo)); \
    } \
}

#else
void readFile(std::vector<TByte>& data,const char* fileName){
    std::ifstream file(fileName, std::ios::in | std::ios::binary | std::ios::ate);
    std::streampos file_length=file.tellg();
    file.seekg(0,std::ios::beg);
    size_t needRead=(size_t)file_length;
    if ((file_length<0)||((std::streamsize)needRead!=(std::streamsize)file_length)) {
        file.close();
        std::cout<<"\nopen file error!\n";
        exit(1);
    }
    data.resize(needRead);
    file.read((char*)data.data(), needRead);
    std::streamsize readed=file.gcount();
    file.close();
    if ((std::streamsize)needRead!=readed)  exit(1);
}

void writeFile(const std::vector<TByte>& data,const char* fileName){
    std::ofstream file(fileName, std::ios::out | std::ios::binary | std::ios::trunc);
    file.write((const char*)data.data(), data.size());
    file.close();
}
#endif


#ifdef  _CompressPlugin_no
#   ifdef _IS_USE_FILE_STREAM_LIMIT_MEMORY
    hdiff_TStreamCompress* compressPlugin=0;
#   else
    hdiff_TCompress* compressPlugin=0;
#   endif
    hpatch_TDecompress* decompressPlugin=0;
#endif
#ifdef  _CompressPlugin_zlib
#   ifdef _IS_USE_FILE_STREAM_LIMIT_MEMORY
    hdiff_TStreamCompress* compressPlugin=&zlibStreamCompressPlugin;
#   else
    hdiff_TCompress* compressPlugin=&zlibCompressPlugin;
#   endif
    hpatch_TDecompress* decompressPlugin=&zlibDecompressPlugin;
#endif
#ifdef  _CompressPlugin_bz2
#   ifdef _IS_USE_FILE_STREAM_LIMIT_MEMORY
    hdiff_TStreamCompress* compressPlugin=&bz2StreamCompressPlugin;
#   else
    hdiff_TCompress* compressPlugin=&bz2CompressPlugin;
#   endif
    hpatch_TDecompress* decompressPlugin=&bz2DecompressPlugin;
#endif
#ifdef  _CompressPlugin_lzma
#   ifdef _IS_USE_FILE_STREAM_LIMIT_MEMORY
    hdiff_TStreamCompress* compressPlugin=&lzmaStreamCompressPlugin;
#   else
    hdiff_TCompress* compressPlugin=&lzmaCompressPlugin;
#   endif
    hpatch_TDecompress* decompressPlugin=&lzmaDecompressPlugin;
#endif

int main(int argc, const char * argv[]){
    clock_t time0=clock();
    if (argc!=4) {
        std::cout<<"HDiffZ command line parameter:\n oldFileName newFileName outDiffFileName\n";
        exit(1);
    }
    const char* oldFileName=argv[1];
    const char* newFileName=argv[2];
    const char* outDiffFileName=argv[3];
    std::cout<<"old:\"" <<oldFileName<< "\"\nnew:\""<<newFileName<<"\"\nout:\""<<outDiffFileName<<"\"\n";
    
    const char* compressType="";
    if (compressPlugin) compressType=compressPlugin->compressType(compressPlugin);
#ifdef _IS_USE_FILE_STREAM_LIMIT_MEMORY
    std::cout<<"limit memory HDiffZ with stream compress plugin: \""<<compressType<<"\"\n";
#else
    std::cout<<"HDiffZ with compress plugin: \""<<compressType<<"\"\n";
#endif
    
    int exitCode=0;
#ifdef _IS_USE_FILE_STREAM_LIMIT_MEMORY
    clock_t time1=0,time2=0;
    TFileStreamInput  oldData;
    TFileStreamInput  newData;
    TFileStreamOutput diffData;
    TFileStreamInput_init(&oldData);
    TFileStreamInput_init(&newData);
    TFileStreamOutput_init(&diffData);
    if (!TFileStreamInput_open(&oldData,oldFileName)) _error_return("open oldFile error!");
    if (!TFileStreamInput_open(&newData,newFileName)) _error_return("open newFile error!");
    if (!TFileStreamOutput_open(&diffData,outDiffFileName,-1)) _error_return("open out diffFile error!");
    TFileStreamOutput_setRandomOut(&diffData,true);
    std::cout<<"oldDataSize : "<<oldData.base.streamSize<<"\nnewDataSize : "<<newData.base.streamSize<<"\n";
    time1=clock();
    try{
        create_compressed_diff_stream(&newData.base,&oldData.base,
                                      &diffData.base,compressPlugin);
        diffData.base.streamSize=diffData.out_length;
    }catch(const std::exception& e){
        _error_return(e.what());
    }
    //TODO: check_compressed_diff
    std::cout<<"\ndiffDataSize: "<<diffData.out_length<<"\n";
    time2=clock();
clear:
    _check_error(!TFileStreamOutput_close(&diffData),"out diffFile close error!");
    //TODO: if (exitCode==0) std::cout<<"  out HDiffZ file ok!\n";
    _check_error(!TFileStreamInput_close(&newData),"newFile close error!");
    _check_error(!TFileStreamInput_close(&oldData),"oldFile close error!");
    if (exitCode!=0) return exitCode;
#else
    std::vector<TByte> oldData; readFile(oldData,oldFileName);
    std::vector<TByte> newData; readFile(newData,newFileName);
    const size_t oldDataSize=oldData.size();
    const size_t newDataSize=newData.size();
    std::cout<<"oldDataSize : "<<oldDataSize<<"\nnewDataSize : "<<newDataSize<<"\n";

    std::vector<TByte> diffData;
    TByte* newData0=newData.data();
    const TByte* oldData0=oldData.data();
    clock_t time1=clock();
    create_compressed_diff(newData0,newData0+newDataSize,oldData0,oldData0+oldDataSize,
                           diffData,compressPlugin);
    clock_t time2=clock();
    if (!check_compressed_diff(newData0,newData0+newDataSize,oldData0,oldData0+oldDataSize,
                               diffData.data(),diffData.data()+diffData.size(),decompressPlugin)){
        std::cout<<"\n  patch check HDiffZ data error!!!\n";
        exit(1);
    }else{
        std::cout<<"diffDataSize: "<<diffData.size()<<"\n";
        std::cout<<"  patch check HDiffZ data ok!\n";
    }
    writeFile(diffData,outDiffFileName);
    std::cout<<"  out HDiffZ file ok!\n";
#endif
    clock_t time3=clock();
    std::cout<<"\nHDiffZ  time:"<<(time2-time1)*(1000.0/CLOCKS_PER_SEC)<<" ms\n";
    std::cout<<"all run time:"<<(time3-time0)*(1000.0/CLOCKS_PER_SEC)<<" ms\n";
    
    return exitCode;
}

