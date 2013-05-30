//
//  unit_test.cpp
//  unitTest
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
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "libHDiffPatch/HDiff/diff.h"
#include "libHDiffPatch/HPatch/patch.h"
using namespace hdiffpatch;

static bool test(const TByte* newData,const TByte* newData_end,const TByte* oldData,const TByte* oldData_end,const char* tag){
    std::vector<TByte> diffData;
    create_diff(newData,newData_end,oldData,oldData_end, diffData);
    if (!check_diff(newData,newData_end,oldData,oldData_end, &diffData[0], &diffData[0]+diffData.size())){
        printf("error!!! tag:%s\n",tag);
        return false;
    }else{
        printf("ok! %s newSize:%d oldSize%d diffSize:%d\n",tag,(int)(newData_end-newData),(int)(oldData_end-oldData),(int)diffData.size());
        return true;
    }
}

static bool test(const char* newStr,const char* oldStr,const char* error_tag){
    const TByte* newData=(const TByte*)newStr;
    const TByte* oldData=(const TByte*)oldStr;
    return test(newData,newData+strlen(newStr),oldData,oldData+strlen(oldStr),error_tag);
}

int main(int argc, const char * argv[]){
    int errorCount=0;
    errorCount+=!test("", "", "1");
    errorCount+=!test("", "1", "2");
    errorCount+=!test("1", "", "3");
    errorCount+=!test("1", "1", "4");
    errorCount+=!test("22", "11", "5");
    errorCount+=!test("22", "22", "6");
    errorCount+=!test("1234567890", "1234567890", "7");
    errorCount+=!test("123456789876543212345677654321234567765432", "asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr", "8");
    errorCount+=!test("123456789876543212345677654321234567765432asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr", "asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr", "9");
    errorCount+=!test("asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr123456789876543212345677654321234567765432", "asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr", "10");
    errorCount+=!test("a123456789876543212345677654321234567765432asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr", "asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr", "11");
    errorCount+=!test("123456789876543212345677654321234567765432asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr1", "asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr", "12");
    errorCount+=!test("a123456789876543212345677654321234567765432asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr1", "asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr", "13");

    const int kRandTestCount=100000;
    const int kMaxDataSize=1024*8;
    const int kMaxCopyCount=33;
    const int kMaxCopyLength=512+256;
    std::vector<int> seeds(kRandTestCount);
    //srand( (unsigned int)time(0) );
    for (int i=0; i<kRandTestCount; ++i)
        seeds[i]=rand();

    for (int i=0; i<kRandTestCount; ++i) {
        char tag[50];
        sprintf(tag, "error==%d testSeed=%d",errorCount,seeds[i]);
        srand(seeds[i]);

        const int oldSize=(int)(rand()*(1.0/RAND_MAX)*rand()*(1.0/RAND_MAX)*kMaxDataSize);
        const int newSize=oldSize+(int)((rand()*(1.0/RAND_MAX)-0.5)*oldSize);
        std::vector<TByte> _newData(newSize);
        TByte* newData=0; if (!_newData.empty()) newData=&_newData[0];
        std::vector<TByte> _oldData(oldSize);
        TByte* oldData=0; if (!_oldData.empty()) oldData=&_oldData[0];
        for (int i=0; i<newSize; ++i)
            newData[i]=rand();
        for (int i=0; i<oldSize; ++i)
            oldData[i]=rand();
        const int copyCount=(int)(rand()*(1.0/RAND_MAX)*rand()*(1.0/RAND_MAX)*kMaxCopyCount);
        for (int i=0; i<copyCount; ++i) {
            const int length=1+(int)(rand()*(1.0/RAND_MAX)*rand()*(1.0/RAND_MAX)*kMaxCopyLength);
            if ((length>oldSize)||(length>newSize)) {
                continue;
            }
            const int oldPos=(oldSize-length==0)?0:rand()%(oldSize-length);
            const int newPos=(newSize-length==0)?0:rand()%(newSize-length);
            memcpy(&newData[0]+newPos, &oldData[0]+oldPos, length);
        }
        errorCount+=!test(&newData[0],&newData[0]+newSize,&oldData[0],&oldData[0]+oldSize,tag);
    }

    printf("\ncheck errorCount:%d\n",errorCount);
    return errorCount;
}

