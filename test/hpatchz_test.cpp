//  hpatchz_test.cpp
//  Created by housisong on 2021/02/04.
/*
 The MIT License (MIT)
 Copyright (c) 2012-2021 HouSisong
 
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
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <string>
#include <fstream>
#include <math.h>
#include <vector>
#include <algorithm>
#include <stdio.h>
#include <assert.h>
#include <sstream>
#define  _IS_NEED_MAIN 0
#define  _IS_NEED_PRINT_LOG 0
#define  _IS_NEED_ORIGINAL 0
#include "../hpatchz.c"

bool        is_attack =true;
size_t      kLoopCount = 1000;
const bool  is_log_attack_tag=true;
size_t      cache_size_memory=0;
int     error_count=0;
size_t  test_i=0;
size_t  caseNum=0;


const size_t kMaxSize=400*1024*1024;

template<class T> inline static
std::string _to_string(const T& v){
    std::ostringstream os;
    os<<v;
    return os.str();
}

#define tcheck(v) { if (!(v)) { assert(false); throw "error!"; } }

void readFile(std::vector<TByte>& data,const char* fileName){
    bool _isInClear=false;
    int result=0;
    const size_t kStepSize =1024*256;
    size_t  readPos=0;
    size_t  fileSize;
    hpatch_TFileStreamInput    fStream;
    hpatch_TFileStreamInput_init(&fStream);

    check(hpatch_TFileStreamInput_open(&fStream,fileName),-1,"open file");
    fileSize=(size_t)fStream.base.streamSize;
    check(fileSize==fStream.base.streamSize,-1,"file size");
    try{
        data.resize(fileSize);
    }catch(...){
        check(false,-1,"memory alloc");
    }
    while (readPos<fileSize){
        size_t readLen=kStepSize;
        if (readPos+readLen>fileSize)
            readLen=fileSize-readPos;
        check(fStream.base.read(&fStream.base,readPos,data.data()+readPos,
                                data.data()+readPos+readLen),-1,"read file data");
        readPos+=readLen;
    }
clear:
    _isInClear=true;
    check(hpatch_TFileStreamInput_close(&fStream),-1,"close file");
    if (0!=result)
        throw result;
}


hpatch_BOOL _hpatchz(const hpatch_TStreamOutput* _out_newData,
                     const hpatch_TStreamInput*  oldData,
                     const hpatch_TStreamInput*  compressedDiff,
                    TByte* temp_cache,TByte* temp_cache_end){
    hpatch_TStreamOutput out_newData=*_out_newData;
    hpatch_TDecompress* decompressPlugin=0;
    hpatch_compressedDiffInfo diffInfo;
    if (!getCompressedDiffInfo(&diffInfo,compressedDiff))
        return hpatch_FALSE;
    if (oldData->streamSize!=diffInfo.oldDataSize)
        return hpatch_FALSE;
    out_newData.streamSize=diffInfo.newDataSize;
    {
        if (strlen(diffInfo.compressType)>0){
#ifdef  _CompressPlugin_zlib
            if ((!decompressPlugin)&&zlibDecompressPlugin.is_can_open(diffInfo.compressType))
                decompressPlugin=&zlibDecompressPlugin;
#endif
#ifdef  _CompressPlugin_bz2
            if ((!decompressPlugin)&&bz2DecompressPlugin.is_can_open(diffInfo.compressType))
                decompressPlugin=&bz2DecompressPlugin;
#endif
#ifdef  _CompressPlugin_lzma
            if ((!decompressPlugin)&&lzmaDecompressPlugin.is_can_open(diffInfo.compressType))
                decompressPlugin=&lzmaDecompressPlugin;
#endif
#if (defined(_CompressPlugin_lz4) || (defined(_CompressPlugin_lz4hc)))
            if ((!decompressPlugin)&&lz4DecompressPlugin.is_can_open(diffInfo.compressType))
                decompressPlugin=&lz4DecompressPlugin;
#endif
#ifdef  _CompressPlugin_zstd
            if ((!decompressPlugin)&&zstdDecompressPlugin.is_can_open(diffInfo.compressType))
                decompressPlugin=&zstdDecompressPlugin;
#endif
        }
        if (decompressPlugin==0)
            return hpatch_FALSE;
    }
    return patch_decompress_with_cache(&out_newData,oldData,compressedDiff,decompressPlugin,
                                       temp_cache,temp_cache_end);
}

int     _attack_seed=1111;
void testCaseByData(const std::string& tag,const std::vector<TByte>& oldData,const std::vector<TByte>& patData,
                    const std::vector<TByte>& newData,int& error_count){
    double  time0=clock_s();
    static std::vector<TByte> out_newData;
    static std::vector<TByte> temp_cache;
    temp_cache.resize(cache_size_memory);
    if (out_newData.empty()) out_newData.reserve(kMaxSize);
    out_newData.resize(newData.size());
    hpatch_TStreamOutput out_newStream;
    mem_as_hStreamOutput(&out_newStream,out_newData.data(),out_newData.data()+out_newData.size());
    std::cout << tag;
    if (is_attack){
        static std::vector<TByte> _oldData;
        static std::vector<TByte> _patData;
        if (_oldData.empty()) _oldData.reserve(kMaxSize);
        if (_patData.empty()) _patData.reserve(kMaxSize);
        _oldData.resize(oldData.size());
        _patData.resize(patData.size());
        hpatch_TStreamInput oldStream;
        hpatch_TStreamInput patStream;

        for (size_t i=0;i<kLoopCount;++i,++test_i){
            srand(_attack_seed);
            _attack_seed+=1;
            //if (test_i<?) continue;

            long in_codeSize=(long)patData.size();
            memcpy(_oldData.data(),oldData.data(),_oldData.size());
            memcpy(_patData.data(),patData.data(),in_codeSize);
            mem_as_hStreamInput(&oldStream,_oldData.data(),_oldData.data()+_oldData.size());
            mem_as_hStreamInput(&patStream,_patData.data(),_patData.data()+in_codeSize);

            if (in_codeSize){
                const long randCount=(long)(1+pow(rand()*(1.0/RAND_MAX),4)*1000);
                for (long r=0; r<randCount; ++r){
                    _patData[rand()%in_codeSize]=rand();
                }
            }
            try{
                if (is_log_attack_tag) std::cout <<tag;
                _hpatchz(&out_newStream,&oldStream,&patStream,
                         temp_cache.data(),temp_cache.data()+temp_cache.size());
                if (is_log_attack_tag) std::cout <<" | test_i=="<<test_i<<" , error_count=="<<error_count<<"\n";
            }catch (...){
                ++error_count;
                std::cout <<" | test_i=="<<test_i<<" , error_count=="<<error_count<<", throw error\n";
            }
        }
    }else{
        hpatch_TStreamInput oldStream;
        hpatch_TStreamInput patStream;
        mem_as_hStreamInput(&oldStream,oldData.data(),oldData.data()+oldData.size());
        mem_as_hStreamInput(&patStream,patData.data(),patData.data()+patData.size());
        for (size_t i=0;i<kLoopCount;++i,++test_i){
            memset(out_newData.data(),0,out_newData.size());
            
            hpatch_BOOL result=_hpatchz(&out_newStream,&oldStream,&patStream,
                                        temp_cache.data(),temp_cache.data()+temp_cache.size());
            if (!result){
                ++error_count;
                std::cout <<" | test_i=="<<test_i<<" , error_count=="<<error_count<<", return error_code=="<<result<<" \n";
            }else if (0!=memcmp(out_newData.data(),newData.data(),out_newData.size())){
                ++error_count;
                std::cout <<" | test_i=="<<test_i<<" , error_count=="<<error_count<<", out error data\n";
            }else if (is_log_attack_tag){
                std::cout <<" | test_i=="<<test_i<<" , error_count=="<<error_count<<"\n";
            }
        }
    }
    std::cout <<" | error_count=="<<error_count<<" , tescaseNum=="<<caseNum<<" , time: "<<(clock_s()-time0)<<" s\n";
}

void testCaseByFile(const std::string& tag,const std::string& oldPath,const std::string& patPath,const std::string& newPath,int& error_count){
    static std::vector<TByte> oldData;
    static std::vector<TByte> patData;
    static std::vector<TByte> newData;
    if (oldData.empty()) oldData.reserve(kMaxSize);
    if (patData.empty()) patData.reserve(kMaxSize);
    if (newData.empty()) newData.reserve(kMaxSize);
    if (oldPath.empty())
        oldData.resize(0);
    else
        readFile(oldData,oldPath.c_str());
    readFile(patData,patPath.c_str());
    readFile(newData,newPath.c_str());

    testCaseByData(tag,oldData,patData,newData,error_count);
}

void testCaseByLine(const std::string& dirPath,const std::string& line,int& error_count){
    //1|12306_5.2.11.apk--12306_5.1.2.apk.hdiffz_-s-16.pat
    size_t pos0=line.find_first_of('|');
    if (pos0==std::string::npos) return;
    pos0+=1;
    const std::string patFile=line.substr(pos0);
    size_t pos1=line.find(".apk--")+4;
    const std::string newFile=line.substr(pos0,pos1-pos0);
    pos0=pos1+2;
    pos1=line.find(".hdiffz_");
    const std::string oldFile=line.substr(pos0,pos1-pos0);
    pos0=pos1+8;
    pos1=line.size()-4;
    const std::string diffOp=line.substr(pos0,pos1-pos0);

    std::string tag=newFile + " <- " + oldFile + " " + diffOp;
    testCaseByFile(tag,oldFile.empty()?oldFile:(dirPath+oldFile),dirPath+patFile,dirPath+newFile,error_count);
}

size_t aToSize(const char* str) {
    size_t result;
    if (!kmg_to_size(str, strlen(str), &result))
        throw str;
    return result;
}

int main(int argc, const char*  argv[]){
    double  time0=clock_s();
    std::cout <<"hpatchz(attack code)\n";
    std::cout <<"input \"patch_test.txt\" file path for test\n";
    tcheck(argc==5);
    std::string txt=argv[1];
    is_attack = (1==aToSize(argv[2]));
    kLoopCount= aToSize(argv[3]);
    cache_size_memory=aToSize(argv[4]);

    size_t pos=txt.find_last_of('\\');
    if (pos==std::string::npos)
        pos=txt.find_last_of('/');
    std::string dirPath=txt.substr(0,(pos==std::string::npos)?0:pos+1);

    std::vector<std::string> lines;
    {
        std::fstream fs(txt);
        std::string  line;
        while(std::getline(fs,line)){
            size_t strLen=line.size();
            if ((strLen>0)&&(line[strLen-1]=='\r')) line.resize(strLen-1);
            lines.push_back(line);
        }
    }

    for (size_t i=0;i<lines.size();++i){
        const std::string& line=lines[i];
        ++caseNum;
        testCaseByLine(dirPath,line,error_count);
    }

    std::cout << "\n  error_count=="<<error_count<<"\n";
    std::cout <<"\ndone!\n";
    std::cout <<"\n time: "<<(clock_s()-time0)<<" s\n";
    return error_count;
}

