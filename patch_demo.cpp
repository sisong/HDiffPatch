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
#include <string>
#include <vector>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "libHDiffPatch/HPatch/patch.h"
typedef unsigned char TByte;

void readFile(std::vector<TByte>& data,const char* fileName){
    FILE	* file=fopen(fileName, "rb");

	fseek(file,0,SEEK_END);
	int file_length = (int)ftell(file);
	fseek(file,0,SEEK_SET);

    data.resize(file_length);
    if (file_length>0)
        fread(&data[0],1,file_length,file);

    fclose(file);
}

void writeFile(const std::vector<TByte>& data,const char* fileName){
    FILE	* file=fopen(fileName, "wb");

    int dataSize=(int)data.size();
    if (dataSize>0)
        fwrite(&data[0], 1,dataSize, file);

    fclose(file);
}

int main(int argc, const char * argv[]){
    if (argc!=4) {
        printf("patch command parameter:\n oldFileName diffFileName outNewFileName\n");
        return 0;
    }
    const char* oldFileName=argv[1];
    const char* diffFileName=argv[2];
    const char* outNewFileName=argv[3];

    std::vector<TByte> oldData; readFile(oldData,oldFileName);
    std::vector<TByte> diffData; readFile(diffData,diffFileName);
    const int oldDataSize=(int)oldData.size();
    const int diffDataSize=(int)diffData.size();
    const int kNewDataSizeSavedSize=4;
    if (diffDataSize<kNewDataSizeSavedSize){
        printf("diffDataSize error!");
        return 1;
    }

    std::vector<TByte> newData;
    const int newDataSize=diffData[0] | (diffData[1]<<8)| (diffData[2]<<16)| (diffData[3]<<24);
    newData.resize(newDataSize);
    TByte* newData_begin=0; if (!newData.empty()) newData_begin=&newData[0];
    const TByte* oldData_begin=0; if (!oldData.empty()) oldData_begin=&oldData[0];
    if (!patch(newData_begin,newData_begin+newDataSize,oldData_begin,oldData_begin+oldDataSize,
               &diffData[0]+kNewDataSizeSavedSize, &diffData[0]+diffDataSize)){
        printf("patch run error!");
        return 2;
    }
    writeFile(newData,outNewFileName);
    printf("patch ok!");
    return 0;
}

