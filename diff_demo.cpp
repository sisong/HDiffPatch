//diff_demo.cpp
// demo for HDiff
//NOTE: use HDiffZ+HPatchZ support compressed diff data
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
#include <string.h>
#include <stdlib.h>
#include "libHDiffPatch/HDiff/diff.h"
#include "libHDiffPatch/HPatch/patch.h"
#include "_clock_for_demo.h"
typedef unsigned char   TByte;
typedef size_t          TUInt;

#define _IS_RUN_PATCH_CHECK

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


int main(int argc, const char * argv[]){
    double time0=clock_s();
    if (argc!=4) {
        std::cout<<"diff command line parameter:\n oldFileName newFileName outDiffFileName\n";
        exit(1);
    }
    const char* oldFileName=argv[1];
    const char* newFileName=argv[2];
    const char* outDiffFileName=argv[3];
    std::cout<<"old:\"" <<oldFileName<< "\"\nnew:\""<<newFileName<<"\"\nout:\""<<outDiffFileName<<"\"\n";

    std::vector<TByte> oldData; readFile(oldData,oldFileName);
    std::vector<TByte> newData; readFile(newData,newFileName);
    const TUInt oldDataSize=oldData.size();
    const TUInt newDataSize=newData.size();

    std::vector<TByte> diffData;
    diffData.push_back((TByte)newDataSize);
    diffData.push_back((TByte)(newDataSize>>8));
    diffData.push_back((TByte)(newDataSize>>16));
    TUInt kNewDataSize=-1;
    if ((newDataSize>>31)==0){
        kNewDataSize=4;
        diffData.push_back((TByte)(newDataSize>>24));
    }else{
        kNewDataSize=9;
        diffData.push_back(0xFF);
        diffData.push_back((TByte)(newDataSize>>24));

        const TUInt highSize=((newDataSize>>16)>>16);
        diffData.push_back((TByte)highSize);
        diffData.push_back((TByte)(highSize>>8));
        diffData.push_back((TByte)(highSize>>16));
        diffData.push_back((TByte)(highSize>>24));
    }

    std::cout<<"oldDataSize : "<<oldDataSize<<"\nnewDataSize : "<<newDataSize<<"\n";
    TByte* newData0=newData.data();
    const TByte* oldData0=oldData.data();
    double time1=clock_s();
    create_diff(newData0,newData0+newDataSize,
                oldData0,oldData0+oldDataSize,diffData);
    double time2=clock_s();
    std::cout<<"diffDataSize: "<<diffData.size()<<"\n";
    writeFile(diffData,outDiffFileName);
    std::cout<<"  out diff file ok!\n";
#ifdef _IS_RUN_PATCH_CHECK
    if (!check_diff(newData0,newData0+newDataSize,
                    oldData0,oldData0+oldDataSize,
                    diffData.data()+kNewDataSize, diffData.data()+diffData.size())){
        std::cout<<"  patch check diff data error!!!\n";
        exit(1);
    }else{
        std::cout<<"  patch check diff data ok!\n";
    }
#endif
    double time3=clock_s();
    std::cout<<"\ndiff    time:"<<(time2-time1)<<" s\n";
    std::cout<<"all run time:"<<(time3-time0)<<" s\n";

    return 0;
}

