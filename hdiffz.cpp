//hdiffz.cpp
// diff tool
//
/*
 The MIT License (MIT)
 Copyright (c) 2012-2019 HouSisong
 
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
#include "dirDiffPatch/file_for_dirDiff.h"
#include "dirDiffPatch/dir_diff/dir_diff.h"
#include "dirDiffPatch/dir_patch/dir_patch.h"

#ifndef _IS_NEED_MAIN
#   define  _IS_NEED_MAIN 1
#endif
#ifndef _IS_NEED_ORIGINAL
#   define  _IS_NEED_ORIGINAL 1
#endif

#ifndef _IS_NEED_DEFAULT_CompressPlugin
#   define _IS_NEED_DEFAULT_CompressPlugin 1
#endif
#if (_IS_NEED_ALL_CompressPlugin)
#   undef  _IS_NEED_DEFAULT_CompressPlugin
#   define _IS_NEED_DEFAULT_CompressPlugin 1
#endif
#if (_IS_NEED_DEFAULT_CompressPlugin)
//===== select needs decompress plugins or change to your plugin=====
#   define _CompressPlugin_zlib  // memroy requires less
#   define _CompressPlugin_bz2
#   define _CompressPlugin_lzma  // better compresser
#endif
#if (_IS_NEED_ALL_CompressPlugin)
//===== select needs decompress plugins or change to your plugin=====
#   define _CompressPlugin_lz4   // faster compresser/decompresser
#   define _CompressPlugin_lz4hc // faster decompresser
#   define _CompressPlugin_zstd  // better compresser / faster decompresser
#endif

#include "compress_plugin_demo.h"
#include "decompress_plugin_demo.h"

#define _ChecksumPlugin_crc32
#define _ChecksumPlugin_adler32
#define _ChecksumPlugin_adler64
#define _ChecksumPlugin_adler32f
#define _ChecksumPlugin_adler64f
#include "checksum_plugin_demo.h"

static void printUsage(){
    printf("diff   usage: hdiffz [options] oldPath newPath outDiffFile\n"
           "test   usage: hdiffz    -t     oldPath newPath testDiffFile\n"
           "resave usage: hdiffz [-c-compressType[-compressLevel]] diffFile outDiffFile\n"
           "memory options:\n"
           "  -m[-matchScore]\n"
           "      all file load into Memory, with matchScore; DEFAULT; best diffFileSize;\n"
           "      requires (newFileSize+ oldFileSize*5(or *9 when oldFileSize>=2GB))+O(1) bytes of memory;\n"
           "      matchScore>=0, DEFAULT 6, recommended bin: 0--4 text: 4--9 etc...\n"
           "  -s[-matchBlockSize]\n"
           "      all file load as Stream, with matchBlockSize; fast;\n"
           "      requires O(oldFileSize*16/matchBlockSize+matchBlockSize*5) bytes of memory;\n"
           "      matchBlockSize>=2, DEFAULT 64, recommended 32,48,1k,64k,1m etc...\n"
           "special options:\n"
           "  -c-compressType[-compressLevel]\n"
           "      set outDiffFile Compress type & level, DEFAULT uncompress;\n"
           "      for resave diffFile,recompress diffFile to outDiffFile by new set;\n"
           "      support compress type & level:\n"
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
           "        -lz4                        no level\n"
#endif
#ifdef _CompressPlugin_lz4hc
           "        -lz4hc[-{3..12}]            DEFAULT level 11\n"
#endif
#ifdef _CompressPlugin_zstd
           "        -zstd[-{0..22}]             DEFAULT level 20\n"
#endif
           "  -d  Diff only, do't run patch check, DEFAULT run patch check.\n"
           "  -t  Test only, run patch check, patch(oldPath,testDiffFile)==newPath ? \n"
           "  -D  force run Directory Diff, DEFAULT need oldPath or newPath is directory.\n"
           "  -n-maxOpenFileNumber\n"
           "      limit Number of open files at same time when stream directory diff;\n"
           "      DEFAULT maxOpenFileNumber==48, the best limit value by different operating system.\n"
#if (_IS_NEED_ORIGINAL)
           "  -o  Original diff, unsupport run with -s or -c; DEPRECATED;\n"
           "      compatible with \"diff_demo.cpp\",\n"
           "      diffFile must patch by \"patch_demo.c\" or \"hpatchz -o ...\"\n"
#endif
           "  -h or -?\n"
           "      output Help info (this usage).\n"
           "  -v  output Version info.\n\n"
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
    HDIFF_RESAVE_FILEREAD_ERROR,
    HDIFF_RESAVE_HDIFFINFO_ERROR,
    HDIFF_RESAVE_COMPRESSTYPE_ERROR,
    HDIFF_RESAVE_ERROR,
    HDIFF_DIR_DIFF_ERROR,
    HDIFF_DIR_PATCH_ERROR,
} THDiffResult;

int hdiff_cmd_line(int argc,const char * argv[]);

int hdiff_dir(const char* oldPath,const char* newPath,const char* outDiffFileName,
              hpatch_BOOL oldIsDir, hpatch_BOOL newIsdir,
              hpatch_BOOL isDiff,hpatch_BOOL isLoadAll,size_t matchValue,hpatch_BOOL isPatchCheck,
              hdiff_TStreamCompress* streamCompressPlugin,hdiff_TCompress* compressPlugin,
              hpatch_TDecompress* decompressPlugin,size_t kMaxOpenFileNumber);
int hdiff(const char* oldFileName,const char* newFileName,const char* outDiffFileName,
          hpatch_BOOL isDiff,hpatch_BOOL isLoadAll,size_t matchValue,hpatch_BOOL isPatchCheck,
          hdiff_TStreamCompress* streamCompressPlugin,hdiff_TCompress* compressPlugin,
          hpatch_TDecompress* decompressPlugin,hpatch_BOOL isOriginal);
int hdiff_resave(const char* diffFileName,const char* outDiffFileName,
                 hdiff_TStreamCompress* streamCompressPlugin);


#if (_IS_NEED_MAIN)
#   if (_IS_USE_WIN32_UTF8_WAPI)
int wmain(int argc,wchar_t* argv_w[]){
    char* argv_utf8[kPathMaxSize*2/sizeof(char*)];
    if (!_wFileNames_to_utf8((const wchar_t**)argv_w,argc,argv_utf8,sizeof(argv_utf8)))
        return HDIFF_OPTIONS_ERROR;
    SetDefaultStringLocale();
    return hdiff_cmd_line(argc,(const char**)argv_utf8);
}
#   else
int main(int argc,char* argv[]){
    return  hdiff_cmd_line(argc,(const char**)argv);
}
#   endif
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
    if ((ctypeLen!=(size_t)(ptypeEnd-ptype))||(0!=strncmp(ptype,ctype,ctypeLen))) return true;
    
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
            const char* pdictSize=plevelEnd+1;
            const char* pdictSizeEnd=find(pdictSize,'-');
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
    if (!(value)) { fprintf(stderr,"options " errorInfo " ERROR!\n"); printUsage(); return HDIFF_OPTIONS_ERROR; } }

#define _kNULL_VALUE    ((hpatch_BOOL)(-1))
#define _kNULL_SIZE     ((size_t)(-1))

int hdiff_cmd_line(int argc, const char * argv[]){
    hpatch_BOOL isOriginal=_kNULL_VALUE;
    hpatch_BOOL isLoadAll=_kNULL_VALUE;
    hpatch_BOOL isPatchCheck=_kNULL_VALUE;
    hpatch_BOOL isDiff=_kNULL_VALUE;
    hpatch_BOOL isForceRunDirDiff=_kNULL_VALUE;
    hpatch_BOOL isOutputHelp=_kNULL_VALUE;
    hpatch_BOOL isOutputVersion=_kNULL_VALUE;
    size_t      matchValue=0;
    size_t      compressLevel=0;
    size_t      kMaxOpenFileNumber=_kNULL_SIZE; //only used in stream dir diff
#ifdef _CompressPlugin_lzma
    size_t      dictSize=0;
#endif
    hdiff_TStreamCompress*  streamCompressPlugin=0;
    hdiff_TCompress*        compressPlugin=0;
    hpatch_TDecompress*     decompressPlugin=0;
    std::vector<const char *> arg_values;
    for (int i=1; i<argc; ++i) {
        const char* op=argv[i];
        _options_check((op!=0)&&(strlen(op)>0),"?");
        if (op[0]!='-'){
            arg_values.push_back(op); //path:file or directory
            continue;
        }
        switch (op[1]) {
#if (_IS_NEED_ORIGINAL)
            case 'o':{
                _options_check((isOriginal==_kNULL_VALUE)&&(op[2]=='\0'),"-o");
                isOriginal=hpatch_TRUE;
            } break;
#endif
            case 'm':{
                _options_check((isLoadAll==_kNULL_VALUE)&&((op[2]=='-')||(op[2]=='\0')),"-m");
                isLoadAll=hpatch_TRUE;
                if (op[2]=='-'){
                    const char* pnum=op+3;
                    _options_check(kmg_to_size(pnum,strlen(pnum),&matchValue),"-m-?");
                    _options_check((0<=(int)matchValue)&&(matchValue==(size_t)(int)matchValue),"-m-?");
                }else{
                    matchValue=kMinSingleMatchScore_default;
                }
            } break;
            case 's':{
                isLoadAll=hpatch_FALSE; //stream
                if (op[2]=='-'){
                    const char* pnum=op+3;
                    _options_check(kmg_to_size(pnum,strlen(pnum),&matchValue),"-s-?");
                }else{
                    matchValue=kMatchBlockSize_default;
                }
            } break;
            case 'n':{
                _options_check((kMaxOpenFileNumber==_kNULL_SIZE)&&(op[2]=='-'),"-n-?")
                const char* pnum=op+3;
                _options_check(kmg_to_size(pnum,strlen(pnum),&kMaxOpenFileNumber),"-n-?");
            } break;
            case '?':
            case 'h':{
                _options_check((isOutputHelp==_kNULL_VALUE)&&(op[2]=='\0'),"-h");
                isOutputHelp=hpatch_TRUE;
            } break;
            case 'v':{
                _options_check((isOutputVersion==_kNULL_VALUE)&&(op[2]=='\0'),"-v");
                isOutputVersion=hpatch_TRUE;
            } break;
            case 't':{
                _options_check((isPatchCheck==_kNULL_VALUE)&&(op[2]=='\0'),"-t");
                isPatchCheck=hpatch_TRUE; //test diffFile
            } break;
            case 'd':{
                _options_check((isDiff==_kNULL_VALUE)&&(op[2]=='\0'),"-d");
                isDiff=hpatch_TRUE; //diff only
            } break;
            case 'D':{
                _options_check((isForceRunDirDiff==_kNULL_VALUE)&&(op[2]=='\0'),"-D");
                isForceRunDirDiff=hpatch_TRUE; //force run DirDiff
            } break;
            case 'c':{
                _options_check((compressPlugin==0)&&(op[2]=='-'),"-c");
                const char* ptype=op+3;
                const char* ptypeEnd=find(ptype,'-');
#ifdef _CompressPlugin_zlib
                _options_check(_trySetCompress(&streamCompressPlugin,&compressPlugin,&decompressPlugin,
                                               &zlibStreamCompressPlugin,&zlibCompressPlugin,&zlibDecompressPlugin,
                                               ptype,ptypeEnd,"zlib" ,&compressLevel,1,9,9),"-c-zlib-?");
                if (compressPlugin==&zlibCompressPlugin) { zlib_compress_level=(int)compressLevel; }
#endif
#ifdef _CompressPlugin_bz2
                _options_check(_trySetCompress(&streamCompressPlugin,&compressPlugin,&decompressPlugin,
                                               &bz2StreamCompressPlugin,&bz2CompressPlugin,&bz2DecompressPlugin,
                                               ptype,ptypeEnd,"bzip2",&compressLevel,1,9,9),"-c-bzip2-?");
                if (compressPlugin==&bz2CompressPlugin) { bz2_compress_level=(int)compressLevel; }
#endif
#ifdef _CompressPlugin_lzma
                _options_check(_trySetCompress(&streamCompressPlugin,&compressPlugin,&decompressPlugin,
                                               &lzmaStreamCompressPlugin,&lzmaCompressPlugin,&lzmaDecompressPlugin,
                                               ptype,ptypeEnd,"lzma" ,&compressLevel,0,9,7,
                                               &dictSize,1<<12,(sizeof(size_t)<=4)?(1<<27):((size_t)3<<29),1<<22),"-c-lzma-?");
                if (compressPlugin==&lzmaCompressPlugin) {
                    lzma_compress_level=(int)compressLevel; lzma_dictSize=(UInt32)dictSize; }
#endif
#ifdef _CompressPlugin_lz4
                _options_check(_trySetCompress(&streamCompressPlugin,&compressPlugin,&decompressPlugin,
                                               &lz4StreamCompressPlugin,&lz4CompressPlugin,&lz4DecompressPlugin,
                                               ptype,ptypeEnd,"lz4"  ),"-c-lz4-?");
#endif
#ifdef _CompressPlugin_lz4hc
                _options_check(_trySetCompress(&streamCompressPlugin,&compressPlugin,&decompressPlugin,
                                               &lz4hcStreamCompressPlugin,&lz4hcCompressPlugin,&lz4DecompressPlugin,
                                               ptype,ptypeEnd,"lz4hc",&compressLevel,3,12,11),"-c-lz4hc-?");
                if (compressPlugin==&lz4hcCompressPlugin) { lz4hc_compress_level=(int)compressLevel; }
#endif
#ifdef _CompressPlugin_zstd
                _options_check(_trySetCompress(&streamCompressPlugin,&compressPlugin,&decompressPlugin,
                                               &zstdStreamCompressPlugin,&zstdCompressPlugin,&zstdDecompressPlugin,
                                               ptype,ptypeEnd,"zstd" ,&compressLevel,0,22,20),"-c-zstd-?");
                if (compressPlugin==&zstdCompressPlugin) { zstd_compress_level=(int)compressLevel; }
#endif
                _options_check(compressPlugin!=0,"-c-?");
            } break;
            default: {
                _options_check(hpatch_FALSE,"-?");
            } break;
        }//swich
    }
    
    if (isOutputHelp==_kNULL_VALUE)
        isOutputHelp=hpatch_FALSE;
    if (isOutputVersion==_kNULL_VALUE)
        isOutputVersion=hpatch_FALSE;
    if (isOutputHelp||isOutputVersion){
        printf("HDiffPatch::hdiffz v" HDIFFPATCH_VERSION_STRING "\n\n");
        if (isOutputHelp)
            printUsage();
        if (arg_values.empty())
            return 0; //ok
    }
    if (kMaxOpenFileNumber==_kNULL_SIZE)
        kMaxOpenFileNumber=kMaxOpenFileNumber_default_diff;
    if (kMaxOpenFileNumber<kMaxOpenFileNumber_default_min)
        kMaxOpenFileNumber=kMaxOpenFileNumber_default_min;
    
    _options_check((arg_values.size()==2)||(arg_values.size()==3),"count");
    if (arg_values.size()==3){
        if (isOriginal==_kNULL_VALUE)
            isOriginal=hpatch_FALSE;
        if (isOriginal){
            _options_check((isLoadAll!=hpatch_FALSE)&&(compressPlugin==0),
                           "-o unsupport run with -s or -c");
        }
        if (isLoadAll==_kNULL_VALUE){
            isLoadAll=hpatch_TRUE;
            matchValue=kMinSingleMatchScore_default;
        }
        if ((isDiff==hpatch_TRUE)&&(isPatchCheck==_kNULL_VALUE))
            isPatchCheck=hpatch_FALSE;
        if ((isDiff==_kNULL_VALUE)&&(isPatchCheck==hpatch_TRUE))
            isDiff=hpatch_FALSE;
        if (isDiff==_kNULL_VALUE)
            isDiff=hpatch_TRUE;
        if (isPatchCheck==_kNULL_VALUE){
    #if (defined(_IS_NEED_PATCH_CHECK) && (_IS_NEED_PATCH_CHECK==0))
            isPatchCheck=hpatch_FALSE;
    #else
            isPatchCheck=hpatch_TRUE;
    #endif
        }
        assert(isPatchCheck||isDiff);
        if (isForceRunDirDiff==_kNULL_VALUE)
            isForceRunDirDiff=hpatch_FALSE;
        
        const char* oldPath        =arg_values[0];
        const char* newPath        =arg_values[1];
        const char* outDiffFileName=arg_values[2];
        TPathType oldType;
        TPathType newType;
        _options_check(getPathTypeByName(oldPath,&oldType,0),"input old path must file or directory");
        _options_check(getPathTypeByName(newPath,&newType,0),"input new path must file or directory");
        hpatch_BOOL isUseDirDiff=isForceRunDirDiff||(kPathType_dir==oldType)||(kPathType_dir==newType);
        if (isUseDirDiff)
            _options_check(!isOriginal,"-o unsupport dir diff");
        
        if (isUseDirDiff){
            return hdiff_dir(oldPath,newPath,outDiffFileName, (kPathType_dir==oldType),
                             (kPathType_dir==newType), isDiff,isLoadAll,matchValue,isPatchCheck,
                             streamCompressPlugin,compressPlugin,decompressPlugin,kMaxOpenFileNumber);
        }else{
            return hdiff(oldPath,newPath,outDiffFileName,isDiff,isLoadAll,matchValue,isPatchCheck,
                         streamCompressPlugin,compressPlugin,decompressPlugin,isOriginal);
        }
    }else{ //resave
        _options_check((isOriginal==_kNULL_VALUE),"-o unsupport run with resave mode");
        _options_check((isLoadAll==_kNULL_VALUE),"-m or -s unsupport run with resave mode");
        _options_check((isPatchCheck==_kNULL_VALUE),"-d unsupport run with resave mode");
        _options_check((isForceRunDirDiff==_kNULL_VALUE),"-D unsupport run with resave mode");
        
        const char* diffFileName   =arg_values[0];
        const char* outDiffFileName=arg_values[1];
        return hdiff_resave(diffFileName,outDiffFileName,streamCompressPlugin);
    }
}

#define _check_readFile(v) { if (!(v)) { TFileStreamInput_close(&file); return hpatch_FALSE; } }
#define _free_file_mem(p) { if (p) { free(p); p=0; } }
static hpatch_BOOL readFileAll(TByte** out_pdata,size_t* out_dataSize,const char* fileName){
    size_t              dataSize;
    TFileStreamInput    file;
    TFileStreamInput_init(&file);
    _check_readFile(TFileStreamInput_open(&file,fileName));
    dataSize=(size_t)file.base.streamSize;
    _check_readFile(dataSize==file.base.streamSize);
    *out_pdata=(TByte*)malloc(dataSize);
    _check_readFile((*out_pdata)!=0);
    *out_dataSize=dataSize;
    _check_readFile(file.base.read(&file.base,0,*out_pdata,(*out_pdata)+dataSize));
    return TFileStreamInput_close(&file);
}

#define _check_writeFile(v) { if (!(v)) { TFileStreamOutput_close(&file); return hpatch_FALSE; } }
static hpatch_BOOL writeFileAll(const TByte* pdata,size_t dataSize,const char* outFileName){
    TFileStreamOutput file;
    TFileStreamOutput_init(&file);
    _check_writeFile(TFileStreamOutput_open(&file,outFileName,dataSize));
    _check_writeFile(file.base.write(&file.base,0,pdata,pdata+dataSize));
    return TFileStreamOutput_close(&file);
}

#if (_IS_NEED_ORIGINAL)
static int saveSize(std::vector<TByte>& outBuf,hpatch_StreamPos_t size){
    outBuf.push_back((TByte)size);
    outBuf.push_back((TByte)(size>>8));
    outBuf.push_back((TByte)(size>>16));
    int writedSize;
    if ((size>>31)==0){
        writedSize=4;
        outBuf.push_back((TByte)(size>>24));
    }else{
        writedSize=9;
        outBuf.push_back(0xFF);
        outBuf.push_back((TByte)(size>>24));
        
        const hpatch_StreamPos_t highSize=(size>>32);
        outBuf.push_back((TByte)highSize);
        outBuf.push_back((TByte)(highSize>>8));
        outBuf.push_back((TByte)(highSize>>16));
        outBuf.push_back((TByte)(highSize>>24));
    }
    return writedSize;
}

static int readSavedSize(const TByte* data,size_t dataSize,hpatch_StreamPos_t* outSize){
    size_t lsize;
    if (dataSize<4) return -1;
    lsize=data[0]|(data[1]<<8)|(data[2]<<16);
    if (data[3]!=0xFF){
        lsize|=data[3]<<24;
        *outSize=lsize;
        return 4;
    }else{
        size_t hsize;
        if (dataSize<9) return -1;
        lsize|=data[4]<<24;
        hsize=data[5]|(data[6]<<8)|(data[7]<<16)|(data[8]<<24);
        *outSize=lsize|(((hpatch_StreamPos_t)hsize)<<32);
        return 9;
    }
}
#endif

#define  check_on_error(errorType) { \
    if (result==HDIFF_SUCCESS) result=errorType; if (!_isInClear){ goto clear; } }
#define check(value,errorType,errorInfo) { \
    std::string erri=std::string()+errorInfo+" ERROR!\n"; \
    if (!(value)){ printStdErrPath_utf8(erri.c_str()); check_on_error(errorType); } }

static int hdiff_m(const char* oldFileName,const char* newFileName,const char* outDiffFileName,
                   hpatch_BOOL isDiff,size_t matchScore,hpatch_BOOL isPatchCheck,
                   hdiff_TCompress* compressPlugin,hpatch_TDecompress* decompressPlugin,hpatch_BOOL isOriginal){
    double diff_time0=clock_s();
    int    result=HDIFF_SUCCESS;
    int    _isInClear=hpatch_FALSE;
    TByte* oldData=0;  size_t oldDataSize=0;
    TByte* newData=0;  size_t newDataSize=0;
    TByte* diffData=0; size_t diffDataSize=0;
    check(readFileAll(&oldData,&oldDataSize,oldFileName),HDIFF_OPENREAD_ERROR,"open oldFile");
    check(readFileAll(&newData,&newDataSize,newFileName),HDIFF_OPENREAD_ERROR,"open newFile");
    printf("oldDataSize : %"PRIu64"\nnewDataSize : %"PRIu64"\n",
           (hpatch_StreamPos_t)oldDataSize,(hpatch_StreamPos_t)newDataSize);
    if (isDiff){
        std::vector<TByte> outDiffData;
        try {
#if (_IS_NEED_ORIGINAL)
            if (isOriginal){
                size_t originalNewDataSavedSize=saveSize(outDiffData,newDataSize);
                assert(originalNewDataSavedSize==outDiffData.size());
                create_diff(newData,newData+newDataSize,oldData,oldData+oldDataSize,outDiffData,(int)matchScore);
            } else
#endif
            {
                create_compressed_diff(newData,newData+newDataSize,oldData,oldData+oldDataSize,
                                       outDiffData,compressPlugin,(int)matchScore);
            }
        }catch(const std::exception& e){
            check(false,HDIFF_DIFF_ERROR,"diff run an error: "+e.what());
        }
        printf("diffDataSize: %"PRIu64"\n",(hpatch_StreamPos_t)outDiffData.size());
        check(writeFileAll(outDiffData.data(),outDiffData.size(),outDiffFileName),
              HDIFF_OPENWRITE_ERROR,"open write diffFile");
        printf("  out diff file ok!\n");
        outDiffData.clear();
        printf("diff time: %.3f s\n",(clock_s()-diff_time0));
    }
    if (isPatchCheck){
        double patch_time0=clock_s();
        printf("\nload diffFile for test by patch:\n");
        check(readFileAll(&diffData,&diffDataSize,outDiffFileName),
              HDIFF_OPENREAD_ERROR,"open diffFile for test");
        printf("diffDataSize: %"PRIu64"\n",(hpatch_StreamPos_t)diffDataSize);
        
        bool diffrt;
#if (_IS_NEED_ORIGINAL)
        if (isOriginal){
            hpatch_StreamPos_t savedNewSize=0;
            size_t originalNewDataSavedSize=readSavedSize(diffData,diffDataSize,&savedNewSize);
            check(originalNewDataSavedSize>0,HDIFF_PATCH_ERROR,"read diffFile savedNewSize");
            check(savedNewSize==newDataSize,HDIFF_PATCH_ERROR, "read diffFile savedNewSize")
            diffrt=check_diff(newData,newData+newDataSize,oldData,oldData+oldDataSize,
                              diffData+originalNewDataSavedSize,diffData+diffDataSize);
        } else
#endif
        {
            diffrt=check_compressed_diff(newData,newData+newDataSize,oldData,oldData+oldDataSize,
                                         diffData,diffData+diffDataSize,decompressPlugin);
        }
        check(diffrt,HDIFF_PATCH_ERROR,"patch check diff data");
        printf("  patch check diff data ok!\n");
        printf("patch time: %.3f s\n",(clock_s()-patch_time0));
    }
clear:
    _isInClear=hpatch_TRUE;
    _free_file_mem(diffData);
    _free_file_mem(newData);
    _free_file_mem(oldData);
    return result;
}

static int hdiff_s(const char* oldFileName,const char* newFileName,const char* outDiffFileName,
                   hpatch_BOOL isDiff,size_t matchBlockSize,hpatch_BOOL isPatchCheck,
                   hdiff_TStreamCompress* streamCompressPlugin,hpatch_TDecompress* decompressPlugin){
    double diff_time0=clock_s();
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
    
    check(TFileStreamInput_open(&oldData,oldFileName),HDIFF_OPENREAD_ERROR,"open oldFile");
    check(TFileStreamInput_open(&newData,newFileName),HDIFF_OPENREAD_ERROR,"open newFile");
    printf("oldDataSize : %"PRIu64"\nnewDataSize : %"PRIu64"\n",
           oldData.base.streamSize,newData.base.streamSize);
    if (isDiff){
        check(TFileStreamOutput_open(&diffData,outDiffFileName,-1),
              HDIFF_OPENWRITE_ERROR,"open out diffFile");
        TFileStreamOutput_setRandomOut(&diffData,hpatch_TRUE);
        try{
            create_compressed_diff_stream(&newData.base,&oldData.base, &diffData.base,
                                          streamCompressPlugin,matchBlockSize);
            diffData.base.streamSize=diffData.out_length;
        }catch(const std::exception& e){
            check(false,HDIFF_DIFF_ERROR,"stream diff run an error: "+e.what());
        }
        check(TFileStreamOutput_close(&diffData),HDIFF_FILECLOSE_ERROR,"out diffFile close");
        printf("diffDataSize: %"PRIu64"\n",diffData.base.streamSize);
        printf("  out diff file ok!\n");
        printf("diff  time: %.3f s\n",(clock_s()-diff_time0));
    }
    if (isPatchCheck){
        double patch_time0=clock_s();
        printf("\nload diffFile for test by patch check:\n");
        check(TFileStreamInput_open(&diffData_in,outDiffFileName),HDIFF_OPENREAD_ERROR,"open check diffFile");
        printf("diffDataSize: %"PRIu64"\n",diffData_in.base.streamSize);
        check(check_compressed_diff_stream(&newData.base,&oldData.base,
                                           &diffData_in.base,decompressPlugin),
              HDIFF_PATCH_ERROR,"patch check diff data");
        printf("  patch check diff data ok!\n");
        printf("patch time: %.3f s\n",(clock_s()-patch_time0));
    }
clear:
    _isInClear=hpatch_TRUE;
    check(TFileStreamOutput_close(&diffData),HDIFF_FILECLOSE_ERROR,"out diffFile close");
    check(TFileStreamInput_close(&diffData_in),HDIFF_FILECLOSE_ERROR,"check diffFile close");
    check(TFileStreamInput_close(&newData),HDIFF_FILECLOSE_ERROR,"newFile close");
    check(TFileStreamInput_close(&oldData),HDIFF_FILECLOSE_ERROR,"oldFile close");
    return result;
}

int hdiff(const char* oldFileName,const char* newFileName,const char* outDiffFileName,
          hpatch_BOOL isDiff,hpatch_BOOL isLoadAll,size_t matchValue,hpatch_BOOL isPatchCheck,
          hdiff_TStreamCompress* streamCompressPlugin,hdiff_TCompress* compressPlugin,
          hpatch_TDecompress* decompressPlugin,hpatch_BOOL isOriginal){
    double time0=clock_s();
    std::string fnameInfo=std::string("old : \"")+oldFileName+"\"\n"
                                     +"new : \""+newFileName+"\"\n"
                             +(isDiff?"out : \"":"test: \"")+outDiffFileName+"\"\n";
    printPath_utf8(fnameInfo.c_str());
    
    if (isDiff) {
        const char* compressType="";
        if (isLoadAll){
            if (compressPlugin) compressType=compressPlugin->compressType(compressPlugin);
        }else{
            if (streamCompressPlugin) compressType=streamCompressPlugin->compressType(streamCompressPlugin);
        }
        printf("hdiffz run with%s compress plugin: \"%s\"\n",(isLoadAll?"":" stream"),compressType);
    }
    
    int exitCode;
    if (isLoadAll){
        exitCode=hdiff_m(oldFileName,newFileName,outDiffFileName,
                         isDiff,matchValue,isPatchCheck,compressPlugin,decompressPlugin,isOriginal);
    }else{
        exitCode=hdiff_s(oldFileName,newFileName,outDiffFileName,
                         isDiff,matchValue,isPatchCheck,streamCompressPlugin,decompressPlugin);
    }
    if (isDiff && isPatchCheck)
        printf("\nall   time: %.3f s\n",(clock_s()-time0));
    return exitCode;
}

static int hdiff_r(const char* diffFileName,const char* outDiffFileName,
                   hdiff_TStreamCompress* streamCompressPlugin){
    int result=HDIFF_SUCCESS;
    hpatch_BOOL _isInClear=hpatch_FALSE;
    std::string dirCompressType;
    hpatch_BOOL isDirDiff=false;
    TFileStreamInput  diffData_in;
    TFileStreamOutput diffData_out;
    TFileStreamInput_init(&diffData_in);
    TFileStreamOutput_init(&diffData_out);
    
    hpatch_TDecompress* decompressPlugin=0;
    check(TFileStreamInput_open(&diffData_in,diffFileName),HDIFF_OPENREAD_ERROR,"open diffFile");
    {
        TDirDiffInfo dirDiffInfo;
        check(getDirDiffInfo(&diffData_in.base,&dirDiffInfo),HDIFF_OPENREAD_ERROR,"read diffFile")
        isDirDiff=dirDiffInfo.isDirDiff;
        hpatch_compressedDiffInfo diffInfo;
        if (isDirDiff){
            diffInfo=dirDiffInfo.hdiffInfo;
            diffInfo.compressedCount+=dirDiffInfo.dirDataIsCompressed?1:0;
        }else if (!getCompressedDiffInfo(&diffInfo,&diffData_in.base)){
            check(!diffData_in.fileError,HDIFF_RESAVE_FILEREAD_ERROR,"read diffFile");
            check(hpatch_FALSE,HDIFF_RESAVE_HDIFFINFO_ERROR,"is hdiff file? get diff info");
        }
        if (strlen(diffInfo.compressType)>0){
#ifdef  _CompressPlugin_zlib
            if ((!decompressPlugin)&&zlibDecompressPlugin.is_can_open(&zlibDecompressPlugin,&diffInfo))
                decompressPlugin=&zlibDecompressPlugin;
#endif
#ifdef  _CompressPlugin_bz2
            if ((!decompressPlugin)&&bz2DecompressPlugin.is_can_open(&bz2DecompressPlugin,&diffInfo))
                decompressPlugin=&bz2DecompressPlugin;
#endif
#ifdef  _CompressPlugin_lzma
            if ((!decompressPlugin)&&lzmaDecompressPlugin.is_can_open(&lzmaDecompressPlugin,&diffInfo))
                decompressPlugin=&lzmaDecompressPlugin;
#endif
#if (defined(_CompressPlugin_lz4) || (defined(_CompressPlugin_lz4hc)))
            if ((!decompressPlugin)&&lz4DecompressPlugin.is_can_open(&lz4DecompressPlugin,&diffInfo))
                decompressPlugin=&lz4DecompressPlugin;
#endif
#ifdef  _CompressPlugin_zstd
            if ((!decompressPlugin)&&zstdDecompressPlugin.is_can_open(&zstdDecompressPlugin,&diffInfo))
                decompressPlugin=&zstdDecompressPlugin;
#endif
        }
        if (!decompressPlugin){
            if (diffInfo.compressedCount>0){
                check(false,HDIFF_RESAVE_COMPRESSTYPE_ERROR,
                      "can no decompress \""+diffInfo.compressType+" data");
            }else{
                if (strlen(diffInfo.compressType)>0)
                    printf("  diffFile added useless compress tag \"%s\"\n",diffInfo.compressType);
                decompressPlugin=0;
            }
        }else{
            printf("resave diffFile%s with decompress plugin: \"%s\" (need decompress %d)\n",(isDirDiff?"(DirDiff)":""),diffInfo.compressType,diffInfo.compressedCount);
        }
    }
    
    check(TFileStreamOutput_open(&diffData_out,outDiffFileName,-1),HDIFF_OPENWRITE_ERROR,
          "open out diffFile");
    TFileStreamOutput_setRandomOut(&diffData_out,hpatch_TRUE);
    printf("inDiffSize : %"PRIu64"\n",diffData_in.base.streamSize);
    try{
        if (isDirDiff){
            resave_dirdiff(&diffData_in.base,decompressPlugin,
                           &diffData_out.base,streamCompressPlugin);
        }else{
            resave_compressed_diff(&diffData_in.base,decompressPlugin,
                                   &diffData_out.base,streamCompressPlugin);
        }
        diffData_out.base.streamSize=diffData_out.out_length;
    }catch(const std::exception& e){
        check(false,HDIFF_RESAVE_ERROR,"resave diff run an error: "+e.what());
    }
    printf("outDiffSize: %"PRIu64"\n",diffData_out.base.streamSize);
    check(TFileStreamOutput_close(&diffData_out),HDIFF_FILECLOSE_ERROR,"out diffFile close");
    printf("  out diff file ok!\n");
clear:
    _isInClear=hpatch_TRUE;
    check(TFileStreamOutput_close(&diffData_out),HDIFF_FILECLOSE_ERROR,"out diffFile close");
    check(TFileStreamInput_close(&diffData_in),HDIFF_FILECLOSE_ERROR,"in diffFile close");
    return result;
}

int hdiff_resave(const char* diffFileName,const char* outDiffFileName,
                 hdiff_TStreamCompress* streamCompressPlugin){
    double time0=clock_s();
    std::string fnameInfo=std::string("in_diff : \"")+diffFileName+"\"\n"
                                     +"out_diff: \""+outDiffFileName+"\"\n";
    printPath_utf8(fnameInfo.c_str());
    
    const char* compressType="";
    if (streamCompressPlugin) compressType=streamCompressPlugin->compressType(streamCompressPlugin);
    printf("resave diffFile with stream compress plugin: \"%s\"\n",compressType);
    
    int exitCode=hdiff_r(diffFileName,outDiffFileName,streamCompressPlugin);
    double time1=clock_s();
    printf("\nhdiffz resave diffFile time: %.3f s\n",(time1-time0));
    return exitCode;
}



struct DirDiffListener:public IDirDiffListener{
    virtual bool isNeedFilter(const std::string& path){
#if (defined(__APPLE__))
        if (pathNameIs(path,".DS_Store")) return true;
#endif
        return false;
    }
    virtual void diffRefInfo(size_t oldPathCount,size_t newPathCount,size_t sameFilePairCount,
                             size_t refOldFileCount,size_t refNewFileCount,
                             hpatch_StreamPos_t refOldFileSize,hpatch_StreamPos_t refNewFileSize){
        printf("DirDiff old path count: %"PRIu64"\n",(hpatch_StreamPos_t)oldPathCount);
        printf("        new path count: %"PRIu64"\n",(hpatch_StreamPos_t)newPathCount);
        printf("       same file count: %"PRIu64"\n",(hpatch_StreamPos_t)sameFilePairCount);
        printf("    ref old file count: %"PRIu64"\n",(hpatch_StreamPos_t)refOldFileCount);
        printf("   diff new file count: %"PRIu64"\n",(hpatch_StreamPos_t)refNewFileCount);
        printf("\nrun hdiffz:\n");
        printf("  oldRefSize  : %"PRIu64"\n",refOldFileSize);
        printf("  newRefSize  : %"PRIu64"\n",refNewFileSize);
    }
    
    double _runHDiffBegin_time0;
    virtual void runHDiffBegin(){ _runHDiffBegin_time0=clock_s(); }
    virtual void runHDiffEnd(hpatch_StreamPos_t diffDataSize){
        printf("  diffDataSize: %"PRIu64"\n",diffDataSize);
        printf("    diff  time: %.3f s\n",clock_s()-_runHDiffBegin_time0);
    }
};


int hdiff_dir(const char* _oldPath,const char* _newPath,const char* outDiffFileName,
              hpatch_BOOL oldIsDir, hpatch_BOOL newIsDir,
              hpatch_BOOL isDiff,hpatch_BOOL isLoadAll,size_t matchValue,hpatch_BOOL isPatchCheck,
              hdiff_TStreamCompress* streamCompressPlugin,hdiff_TCompress* compressPlugin,
              hpatch_TDecompress* decompressPlugin,size_t kMaxOpenFileNumber){
    double time0=clock_s();
    std::string oldPatch(_oldPath);
    std::string newPatch(_newPath);
    if (oldIsDir) assignDirTag(oldPatch); else assert(!getIsDirName(oldPatch.c_str()));
    if (newIsDir) assignDirTag(newPatch); else assert(!getIsDirName(newPatch.c_str()));
    std::string fnameInfo=std::string("")
        +(oldIsDir?"oldDir : \"":"oldFile: \"")+oldPatch+"\"\n"
        +(newIsDir?"newDir : \"":"newFile: \"")+newPatch+"\"\n"
        +(isDiff?  "outDiff: \"":"   test: \"")+outDiffFileName+"\"\n";
    printPath_utf8(fnameInfo.c_str());
    
    if (isDiff) {
        const char* compressType="";
        if (isLoadAll){
            if (compressPlugin) compressType=compressPlugin->compressType(compressPlugin);
        }else{
            if (streamCompressPlugin) compressType=streamCompressPlugin->compressType(streamCompressPlugin);
        }
        printf("hdiffz run DirDiff with%s compress plugin: \"%s\"\n",(isLoadAll?"":" stream"),compressType);
    }

    int  result=HDIFF_SUCCESS;
    bool _isInClear=false;
    TFileStreamOutput diffData_out;
    TFileStreamInput diffData_in;
    TFileStreamOutput_init(&diffData_out);
    TFileStreamInput_init(&diffData_in);
    if (isDiff){
        try {
            check(TFileStreamOutput_open(&diffData_out,outDiffFileName,-1),
                  HDIFF_OPENWRITE_ERROR,"open out diffFile");
            TFileStreamOutput_setRandomOut(&diffData_out,hpatch_TRUE);
            DirDiffListener listener;
            dir_diff(&listener,oldPatch,newPatch,&diffData_out.base,isLoadAll!=0,matchValue,
                     streamCompressPlugin,compressPlugin,kMaxOpenFileNumber);
            diffData_out.base.streamSize=diffData_out.out_length;
        }catch(const std::exception& e){
            check(false,HDIFF_DIR_DIFF_ERROR,"dir diff run an error: "+e.what());
        }
        printf("\nDirDiff  size: %"PRIu64"\n",diffData_out.base.streamSize);
        printf("DirDiff  time: %.3f s\n",(clock_s()-time0));
        check(TFileStreamOutput_close(&diffData_out),HDIFF_FILECLOSE_ERROR,"out diffFile close");
        printf("  out dir diffFile ok!\n");
    }
    if (isPatchCheck){
        double patch_time0=clock_s();
        printf("\nload dir diffFile for test by DirPatch check:\n");
        check(TFileStreamInput_open(&diffData_in,outDiffFileName),HDIFF_OPENREAD_ERROR,"open check diffFile");
        printf("diffDataSize : %"PRIu64"\n",diffData_in.base.streamSize);
        DirDiffListener listener;
        check(check_dirdiff(&listener,oldPatch,newPatch,&diffData_in.base,decompressPlugin,kMaxOpenFileNumber),
              HDIFF_DIR_PATCH_ERROR,"DirPatch check diff data");
        printf("  DirPatch check diff data ok!\n");
        printf("DirPatch time: %.3f s\n",(clock_s()-patch_time0));
    }
    
    printf("\nall   time: %.3f s\n",(clock_s()-time0));
clear:
    _isInClear=true;
    check(TFileStreamOutput_close(&diffData_out),HDIFF_FILECLOSE_ERROR,"check diffFile close");
    return result;
}
