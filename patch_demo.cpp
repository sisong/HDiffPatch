//
//  patch_demo.cpp
//  HPatch
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

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "assert.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "libHDiffPatch/HPatch/patch.h"
typedef unsigned char TByte;
typedef size_t      TUInt;
typedef ptrdiff_t   TInt;

void readFile(std::vector<TByte>& data,const char* fileName){
    std::ifstream file(fileName);
    file.seekg(0,std::ios::end);
    std::streampos file_length=file.tellg();
    assert(file_length>=0);
    file.seekg(0,std::ios::beg);
    size_t needRead=(size_t)file_length;
    if (needRead!=file_length) {
        file.close();
        exit(1);
    }
    data.resize(needRead);
    file.read((char*)&data[0], needRead);
    std::streamsize readed=file.gcount();
    file.close();
    if (readed!=(std::streamsize)file_length)  exit(1);
}

void writeFile(const std::vector<TByte>& data,const char* fileName){
    std::ofstream file(fileName);
    file.write((const char*)&data[0], data.size());
    file.close();
}

int main(int argc, const char * argv[]){
    clock_t time0=clock();
    if (argc!=4) {
        std::cout<<"patch command parameter:\n oldFileName diffFileName outNewFileName\n";
        return 0;
    }
    const char* oldFileName=argv[1];
    const char* diffFileName=argv[2];
    const char* outNewFileName=argv[3];
    std::cout<<"old :\"" <<oldFileName<< "\"\ndiff:\""<<diffFileName<<"\"\nout :\""<<outNewFileName<<"\"\n";

    std::vector<TByte> diffData; readFile(diffData,diffFileName);
    const TUInt diffFileDataSize=(TUInt)diffData.size();
    if (diffFileDataSize<4){
        std::cout<<"diffFileDataSize error!\n";
        exit(2);
    }
    TUInt kNewDataSizeSavedSize=-1;
    TUInt newDataSize=diffData[0] | (diffData[1]<<8)| (diffData[2]<<16);
    if (diffData[3]!=0xFF){
        kNewDataSizeSavedSize=4;
        newDataSize |=(diffData[3]<<24);
    }else{
        kNewDataSizeSavedSize=9;
        if ((sizeof(TUInt)<=4)||(diffFileDataSize<9)){
            std::cout<<"diffFileDataSize error!\n";
            exit(2);
        }
        newDataSize |=(diffData[4]<<24);
        const TUInt highSize=diffData[5] | (diffData[6]<<8)| (diffData[7]<<16)| (diffData[8]<<24);
        newDataSize |=((highSize<<16)<<16);
    }
    
    std::vector<TByte> oldData; readFile(oldData,oldFileName);
    const TUInt oldDataSize=(TUInt)oldData.size();

    std::vector<TByte> newData;
    newData.resize(newDataSize);
    TByte* newData_begin=0; if (!newData.empty()) newData_begin=&newData[0];
    const TByte* oldData_begin=0; if (!oldData.empty()) oldData_begin=&oldData[0];
    clock_t time1=clock();
    if (!patch(newData_begin,newData_begin+newDataSize,oldData_begin,oldData_begin+oldDataSize,
               &diffData[0]+kNewDataSizeSavedSize, &diffData[0]+diffFileDataSize)){
        printf("  patch run error!!!\n");
        exit(3);
    }
    clock_t time2=clock();
    writeFile(newData,outNewFileName);
    clock_t time3=clock();
    std::cout<<"  patch ok!\n";
    std::cout<<"oldDataSize : "<<oldDataSize<<"\ndiffDataSize: "<<diffData.size()<<"\nnewDataSize : "<<newDataSize<<"\n";
    std::cout<<"\npatch   time:"<<(time2-time1)*(1000.0/CLOCKS_PER_SEC)<<" ms\n";
    std::cout<<"all run time:"<<(time3-time0)*(1000.0/CLOCKS_PER_SEC)<<" ms\n";
    return 0;
}

