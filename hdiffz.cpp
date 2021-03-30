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
#include <stdio.h>
#include <algorithm>  //find search
#include "libHDiffPatch/HDiff/diff.h"
#include "libHDiffPatch/HPatch/patch.h"
#include "_clock_for_demo.h"
#include "_atosize.h"
#include "file_for_patch.h"
#include "libHDiffPatch/HDiff/private_diff/mem_buf.h"

#include "_dir_ignore.h"
#if (_IS_NEED_DIR_DIFF_PATCH)
#include "dirDiffPatch/dir_diff/dir_diff.h"
#endif

#ifndef _IS_NEED_MAIN
#   define  _IS_NEED_MAIN 1
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
#   define _CompressPlugin_lzma2 // better compresser
#endif
#if (_IS_NEED_ALL_CompressPlugin)
//===== select needs decompress plugins or change to your plugin=====
#   define _CompressPlugin_lz4   // faster compresser / faster decompresser
#   define _CompressPlugin_lz4hc // faster decompresser
#   define _CompressPlugin_zstd  // better compresser / faster decompresser
#   define _CompressPlugin_brotli// better compresser / faster decompresser
#   define _CompressPlugin_lzham // better compresser / decompress faster than lzma2
#endif

#include "compress_plugin_demo.h"
#include "decompress_plugin_demo.h"


#if (_IS_NEED_DIR_DIFF_PATCH)

#ifndef _IS_NEED_DEFAULT_ChecksumPlugin
#   define _IS_NEED_DEFAULT_ChecksumPlugin 1
#endif
#if (_IS_NEED_ALL_ChecksumPlugin)
#   undef  _IS_NEED_DEFAULT_ChecksumPlugin
#   define _IS_NEED_DEFAULT_ChecksumPlugin 1
#endif
#if (_IS_NEED_DEFAULT_ChecksumPlugin)
//===== select needs checksum plugins or change to your plugin=====
#   define _ChecksumPlugin_crc32    // =  32 bit effective  //need zlib
#   define _ChecksumPlugin_fadler64 // ?  63 bit effective
#endif
#if (_IS_NEED_ALL_ChecksumPlugin)
//===== select needs checksum plugins or change to your plugin=====
#   define _ChecksumPlugin_adler32  // ?  29 bit effective
#   define _ChecksumPlugin_adler64  // ?  30 bit effective
#   define _ChecksumPlugin_fadler32 // ~  32 bit effective
#   define _ChecksumPlugin_fadler128// ?  81 bit effective
#   define _ChecksumPlugin_md5      // ? 128 bit effective
#endif


#include "checksum_plugin_demo.h"
#endif

static void printVersion(){
    printf("HDiffPatch::hdiffz v" HDIFFPATCH_VERSION_STRING "\n\n");
}

static void printUsage(){
    printVersion();
    printf("diff    usage: hdiffz [options] oldPath newPath outDiffFile\n"
           "test    usage: hdiffz    -t     oldPath newPath testDiffFile\n"
           "resave  usage: hdiffz [-c-...]  diffFile outDiffFile\n"
#if (_IS_NEED_DIR_DIFF_PATCH)
           "get  manifest: hdiffz [-g#...] [-C-checksumType] inputPath -M#outManifestTxtFile\n"
           "manifest diff: hdiffz [options] -M-old#oldManifestFile -M-new#newManifestFile\n"
           "                      oldPath newPath outDiffFile\n"
           "  oldPath newPath inputPath can be file or directory(folder),\n"
#endif
           "  oldPath can empty, and input parameter \"\"\n"
           "memory options:\n"
           "  -m[-matchScore]\n"
           "      DEFAULT; all file load into Memory; best diffFileSize;\n"
           "      requires (newFileSize+ oldFileSize*5(or *9 when oldFileSize>=2GB))+O(1)\n"
           "        bytes of memory;\n"
           "      matchScore>=0, DEFAULT -m-6, recommended bin: 0--4 text: 4--9 etc...\n"
           "  -s[-matchBlockSize]\n"
           "      all file load as Stream; fast;\n"
           "      requires O(oldFileSize*16/matchBlockSize+matchBlockSize*5)bytes of memory;\n"
           "      matchBlockSize>=4, DEFAULT -s-64, recommended 16,32,48,1k,64k,1m etc...\n"
           "special options:\n"
#if (_IS_NEED_SINGLE_STREAM_DIFF)
           "  -SD[-stepSize]\n"
           "      create single compressed stream diffData! \n"
           "      now only for test, must run with -m, and not supprot directory(folder).\n"
           "      stepSize>=" _HDIFFPATCH_EXPAND_AND_QUOTE(hpatch_kStreamCacheSize) ", DEFAULT -SD-256k, recommended 64k,2m etc...\n"
#endif
#if (_IS_USED_MULTITHREAD)
           "  -p-parallelThreadNumber\n"
           "      if parallelThreadNumber>1 then open multi-thread Parallel mode;\n"
           "      DEFAULT -p-4; requires more memory!\n"
#endif
           "  -c-compressType[-compressLevel]\n"
           "      set outDiffFile Compress type & level, DEFAULT uncompress;\n"
           "      for resave diffFile,recompress diffFile to outDiffFile by new set;\n"
           "      support compress type & level:\n"
           "       (re. https://github.com/sisong/lzbench/blob/master/lzbench171_sorted.md )\n"
#ifdef _CompressPlugin_zlib
           "        -c-zlib[-{1..9}]                DEFAULT level 9\n"
#   if (_IS_USED_MULTITHREAD)
           "        -c-pzlib[-{1..9}]               DEFAULT level 6\n"
           "            support run by multi-thread parallel, fast!\n"
           "            WARNING: code not compatible with it compressed by -c-zlib!\n"
           "              and code size may be larger than if it compressed by -c-zlib. \n"
#   endif
#endif
#ifdef _CompressPlugin_bz2
           "        -c-bzip2[-{1..9}]               (or -bz2) DEFAULT level 9\n"
#   if (_IS_USED_MULTITHREAD)
           "        -c-pbzip2[-{1..9}]              (or -pbz2) DEFAULT level 8\n"
           "            support run by multi-thread parallel, fast!\n"
           "            WARNING: code not compatible with it compressed by -c-bzip2!\n"
           "               and code size may be larger than if it compressed by -c-bzip2.\n"
#   endif
#endif
#ifdef _CompressPlugin_lzma
           "        -c-lzma[-{0..9}[-dictSize]]     DEFAULT level 7\n"
           "            dictSize can like 4096 or 4k or 4m or 128m etc..., DEFAULT 8m\n"
#   if (_IS_USED_MULTITHREAD)
           "            support run by 2-thread parallel.\n"
#   endif
#endif
#ifdef _CompressPlugin_lzma2
           "        -c-lzma2[-{0..9}[-dictSize]]    DEFAULT level 7\n"
           "            dictSize can like 4096 or 4k or 4m or 128m etc..., DEFAULT 8m\n"
#   if (_IS_USED_MULTITHREAD)
           "            support run by multi-thread parallel, fast!\n"
#   endif
           "            WARNING: code not compatible with it compressed by -c-lzma!\n"
#endif
#ifdef _CompressPlugin_lz4
           "        -c-lz4[-{1..50}]                DEFAULT level 50 (as lz4 acceleration 1)\n"
#endif
#ifdef _CompressPlugin_lz4hc
           "        -c-lz4hc[-{3..12}]              DEFAULT level 11\n"
#endif
#ifdef _CompressPlugin_zstd
           "        -c-zstd[-{0..22}[-dictBits]]    DEFAULT level 20\n"
           "            dictBits can 10--31, DEFAULT 24.\n"
#   if (_IS_USED_MULTITHREAD)
           "            support run by multi-thread parallel, fast!\n"
#   endif
#endif
#ifdef _CompressPlugin_brotli
           "        -c-brotli[-{0..11}[-dictBits]]  DEFAULT level 9\n"
           "            dictBits can 10--30, DEFAULT 24.\n"
#endif
#ifdef _CompressPlugin_lzham
           "        -c-lzham[-{0..5}[-dictBits]]    DEFAULT level 4\n"
           "            dictBits can 15--29, DEFAULT 24.\n"
#   if (_IS_USED_MULTITHREAD)
           "            support run by multi-thread parallel, fast!\n"
#   endif
#endif
#if (_IS_NEED_DIR_DIFF_PATCH)
           "  -C-checksumType\n"
           "      set outDiffFile Checksum type for directory diff, DEFAULT "
#ifdef _ChecksumPlugin_fadler64
           "-C-fadler64;\n"
#else
#   ifdef _ChecksumPlugin_crc32
           "-C-crc32;\n"
#   else
           "no checksum;\n"
#   endif
           "      (if need checksum for diff between two files, add -D)\n"
#endif
           "      support checksum type:\n"
           "        -C-no                   no checksum\n"
#ifdef _ChecksumPlugin_crc32
#   ifdef _ChecksumPlugin_fadler64
           "        -C-crc32\n"
#   else
           "        -C-crc32                DEFAULT\n"
#   endif
#endif
#ifdef _ChecksumPlugin_adler32
           "        -C-adler32\n"
#endif
#ifdef _ChecksumPlugin_adler64
           "        -C-adler64\n"
#endif
#ifdef _ChecksumPlugin_fadler32
           "        -C-fadler32\n"
#endif
#ifdef _ChecksumPlugin_fadler64
           "        -C-fadler64             DEFAULT\n"
#endif
#ifdef _ChecksumPlugin_fadler128
           "        -C-fadler128\n"
#endif
#ifdef _ChecksumPlugin_md5
           "        -C-md5\n"
#endif
           "  -n-maxOpenFileNumber\n"
           "      limit Number of open files at same time when stream directory diff;\n"
           "      maxOpenFileNumber>=8, DEFAULT -n-48, the best limit value by different\n"
           "        operating system.\n"
           "  -g#ignorePath[#ignorePath#...]\n"
           "      set iGnore path list when Directory Diff; ignore path list such as:\n"
           "        #.DS_Store#desktop.ini#*thumbs*.db#.git*#.svn/#cache_*/00*11/*.tmp\n"
           "      # means separator between names; (if char # in name, need write #: )\n"
           "      * means can match any chars in name; (if char * in name, need write *: );\n"
           "      / at the end of name means must match directory;\n"
           "  -g-old#ignorePath[#ignorePath#...]\n"
           "      set iGnore path list in oldPath when Directory Diff;\n"
           "      if oldFile can be changed, need add it in old ignore list;\n"
           "  -g-new#ignorePath[#ignorePath#...]\n"
           "      set iGnore path list in newPath when Directory Diff;\n"
           "      in general, new ignore list should is empty;\n"
           "  -M#outManifestTxtFile\n"
           "      create a Manifest file for inputPath; it is a text file, saved infos of\n"
           "      all files and directoriy list in inputPath; this file while be used in \n"
           "      manifest diff, support re-checksum data by manifest diff;\n"
           "      can be used to protect historical versions be modified!\n"
           "  -M-old#oldManifestFile\n"
           "      oldManifestFile is created from oldPath; if no oldPath not need -M-old;\n"
           "  -M-new#newManifestFile\n"
           "      newManifestFile is created from newPath;\n"
           "  -D  force run Directory diff between two files; DEFAULT (no -D) run \n"
           "      directory diff need oldPath or newPath is directory.\n"
#endif //_IS_NEED_DIR_DIFF_PATCH
           "  -d  Diff only, do't run patch check, DEFAULT run patch check.\n"
           "  -t  Test only, run patch check, patch(oldPath,testDiffFile)==newPath ? \n"
           "  -f  Force overwrite, ignore write path already exists;\n"
           "      DEFAULT (no -f) not overwrite and then return error;\n"
           "      if used -f and write path is exist directory, will always return error.\n"
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
    HDIFF_RESAVE_DIFFINFO_ERROR,
    HDIFF_RESAVE_COMPRESSTYPE_ERROR,
    HDIFF_RESAVE_ERROR,
    HDIFF_RESAVE_CHECKSUMTYPE_ERROR,
    
    HDIFF_PATHTYPE_ERROR, //adding begin v3.0
    HDIFF_TEMPPATH_ERROR,
    HDIFF_DELETEPATH_ERROR,
    HDIFF_RENAMEPATH_ERROR,
    
    DIRDIFF_DIFF_ERROR=101,
    DIRDIFF_PATCH_ERROR,
    MANIFEST_CREATE_ERROR,
    MANIFEST_TEST_ERROR,
} THDiffResult;

int hdiff_cmd_line(int argc,const char * argv[]);

#if (_IS_NEED_DIR_DIFF_PATCH)
int hdiff_dir(const char* oldPath,const char* newPath,const char* outDiffFileName,
              hpatch_BOOL oldIsDir, hpatch_BOOL newIsDir,
              hpatch_BOOL isDiff,hpatch_BOOL isLoadAll,size_t matchValue,hpatch_BOOL isPatchCheck,
              const hdiff_TCompress* compressPlugin,hpatch_TDecompress* decompressPlugin,
              hpatch_TChecksum* checksumPlugin,size_t kMaxOpenFileNumber,
              const std::vector<std::string>& ignorePathListBase,const std::vector<std::string>& ignoreOldPathList,
              const std::vector<std::string>& ignoreNewPathList,
              const std::string& oldManifestFileName,const std::string& newManifestFileName);
int create_manifest(const char* inputPath,const char* outManifestFileName,
                    hpatch_TChecksum* checksumPlugin,const std::vector<std::string>& ignorePathList);
#endif
int hdiff(const char* oldFileName,const char* newFileName,const char* outDiffFileName,hpatch_BOOL isDiff,
          hpatch_BOOL isLoadAll,size_t matchValue,hpatch_BOOL isPatchCheck,
          const hdiff_TCompress* compressPlugin,hpatch_TDecompress* decompressPlugin,
          hpatch_BOOL isSingleStreamDiff,size_t singleStreamStepSize);
int hdiff_resave(const char* diffFileName,const char* outDiffFileName,
                 const hdiff_TCompress* compressPlugin);


#if (_IS_NEED_MAIN)
#   if (_IS_USED_WIN32_UTF8_WAPI)
int wmain(int argc,wchar_t* argv_w[]){
    hdiff_private::TAutoMem  _mem(hpatch_kPathMaxSize*4);
    char** argv_utf8=(char**)_mem.data();
    if (!_wFileNames_to_utf8((const wchar_t**)argv_w,argc,argv_utf8,_mem.size()))
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


static hpatch_BOOL _getIsCompressedDiffFile(const char* diffFileName){
    hpatch_TFileStreamInput diffData;
    hpatch_TFileStreamInput_init(&diffData);
    if (!hpatch_TFileStreamInput_open(&diffData,diffFileName)) return hpatch_FALSE;
    hpatch_compressedDiffInfo diffInfo;
    hpatch_BOOL result=getCompressedDiffInfo(&diffInfo,&diffData.base);
    if (!hpatch_TFileStreamInput_close(&diffData)) return hpatch_FALSE;
    return result;
}

static void _trySetDecompress(hpatch_TDecompress** out_decompressPlugin,const char* compressType,
                            hpatch_TDecompress* testDecompressPlugin){
    if ((*out_decompressPlugin)!=0) return;
    if (testDecompressPlugin->is_can_open(compressType))
        *out_decompressPlugin=testDecompressPlugin;
}
static hpatch_BOOL findDecompress(hpatch_TDecompress** out_decompressPlugin,const char* compressType){
    *out_decompressPlugin=0;
    if (strlen(compressType)==0) return hpatch_TRUE;
#ifdef  _CompressPlugin_zlib
    _trySetDecompress(out_decompressPlugin,compressType,&zlibDecompressPlugin);
#endif
#ifdef  _CompressPlugin_bz2
    _trySetDecompress(out_decompressPlugin,compressType,&bz2DecompressPlugin);
#endif
#ifdef  _CompressPlugin_lzma
    _trySetDecompress(out_decompressPlugin,compressType,&lzmaDecompressPlugin);
#endif
#ifdef  _CompressPlugin_lzma2
    _trySetDecompress(out_decompressPlugin,compressType,&lzma2DecompressPlugin);
#endif
#if (defined(_CompressPlugin_lz4) || (defined(_CompressPlugin_lz4hc)))
    _trySetDecompress(out_decompressPlugin,compressType,&lz4DecompressPlugin);
#endif
#ifdef  _CompressPlugin_zstd
    _trySetDecompress(out_decompressPlugin,compressType,&zstdDecompressPlugin);
#endif
#ifdef  _CompressPlugin_brotli
    _trySetDecompress(out_decompressPlugin,compressType,&brotliDecompressPlugin);
#endif
#ifdef  _CompressPlugin_lzham
    _trySetDecompress(out_decompressPlugin,compressType,&lzhamDecompressPlugin);
#endif
    return 0!=*out_decompressPlugin;
}

#if (_IS_NEED_DIR_DIFF_PATCH)
static void _trySetChecksum(hpatch_TChecksum** out_checksumPlugin,const char* checksumType,
                            hpatch_TChecksum* testChecksumPlugin){
    if ((*out_checksumPlugin)!=0) return;
    if (0==strcmp(checksumType,testChecksumPlugin->checksumType()))
        *out_checksumPlugin=testChecksumPlugin;
}
static hpatch_BOOL findChecksum(hpatch_TChecksum** out_checksumPlugin,const char* checksumType){
    *out_checksumPlugin=0;
    if (strlen(checksumType)==0) return hpatch_TRUE;
#ifdef _ChecksumPlugin_crc32
    _trySetChecksum(out_checksumPlugin,checksumType,&crc32ChecksumPlugin);
#endif
#ifdef _ChecksumPlugin_adler32
    _trySetChecksum(out_checksumPlugin,checksumType,&adler32ChecksumPlugin);
#endif
#ifdef _ChecksumPlugin_adler64
    _trySetChecksum(out_checksumPlugin,checksumType,&adler64ChecksumPlugin);
#endif
#ifdef _ChecksumPlugin_fadler32
    _trySetChecksum(out_checksumPlugin,checksumType,&fadler32ChecksumPlugin);
#endif
#ifdef _ChecksumPlugin_fadler64
    _trySetChecksum(out_checksumPlugin,checksumType,&fadler64ChecksumPlugin);
#endif
#ifdef _ChecksumPlugin_fadler128
    _trySetChecksum(out_checksumPlugin,checksumType,&fadler128ChecksumPlugin);
#endif
#ifdef _ChecksumPlugin_md5
    _trySetChecksum(out_checksumPlugin,checksumType,&md5ChecksumPlugin);
#endif
    return (0!=*out_checksumPlugin);
}

static hpatch_BOOL _getOptChecksum(hpatch_TChecksum** out_checksumPlugin,
                                   const char* checksumType,const char* kNoChecksum){
    assert(0==*out_checksumPlugin);
    if (0==strcmp(checksumType,kNoChecksum))
        return hpatch_TRUE;
    else if (strlen(checksumType)==0)
        return hpatch_FALSE;
    else
        return findChecksum(out_checksumPlugin,checksumType);
}

#endif //_IS_NEED_DIR_DIFF_PATCH


static bool _tryGetCompressSet(const char** isMatchedType,const char* ptype,const char* ptypeEnd,
                               const char* cmpType,const char* cmpType2=0,
                               size_t* compressLevel=0,size_t levelMin=0,size_t levelMax=0,size_t levelDefault=0,
                               size_t* dictSize=0,size_t dictSizeMin=0,size_t dictSizeMax=0,size_t dictSizeDefault=0){
    if (*isMatchedType) return true; //ok
    const size_t ctypeLen=strlen(cmpType);
    const size_t ctype2Len=(cmpType2!=0)?strlen(cmpType2):0;
    if ( ((ctypeLen==(size_t)(ptypeEnd-ptype))&&(0==strncmp(ptype,cmpType,ctypeLen)))
        || ((cmpType2!=0)&&(ctype2Len==(size_t)(ptypeEnd-ptype))&&(0==strncmp(ptype,cmpType2,ctype2Len))) )
        *isMatchedType=cmpType; //ok
    else
        return true;//type mismatch
    
    if ((compressLevel)&&(ptypeEnd[0]=='-')){
        const char* plevel=ptypeEnd+1;
        const char* plevelEnd=findUntilEnd(plevel,'-');
        if (!a_to_size(plevel,plevelEnd-plevel,compressLevel)) return false; //error
        if (*compressLevel<levelMin) *compressLevel=levelMin;
        else if (*compressLevel>levelMax) *compressLevel=levelMax;
        if ((dictSize)&&(plevelEnd[0]=='-')){
            const char* pdictSize=plevelEnd+1;
            const char* pdictSizeEnd=findUntilEnd(pdictSize,'-');
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
    if (!(value)) { fprintf(stderr,"options " errorInfo " ERROR!\n\n"); printUsage(); return HDIFF_OPTIONS_ERROR; } }

static int _checkSetCompress(hdiff_TCompress** out_compressPlugin,
                             hpatch_TDecompress** out_decompressPlugin,
                             const char* ptype,const char* ptypeEnd){
    const char* isMatchedType=0;
    size_t      compressLevel=0;
#if (defined _CompressPlugin_lzma)||(defined _CompressPlugin_lzma2)
    size_t       dictSize=0;
    const size_t defaultDictSize=(1<<20)*8;
#endif
#if (defined _CompressPlugin_zstd)||(defined _CompressPlugin_brotli)||(defined _CompressPlugin_lzham)
    size_t       dictBits=0;
    const size_t defaultDictBits=20+4;
#endif
#ifdef _CompressPlugin_zlib
    _options_check(_tryGetCompressSet(&isMatchedType,ptype,ptypeEnd,"zlib",0,
                                      &compressLevel,1,9,9),"-c-zlib-?");
     if ((isMatchedType)&&(0==strcmp(isMatchedType,"zlib"))){
         static TCompressPlugin_zlib _zlibCompressPlugin=zlibCompressPlugin;
         _zlibCompressPlugin.compress_level=(int)compressLevel;
         *out_compressPlugin=&_zlibCompressPlugin.base;
         *out_decompressPlugin=&zlibDecompressPlugin; }
#   if (_IS_USED_MULTITHREAD)
    //pzlib
    _options_check(_tryGetCompressSet(&isMatchedType,ptype,ptypeEnd,"pzlib",0,
                                     &compressLevel,1,9,6),"-c-pzlib-?");
    if ((isMatchedType)&&(0==strcmp(isMatchedType,"pzlib"))){
        static TCompressPlugin_pzlib _pzlibCompressPlugin=pzlibCompressPlugin;
        _pzlibCompressPlugin.base.compress_level=(int)compressLevel;
        *out_compressPlugin=&_pzlibCompressPlugin.base.base;
        *out_decompressPlugin=&zlibDecompressPlugin; }
#   endif // _IS_USED_MULTITHREAD
#endif
#ifdef _CompressPlugin_bz2
    _options_check(_tryGetCompressSet(&isMatchedType,ptype,ptypeEnd,"bzip2","bz2",
                                      &compressLevel,1,9,9),"-c-bzip2-?");
     if ((isMatchedType)&&((0==strcmp(isMatchedType,"bzip2"))||(0==strcmp(isMatchedType,"bz2")))){
         static TCompressPlugin_bz2 _bz2CompressPlugin=bz2CompressPlugin;
         _bz2CompressPlugin.compress_level=(int)compressLevel;
         *out_compressPlugin=&_bz2CompressPlugin.base;
         *out_decompressPlugin=&bz2DecompressPlugin;  }
#   if (_IS_USED_MULTITHREAD)
    //pbzip2
    _options_check(_tryGetCompressSet(&isMatchedType,ptype,ptypeEnd,"pbzip2","pbz2",
                                      &compressLevel,1,9,8),"-c-pbzip2-?");
    if ((isMatchedType)&&((0==strcmp(isMatchedType,"pbzip2"))||(0==strcmp(isMatchedType,"pbz2")))){
        static TCompressPlugin_pbz2 _pbz2CompressPlugin=pbz2CompressPlugin;
        _pbz2CompressPlugin.base.compress_level=(int)compressLevel;
        *out_compressPlugin=&_pbz2CompressPlugin.base.base;
        *out_decompressPlugin=&bz2DecompressPlugin; }
#   endif // _IS_USED_MULTITHREAD
#endif
#ifdef _CompressPlugin_lzma
    _options_check(_tryGetCompressSet(&isMatchedType,ptype,ptypeEnd,"lzma",0,
                                      &compressLevel,0,9,7, &dictSize,1<<12,
                                      (sizeof(size_t)<=4)?(1<<27):((size_t)3<<29),defaultDictSize),"-c-lzma-?");
     if ((isMatchedType)&&(0==strcmp(isMatchedType,"lzma"))){
         static TCompressPlugin_lzma _lzmaCompressPlugin=lzmaCompressPlugin;
         _lzmaCompressPlugin.compress_level=(int)compressLevel;
         _lzmaCompressPlugin.dict_size=(int)dictSize;
         *out_compressPlugin=&_lzmaCompressPlugin.base;
         *out_decompressPlugin=&lzmaDecompressPlugin;  }
#endif
#ifdef _CompressPlugin_lzma2
    _options_check(_tryGetCompressSet(&isMatchedType,ptype,ptypeEnd,"lzma2",0,
                                      &compressLevel,0,9,7, &dictSize,1<<12,
                                      (sizeof(size_t)<=4)?(1<<27):((size_t)3<<29),defaultDictSize),"-c-lzma2-?");
     if ((isMatchedType)&&(0==strcmp(isMatchedType,"lzma2"))){
         static TCompressPlugin_lzma2 _lzma2CompressPlugin=lzma2CompressPlugin;
         _lzma2CompressPlugin.compress_level=(int)compressLevel;
         _lzma2CompressPlugin.dict_size=(int)dictSize;
         *out_compressPlugin=&_lzma2CompressPlugin.base;
         *out_decompressPlugin=&lzma2DecompressPlugin;  }
#endif
#ifdef _CompressPlugin_lz4
    _options_check(_tryGetCompressSet(&isMatchedType,ptype,ptypeEnd,"lz4",0,
                                      &compressLevel,1,50,50),"-c-lz4-?");
     if ((isMatchedType)&&(0==strcmp(isMatchedType,"lz4"))){
         static TCompressPlugin_lz4 _lz4CompressPlugin=lz4CompressPlugin;
         _lz4CompressPlugin.compress_level=(int)compressLevel;
         *out_compressPlugin=&_lz4CompressPlugin.base;
         *out_decompressPlugin=&lz4DecompressPlugin;  }
#endif
#ifdef _CompressPlugin_lz4hc
    _options_check(_tryGetCompressSet(&isMatchedType,ptype,ptypeEnd,"lz4hc",0,
                                      &compressLevel,3,12,11),"-c-lz4hc-?");
    if ((isMatchedType)&&(0==strcmp(isMatchedType,"lz4hc"))){
        static TCompressPlugin_lz4hc _lz4hcCompressPlugin=lz4hcCompressPlugin;
        _lz4hcCompressPlugin.compress_level=(int)compressLevel;
        *out_compressPlugin=&_lz4hcCompressPlugin.base;
        *out_decompressPlugin=&lz4DecompressPlugin;  }
#endif
#ifdef _CompressPlugin_zstd
    _options_check(_tryGetCompressSet(&isMatchedType,ptype,ptypeEnd,"zstd",0,
                                      &compressLevel,0,22,20, &dictBits,10,
                                      (sizeof(size_t)<=4)?30:31,defaultDictBits),"-c-zstd-?");
     if ((isMatchedType)&&(0==strcmp(isMatchedType,"zstd"))){
         static TCompressPlugin_zstd _zstdCompressPlugin=zstdCompressPlugin;
         _zstdCompressPlugin.compress_level=(int)compressLevel;
         _zstdCompressPlugin.dict_bits = (int)dictBits;
         *out_compressPlugin=&_zstdCompressPlugin.base;
         *out_decompressPlugin=&zstdDecompressPlugin;  }
#endif
#ifdef _CompressPlugin_brotli
    _options_check(_tryGetCompressSet(&isMatchedType,ptype,ptypeEnd,"brotli",0,
                                      &compressLevel,0,11,9, &dictBits,10,
                                      30,defaultDictBits),"-c-brotli-?");
     if ((isMatchedType)&&(0==strcmp(isMatchedType,"brotli"))){
         static TCompressPlugin_brotli _brotliCompressPlugin=brotliCompressPlugin;
         _brotliCompressPlugin.compress_level=(int)compressLevel;
         _brotliCompressPlugin.dict_bits = (int)dictBits;
         *out_compressPlugin=&_brotliCompressPlugin.base;
         *out_decompressPlugin=&brotliDecompressPlugin;  }
#endif
#ifdef _CompressPlugin_lzham
    _options_check(_tryGetCompressSet(&isMatchedType,ptype,ptypeEnd,"lzham",0,
                                      &compressLevel,0,5,4, &dictBits,15,
                                      (sizeof(size_t)<=4)?26:29,defaultDictBits),"-c-lzham-?");
     if ((isMatchedType)&&(0==strcmp(isMatchedType,"lzham"))){
         static TCompressPlugin_lzham _lzhamCompressPlugin=lzhamCompressPlugin;
         _lzhamCompressPlugin.compress_level=(int)compressLevel;
         _lzhamCompressPlugin.dict_bits = (int)dictBits;
         *out_compressPlugin=&_lzhamCompressPlugin.base;
         *out_decompressPlugin=&lzhamDecompressPlugin;  }
#endif
    _options_check((*out_compressPlugin!=0)&&(*out_decompressPlugin!=0),"-c-?");
    return HDIFF_SUCCESS;
}

#define _return_check(value,exitCode,errorInfo){ \
    if (!(value)) { fprintf(stderr,errorInfo " ERROR!\n"); return exitCode; } }

#define _kNULL_VALUE    ((hpatch_BOOL)(-1))
#define _kNULL_SIZE     (~(size_t)0)

#define _THREAD_NUMBER_NULL     0
#define _THREAD_NUMBER_MIN      1
#define _THREAD_NUMBER_DEFUALT  kDefaultCompressThreadNumber
#define _THREAD_NUMBER_MAX      (1<<8)

int hdiff_cmd_line(int argc, const char * argv[]){
    hpatch_BOOL isLoadAll=_kNULL_VALUE;
    hpatch_BOOL isPatchCheck=_kNULL_VALUE;
    hpatch_BOOL isDiff=_kNULL_VALUE;
    hpatch_BOOL isForceOverwrite=_kNULL_VALUE;
    hpatch_BOOL isOutputHelp=_kNULL_VALUE;
    hpatch_BOOL isOutputVersion=_kNULL_VALUE;
    hpatch_BOOL isOldPathInputEmpty=_kNULL_VALUE;
    size_t      matchValue=0;
    size_t      threadNum = _THREAD_NUMBER_NULL;
    hdiff_TCompress*        compressPlugin=0;
    hpatch_TDecompress*     decompressPlugin=0;
    hpatch_BOOL isSingleStreamDiff=_kNULL_VALUE;
    size_t    singleStreamStepSize=_kNULL_SIZE;
#if (_IS_NEED_DIR_DIFF_PATCH)
    hpatch_BOOL             isForceRunDirDiff=_kNULL_VALUE;
    size_t                  kMaxOpenFileNumber=_kNULL_SIZE; //only used in stream dir diff
    hpatch_BOOL             isSetChecksum=_kNULL_VALUE;
    hpatch_TChecksum*       checksumPlugin=0;
    std::string             manifestOut;
    std::string             manifestOld;
    std::string             manifestNew;
    std::vector<std::string>    ignorePathList;
    std::vector<std::string>    ignoreOldPathList;
    std::vector<std::string>    ignoreNewPathList;
#endif
    std::vector<const char *> arg_values;
    for (int i=1; i<argc; ++i) {
        const char* op=argv[i];
        _options_check(op!=0,"?");
        if (op[0]!='-'){
            hpatch_BOOL isEmpty=(strlen(op)==0);
            if (isEmpty){
                if (isOldPathInputEmpty==_kNULL_VALUE)
                    isOldPathInputEmpty=hpatch_TRUE;
                else
                    _options_check(!isEmpty,"?"); //error return
            }else{
                if (isOldPathInputEmpty==_kNULL_VALUE)
                    isOldPathInputEmpty=hpatch_FALSE;
            }
            arg_values.push_back(op); //path:file or directory
            continue;
        }
        switch (op[1]) {
            case 'm':{ //diff in memory
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
#if (_IS_NEED_SINGLE_STREAM_DIFF)
                _options_check((isSingleStreamDiff!=hpatch_TRUE),"-SD");
#endif
                isLoadAll=hpatch_FALSE; //diff by stream
                if (op[2]=='-'){
                    const char* pnum=op+3;
                    _options_check(kmg_to_size(pnum,strlen(pnum),&matchValue),"-s-?");
                }else{
                    matchValue=kMatchBlockSize_default;
                }
            } break;
#if (_IS_NEED_SINGLE_STREAM_DIFF)
            case 'S':{
                _options_check((isSingleStreamDiff==_kNULL_VALUE)&&(isLoadAll!=hpatch_FALSE)
                               &&(op[2]=='D')&&((op[3]=='\0')||(op[3]=='-')),"-SD");
                isSingleStreamDiff=hpatch_TRUE;
                if (op[3]=='-'){
                    const char* pnum=op+4;
                    _options_check(kmg_to_size(pnum,strlen(pnum),&singleStreamStepSize),"-SD-?");
                    _options_check((singleStreamStepSize>=hpatch_kStreamCacheSize),"-SD-?");
                }else{
                    singleStreamStepSize=kDefaultStepMemSize;
                }
            } break;
#endif
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
            case 'f':{
                _options_check((isForceOverwrite==_kNULL_VALUE)&&(op[2]=='\0'),"-f");
                isForceOverwrite=hpatch_TRUE;
            } break;
            case 'd':{
                _options_check((isDiff==_kNULL_VALUE)&&(op[2]=='\0'),"-d");
                isDiff=hpatch_TRUE; //diff only
            } break;
#if (_IS_USED_MULTITHREAD)
            case 'p':{
                _options_check((threadNum==_THREAD_NUMBER_NULL)&&(op[2]=='-'),"-p-?");
                const char* pnum=op+3;
                _options_check(a_to_size(pnum,strlen(pnum),&threadNum),"-p-?");
                _options_check(threadNum>=_THREAD_NUMBER_MIN,"-p-?");
            } break;
#endif
            case 'c':{
                _options_check((compressPlugin==0)&&(op[2]=='-'),"-c");
                const char* ptype=op+3;
                const char* ptypeEnd=findUntilEnd(ptype,'-');
                int result=_checkSetCompress(&compressPlugin,&decompressPlugin,ptype,ptypeEnd);
                if (HDIFF_SUCCESS!=result)
                    return result;
            } break;
#if (_IS_NEED_DIR_DIFF_PATCH)
            case 'C':{
                _options_check((isSetChecksum==_kNULL_VALUE)&&(checksumPlugin==0)&&(op[2]=='-'),"-C");
                const char* ptype=op+3;
                isSetChecksum=hpatch_TRUE;
                _options_check(_getOptChecksum(&checksumPlugin,ptype,"no"),"-C-?");
            } break;
            case 'n':{
                _options_check((kMaxOpenFileNumber==_kNULL_SIZE)&&(op[2]=='-'),"-n-?")
                const char* pnum=op+3;
                _options_check(kmg_to_size(pnum,strlen(pnum),&kMaxOpenFileNumber),"-n-?");
            } break;
            case 'g':{
                if (op[2]=='#'){ //-g#
                    const char* plist=op+3;
                    _options_check(_getIgnorePathSetList(ignorePathList,plist),"-g#?");
                }else if (op[2]=='-'){
                    const char* plist=op+7;
                    if ((op[3]=='o')&&(op[4]=='l')&&(op[5]=='d')&&(op[6]=='#')){
                        _options_check(_getIgnorePathSetList(ignoreOldPathList,plist),"-g-old#?");
                    }else if ((op[3]=='n')&&(op[4]=='e')&&(op[5]=='w')&&(op[6]=='#')){
                        _options_check(_getIgnorePathSetList(ignoreNewPathList,plist),"-g-new#?");
                    }else{
                        _options_check(hpatch_FALSE,"-g-?");
                    }
                }else{
                    _options_check(hpatch_FALSE,"-g?");
                }
            } break;
            case 'M':{
                if (op[2]=='#'){ //-M#
                    const char* plist=op+3;
                    _options_check(manifestOut.empty()&&manifestOld.empty()&&manifestNew.empty(),"-M#");
                    manifestOut=plist;
                }else if (op[2]=='-'){
                    const char* plist=op+7;
                    if ((op[3]=='o')&&(op[4]=='l')&&(op[5]=='d')&&(op[6]=='#')){
                        _options_check(manifestOut.empty()&&manifestOld.empty(),"-M-old#");
                        manifestOld=plist;
                    }else if ((op[3]=='n')&&(op[4]=='e')&&(op[5]=='w')&&(op[6]=='#')){
                        _options_check(manifestOut.empty()&&manifestNew.empty(),"-M-new#");
                        manifestNew=plist;
                    }else{
                        _options_check(hpatch_FALSE,"-M-?");
                    }
                }else{
                    _options_check(hpatch_FALSE,"-M?");
                }
            } break;
            case 'D':{
                _options_check((isForceRunDirDiff==_kNULL_VALUE)&&(op[2]=='\0'),"-D");
                isForceRunDirDiff=hpatch_TRUE; //force run DirDiff
            } break;
#endif
            default: {
                _options_check(hpatch_FALSE,"-?");
            } break;
        }//switch
    }
    
    if (isOutputHelp==_kNULL_VALUE)
        isOutputHelp=hpatch_FALSE;
    if (isOutputVersion==_kNULL_VALUE)
        isOutputVersion=hpatch_FALSE;
    if (isForceOverwrite==_kNULL_VALUE)
        isForceOverwrite=hpatch_FALSE;
    if (isOutputHelp||isOutputVersion){
        if (isOutputHelp)
            printUsage();//with version
        else
            printVersion();
        if (arg_values.empty())
            return 0; //ok
    }
    if (isSingleStreamDiff==_kNULL_VALUE)
        isSingleStreamDiff=hpatch_FALSE;
#if (_IS_NEED_DIR_DIFF_PATCH)
    if (kMaxOpenFileNumber==_kNULL_SIZE)
        kMaxOpenFileNumber=kMaxOpenFileNumber_default_diff;
    if (kMaxOpenFileNumber<kMaxOpenFileNumber_default_min)
        kMaxOpenFileNumber=kMaxOpenFileNumber_default_min;
#endif
    if (threadNum==_THREAD_NUMBER_NULL)
        threadNum=_THREAD_NUMBER_DEFUALT;
    else if (threadNum>_THREAD_NUMBER_MAX)
        threadNum=_THREAD_NUMBER_MAX;
    if (compressPlugin!=0){
        compressPlugin->setParallelThreadNumber(compressPlugin,(int)threadNum);
    }
    
    if (isOldPathInputEmpty==_kNULL_VALUE)
        isOldPathInputEmpty=hpatch_FALSE;
    _options_check((arg_values.size()==1)||(arg_values.size()==2)||(arg_values.size()==3),"input count");
    if (arg_values.size()==3){ //diff
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
        
#if (_IS_NEED_DIR_DIFF_PATCH)
        if (isForceRunDirDiff==_kNULL_VALUE)
            isForceRunDirDiff=hpatch_FALSE;
        if (isSetChecksum==_kNULL_VALUE)
            isSetChecksum=hpatch_FALSE;
        
        if ((!manifestOld.empty())||(!manifestNew.empty())){
            isForceRunDirDiff=hpatch_TRUE;
            _options_check(manifestOut.empty()&&(!manifestNew.empty()),"-M?");
            if (isOldPathInputEmpty){
                _options_check(manifestOld.empty(),"-M?");
            }else{
                _options_check(!manifestOld.empty(),"-M?");
            }
            _options_check(ignorePathList.empty()&&ignoreOldPathList.empty()
                           &&ignoreNewPathList.empty(),"-M can't run with -g");
        }
#endif

        const char* oldPath        =arg_values[0];
        const char* newPath        =arg_values[1];
        const char* outDiffFileName=arg_values[2];
        
        _return_check(!hpatch_getIsSamePath(oldPath,outDiffFileName),
                      HDIFF_PATHTYPE_ERROR,"oldPath outDiffFile same path");
        _return_check(!hpatch_getIsSamePath(newPath,outDiffFileName),
                      HDIFF_PATHTYPE_ERROR,"newPath outDiffFile same path");
        if (!isForceOverwrite){
            hpatch_TPathType   outDiffFileType;
            _return_check(hpatch_getPathStat(outDiffFileName,&outDiffFileType,0),
                          HDIFF_PATHTYPE_ERROR,"get outDiffFile type");
            _return_check((outDiffFileType==kPathType_notExist)||(!isDiff),
                          HDIFF_PATHTYPE_ERROR,"diff outDiffFile already exists, overwrite");
        }
        hpatch_TPathType oldType;
        hpatch_TPathType newType;
        if (isOldPathInputEmpty){
            oldType=kPathType_file;     //as empty file
            isLoadAll=hpatch_FALSE;     //not need -m, set as -s
            matchValue=hpatch_kStreamCacheSize; //not used
        }else{
            _return_check(hpatch_getPathStat(oldPath,&oldType,0),HDIFF_PATHTYPE_ERROR,"get oldPath type");
            _return_check((oldType!=kPathType_notExist),HDIFF_PATHTYPE_ERROR,"oldPath not exist");
        }
        _return_check(hpatch_getPathStat(newPath,&newType,0),HDIFF_PATHTYPE_ERROR,"get newPath type");
        _return_check((newType!=kPathType_notExist),HDIFF_PATHTYPE_ERROR,"newPath not exist");
#if (_IS_NEED_DIR_DIFF_PATCH)
        hpatch_BOOL isUseDirDiff=isForceRunDirDiff||(kPathType_dir==oldType)||(kPathType_dir==newType);
        if (isUseDirDiff){
#ifdef _ChecksumPlugin_fadler64
            if (isSetChecksum==hpatch_FALSE){
                checksumPlugin=&fadler64ChecksumPlugin; //DEFAULT
                isSetChecksum=hpatch_TRUE;
            }
#else
#   ifdef _ChecksumPlugin_crc32
            if (isSetChecksum==hpatch_FALSE){
                checksumPlugin=&crc32ChecksumPlugin; //DEFAULT
                isSetChecksum=hpatch_TRUE;
            }
#   endif
#endif
        }else
        {
            _options_check(checksumPlugin==0,"-C now only support dir diff, unsupport diff");
        }
#endif //_IS_NEED_DIR_DIFF_PATCH

#if (_IS_NEED_DIR_DIFF_PATCH)
        if (isUseDirDiff){
            return hdiff_dir(oldPath,newPath,outDiffFileName, (kPathType_dir==oldType),
                             (kPathType_dir==newType), isDiff,isLoadAll,matchValue,isPatchCheck,
                             compressPlugin,decompressPlugin,checksumPlugin,kMaxOpenFileNumber,
                             ignorePathList,ignoreOldPathList,ignoreNewPathList,
                             manifestOld,manifestNew);
        }else
#endif
        {
            return hdiff(oldPath,newPath,outDiffFileName,isDiff,isLoadAll,matchValue,isPatchCheck,
                         compressPlugin,decompressPlugin,isSingleStreamDiff,singleStreamStepSize);
        }
#if (_IS_NEED_DIR_DIFF_PATCH)
    }else if (!manifestOut.empty()){ //() //create manifest
        _options_check(arg_values.size()==1,"create manifest file used one inputPath");
        _options_check(ignoreOldPathList.empty(),"-g-old unsupport run with create manifest file mode");
        _options_check(ignoreNewPathList.empty(),"-g-new unsupport run with create manifest file mode");
        if (isSetChecksum==_kNULL_VALUE)
            isSetChecksum=hpatch_FALSE;
#ifdef _ChecksumPlugin_fadler64
        if (isSetChecksum==hpatch_FALSE){
            checksumPlugin=&fadler64ChecksumPlugin; //DEFAULT
            isSetChecksum=hpatch_TRUE;
        }
#else
#   ifdef _ChecksumPlugin_crc32
        if (isSetChecksum==hpatch_FALSE){
            checksumPlugin=&crc32ChecksumPlugin; //DEFAULT
            isSetChecksum=hpatch_TRUE;
        }
#   endif
#endif

        if (checksumPlugin==0) { printf("WARNING: create manifest file not set chercksum!\n\n"); }
        const char* inputPath =arg_values[0];
        if (!isForceOverwrite){
            hpatch_TPathType   outFileType;
            _return_check(hpatch_getPathStat(manifestOut.c_str(),&outFileType,0),
                          HDIFF_PATHTYPE_ERROR,"get outManifestFile type");
            _return_check(outFileType==kPathType_notExist,
                          HDIFF_PATHTYPE_ERROR,"create outManifestFile already exists, overwrite");
        }
        return create_manifest(inputPath,manifestOut.c_str(),checksumPlugin,ignorePathList);
#endif
    }else{// (arg_values.size()==2)  //resave
        _options_check(!isOldPathInputEmpty,"can't resave, must input a diffFile");
        _options_check((isLoadAll==_kNULL_VALUE),"-m or -s unsupport run with resave mode");
        _options_check((isDiff==_kNULL_VALUE),"-d unsupport run with resave mode");
        _options_check((isPatchCheck==_kNULL_VALUE),"-t unsupport run with resave mode");
        
#if (_IS_NEED_DIR_DIFF_PATCH)
        _options_check((isForceRunDirDiff==_kNULL_VALUE),"-D unsupport run with resave mode");
        _options_check((checksumPlugin==0),"-C unsupport run with resave mode");
#endif
        const char* diffFileName   =arg_values[0];
        const char* outDiffFileName=arg_values[1];
        hpatch_BOOL isDiffFile=_getIsCompressedDiffFile(diffFileName);
#if (_IS_NEED_DIR_DIFF_PATCH)
        isDiffFile=isDiffFile || getIsDirDiffFile(diffFileName);
#endif
        _return_check(isDiffFile,HDIFF_RESAVE_DIFFINFO_ERROR,"can't resave, input file is not diffFile");
        if (!isForceOverwrite){
            hpatch_TPathType   outDiffFileType;
            _return_check(hpatch_getPathStat(outDiffFileName,&outDiffFileType,0),
                          HDIFF_PATHTYPE_ERROR,"get outDiffFile type");
            _return_check(outDiffFileType==kPathType_notExist,
                          HDIFF_PATHTYPE_ERROR,"resave outDiffFile already exists, overwrite");
        }
        hpatch_BOOL isSamePath=hpatch_getIsSamePath(diffFileName,outDiffFileName);
        if (isSamePath)
            _return_check(isForceOverwrite,HDIFF_PATHTYPE_ERROR,"diffFile outDiffFile same name");
        
        if (!isSamePath){
            return hdiff_resave(diffFileName,outDiffFileName,compressPlugin);
        }else{
            // 1. resave to newDiffTempName
            // 2. if resave ok    then  { delelte oldDiffFile; rename newDiffTempName to oldDiffName; }
            //    if resave error then  { delelte newDiffTempName; }
            char newDiffTempName[hpatch_kPathMaxSize];
            _return_check(hpatch_getTempPathName(outDiffFileName,newDiffTempName,newDiffTempName+hpatch_kPathMaxSize),
                          HDIFF_TEMPPATH_ERROR,"getTempPathName(diffFile)");
            printf("NOTE: out_diff temp file will be rename to in_diff name after resave!\n");
            int result=hdiff_resave(diffFileName,newDiffTempName,compressPlugin);
            if (result==0){//resave ok
                _return_check(hpatch_removeFile(diffFileName),
                              HDIFF_DELETEPATH_ERROR,"removeFile(diffFile)");
                _return_check(hpatch_renamePath(newDiffTempName,diffFileName),
                              HDIFF_RENAMEPATH_ERROR,"renamePath(temp,diffFile)");
                printf("out_diff temp file renamed to in_diff name!\n");
            }else{//resave error
                if (!hpatch_removeFile(newDiffTempName)){
                    printf("WARNING: can't remove temp file \"");
                    hpatch_printPath_utf8(newDiffTempName); printf("\"\n");
                }
            }
            return result;
        }
    }
}

#define _check_readFile(value) { if (!(value)) { hpatch_TFileStreamInput_close(&file); return hpatch_FALSE; } }
static hpatch_BOOL readFileAll(hdiff_private::TAutoMem& out_mem,const char* fileName){
    size_t              dataSize;
    hpatch_TFileStreamInput    file;
    hpatch_TFileStreamInput_init(&file);
    _check_readFile(hpatch_TFileStreamInput_open(&file,fileName));
    dataSize=(size_t)file.base.streamSize;
    _check_readFile(dataSize==file.base.streamSize);
    try {
        out_mem.realloc(dataSize);
    } catch (...) {
        _check_readFile(false);
    }
    _check_readFile(file.base.read(&file.base,0,out_mem.data(),out_mem.data_end()));
    return hpatch_TFileStreamInput_close(&file);
}

#define _check_writeFile(value) { if (!(value)) { hpatch_TFileStreamOutput_close(&file); return hpatch_FALSE; } }
static hpatch_BOOL writeFileAll(const TByte* pdata,size_t dataSize,const char* outFileName){
    hpatch_TFileStreamOutput file;
    hpatch_TFileStreamOutput_init(&file);
    _check_writeFile(hpatch_TFileStreamOutput_open(&file,outFileName,dataSize));
    _check_writeFile(file.base.write(&file.base,0,pdata,pdata+dataSize));
    return hpatch_TFileStreamOutput_close(&file);
}

#define  _check_on_error(errorType) { \
    if (result==HDIFF_SUCCESS) result=errorType; if (!_isInClear){ goto clear; } }
#define check(value,errorType,errorInfo) { \
    std::string erri=std::string()+errorInfo+" ERROR!\n"; \
    if (!(value)){ hpatch_printStdErrPath_utf8(erri.c_str()); _check_on_error(errorType); } }

static int hdiff_mem(const char* oldFileName,const char* newFileName,const char* outDiffFileName,
                     hpatch_BOOL isDiff,size_t matchScore,hpatch_BOOL isPatchCheck,
                     const hdiff_TCompress* compressPlugin,hpatch_TDecompress* decompressPlugin,
                     hpatch_BOOL isSingleStreamDiff,size_t singleStreamStepSize){
    double diff_time0=clock_s();
    int    result=HDIFF_SUCCESS;
    int    _isInClear=hpatch_FALSE;
    hdiff_private::TAutoMem oldMem(0);
    hdiff_private::TAutoMem newMem(0);
    check(readFileAll(oldMem,oldFileName),HDIFF_OPENREAD_ERROR,"open oldFile");
    check(readFileAll(newMem,newFileName),HDIFF_OPENREAD_ERROR,"open newFile");
    printf("oldDataSize : %" PRIu64 "\nnewDataSize : %" PRIu64 "\n",
           (hpatch_StreamPos_t)oldMem.size(),(hpatch_StreamPos_t)newMem.size());
    if (isDiff){
        std::vector<TByte> outDiffData;
        try {
#if (_IS_NEED_SINGLE_STREAM_DIFF)
            if (isSingleStreamDiff){
                printf("create single compressed stream diffData!\n");
                create_single_compressed_diff(newMem.data(),newMem.data_end(),oldMem.data(),oldMem.data_end(),
                                              outDiffData,0,compressPlugin,(int)matchScore,singleStreamStepSize);
            }else
#endif
            create_compressed_diff(newMem.data(),newMem.data_end(),oldMem.data(),oldMem.data_end(),
                                   outDiffData,compressPlugin,(int)matchScore);
        }catch(const std::exception& e){
            check(false,HDIFF_DIFF_ERROR,"diff run an error: "+e.what());
        }
        printf("diffDataSize: %" PRIu64 "\n",(hpatch_StreamPos_t)outDiffData.size());
        check(writeFileAll(outDiffData.data(),outDiffData.size(),outDiffFileName),
              HDIFF_OPENWRITE_ERROR,"open write diffFile");
        printf("diff    time: %.3f s\n",(clock_s()-diff_time0));
        printf("  out diff file ok!\n");
        outDiffData.clear();
    }
    if (isPatchCheck){
        isSingleStreamDiff=hpatch_FALSE;
        hdiff_private::TAutoMem diffMem(0);
        double patch_time0=clock_s();
        printf("\nload diffFile for test by patch:\n");
        check(readFileAll(diffMem,outDiffFileName),
              HDIFF_OPENREAD_ERROR,"open diffFile for test");
        printf("diffDataSize: %" PRIu64 "\n",(hpatch_StreamPos_t)diffMem.size());
        
        bool diffrt;
        {
            hpatch_TDecompress* saved_decompressPlugin=0;
            {
                hpatch_compressedDiffInfo diffinfo;
                if (!getCompressedDiffInfo_mem(&diffinfo,diffMem.data(),diffMem.data_end())){
#if (_IS_NEED_SINGLE_STREAM_DIFF)
                    hpatch_singleCompressedDiffInfo sdiffInfo;
                    if (getSingleCompressedDiffInfo_mem(&sdiffInfo,diffMem.data(),diffMem.data_end())){
                        memcpy(diffinfo.compressType,sdiffInfo.compressType,strlen(sdiffInfo.compressType)+1);
                        isSingleStreamDiff=hpatch_TRUE;
                        if (!isDiff)
                            printf("test single compressed stream diffData!\n");
                    }else
#endif
                        check(hpatch_FALSE,HDIFF_PATCH_ERROR,"get diff info");
                }
                check(findDecompress(&saved_decompressPlugin,diffinfo.compressType),
                      HDIFF_PATCH_ERROR,"diff data saved compress type");
            }
#if (_IS_NEED_SINGLE_STREAM_DIFF)
            if (isSingleStreamDiff){
                diffrt=check_single_compressed_diff(newMem.data(),newMem.data_end(),oldMem.data(),oldMem.data_end(),
                                                    diffMem.data(),diffMem.data_end(),saved_decompressPlugin);
            }else
#endif
                diffrt=check_compressed_diff(newMem.data(),newMem.data_end(),oldMem.data(),oldMem.data_end(),
                                             diffMem.data(),diffMem.data_end(),saved_decompressPlugin);
        }
        check(diffrt,HDIFF_PATCH_ERROR,"patch check diff data");
        printf("patch   time: %.3f s\n",(clock_s()-patch_time0));
        printf("  patch check diff data ok!\n");
    }
clear:
    _isInClear=hpatch_TRUE;
    return result;
}

static int hdiff_stream(const char* oldFileName,const char* newFileName,const char* outDiffFileName,
                        hpatch_BOOL isDiff,size_t matchBlockSize,hpatch_BOOL isPatchCheck,
                        const hdiff_TCompress* compressPlugin,hpatch_TDecompress* decompressPlugin){
    double diff_time0=clock_s();
    int result=HDIFF_SUCCESS;
    int _isInClear=hpatch_FALSE;
    hpatch_TFileStreamInput  oldData;
    hpatch_TFileStreamInput  newData;
    hpatch_TFileStreamOutput diffData_out;
    hpatch_TFileStreamInput  diffData_in;
    hpatch_TFileStreamInput_init(&oldData);
    hpatch_TFileStreamInput_init(&newData);
    hpatch_TFileStreamOutput_init(&diffData_out);
    hpatch_TFileStreamInput_init(&diffData_in);
    
    if (0==strcmp(oldFileName,"")){ // isOldPathInputEmpty
        mem_as_hStreamInput(&oldData.base,0,0);
    }else{
        check(hpatch_TFileStreamInput_open(&oldData,oldFileName),HDIFF_OPENREAD_ERROR,"open oldFile");
    }
    check(hpatch_TFileStreamInput_open(&newData,newFileName),HDIFF_OPENREAD_ERROR,"open newFile");
    printf("oldDataSize : %" PRIu64 "\nnewDataSize : %" PRIu64 "\n",
           oldData.base.streamSize,newData.base.streamSize);
    if (isDiff){
        check(hpatch_TFileStreamOutput_open(&diffData_out,outDiffFileName,~(hpatch_StreamPos_t)0),
              HDIFF_OPENWRITE_ERROR,"open out diffFile");
        hpatch_TFileStreamOutput_setRandomOut(&diffData_out,hpatch_TRUE);
        try{
            create_compressed_diff_stream(&newData.base,&oldData.base, &diffData_out.base,
                                          compressPlugin,matchBlockSize);
            diffData_out.base.streamSize=diffData_out.out_length;
        }catch(const std::exception& e){
            check(false,HDIFF_DIFF_ERROR,"stream diff run an error: "+e.what());
        }
        check(hpatch_TFileStreamOutput_close(&diffData_out),HDIFF_FILECLOSE_ERROR,"out diffFile close");
        printf("diffDataSize: %" PRIu64 "\n",diffData_out.base.streamSize);
        printf("diff    time: %.3f s\n",(clock_s()-diff_time0));
        printf("  out diff file ok!\n");
    }
    if (isPatchCheck){
        double patch_time0=clock_s();
        printf("\nload diffFile for test by patch check:\n");
        check(hpatch_TFileStreamInput_open(&diffData_in,outDiffFileName),HDIFF_OPENREAD_ERROR,"open check diffFile");
        printf("diffDataSize: %" PRIu64 "\n",diffData_in.base.streamSize);
        
        hpatch_TDecompress* saved_decompressPlugin=0;
        {
            hpatch_compressedDiffInfo diffinfo;
            check(getCompressedDiffInfo(&diffinfo,&diffData_in.base),HDIFF_PATCH_ERROR,"get diff info");
            check(findDecompress(&saved_decompressPlugin,diffinfo.compressType),
                  HDIFF_PATCH_ERROR,"diff data saved compress type");
        }
        check(check_compressed_diff_stream(&newData.base,&oldData.base,
                                           &diffData_in.base,saved_decompressPlugin),
              HDIFF_PATCH_ERROR,"patch check diff data");
        printf("patch   time: %.3f s\n",(clock_s()-patch_time0));
        printf("  patch check diff data ok!\n");
    }
clear:
    _isInClear=hpatch_TRUE;
    check(hpatch_TFileStreamOutput_close(&diffData_out),HDIFF_FILECLOSE_ERROR,"out diffFile close");
    check(hpatch_TFileStreamInput_close(&diffData_in),HDIFF_FILECLOSE_ERROR,"in diffFile close");
    check(hpatch_TFileStreamInput_close(&newData),HDIFF_FILECLOSE_ERROR,"newFile close");
    check(hpatch_TFileStreamInput_close(&oldData),HDIFF_FILECLOSE_ERROR,"oldFile close");
    return result;
}

int hdiff(const char* oldFileName,const char* newFileName,const char* outDiffFileName,hpatch_BOOL isDiff,
          hpatch_BOOL isLoadAll,size_t matchValue,hpatch_BOOL isPatchCheck,
          const hdiff_TCompress* compressPlugin,hpatch_TDecompress* decompressPlugin,
          hpatch_BOOL isSingleStreamDiff,size_t singleStreamStepSize){
    double time0=clock_s();
    std::string fnameInfo=std::string("old : \"")+oldFileName+"\"\n"
                                     +"new : \""+newFileName+"\"\n"
                             +(isDiff?"out : \"":"test: \"")+outDiffFileName+"\"\n";
    hpatch_printPath_utf8(fnameInfo.c_str());
    
    if (isDiff) {
        const char* compressType="";
        if (compressPlugin) compressType=compressPlugin->compressType();
        printf("hdiffz run with compress plugin: \"%s\"\n",compressType);
    }
    
    int exitCode;
    if (isLoadAll){
        exitCode=hdiff_mem(oldFileName,newFileName,outDiffFileName,isDiff,matchValue,isPatchCheck,
                           compressPlugin,decompressPlugin,isSingleStreamDiff,singleStreamStepSize);
    }else{
        assert(!isSingleStreamDiff); //now not support
        exitCode=hdiff_stream(oldFileName,newFileName,outDiffFileName,
                              isDiff,matchValue,isPatchCheck,compressPlugin,decompressPlugin);
    }
    if (isDiff && isPatchCheck)
        printf("\nall   time: %.3f s\n",(clock_s()-time0));
    return exitCode;
}

int hdiff_resave(const char* diffFileName,const char* outDiffFileName,
                 const hdiff_TCompress* compressPlugin){
    double time0=clock_s();
    std::string fnameInfo=std::string("in_diff : \"")+diffFileName+"\"\n"
        +"out_diff: \""+outDiffFileName+"\"\n";
    hpatch_printPath_utf8(fnameInfo.c_str());
    
    int result=HDIFF_SUCCESS;
    hpatch_BOOL  _isInClear=hpatch_FALSE;
    hpatch_BOOL  isDirDiff=false;
#if (_IS_NEED_DIR_DIFF_PATCH)
    std::string  dirCompressType;
    TDirDiffInfo dirDiffInfo;
    hpatch_TChecksum* checksumPlugin=0;
#endif
    hpatch_TFileStreamInput  diffData_in;
    hpatch_TFileStreamOutput diffData_out;
    hpatch_TFileStreamInput_init(&diffData_in);
    hpatch_TFileStreamOutput_init(&diffData_out);
    
    hpatch_TDecompress* decompressPlugin=0;
    check(hpatch_TFileStreamInput_open(&diffData_in,diffFileName),HDIFF_OPENREAD_ERROR,"open diffFile");
#if (_IS_NEED_DIR_DIFF_PATCH)
    check(getDirDiffInfo(&diffData_in.base,&dirDiffInfo),HDIFF_OPENREAD_ERROR,"read diffFile");
    isDirDiff=dirDiffInfo.isDirDiff;
#endif
    { //decompressPlugin
        hpatch_compressedDiffInfo diffInfo;
#if (_IS_NEED_DIR_DIFF_PATCH)
        if (isDirDiff){
            diffInfo=dirDiffInfo.hdiffInfo;
            diffInfo.compressedCount+=dirDiffInfo.dirDataIsCompressed?1:0;
        }else
#endif
        if (!getCompressedDiffInfo(&diffInfo,&diffData_in.base)){
            check(!diffData_in.fileError,HDIFF_RESAVE_FILEREAD_ERROR,"read diffFile");
            check(hpatch_FALSE,HDIFF_RESAVE_DIFFINFO_ERROR,"is hdiff file? get diff info");
        }
        findDecompress(&decompressPlugin,diffInfo.compressType);
        if (decompressPlugin==0){
            if (diffInfo.compressedCount>0){
                check(false,HDIFF_RESAVE_COMPRESSTYPE_ERROR,
                      "can no decompress \""+diffInfo.compressType+" data");
            }else{
                if (strlen(diffInfo.compressType)>0)
                    printf("  diffFile added useless compress tag \"%s\"\n",diffInfo.compressType);
                decompressPlugin=0;
            }
        }else{
            printf("resave %s with decompress plugin: \"%s\" (need decompress %d)\n",(isDirDiff?"dirDiffFile":"diffFile"),diffInfo.compressType,diffInfo.compressedCount);
        }
    }
    {
        const char* compressType="";
        if (compressPlugin) compressType=compressPlugin->compressType();
        printf("resave %s with compress plugin: \"%s\"\n",
               (isDirDiff?"dirDiffFile":"diffFile"),compressType);
    }
#if (_IS_NEED_DIR_DIFF_PATCH)
    if (isDirDiff){ //checksumPlugin
        if (strlen(dirDiffInfo.checksumType)>0){
            check(findChecksum(&checksumPlugin,dirDiffInfo.checksumType), HDIFF_RESAVE_CHECKSUMTYPE_ERROR,
                  "not found checksum plugin dirDiffFile used: \""+dirDiffInfo.checksumType+"\"\n");
            check(checksumPlugin->checksumByteSize()==dirDiffInfo.checksumByteSize,HDIFF_RESAVE_CHECKSUMTYPE_ERROR,
                  "found checksum plugin not same as dirDiffFile used: \""+dirDiffInfo.checksumType+"\"\n");
        }
        printf("resave dirDiffFile with checksum plugin: \"%s\"\n",dirDiffInfo.checksumType);
    }
#endif
    
    check(hpatch_TFileStreamOutput_open(&diffData_out,outDiffFileName,~(hpatch_StreamPos_t)0),HDIFF_OPENWRITE_ERROR,
          "open out diffFile");
    hpatch_TFileStreamOutput_setRandomOut(&diffData_out,hpatch_TRUE);
    printf("inDiffSize : %" PRIu64 "\n",diffData_in.base.streamSize);
    try{
#if (_IS_NEED_DIR_DIFF_PATCH)
        if (isDirDiff){
            resave_dirdiff(&diffData_in.base,decompressPlugin,
                           &diffData_out.base,compressPlugin,checksumPlugin);
        }else
#endif
        {
#if (_IS_NEED_DIR_DIFF_PATCH)
            _options_check(checksumPlugin==0,"-C now only support dir diff, unsupport diff");
#endif
            resave_compressed_diff(&diffData_in.base,decompressPlugin,
                                   &diffData_out.base,compressPlugin);
        }
        diffData_out.base.streamSize=diffData_out.out_length;
    }catch(const std::exception& e){
        check(false,HDIFF_RESAVE_ERROR,"resave diff run an error: "+e.what());
    }
    printf("outDiffSize: %" PRIu64 "\n",diffData_out.base.streamSize);
    check(hpatch_TFileStreamOutput_close(&diffData_out),HDIFF_FILECLOSE_ERROR,"out diffFile close");
    printf("  out diff file ok!\n");
    
    printf("\nhdiffz resave diffFile time: %.3f s\n",(clock_s()-time0));
clear:
    _isInClear=hpatch_TRUE;
    check(hpatch_TFileStreamOutput_close(&diffData_out),HDIFF_FILECLOSE_ERROR,"out diffFile close");
    check(hpatch_TFileStreamInput_close(&diffData_in),HDIFF_FILECLOSE_ERROR,"in diffFile close");
    return result;
}


#if (_IS_NEED_DIR_DIFF_PATCH)

struct DirPathIgnoreListener:public CDirPathIgnore,IDirPathIgnore{
    DirPathIgnoreListener(const std::vector<std::string>& ignorePathListBase,
                          const std::vector<std::string>& ignorePathList,bool isPrintIgnore=true)
    :CDirPathIgnore(ignorePathListBase,ignorePathList,isPrintIgnore){}
    //IDirPathIgnore
    virtual bool isNeedIgnore(const std::string& path,size_t rootPathNameLen){
        return CDirPathIgnore::isNeedIgnore(path,rootPathNameLen);
    }
};

struct DirDiffListener:public IDirDiffListener{
    virtual bool isExecuteFile(const std::string& fileName) {
        bool result= 0!=hpatch_getIsExecuteFile(fileName.c_str());
        if (result){
            std::string info="  got new file Execute tag:\""+fileName+"\"\n";
            hpatch_printPath_utf8(info.c_str());
        }
        return result;
    }
    virtual void diffRefInfo(size_t oldPathCount,size_t newPathCount,size_t sameFilePairCount,
                             hpatch_StreamPos_t sameFileSize,size_t refOldFileCount,size_t refNewFileCount,
                             hpatch_StreamPos_t refOldFileSize,hpatch_StreamPos_t refNewFileSize){
        printf("\n");
        printf("DirDiff old path count: %" PRIu64 "\n",(hpatch_StreamPos_t)oldPathCount);
        printf("        new path count: %" PRIu64 " (fileCount:%" PRIu64 ")\n",
               (hpatch_StreamPos_t)newPathCount,(hpatch_StreamPos_t)(sameFilePairCount+refNewFileCount));
        printf("       same file count: %" PRIu64 " (dataSize: %" PRIu64 ")\n",
               (hpatch_StreamPos_t)sameFilePairCount,sameFileSize);
        printf("    ref old file count: %" PRIu64 "\n",(hpatch_StreamPos_t)refOldFileCount);
        printf("   diff new file count: %" PRIu64 "\n",(hpatch_StreamPos_t)refNewFileCount);
        printf("\nrun hdiffz:\n");
        printf("  oldRefSize  : %" PRIu64 "\n",refOldFileSize);
        printf("  newRefSize  : %" PRIu64 " (all newSize: %" PRIu64 ")\n",refNewFileSize,refNewFileSize+sameFileSize);
    }
    
    double _runHDiffBegin_time0;
    virtual void runHDiffBegin(){ _runHDiffBegin_time0=clock_s(); }
    virtual void runHDiffEnd(hpatch_StreamPos_t diffDataSize){
        printf("  diffDataSize: %" PRIu64 "\n",diffDataSize);
        printf("  diff    time: %.3f s\n",clock_s()-_runHDiffBegin_time0);
    }
};

static void check_manifest(TManifest& out_manifest,const std::string& rootPath,const std::string& manifestFileName){
    TManifestSaved  manifest;
    load_manifestFile(manifest,rootPath,manifestFileName);
    hpatch_TChecksum* checksumPlugin=0;
    findChecksum(&checksumPlugin,manifest.checksumType.c_str());
    checksum_manifest(manifest,checksumPlugin);
    out_manifest.rootPath.swap(manifest.rootPath);
    out_manifest.pathList.swap(manifest.pathList);
}

int hdiff_dir(const char* _oldPath,const char* _newPath,const char* outDiffFileName,
              hpatch_BOOL oldIsDir, hpatch_BOOL newIsDir,
              hpatch_BOOL isDiff,hpatch_BOOL isLoadAll,size_t matchValue,hpatch_BOOL isPatchCheck,
              const hdiff_TCompress* compressPlugin,hpatch_TDecompress* decompressPlugin,
              hpatch_TChecksum* checksumPlugin,size_t kMaxOpenFileNumber,
              const std::vector<std::string>& ignorePathListBase,const std::vector<std::string>& ignoreOldPathList,
              const std::vector<std::string>& ignoreNewPathList,
              const std::string& oldManifestFileName,const std::string& newManifestFileName){
    double time0=clock_s();
    std::string oldPath(_oldPath);
    std::string newPath(_newPath);
    if (oldIsDir) assignDirTag(oldPath); else assert(!hpatch_getIsDirName(oldPath.c_str()));
    if (newIsDir) assignDirTag(newPath); else assert(!hpatch_getIsDirName(newPath.c_str()));
    std::string fnameInfo=std::string("")
        +(oldIsDir?"oldDir : \"":"oldFile: \"")+oldPath+"\"\n"
        +(newIsDir?"newDir : \"":"newFile: \"")+newPath+"\"\n"
        +(isDiff?  "outDiff: \"":"  test : \"")+outDiffFileName+"\"\n";
    hpatch_printPath_utf8(fnameInfo.c_str());
    
    bool isManifest= (!newManifestFileName.empty());
    if (isDiff) {
        const char* checksumType="";
        const char* compressType="";
        if (checksumPlugin)
            checksumType=checksumPlugin->checksumType();
        if (compressPlugin) compressType=compressPlugin->compressType();
        printf("hdiffz run %sdir diff with compress plugin: \"%s\"\n",isManifest?"manifest ":"",compressType);
        printf("hdiffz run %sdir diff with checksum plugin: \"%s\"\n",isManifest?"manifest ":"",checksumType);
    }
    printf("\n");
    
    int  result=HDIFF_SUCCESS;
    bool _isInClear=false;
    hpatch_TFileStreamOutput diffData_out;
    hpatch_TFileStreamInput diffData_in;
    hpatch_TFileStreamOutput_init(&diffData_out);
    hpatch_TFileStreamInput_init(&diffData_in);
    
    TManifest   oldManifest;
    TManifest   newManifest;
    if (isManifest){
        double check_time0=clock_s();
        try {
            if (!oldPath.empty())// isOldPathInputEmpty
                check_manifest(oldManifest,oldPath,oldManifestFileName);
            check_manifest(newManifest,newPath,newManifestFileName);
        }catch(const std::exception& e){
            check(false,MANIFEST_TEST_ERROR,"check by manifest found an error: "+e.what());
        }
        printf("check manifest time: %.3f s\n",(clock_s()-check_time0));
        printf("  check path datas by manifest ok!\n\n");
    }else{
        DirPathIgnoreListener oldDirPathIgnore(ignorePathListBase,ignoreOldPathList);
        DirPathIgnoreListener newDirPathIgnore(ignorePathListBase,ignoreNewPathList);
        get_manifest(&oldDirPathIgnore,oldPath,oldManifest);
        get_manifest(&newDirPathIgnore,newPath,newManifest);
    }

    if (isDiff){
        double diff_time0=clock_s();
        try {
            check(hpatch_TFileStreamOutput_open(&diffData_out,outDiffFileName,~(hpatch_StreamPos_t)0),
                  HDIFF_OPENWRITE_ERROR,"open out diffFile");
            hpatch_TFileStreamOutput_setRandomOut(&diffData_out,hpatch_TRUE);
            DirDiffListener listener;
            dir_diff(&listener,oldManifest,newManifest,&diffData_out.base,isLoadAll!=0,matchValue,
                     compressPlugin,checksumPlugin,kMaxOpenFileNumber);
            diffData_out.base.streamSize=diffData_out.out_length;
        }catch(const std::exception& e){
            check(false,DIRDIFF_DIFF_ERROR,"dir diff run an error: "+e.what());
        }
        printf("\ndiffDataSize  : %" PRIu64 "\n",diffData_out.base.streamSize);
        printf("dir  diff time: %.3f s\n",(clock_s()-diff_time0));
        check(hpatch_TFileStreamOutput_close(&diffData_out),HDIFF_FILECLOSE_ERROR,"out diffFile close");
        printf("  out dirDiffFile ok!\n");
    }
    if (isPatchCheck){
        double patch_time0=clock_s();
        printf("\nload %sdirDiffFile for test by patch check:\n",isManifest?"manifest ":"");
        check(hpatch_TFileStreamInput_open(&diffData_in,outDiffFileName),
              HDIFF_OPENREAD_ERROR,"open check diffFile");
        printf("diffDataSize  : %" PRIu64 "\n",diffData_in.base.streamSize);
        hpatch_TDecompress* saved_decompressPlugin=0;
        hpatch_TChecksum* saved_checksumPlugin=0;
        {
            TDirDiffInfo dirinfo;
            check(getDirDiffInfo(&diffData_in.base,&dirinfo),DIRDIFF_PATCH_ERROR,"get dir diff info");
            check(dirinfo.isDirDiff,DIRDIFF_PATCH_ERROR,"dir diffFile data");
            findDecompress(&saved_decompressPlugin,dirinfo.hdiffInfo.compressType);
            findChecksum(&saved_checksumPlugin,dirinfo.checksumType);
        }
        //check(check_dirOldDataChecksum(oldPath.c_str(),&diffData_in.base,
        //      saved_decompressPlugin,saved_checksumPlugin),
        //      DIRDIFF_PATCH_ERROR,"part diff data check_dirOldDataChecksum");
        DirDiffListener listener;
        check(check_dirdiff(&listener,oldManifest,newManifest,&diffData_in.base,
                            saved_decompressPlugin,saved_checksumPlugin,kMaxOpenFileNumber),
              DIRDIFF_PATCH_ERROR,"dir patch check diff data");

        printf("dir patch time: %.3f s\n",(clock_s()-patch_time0));
        printf("  patch check diff data ok!\n");
    }
    
    printf("\nall   time: %.3f s\n",(clock_s()-time0));
clear:
    _isInClear=true;
    check(hpatch_TFileStreamOutput_close(&diffData_out),HDIFF_FILECLOSE_ERROR,"out diffFile close");
    check(hpatch_TFileStreamInput_close(&diffData_in),HDIFF_FILECLOSE_ERROR,"in diffFile close");
    return result;
}

int create_manifest(const char* _inputPath,const char* outManifestFileName,
                    hpatch_TChecksum* checksumPlugin,const std::vector<std::string>& ignorePathList){
    double time0=clock_s();
    int  result=HDIFF_SUCCESS;
    bool _isInClear=false;
    std::string inputPath(_inputPath);
    {
        hpatch_TPathType inputType;
        check(hpatch_getPathStat(_inputPath,&inputType,0),HDIFF_PATHTYPE_ERROR,"get inputPath type");
        check((inputType!=kPathType_notExist),HDIFF_PATHTYPE_ERROR,"inputPath not exist");
        hpatch_BOOL inputIsDir=(inputType==kPathType_dir);
        if (inputIsDir) assignDirTag(inputPath);
        std::string fnameInfo=std::string("")
            +(inputIsDir? "inputDir   : \"":"inputFile:   \"")+inputPath+"\"\n"
                        + "outManifest: \""+outManifestFileName+"\"\n";
        hpatch_printPath_utf8(fnameInfo.c_str());
    }
    {
        const char* checksumType="";
        if (checksumPlugin) checksumType=checksumPlugin->checksumType();
        printf("hdiffz run create Manifest with checksum plugin: \"%s\"\n",checksumType);
        printf("\n");
    }
    
    hpatch_TFileStreamOutput manifestData_out;
    hpatch_TFileStreamOutput_init(&manifestData_out);
    {//create
        try {
            std::vector<std::string> emptyPathList;
            check(hpatch_TFileStreamOutput_open(&manifestData_out,outManifestFileName,~(hpatch_StreamPos_t)0),
                  HDIFF_OPENWRITE_ERROR,"open out manifestFile");
            //hpatch_TFileStreamOutput_setRandomOut(&manifestData_out,hpatch_TRUE);
            DirPathIgnoreListener dirPathIgnore(ignorePathList,emptyPathList);
            save_manifest(&dirPathIgnore,inputPath,&manifestData_out.base,checksumPlugin);
            manifestData_out.base.streamSize=manifestData_out.out_length;
        }catch(const std::exception& e){
            check(false,MANIFEST_CREATE_ERROR,"create manifest run an error: "+e.what());
        }
        printf("\nManifestFile size: %" PRIu64 "\n",manifestData_out.base.streamSize);
        check(hpatch_TFileStreamOutput_close(&manifestData_out),HDIFF_FILECLOSE_ERROR,"check manifestFile close");
        printf("  out manifestFile ok!\n");
        printf("create manifest time: %.3f s\n",(clock_s()-time0));
    }
    {//test
        double time1=clock_s();
        TManifest    manifest;
        try {
            check_manifest(manifest,inputPath,outManifestFileName);
        }catch(const std::exception& e){
            check(false,MANIFEST_TEST_ERROR,"test manifestFile found an error: "+e.what());
        }
        printf("  test manifestFile ok!\n");
        printf("test   manifest time: %.3f s\n",(clock_s()-time1));
    }
    
    printf("\nall   time: %.3f s\n",(clock_s()-time0));
clear:
    _isInClear=true;
    check(hpatch_TFileStreamOutput_close(&manifestData_out),HDIFF_FILECLOSE_ERROR,"check manifestFile close");
    return result;
}
#endif //_IS_NEED_DIR_DIFF_PATCH
