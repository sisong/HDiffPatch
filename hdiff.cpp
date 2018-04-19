//hdiff.cpp
// diff tool
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
#include <string.h>
#include <stdlib.h>
#include "libHDiffPatch/HDiff/diff.h"
#include "libHDiffPatch/HPatch/patch.h"
#include "_clock_for_demo.h"
#include "_atosize.h"
#include "file_for_patch.h"

#ifndef _IS_NEED_PATCH_CHECK
#   define _IS_NEED_PATCH_CHECK 1
#endif

#ifndef _IS_NEED_MAIN
#   define  _IS_NEED_MAIN 1
#endif
#ifndef _IS_NEED_ORIGINAL
#   define  _IS_NEED_ORIGINAL 1
#endif
#ifndef _IS_NEED_ALL_CompressPlugin
#   define _IS_NEED_ALL_CompressPlugin 1
#endif
#if (_IS_NEED_ALL_CompressPlugin)
//===== select needs decompress plugins or change to your plugin=====
#   define _CompressPlugin_zlib  // memroy requires less
#   define _CompressPlugin_bz2
#   define _CompressPlugin_lzma  // better compresser
#   define _CompressPlugin_lz4   // faster compresser/decompresser
#   define _CompressPlugin_lz4hc // faster decompresser
#   define _CompressPlugin_zstd  // better compresser / faster decompresser
#endif

#include "compress_plugin_demo.h"
#include "decompress_plugin_demo.h"

#define _error_return(info){ \
    if (strlen(info)>0)      \
        printf("\n  ERROR: %s\n",(info)); \
    exitCode=1; \
    goto clear; \
}

#define _check_error(is_error,errorInfo){ \
    if (is_error){  \
        exitCode=1; \
        printf("\n  ERROR: %s\n",(errorInfo)); \
    } \
}

static void printUsage(){
    printf("usage: hdiff [-m[-matchScore]|-s[-matchBlockSize]] [-c-compressType[-compressLevel]] "
#if (_IS_NEED_ORIGINAL)
           "[-o] "
#endif
           "oldFile newFile outDiffFile\n"
           "memory options:\n"
           "  -m-matchScore\n"
           "      all file load into Memory, DEFAULT, with matchScore;\n"
           "      best diffFileSize, max memory requires O(oldFileSize*9+newFileSize),\n"
           "      matchScore>=0, DEFAULT 6, recommended bin: 0--4 text: 4--9 etc... \n"
           "  -s-matchBlockSize\n"
           "      all file load as Stream, with matchBlockSize;\n"
           "      fast, memory requires O(oldSize*16/matchBlockSize+matchBlockSize*5),\n"
           "      matchBlockSize>=2, DEFAULT 128, recommended 2^3--2^14 32k 1m etc...\n"
           "special options:\n"
           "  -c-compressType-compressLevel \n"
           "      set diffFile Compress type & level, otherwise DEFAULT uncompress;\n"
           "      compress type & level can:\n"
           "        (reference: https://github.com/sisong/lzbench/blob/master/lzbench171_sorted.md )\n"
#ifdef _CompressPlugin_zlib
           "        -zlib[-{1..9}]              DEFAULT level 9\n"
#endif
#ifdef _CompressPlugin_bz2
           "        -bzip2[-{1..9}]             DEFAULT level 9\n"
#endif
#ifdef _CompressPlugin_lzma
           "        -lzma[-{0..9}[-dictSize]]   DEFAULT level 7\n"
           "            dictSize can like 4096 or 4k or 4m or 128m etc..., DEFAULT 4m\n"
#endif
#ifdef _CompressPlugin_lz4
           "        -lz4\n"
#endif
#ifdef _CompressPlugin_lz4hc
           "        -lz4hc[-{1..12}]            DEFAULT level 11\n"
#endif
#ifdef _CompressPlugin_zstd
           "        -zstd[-{0..22}]             DEFAULT level 20\n"
#endif
#if (_IS_NEED_ORIGINAL)
           "  -o  Original diff, unsupport run with -s or -c, DEPRECATED;\n"
           "      compatible with \"diff_demo.c\",\n"
           "      diffFile must patch by \"patch_demo.cpp\" or \"hpatch -o ...\"\n"
#endif
           );
}

typedef enum THDiffResult {
    HDIFF_SUCCESS=0,
    HDIFF_OPTIONS_ERROR,
    HDIFF_OPENREAD_ERROR,
    HDIFF_OPENWRITE_ERROR,
    HDIFF_FILECLOSE_ERROR,
    HDIFF_MEM_ERROR,
    HDIFF_DIFF_ERROR,
    HDIFF_PATCH_ERROR,
} THDiffResult;

int hdiff_cmd_line(int argc, const char * argv[]);

int hdiff(const char* oldFileName,const char* newFileName,const char* outDiffFileName,
          hpatch_BOOL isOriginal,hpatch_BOOL isLoadAll,size_t matchValue,
          hdiff_TStreamCompress* streamCompressPlugin,hdiff_TCompress* compressPlugin,hpatch_TDecompress* decompressPlugin);

#if (_IS_NEED_MAIN)
int main(int argc, const char * argv[]){
    return  hdiff_cmd_line(argc,argv);
}
#endif


hpatch_inline static const char* find(const char* str,char c){
    const char* result=strchr(str,c);
    return (result!=0)?result:(str+strlen(str));
}

static bool _trySetCompress(hdiff_TStreamCompress** streamCompressPlugin,
                            hdiff_TCompress** compressPlugin,hpatch_TDecompress** decompressPlugin,
                            hdiff_TStreamCompress* _streamCompressPlugin,
                            hdiff_TCompress* _compressPlugin,hpatch_TDecompress* _decompressPlugin,
                            const char* ptype,const char* ptypeEnd,const char* ctype,
                            size_t* compressLevel=0,size_t levelMin=0,size_t levelMax=0,size_t levelDefault=0,
                            size_t* dictSize=0,size_t dictSizeMin=0,size_t dictSizeMax=0,size_t dictSizeDefault=0){
    if ((*compressPlugin)!=0) return true;
    const size_t ctypeLen=strlen(ctype);
    if ((ctypeLen!=ptypeEnd-ptype)||(0!=strncmp(ptype,ctype,ctypeLen))) return true;
    
    *streamCompressPlugin=_streamCompressPlugin;
    *compressPlugin=_compressPlugin;
    *decompressPlugin=_decompressPlugin;
    if ((compressLevel)&&(ptypeEnd[0]=='-')){
        const char* plevel=ptypeEnd+1;
        const char* plevelEnd=find(plevel,'-');
        if (!a_to_size(plevel,plevelEnd-plevel,compressLevel)) return false; //error
        if (*compressLevel<levelMin) *compressLevel=levelMin;
        else if (*compressLevel>levelMax) *compressLevel=levelMax;
        if ((dictSize)&&(plevelEnd[0]=='-')){
            const char* pdictSize=ptypeEnd+1;
            const char* pdictSizeEnd=find(plevel,'-');
            if (!kmg_to_size(pdictSize,pdictSizeEnd-pdictSize,dictSize)) return false; //error
            if (*dictSize<dictSizeMin) *dictSize=dictSizeMin;
            else if (*dictSize>dictSizeMax) *dictSize=dictSizeMax;
        }else{
            if (plevelEnd[0]!='\0') return false; //error
            if (dictSize) *dictSize=dictSizeDefault;
        }
    }else{
        if (ptypeEnd[0]!='\0') return false; //error
        if (compressLevel) *compressLevel=levelDefault;
        if (dictSize) *dictSize=dictSizeDefault;
    }
    return true;
}


#define _options_check(value,errorInfo){ \
    if (!(value)) { printf(errorInfo); printUsage(); return HDIFF_OPTIONS_ERROR; } }


int hdiff_cmd_line(int argc, const char * argv[]){
    hpatch_BOOL isOriginal=hpatch_FALSE;
    hpatch_BOOL isLoadAll=hpatch_TRUE;
    size_t      matchValue=0;
    size_t      compressLevel=0;
    size_t      dictSize=0;
    hdiff_TStreamCompress*  streamCompressPlugin=0;
    hdiff_TCompress*        compressPlugin=0;
    hpatch_TDecompress*     decompressPlugin=0;
    _options_check(argc>=4,"argc ERROR!\n");
    for (int i=1; i<argc-3; ++i) {
        const char* op=argv[i];
        _options_check((op!=0)&&(op[0]=='-'),"options ERROR!\n");
        switch (op[1]) {
#if (_IS_NEED_ORIGINAL)
            case 'o':{
                _options_check((!isOriginal)&&(op[2]=='\0'),"options -o ERROR!\n");
                isOriginal=hpatch_TRUE;
            } break;
#endif
            case 'm':{
                _options_check((isLoadAll)&&(matchValue==0),"options -m ERROR!\n");
                isLoadAll=hpatch_TRUE;
                if (op[2]=='-'){
                    const char* pnum=op+3;
                    _options_check(a_to_size(pnum,strlen(pnum),&matchValue),"options -m-? ERROR!\n");
                }else{
                    _options_check(op[2]=='\0',"options -m ERROR!\n");
                }
            } break;
            case 's':{
                _options_check((isLoadAll)&&(matchValue==0),"options -s ERROR!\n");
                isLoadAll=hpatch_FALSE;
                if (op[2]=='-'){
                    const char* pnum=op+3;
                    _options_check(kmg_to_size(pnum,strlen(pnum),&matchValue),"options -s-? ERROR!\n");
                }else{
                    _options_check(op[2]=='\0',"options -s ERROR!\n");
                }
            } break;
            case 'c':{
                _options_check((compressPlugin==0)&&(op[2]=='-'),"options -c ERROR!\n");
                const char* ptype=op+3;
                const char* ptypeEnd=find(ptype,'-');
#ifdef _CompressPlugin_zlib
                _options_check(_trySetCompress(&streamCompressPlugin,&compressPlugin,&decompressPlugin,
                                               &zlibStreamCompressPlugin,&zlibCompressPlugin,&zlibDecompressPlugin,
                                               ptype,ptypeEnd,"zlib" ,&compressLevel,1,9,9),"options -c-zlib-? ERROR!\n");
                if (compressPlugin==&zlibCompressPlugin) { zlib_compress_level=(int)compressLevel; }
#endif
#ifdef _CompressPlugin_bz2
                _options_check(_trySetCompress(&streamCompressPlugin,&compressPlugin,&decompressPlugin,
                                               &bz2StreamCompressPlugin,&bz2CompressPlugin,&bz2DecompressPlugin,
                                               ptype,ptypeEnd,"bzip2",&compressLevel,1,9,9),"options -c-bzip2-? ERROR!\n");
                if (compressPlugin==&bz2CompressPlugin) { bz2_compress_level=(int)compressLevel; }
#endif
#ifdef _CompressPlugin_lzma
                _options_check(_trySetCompress(&streamCompressPlugin,&compressPlugin,&decompressPlugin,
                                               &lzmaStreamCompressPlugin,&lzmaCompressPlugin,&lzmaDecompressPlugin,
                                               ptype,ptypeEnd,"lzma" ,&compressLevel,0,9,7,
                                               &dictSize,4<<10,1<<27,4<<20),"options -c-lzma-? ERROR!\n");
                if (compressPlugin==&lzmaCompressPlugin) {
                    lzma_compress_level=(int)compressLevel; lzma_dictSize=(UInt32)dictSize; }
#endif
#ifdef _CompressPlugin_lz4
                _options_check(_trySetCompress(&streamCompressPlugin,&compressPlugin,&decompressPlugin,
                                               &lz4StreamCompressPlugin,&lz4CompressPlugin,&lz4DecompressPlugin,
                                               ptype,ptypeEnd,"lz4"  ),"options -c-lz4-? ERROR!\n");
#endif
#ifdef _CompressPlugin_lz4hc
                _options_check(_trySetCompress(&streamCompressPlugin,&compressPlugin,&decompressPlugin,
                                               &lz4hcStreamCompressPlugin,&lz4hcCompressPlugin,&lz4DecompressPlugin,
                                               ptype,ptypeEnd,"lz4hc",&compressLevel,1,12,11),"options -c-lz4hc-? ERROR!\n");
                if (compressPlugin==&lz4hcCompressPlugin) { lz4hc_compress_level=(int)compressLevel; }
#endif
#ifdef _CompressPlugin_zstd
                _options_check(_trySetCompress(&streamCompressPlugin,&compressPlugin,&decompressPlugin,
                                               &zstdStreamCompressPlugin,&zstdCompressPlugin,&zstdDecompressPlugin,
                                               ptype,ptypeEnd,"zstd" ,&compressLevel,0,22,20),"options -c-zstd-? ERROR!\n");
                if (compressPlugin==&zstdCompressPlugin) { zstd_compress_level=(int)compressLevel; }
#endif
                _options_check(compressPlugin!=0,"options -c-? ERROR!\n");
            } break;
            default: {
                _options_check(hpatch_FALSE,"options -? ERROR!\n");
            } break;
        }//swich
    }
    
    if (isOriginal){
        _options_check((isLoadAll)&&(compressPlugin==0),"options -o unsupport run with -s or -c ERROR!\n");
    }
    
    {
        const char* oldFileName=argv[argc-3];
        const char* newFileName=argv[argc-2];
        const char* outDiffFileName=argv[argc-1];
        return hdiff(oldFileName,newFileName,outDiffFileName,isOriginal,isLoadAll,matchValue,
                     streamCompressPlugin,compressPlugin,decompressPlugin);
    }
}


static hpatch_BOOL readFileAll(TByte** out_pdata,size_t* out_dataSize,const char* fileName){
    size_t dataSize;
    hpatch_StreamPos_t  file_length=0;
    FILE*               file=0;
    if (!fileOpenForRead(fileName,&file,&file_length)) _file_error(file);
    
    assert((*out_pdata)==0);
    dataSize=(size_t)file_length;
    if (dataSize!=file_length) _file_error(file);
    *out_pdata=(TByte*)malloc(dataSize);
    if (*out_pdata==0) _file_error(file);
    *out_dataSize=dataSize;
    
    if (!fileRead(file,*out_pdata,(*out_pdata)+dataSize)) _file_error(file);
    return fileClose(&file);
}

#define _free_mem(p) { if (p) { free(p); p=0; } }

static hpatch_BOOL writeFileAll(const TByte* pdata,size_t dataSize,const char* outFileName){
    FILE*   file=0;
    if (!fileOpenForCreateOrReWrite(outFileName,&file)) _file_error(file);
    if (!fileWrite(file,pdata,pdata+dataSize)) _file_error(file);
    return fileClose(&file);
}

#define  check(value,errorType,errorInfo) { \
    if (!(value)){ printf(errorInfo);  \
        if (result==HDIFF_SUCCESS) result=errorType; if (!_isInClear){ goto clear; } } }

static int hdiff_m(const char* oldFileName,const char* newFileName,const char* outDiffFileName,
                   hpatch_BOOL isOriginal,size_t matchScore,
                   hdiff_TCompress* compressPlugin,hpatch_TDecompress* decompressPlugin){
    int     result=HDIFF_SUCCESS;
    int     _isInClear=hpatch_FALSE;
    TByte* oldData=0; size_t oldDataSize=0;
    TByte* newData=0; size_t newDataSize=0;
    std::vector<TByte> diffData;
    check(readFileAll(&oldData,&oldDataSize,oldFileName),HDIFF_OPENREAD_ERROR,"open oldFile ERROR!");
    check(readFileAll(&newData,&newDataSize,newFileName),HDIFF_OPENREAD_ERROR,"open newFile ERROR!");
    std::cout<<"oldDataSize : "<<oldDataSize<<"\nnewDataSize : "<<newDataSize<<"\n";
    try {
        if (isOriginal)
            create_diff(newData,newData+newDataSize,oldData,oldData+oldDataSize,diffData,(int)matchScore);
        else
            create_compressed_diff(newData,newData+newDataSize,oldData,oldData+oldDataSize,
                                   diffData,compressPlugin,(int)matchScore);
    }catch(const std::exception& e){
        std::cout<<"diff run ERROR! "<<e.what()<<"\n";
        check(hpatch_FALSE,HDIFF_DIFF_ERROR,"");
    }
    std::cout<<"diffDataSize: "<<diffData.size()<<"\n";
    check(writeFileAll(diffData.data(),diffData.size(),outDiffFileName),
          HDIFF_OPENWRITE_ERROR,"open write diffFile ERROR!");
    std::cout<<"  out hdiff file ok!\n";
#if (_IS_NEED_PATCH_CHECK)
    if (isOriginal){
        check(check_diff(newData,newData+newDataSize,oldData,oldData+oldDataSize,
                         diffData.data(),diffData.data()+diffData.size()),
              HDIFF_PATCH_ERROR,"patch check hdiff data ERROR!\n");
    }else{
        check(check_compressed_diff(newData,newData+newDataSize,oldData,oldData+oldDataSize,
                                    diffData.data(),diffData.data()+diffData.size(),decompressPlugin),
              HDIFF_PATCH_ERROR,"patch check hdiff data ERROR!\n");
    }
    std::cout<<"  patch check hdiff data ok!\n";
#endif
clear:
    _isInClear=hpatch_TRUE;
    _free_mem(newData);
    _free_mem(oldData);
    return result;
}

static int hdiff_s(const char* oldFileName,const char* newFileName,const char* outDiffFileName,
                   size_t matchBlockSize,hdiff_TStreamCompress* streamCompressPlugin,hpatch_TDecompress* decompressPlugin){
    int result=HDIFF_SUCCESS;
    int _isInClear=hpatch_FALSE;
    TFileStreamInput  oldData;
    TFileStreamInput  newData;
    TFileStreamOutput diffData;
    TFileStreamInput  diffData_in;
    TFileStreamInput_init(&oldData);
    TFileStreamInput_init(&newData);
    TFileStreamOutput_init(&diffData);
    TFileStreamInput_init(&diffData_in);
    
    check(TFileStreamInput_open(&oldData,oldFileName),HDIFF_OPENREAD_ERROR,"open oldFile ERROR!");
    check(TFileStreamInput_open(&newData,newFileName),HDIFF_OPENREAD_ERROR,"open newFile ERROR!");
    check(TFileStreamOutput_open(&diffData,outDiffFileName,-1),HDIFF_OPENWRITE_ERROR,"open out diffFile ERROR!");
    TFileStreamOutput_setRandomOut(&diffData,hpatch_TRUE);
    std::cout<<"oldDataSize : "<<oldData.base.streamSize<<"\nnewDataSize : "<<newData.base.streamSize<<"\n";
    try{
        create_compressed_diff_stream(&newData.base,&oldData.base, &diffData.base,
                                      streamCompressPlugin,matchBlockSize);
        diffData.base.streamSize=diffData.out_length;
    }catch(const std::exception& e){
        std::cout<<"diff run ERROR! "<<e.what()<<"\n";
        check(hpatch_FALSE,HDIFF_DIFF_ERROR,"");
    }
    check(TFileStreamOutput_close(&diffData),HDIFF_FILECLOSE_ERROR,"out diffFile close ERROR!");
    std::cout<<"diffDataSize: "<<diffData.base.streamSize<<"\n";
    std::cout<<"  out hdiff file ok!\n";
#if (_IS_NEED_PATCH_CHECK)
    //check diff
    check(TFileStreamInput_open(&diffData_in,outDiffFileName),HDIFF_OPENREAD_ERROR,"open check diffFile ERROR!");
    check(check_compressed_diff_stream(&newData.base,&oldData.base,
                                      &diffData_in.base,decompressPlugin),
          HDIFF_PATCH_ERROR,"patch check hdiff data ERROR!!!");
    std::cout<<"  patch check hdiff data ok!\n";
#endif
clear:
    _isInClear=hpatch_TRUE;
    check(TFileStreamOutput_close(&diffData),HDIFF_FILECLOSE_ERROR,"out diffFile close ERROR!");
    check(TFileStreamInput_close(&diffData_in),HDIFF_FILECLOSE_ERROR,"check diffFile close ERROR!");
    check(TFileStreamInput_close(&newData),HDIFF_FILECLOSE_ERROR,"newFile close ERROR!");
    check(TFileStreamInput_close(&oldData),HDIFF_FILECLOSE_ERROR,"oldFile close ERROR!");
    return result;
}

int hdiff(const char* oldFileName,const char* newFileName,const char* outDiffFileName,
          hpatch_BOOL isOriginal,hpatch_BOOL isLoadAll,size_t matchValue,
          hdiff_TStreamCompress* streamCompressPlugin,hdiff_TCompress* compressPlugin,hpatch_TDecompress* decompressPlugin){
    double time0=clock_s();
    std::cout<<"old:\"" <<oldFileName<< "\"\nnew:\""<<newFileName<<"\"\nout:\""<<outDiffFileName<<"\"\n";
    
    const char* compressType="";
    if (isLoadAll){
        if (compressPlugin) compressType=compressPlugin->compressType(compressPlugin);
    }else{
        if (streamCompressPlugin) compressType=streamCompressPlugin->compressType(streamCompressPlugin);
    }
    std::cout<<"hdiff run with "<<(isLoadAll?"":"stream ")<<"compress plugin: \""<<compressType<<"\"\n";
    
    int exitCode;
    if (isLoadAll)
        exitCode=hdiff_m(oldFileName,newFileName,outDiffFileName,isOriginal,matchValue,compressPlugin,decompressPlugin);
    else
        exitCode=hdiff_s(oldFileName,newFileName,outDiffFileName,matchValue,streamCompressPlugin,decompressPlugin);
    double time1=clock_s();
    std::cout<<"\nhdiff  time:"<<(time1-time0)<<" s\n";
    return exitCode;
}

