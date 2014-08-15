//
//  diff_demo.cpp
//  HDiff
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
#include <time.h>
#include <vector>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "libHDiffPatch/HDiff/diff.h"
#include "libHDiffPatch/HPatch/patch.h"
typedef unsigned char  TByte;
typedef unsigned int TUInt32;
typedef ptrdiff_t TInt;

void readFile(std::vector<TByte>& data,const char* fileName){
    FILE	* file=fopen(fileName, "rb");
    if (file==0) exit(1);
    
	fseek(file,0,SEEK_END);
	TInt file_length = (TInt)ftell(file);
	fseek(file,0,SEEK_SET);
    
    data.resize(file_length);
    TInt readed=0;
    if (file_length>0)
        readed=(TInt)fread(&data[0],1,file_length,file);
    
    fclose(file);
    if (readed!=file_length) exit(1);
}

void writeFile(const std::vector<TByte>& data,const char* fileName){
    FILE	* file=fopen(fileName, "wb");
    if (file==0) exit(1);
    
    TInt dataSize=(TInt)data.size();
    TInt writed=0;
    if (dataSize>0)
        writed=(TInt)fwrite(&data[0], 1,dataSize, file);
    
    fclose(file);
    if (writed!=dataSize) exit(1);
}


int main(int argc, const char * argv[]){
    if (argc!=4) {
        printf("diff command line parameter:\n oldFileName newFileName outDiffFileName\n");
        return 0;
    }
    const char* oldFileName=argv[1];
    const char* newFileName=argv[2];
    const char* outDiffFileName=argv[3];
    printf("old:\"%s\"  new:\"%s\" out:\"%s\"\n",oldFileName,newFileName,outDiffFileName);

    std::vector<TByte> oldData; readFile(oldData,oldFileName);
    std::vector<TByte> newData; readFile(newData,newFileName);
    const TUInt32 kMaxDataSize= (1<<30)-1 + (1<<30);//2G
    if ((oldData.size()>kMaxDataSize)||(newData.size()>kMaxDataSize)) {
        printf("not support:old filesize or new filesize too big than 2GB.\n");
        return 1;
    }
    const TUInt32 oldDataSize=(TUInt32)oldData.size();
    const TUInt32 newDataSize=(TUInt32)newData.size();
    clock_t time1=clock();

    std::vector<TByte> diffData;
    diffData.push_back(newDataSize);
    diffData.push_back(newDataSize>>8);
    diffData.push_back(newDataSize>>16);
    diffData.push_back(newDataSize>>24);
    const TUInt32 kNewDataSize=4;


    TByte* newData_begin=0; if (!newData.empty()) newData_begin=&newData[0];
    const TByte* oldData_begin=0; if (!oldData.empty()) oldData_begin=&oldData[0];
    create_diff(newData_begin,newData_begin+newDataSize,oldData_begin, oldData_begin+oldDataSize, diffData);
    if (!check_diff(newData_begin,newData_begin+newDataSize,oldData_begin,oldData_begin+oldDataSize, &diffData[0]+kNewDataSize, &diffData[0]+diffData.size())){
        printf("check diff data error!!!");
        return 1;
    }else{
        printf("check diff data ok!\n");
    }
    writeFile(diffData,outDiffFileName);
    printf("out diff file ok!\nnewDataSize: %d\noldDataSize: %d\ndiffDataSize: %d\n",newDataSize,oldDataSize,(TUInt32)diffData.size());
    clock_t time2=clock();
    printf("\nrun time:%.0f ms\n",(time2-time1)*(1000.0/CLOCKS_PER_SEC));

    return 0;
}

