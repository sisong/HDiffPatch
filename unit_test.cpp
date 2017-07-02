//unit_test.cpp
// for unitTest
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
#include <string>
#include <vector>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "math.h"
#include "libHDiffPatch/HDiff/diff.h"
#include "libHDiffPatch/HPatch/patch.h"
typedef unsigned char   TByte;
typedef ptrdiff_t       TInt;
typedef size_t          TUInt;
//#define _AttackPacth_ON

static long _read_mem_stream(hpatch_TStreamInputHandle streamHandle,const hpatch_StreamPos_t readFromPos,
                             unsigned char* out_data,unsigned char* out_data_end){
    const TByte* data=(const TByte*)streamHandle;
    memcpy(out_data, data+readFromPos, out_data_end-out_data);
    return (long)(out_data_end-out_data);
}
static long _write_mem_stream(hpatch_TStreamInputHandle streamHandle,const hpatch_StreamPos_t writeToPos,
                              const unsigned char* data,const unsigned char* data_end){
    TByte* out_dst=(TByte*)streamHandle;
    memcpy(out_dst+writeToPos,data,data_end-data);
    return (long)(data_end-data);
}

static bool pacth_mem_stream(TByte* newData,TByte* newData_end,
                              const TByte* oldData,const TByte* oldData_end,const TByte* diff,const TByte* diff_end){
    struct hpatch_TStreamOutput out_newDataStream;
    out_newDataStream.streamHandle=newData;
    out_newDataStream.streamSize=newData_end-newData;
    out_newDataStream.write=_write_mem_stream;

    struct hpatch_TStreamInput  oldDataStream;
    oldDataStream.streamHandle=(void*)oldData;
    oldDataStream.streamSize=oldData_end-oldData;
    oldDataStream.read=_read_mem_stream;
    struct hpatch_TStreamInput  serializedDiffStream;
    serializedDiffStream.streamHandle=(void*)diff;
    serializedDiffStream.streamSize=diff_end-diff;
    serializedDiffStream.read=_read_mem_stream;

    return 0!=patch_stream(&out_newDataStream,&oldDataStream,&serializedDiffStream);
}

static bool check_diff_stream(const TByte* newData,const TByte* newData_end,
                       const TByte* oldData,const TByte* oldData_end,const TByte* diff,const TByte* diff_end){
    std::vector<TByte> testNewData(newData_end-newData);
    TByte* testNewData_begin=0;
    if (!testNewData.empty()) testNewData_begin=&testNewData[0];

    if (!pacth_mem_stream(testNewData_begin,testNewData_begin+testNewData.size(),oldData,oldData_end, diff,diff_end))
        return false;
    for (TUInt i=0; i<(TUInt)testNewData.size(); ++i) {
        if (testNewData[i]!=newData[i])
            return false;
    }
    return true;
}


#ifdef _AttackPacth_ON
long attackPacth(TByte* out_newData,TByte* out_newData_end,
                        const TByte* oldData,const TByte* oldData_end,
                        const TByte* diffData,const TByte* diffData_end,
                        const char* error_tag){
    bool rt0=patch(out_newData,out_newData_end,oldData,oldData_end,diffData,diffData_end);
    bool rt1=pacth_mem_stream(out_newData,out_newData_end,oldData,oldData_end,diffData,diffData_end);
    if (rt0!=rt1){
        printf("\n attackPacth error!!! tag:%s\n",error_tag);
        return 1;
    }else{
        //if (rt0) printf("\n attackPacth nullity!!! tag:%s\n",error_tag);
        return 0;
    }
}

long attackPacth(TInt newSize,const TByte* oldData,const TByte* oldData_end,
                 const TByte* _diffData,const TByte* _diffData_end,int seed){
    char tag[250]="\0";
    srand(seed);
    const long kLoopCount=1000;
    long exceptionCount=0;
    std::vector<TByte> _newData(newSize);
    TByte* newData=_newData.empty()?0:&_newData[0];
    TByte* newData_end=newData+_newData.size();
    const TInt diffSize=_diffData_end-_diffData;
    std::vector<TByte> new_diffData(diffSize);
    TByte* diffData=new_diffData.empty()?0:&new_diffData[0];
    TByte* diffData_end=diffData+diffSize;
    try {
        for (long i=0; i<kLoopCount; ++i) {
            sprintf(tag, "attackPacth exceptionCount=%ld testSeed=%d i=%ld",exceptionCount,seed,i);
            memcpy(diffData,_diffData,_diffData_end-_diffData);
            const int randCount=1+rand()*(1.0/RAND_MAX)*rand()*(1.0/RAND_MAX)*diffSize/3;
            for (int r=0; r<randCount; ++r){
                diffData[rand()%diffSize]=rand();
            }
            exceptionCount+=attackPacth(newData,newData_end,oldData,oldData_end,diffData,diffData_end,tag);
        }
        return exceptionCount;
    } catch (...) {
        printf("exception!!! tag:%s\n",tag);
        return exceptionCount+1;
    }
}
#endif

long test(const TByte* newData,const TByte* newData_end,
                 const TByte* oldData,const TByte* oldData_end,const char* tag,size_t* out_diffSize=0){
    std::vector<TByte> diffData;
    printf("%s newSize:%ld oldSize:%ld ",tag, (long)(newData_end-newData), (long)(oldData_end-oldData));
    create_diff(newData,newData_end,oldData,oldData_end, diffData);
    if (out_diffSize!=0)
        *out_diffSize=diffData.size();
    if ((!check_diff(newData,newData_end,oldData,oldData_end, &diffData[0], &diffData[0]+diffData.size()))
        ||(!check_diff_stream(newData,newData_end,oldData,oldData_end, &diffData[0], &diffData[0]+diffData.size())) ){
        printf("\n  error!!! tag:%s\n",tag);
        return 1;
    }else{
        printf(" ok! diffSize:%ld\n", (long)(diffData.size()));
#ifdef _AttackPacth_ON
        long exceptionCount=attackPacth(newData_end-newData,oldData,oldData_end,
                                        &diffData[0], &diffData[0]+diffData.size(),rand());
        if (exceptionCount>0) return exceptionCount;
#endif
        return 0;
    }
}

static inline long test(const char* newStr,const char* oldStr,const char* error_tag){
    const TByte* newData=(const TByte*)newStr;
    const TByte* oldData=(const TByte*)oldStr;
    return test(newData,newData+strlen(newStr),oldData,oldData+strlen(oldStr),error_tag);
}


void setRandDataSize(int kMaxDataSize,std::vector<TByte>& oldData,std::vector<TByte>& newData,
                            double sunSizeMin,double subSizeMax){
    const TInt oldSize=(TInt)(rand()*(1.0/RAND_MAX)*rand()*(1.0/RAND_MAX)*kMaxDataSize);
    const TInt newSize=(TInt)(oldSize*(sunSizeMin+rand()*(1.0/RAND_MAX)*(subSizeMax-sunSizeMin)));
    newData.resize(newSize);
    oldData.resize(oldSize);
}

void setRandData(std::vector<TByte>& data){
    for (TInt i=0; i<(TInt)data.size(); ++i)
        data[i]=rand();
}


int main(int argc, const char * argv[]){
    clock_t time1=clock();
    long errorCount=0;
    errorCount+=test("", "", "1");
    errorCount+=test("", "1", "2");
    errorCount+=test("1", "", "3");
    errorCount+=test("1", "1", "4");
    errorCount+=test("22", "11", "5");
    errorCount+=test("22", "22", "6");
    errorCount+=test("1234567890", "1234567890", "7");
    errorCount+=test("123456789876543212345677654321234567765432", "asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr", "8");
    errorCount+=test("123456789876543212345677654321234567765432asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr", "asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr", "9");
    errorCount+=test("asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr123456789876543212345677654321234567765432", "asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr", "10");
    errorCount+=test("a123456789876543212345677654321234567765432asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr", "asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr", "11");
    errorCount+=test("123456789876543212345677654321234567765432asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr1", "asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr", "12");
    errorCount+=test("a123456789876543212345677654321234567765432asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr1", "asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr", "13");

    {
        const char* _strData14="a123456789876543212345677654321234567765432asadsdasfefw45fg4gacasc234fervsvdfdsfef4g4gr1";
        const TByte* data14=(const TByte*)_strData14;
        const size_t dataSize=strlen(_strData14);
        size_t diffSize14=0;
        errorCount+=test(data14, data14+dataSize,data14, data14+dataSize, "14",&diffSize14);
        if (diffSize14>=dataSize){
            ++errorCount;
            printf("error!!! tag:%s\n","14 test diff size");
        }
    }

    const long kRandTestCount=100000;
    const long kMaxDataSize=1024*16;
    std::vector<int> seeds(kRandTestCount);
    srand(0);
    for (int i=0; i<kRandTestCount; ++i)
        seeds[i]=rand();

    double sumNewSize=0;
    double sumOldSize=0;
    double sumDiffSize=0;
    std::vector<TByte> _newData;
    std::vector<TByte> _oldData;
    for (TInt i=0; i<kRandTestCount; ++i) {
        char tag[250];
        sprintf(tag, "error==%ld testSeed=%d",errorCount,seeds[i]);
        srand(seeds[i]);

        setRandDataSize(kMaxDataSize,_oldData,_newData,0.7,1.5);
        setRandData(_oldData);
        setRandData(_newData);
        const TInt oldSize=(TInt)_oldData.size();
        const TInt newSize=(TInt)_newData.size();
        const TInt kMaxCopyCount=(TInt)sqrt((double)oldSize);
        TByte* newData=_newData.empty()?0:&_newData[0];
        TByte* oldData=_oldData.empty()?0:&_oldData[0];
        const TInt copyCount=0+(TInt)((1-rand()*(1.0/RAND_MAX)*rand()*(1.0/RAND_MAX))*kMaxCopyCount*4);
        const TInt kMaxCopyLength=(TInt)(1+rand()*(1.0/RAND_MAX)*kMaxCopyCount*4);
        for (TInt i=0; i<copyCount; ++i) {
            const TInt length=1+(TInt)(rand()*(1.0/RAND_MAX)*kMaxCopyLength);
            if ((length>oldSize*4/5)||(length>newSize*4/5)) {
                continue;
            }
            const TInt oldPos=(oldSize-length==0)?0:(TInt)(rand()*(1.0/RAND_MAX)*(oldSize-length));
            const TInt newPos=(newSize-length==0)?0:(TInt)(rand()*(1.0/RAND_MAX)*(newSize-length));
            memcpy(&newData[0]+newPos, &oldData[0]+oldPos, length);
        }
        size_t diffSize=0;
        errorCount+=test(&newData[0],&newData[0]+newSize,&oldData[0],&oldData[0]+oldSize,tag,&diffSize);
        sumNewSize+=newSize;
        sumOldSize+=oldSize;
        sumDiffSize+=diffSize;
    }

    printf("\nchecked:%ld  errorCount:%ld\n",kRandTestCount,errorCount);
    printf("newSize:100%% oldSize:%2.2f%% diffSize:%2.2f%%\n",sumOldSize*100.0/sumNewSize,sumDiffSize*100.0/sumNewSize);
    clock_t time2=clock();
    printf("\nrun time:%.0f ms\n",(time2-time1)*(1000.0/CLOCKS_PER_SEC));

    return (int)errorCount;
}
