//patch_demo.cpp
// demo for HPatch
//NOTE: use HDiffZ+HPatchZ support compressed diff data
//
/*
 This is the HDiffPatch copyright.

 Copyright (c) 2012-2013 HouSisong All Rights Reserved.

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

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "libHDiffPatch/HPatch/patch.h"

//#define _IS_USE_PATCH_CACHE      //ON: faster, add some memory for patch cache
//#define _IS_USE_OLD_FILE_STREAM  //ON: slower, memroy needs less,because not need load oldFile

#ifdef _IS_USE_PATCH_CACHE
#   define  k_patch_cache_size  (1<<22)
#endif

#include "file_for_patch.h"

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

//  #include <time.h>
//  static double clock_s(){ return clock()*(1.0/CLOCKS_PER_SEC); }
#ifdef _WIN32
    #include <windows.h>
    static double clock_s(){ return GetTickCount()/1000.0; }
#else
    //Unix-like system
    #include <sys/time.h>
    static double clock_s(){
        struct timeval t={0,0};
        int ret=gettimeofday(&t,0);
        assert(ret==0);
        return t.tv_sec + t.tv_usec/1000000.0;
    }//*/
#endif

#ifndef PRId64
#   ifdef _MSC_VER
#       define PRId64 "I64d"
#   else
#       define PRId64 "lld"
#   endif
#endif

#define _free_mem(p) { if (p) { free(p); p=0; } }

#define _error_return(info){ \
    if (strlen(info)>0)      \
        printf("%s",(info)); \
    exitCode=1; \
    goto clear; \
}

#define _check_error(is_error,errorInfo){ \
    if (is_error){  \
        exitCode=1; \
        printf("%s",(errorInfo)); \
    } \
}


//diffFile need create by HDiff
int main(int argc, const char * argv[]){
    int     exitCode=0;
    double  time0=clock_s();
    double  time1,time2,time3;
    TFileStreamOutput   newData;
    TFileStreamInput    diffData;
#ifdef _IS_USE_OLD_FILE_STREAM
    TFileStreamInput     oldData;
    hpatch_TStreamInput* poldData=&oldData.base;
#else
    TByte*               poldData_mem=0;
    size_t               oldDataSize=0;
    hpatch_TStreamInput  oldData;
    hpatch_TStreamInput* poldData=&oldData;
#endif
#ifdef _IS_USE_PATCH_CACHE
    TByte*            temp_cache=0;
#endif
    if (argc!=4) {
        printf("patch command parameter:\n oldFileName diffFileName outNewFileName\n");
        return 1;
    }
#ifdef _IS_USE_OLD_FILE_STREAM
    TFileStreamInput_init(&oldData);
#endif
    TFileStreamInput_init(&diffData);
    TFileStreamOutput_init(&newData);
    {//open file
        int kNewDataSizeSavedSize=9;
        TByte buf[9];
        hpatch_StreamPos_t savedNewSize=0;
        const char* oldFileName=argv[1];
        const char* diffFileName=argv[2];
        const char* outNewFileName=argv[3];
        printf("old :\"%s\"\ndiff:\"%s\"\nout :\"%s\"\n",oldFileName,diffFileName,outNewFileName);
#ifdef _IS_USE_OLD_FILE_STREAM
        if (!TFileStreamInput_open(&oldData,oldFileName))
            _error_return("\nopen oldFile for read error!\n");
#else
        if (!readFileAll(&poldData_mem,&oldDataSize,oldFileName))
            _error_return("\nopen read oldFile error!\n");
        mem_as_hStreamInput(&oldData,poldData_mem,poldData_mem+oldDataSize);
#endif
        if (!TFileStreamInput_open(&diffData,diffFileName))
            _error_return("\nopen diffFile error!\n");
        //read savedNewSize
        if (kNewDataSizeSavedSize>diffData.base.streamSize)
            kNewDataSizeSavedSize=(int)diffData.base.streamSize;
        if (kNewDataSizeSavedSize!=diffData.base.read(diffData.base.streamHandle,0,
                                                      buf,buf+kNewDataSizeSavedSize))
            _error_return("\nread savedNewSize error!\n");
        kNewDataSizeSavedSize=readSavedSize(buf,kNewDataSizeSavedSize,&savedNewSize);
        if (kNewDataSizeSavedSize<=0) _error_return("\nread savedNewSize error!\n");
        TFileStreamInput_setOffset(&diffData,kNewDataSizeSavedSize);
        
        if (!TFileStreamOutput_open(&newData, outNewFileName,savedNewSize))
            _error_return("\nopen out newFile error!\n");
    }
    printf("oldDataSize : %" PRId64 "\ndiffDataSize: %" PRId64 "\nnewDataSize : %" PRId64 "\n",
           poldData->streamSize,diffData.base.streamSize,newData.base.streamSize);
    
    time1=clock_s();
#ifdef _IS_USE_PATCH_CACHE
    temp_cache=(TByte*)malloc(k_patch_cache_size);
    if (!temp_cache) _error_return("\nalloc cache memory error!\n");
    if (!patch_stream_with_cache(&newData.base,poldData,&diffData.base,
                                 temp_cache,temp_cache+k_patch_cache_size)){
        const char* kRunErrInfo="\npatch_with_cache() run error!\n";
#else
    if (!patch_stream(&newData.base,poldData,&diffData.base)){
        const char* kRunErrInfo="\npatch_stream() run error!\n";
#endif
#ifdef _IS_USE_OLD_FILE_STREAM
        _check_error(oldData.fileError,"\noldFile read error!\n");
#endif
        _check_error(diffData.fileError,"\ndiffFile read error!\n");
        _check_error(newData.fileError,"\nout newFile write error!\n");
        _error_return(kRunErrInfo);
    }
    if (newData.out_length!=newData.base.streamSize){
        printf("\nerror! out newFile dataSize %" PRId64 " != saved newDataSize %" PRId64 "\n",
               newData.out_length,newData.base.streamSize);
        _error_return("");
    }
    time2=clock_s();
    printf("  patch ok!\n");
    printf("\npatch   time: %.3f s\n",(time2-time1));
    
clear:
    _check_error(!TFileStreamOutput_close(&newData),"\nout newFile close error!\n");
    _check_error(!TFileStreamInput_close(&diffData),"\ndiffFile close error!\n");
#ifdef _IS_USE_OLD_FILE_STREAM
    _check_error(!TFileStreamInput_close(&oldData),"\noldFile close error!\n");
#else
    _free_mem(poldData_mem);
#endif
#ifdef _IS_USE_PATCH_CACHE
    _free_mem(temp_cache);
#endif
    time3=clock_s();
    printf("all run time: %.3f s\n",(time3-time0));
    return exitCode;
}


