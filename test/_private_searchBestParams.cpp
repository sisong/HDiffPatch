//_private_searchBestParams.cpp
// tool for HDiff
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
#include <sstream>
#include "assert.h"
#include <vector>
#include <math.h> //pow
#include <string.h>
#include <stdlib.h>
#include "../libHDiffPatch/HDiff/diff.h"
#include "../libHDiffPatch/HPatch/patch.h"
#include "../libHDiffPatch/HDiff/private_diff/suffix_string.h"

typedef unsigned char   TByte;
typedef unsigned int    TUInt32;
typedef ptrdiff_t       TInt;

void readFile(std::vector<TByte>& data,const char* fileName){
    std::ifstream file(fileName, std::ios::in | std::ios::binary | std::ios::ate);
    std::streampos file_length=file.tellg();
    file.seekg(0,std::ios::beg);
    size_t needRead=(size_t)file_length;
    if ((file_length<0)||((std::streamsize)needRead!=(std::streamsize)file_length)) {
        file.close();
        std::cout<<"open read file \""<<fileName<<"\" ERROR!\n";
        exit(1);
    }
    data.resize(needRead);
    file.read((char*)data.data(), needRead);
    std::streamsize readed=file.gcount();
    file.close();
    if ((std::streamsize)needRead!=readed){
        std::cout<<"read file \""<<fileName<<"\" ERROR!\n";
        exit(1);
    }
}

void writeFile(const std::vector<TByte>& data,const char* fileName){
    std::ofstream file(fileName, std::ios::out | std::ios::binary | std::ios::trunc);
    file.write((const char*)data.data(), data.size());
    file.close();
}

#define IS_NOTICE_compress_canceled 0 //for test, close compress fail notice
#define IS_REUSE_compress_handle    1 //for test, must in single thread

//===== select compress plugin =====
#define _CompressPlugin_no
#define _CompressPlugin_zlib
#define _CompressPlugin_bz2
#define _CompressPlugin_lzma

#include "../compress_plugin_demo.h"
#include "../decompress_plugin_demo.h"

struct THDiffPrivateParams{
    int out0;
    int out1;
    std::string asString()const{
        std::stringstream str;
        str<<out0;str<<',';
        str<<out1;
        return str.str();
    }
};

struct TDiffInfo{
    hdiff_private::TSuffixString sstring;
    std::vector<TByte>  oldData;
    std::vector<TByte>  newData;
    std::string         oldFileName;
    size_t              oldFileSize;
    std::string         newFileName;
    size_t              newFileSize;
    THDiffPrivateParams kP;
    size_t              diffSize;
    size_t              zipSize;
    size_t              bz2Size;
    size_t              lzmaSize;
    std::string asString()const{
        std::stringstream str;
        
        //str<<getFileName(oldFileName); str<<'\t';
        //str<<getFileName(newFileName); str<<'\t';
        //str<<oldFileSize; str<<'\t';
        //str<<newFileSize; str<<'\t';
        str<<kP.asString(); str<<'\t';
        str<<diffSize; str<<'\t';
        str<<zipSize; str<<'\t';
        str<<bz2Size; str<<'\t';
        str<<lzmaSize;
        
        return str.str();
    }
    std::string getFileName(const std::string& fullFileName)const{
        size_t pos=fullFileName.find_last_of('/');
        if (pos==std::string::npos)
            return fullFileName;
        else
            return fullFileName.c_str()+pos+1;
    }
};

static size_t _compress_diff(const TDiffInfo& di,const hdiff_TCompress* compressPlugin,
                             hpatch_TDecompress* decompressPlugin){
    extern void __hdiff_private__create_compressed_diff(const TByte* newData,const TByte* newData_end,
                                                        const TByte* oldData,const TByte* oldData_end,
                                                        std::vector<TByte>& out_diff,
                                                        const hdiff_TCompress* compressPlugin,
                                                        int kMinSingleMatchScore,
                                                        const hdiff_private::TSuffixString* sstring);
    std::vector<TByte> diffData;
    const TByte* newData0=di.newData.data();
    const TByte* oldData0=di.oldData.data();
    __hdiff_private__create_compressed_diff(newData0,newData0+di.newData.size(),
                                             oldData0,oldData0+di.oldData.size(),diffData,
                                             compressPlugin,di.kP.out0,&di.sstring);
    /*
    if (!check_compressed_diff(newData0,newData0+di.newData.size(),
                               oldData0,oldData0+di.oldData.size(),
                               diffData.data(),diffData.data()+diffData.size(),
                               decompressPlugin)){
        std::cout<<"\ncheck hdiffz data error!!!\n";
        exit(1);
    }//*/
    return diffData.size();
}

void doDiff(TDiffInfo& di){
    if (di.sstring.SASize()==0){
        readFile(di.oldData,di.oldFileName.c_str());
        readFile(di.newData,di.newFileName.c_str());
        di.oldFileSize=di.oldData.size();
        di.newFileSize=di.newData.size();
        const TByte* oldData0=di.oldData.data();
        di.sstring.resetSuffixString(oldData0,oldData0+di.oldData.size());
    }
    
    di.diffSize=_compress_diff(di,0,0);
    di.zipSize=_compress_diff(di,&zlibCompressPlugin,&zlibDecompressPlugin);
    di.bz2Size=_compress_diff(di,&bz2CompressPlugin,&bz2DecompressPlugin);
    di.lzmaSize=_compress_diff(di,&lzmaCompressPlugin,&lzmaDecompressPlugin);
}

static std::string rToStr(double R){
    char buf[256];
    sprintf(buf,"%0.6f",R);
    return buf;
}

static std::string rToTag(double cur,double& best){
    if (cur<best){
        best=cur;
        return "*";
    }else if (cur==best){
        return "-";
    }else{
        return " ";
    }
}

void getBestHDiffPrivateParams(const std::vector<std::string>& fileNames){
    const int kDoCount=(int)fileNames.size()/2;
    std::vector<TDiffInfo> DiList(kDoCount);
    for (int doi=0; doi<kDoCount; ++doi) {
        TDiffInfo& curDi=DiList[doi];
        curDi.oldFileName=fileNames[doi*2+0];
        curDi.newFileName=fileNames[doi*2+1];
    }
    
    double bestDiffR=1e308;
    double bestZipDiffR=1e308;
    double bestBz2DiffR=1e308;
    double bestLzmaDiffR=1e308;
    double bestCompressDiffR=1e308;
    bool isOutSrcSize=false;
    
    int kMinSingleMatchScore;
    for (kMinSingleMatchScore=8; kMinSingleMatchScore>=0; kMinSingleMatchScore--){{
        THDiffPrivateParams kP={kMinSingleMatchScore,0};
        
        double sumDiffR=1;
        double sumZipDiffR=1;
        double sumBz2DiffR=1;
        double sumLzmaDiffR=1;
        size_t sumOldSize=0;
        size_t sumNewSize=0;
        size_t sumDiffSize=0;
        size_t sumZipDiffSize=0;
        size_t sumBz2DiffSize=0;
        size_t sumLzmaDiffSize=0;
        for (size_t doi=0; doi<DiList.size(); ++doi) {
            TDiffInfo& curDi=DiList[doi];
            curDi.kP=kP;
            
            doDiff(curDi);
            double curDiffRi=curDi.diffSize*1.0/curDi.newFileSize;
            double curZipDiffRi=curDi.zipSize*1.0/curDi.newFileSize;
            double curBz2DiffRi=curDi.bz2Size*1.0/curDi.newFileSize;
            double curLzmaDiffRi=curDi.lzmaSize*1.0/curDi.newFileSize;
            sumDiffR*=curDiffRi;
            sumZipDiffR*=curZipDiffRi;
            sumBz2DiffR*=curBz2DiffRi;
            sumLzmaDiffR*=curLzmaDiffRi;
            sumNewSize+=curDi.newFileSize;
            sumOldSize+=curDi.oldFileSize;
            sumDiffSize+=curDi.diffSize;
            sumZipDiffSize+=curDi.zipSize;
            sumBz2DiffSize+=curDi.bz2Size;
            sumLzmaDiffSize+=curDi.lzmaSize;
            //std::cout<<curDi.asString()<<"\t"<<curDiffRi<<"\n";
        }
        
        const double curDiffR=pow(sumDiffR,1.0/kDoCount);
        const double curZipDiffR=pow(sumZipDiffR,1.0/kDoCount);
        const double curBz2DiffR=pow(sumBz2DiffR,1.0/kDoCount);
        const double curLzmaDiffR=pow(sumLzmaDiffR,1.0/kDoCount);
        const double curCompressDiffR=(curZipDiffR*1+curBz2DiffR*1+curLzmaDiffR*1)/(1+1+1);
        {
            TDiffInfo curDi;
            curDi.oldFileName="";
            curDi.newFileName="";
            curDi.kP=kP;
            curDi.oldFileSize=sumOldSize;
            curDi.newFileSize=sumNewSize;
            curDi.diffSize=sumDiffSize;
            curDi.zipSize=sumZipDiffSize;
            curDi.bz2Size=sumBz2DiffSize;
            curDi.lzmaSize=sumLzmaDiffSize;
            
            std:: string tag="";
            tag+=rToTag(curDiffR,bestDiffR);
            tag+=rToTag(curZipDiffR,bestZipDiffR);
            tag+=rToTag(curBz2DiffR,bestBz2DiffR);
            tag+=rToTag(curLzmaDiffR,bestLzmaDiffR);
            tag+="| "+rToTag(curCompressDiffR,bestCompressDiffR);
            if (!isOutSrcSize){
                isOutSrcSize=true;
                std::cout<<"null zlab bz2 lzma "<<"\t";
                std::cout<<"diff( "<<curDi.oldFileSize<<", ";
                std::cout<<curDi.newFileSize<<")\n";
            }
            std::cout<<tag<<"\t";
            std::cout<<curDi.asString()<<"\t"
                     <<rToStr(curDiffR)<<"\t"
                     <<rToStr(curZipDiffR)<<"\t"
                     <<rToStr(curBz2DiffR)<<"\t"
                     <<rToStr(curLzmaDiffR);
            
            std::cout<<"\n";
        }
    }}
}


int main(int argc, const char * argv[]){
    if ((argc<3)||((argc-1)%2!=0)) {
        throw argc;
    }
    std::vector<std::string> fileNames;
    for (int i=1; i<argc; ++i) {
        std::cout<<argv[i]<<"\n";
        fileNames.push_back(argv[i]);
    }

    getBestHDiffPrivateParams(fileNames);
    
    std::cout<<"\nok!\n";
    return 0;
}


