//patch_demo.cpp
// demo for HPatch
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

#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include "assert.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "libHDiffPatch/HPatch/patch.h"
typedef unsigned char   TByte;
typedef size_t          TUInt;
typedef ptrdiff_t       TInt;

#define IS_USES_PATCH_STREAM

inline static hpatch_StreamPos_t getFilePos64(FILE* file){
    fpos_t pos;
    int rt=fgetpos(file, &pos); //support 64bit?
    assert(rt==0);
#if defined(__linux) || defined(__linux__)
    return pos.__pos;
#else //windows macosx
    return pos;
#endif
}

inline void setFilePos64(FILE* file,hpatch_StreamPos_t seekPos){
    fpos_t pos;
#if defined(__linux) || defined(__linux__)
    memset(&pos, 0, sizeof(pos));
    pos.__pos=seekPos; //safe?
#else //windows macosx
    pos=seekPos;
#endif
    int rt=fsetpos(file,&pos); //support 64bit?
    assert(rt==0);
}

static hpatch_StreamPos_t readSavedSize(const std::vector<TByte>& diffData,TUInt* out_sizeCodeLength){
    const TUInt diffFileDataSize=(TUInt)diffData.size();
    if (diffFileDataSize<4){
        std::cout<<"diffFileDataSize error!\n";
        exit(2);
    }
    TUInt sizeCodeLength=-1;
    hpatch_StreamPos_t newDataSize=diffData[0] | (diffData[1]<<8)| (diffData[2]<<16);
    if (diffData[3]!=0xFF){
        sizeCodeLength=4;
        newDataSize |=(diffData[3]<<24);
    }else{
        sizeCodeLength=9;
        if ((sizeof(TUInt)<=4)||(diffFileDataSize<9)){
            std::cout<<"diffFileDataSize error!\n";
            exit(2);
        }
        newDataSize |=(diffData[4]<<24);
        const hpatch_StreamPos_t highSize=diffData[5] | (diffData[6]<<8)| (diffData[7]<<16)| (diffData[8]<<24);
        newDataSize |=((highSize<<16)<<16);
    }

    *out_sizeCodeLength=sizeCodeLength;
    return newDataSize;
}


#ifndef IS_USES_PATCH_STREAM

void readFile(std::vector<TByte>& data,const char* fileName){
    FILE* file=fopen(fileName,"rb");
    if (file==0) exit(1);
    fseek(file,0,SEEK_END);
    hpatch_StreamPos_t file_length=getFilePos64(file);
    fseek(file,0,SEEK_SET);
    size_t needRead=(size_t)file_length;
    if (needRead!=file_length){
        fclose(file);
        file=0;
        exit(1);
    }
    data.resize(needRead);

    TByte* curData=0; if (!data.empty()) curData=&data[0];
    size_t dataSize=data.size();
    while (dataSize>0) {
        size_t readStep=(1<<20);
        if (readStep>dataSize) readStep=dataSize;
        size_t readed=fread(curData, 1,readStep, file);
        assert(readed==readStep);
        curData+=readStep;
        dataSize-=readStep;
    }
    fclose(file);
}

void writeFile(const std::vector<TByte>& data,const char* fileName){
    FILE* file=fopen(fileName,"wb+");
    if (file==0) exit(1);
    const TByte* curData=0; if (!data.empty()) curData=&data[0];
    size_t dataSize=data.size();
    while (dataSize>0) {
        size_t writeStep=(1<<20);
        if (writeStep>dataSize) writeStep=dataSize;
        size_t writed=fwrite(curData, 1,writeStep, file);
        assert(writed==writeStep);
        curData+=writeStep;
        dataSize-=writeStep;
    }
    fclose(file);
}

int main(int argc, const char * argv[]){
    if (argc!=4) {
        std::cout<<"patch command parameter:\n oldFileName diffFileName outNewFileName\n";
        return 0;
    }
    const char* oldFileName=argv[1];
    const char* diffFileName=argv[2];
    const char* outNewFileName=argv[3];
    std::cout<<"old :\"" <<oldFileName<< "\"\ndiff:\""<<diffFileName<<"\"\nout :\""<<outNewFileName<<"\"\n";
    clock_t time0=clock();
    {
        std::vector<TByte> diffData; readFile(diffData,diffFileName);
        const TUInt diffFileDataSize=(TUInt)diffData.size();
        TUInt kNewDataSizeSavedSize=-1;
        const hpatch_StreamPos_t _newFileDataSize=readSavedSize(diffData,&kNewDataSizeSavedSize);
        const TUInt newDataSize=(TUInt)_newFileDataSize;
        if (newDataSize!=_newFileDataSize) exit(1);

        std::vector<TByte> oldData; readFile(oldData,oldFileName);
        const TUInt oldDataSize=(TUInt)oldData.size();

        std::vector<TByte> newData;
        newData.resize(newDataSize);
        TByte* newData_begin=0; if (!newData.empty()) newData_begin=&newData[0];
        const TByte* oldData_begin=0; if (!oldData.empty()) oldData_begin=&oldData[0];
        clock_t time1=clock();
        if (!patch(newData_begin,newData_begin+newDataSize,oldData_begin,oldData_begin+oldDataSize,
                   &diffData[0]+kNewDataSizeSavedSize, &diffData[0]+diffFileDataSize)){
            std::cout<<"  patch run error!!!\n";
            exit(3);
        }
        clock_t time2=clock();
        writeFile(newData,outNewFileName);
        std::cout<<"  patch ok!\n";
        std::cout<<"oldDataSize : "<<oldDataSize<<"\ndiffDataSize: "<<diffData.size()<<"\nnewDataSize : "<<newDataSize<<"\n";
        std::cout<<"\npatch   time:"<<(time2-time1)*(1000.0/CLOCKS_PER_SEC)<<" ms\n";
    }
    clock_t time3=clock();
    std::cout<<"all run time:"<<(time3-time0)*(1000.0/CLOCKS_PER_SEC)<<" ms\n";
    return 0;
}

#else
//IS_USES_PATCH_STREAM

struct TFileStreamInput:public hpatch_TStreamInput{
    explicit TFileStreamInput(const char* fileName):m_file(0),m_offset(0),m_fpos(0){
        m_file=fopen(fileName, "rb");
        if (m_file==0) exit(1);
        //setvbuf(m_file, 0, _IOFBF, 1024*2);
        //setvbuf(m_file, 0, _IONBF, 0);
        fseek(m_file, 0, SEEK_END);
        hpatch_StreamPos_t file_length=getFilePos64(m_file);
        fseek(m_file, 0, SEEK_SET);
        hpatch_TStreamInput::streamHandle=this;
        hpatch_TStreamInput::streamSize=file_length;
        hpatch_TStreamInput::read=read_file;
    }
    static long read_file(hpatch_TStreamInputHandle streamHandle,const hpatch_StreamPos_t readFromPos,
                          unsigned char* out_data,unsigned char* out_data_end){
        TFileStreamInput& fileStreamInput=*(TFileStreamInput*)streamHandle;
        hpatch_StreamPos_t curPos=fileStreamInput.m_offset+readFromPos;
        if (fileStreamInput.m_fpos!=curPos){
            setFilePos64(fileStreamInput.m_file, curPos);
        }
        size_t readed=fread(out_data, 1, (size_t)(out_data_end-out_data), fileStreamInput.m_file);
        assert(readed==(TUInt)(out_data_end-out_data));
        fileStreamInput.m_fpos=curPos+readed;
        return (long)readed;
    }
    
    void setHeadSize(TUInt headSize){
        assert(m_offset==0);
        m_offset=headSize;
        hpatch_TStreamInput::streamSize-=headSize;
    }
    FILE*               m_file;
    TUInt               m_offset;
    hpatch_StreamPos_t  m_fpos;
    ~TFileStreamInput(){
        if (m_file!=0) fclose(m_file);
    }
};

static hpatch_StreamPos_t readNewDataSize(TFileStreamInput& diffFileData){
    TUInt readLength=9;
    if (readLength>diffFileData.streamSize)
        readLength=(TUInt)diffFileData.streamSize;

    std::vector<TByte> diffData(readLength);
    TByte* diffData_begin=0; if (!diffData.empty()) diffData_begin=&diffData[0];
    assert(diffFileData.m_offset==0);
    diffFileData.read(diffFileData.streamHandle,0,diffData_begin,diffData_begin+readLength);

    TUInt kNewDataSizeSavedSize=-1;
    const hpatch_StreamPos_t newDataSize=readSavedSize(diffData,&kNewDataSizeSavedSize);
    diffFileData.setHeadSize(kNewDataSizeSavedSize);//4 or 9 byte
    return newDataSize;
}

struct TFileStreamOutput:public hpatch_TStreamOutput{
    TFileStreamOutput(const char* fileName,hpatch_StreamPos_t file_length):m_file(0){
        m_file=fopen(fileName, "wb+");
        if (m_file==0) exit(1);
        hpatch_TStreamOutput::streamHandle=m_file;
        hpatch_TStreamOutput::streamSize=file_length;
        hpatch_TStreamOutput::write=write_file;
    }
    static long write_file(hpatch_TStreamInputHandle streamHandle,const hpatch_StreamPos_t writeToPos,
                           const unsigned char* data,const unsigned char* data_end){
        FILE* m_file=(FILE*)streamHandle;
        size_t writed=fwrite(data,1,(TUInt)(data_end-data),m_file);
        return (long)writed;
    }
    FILE*   m_file;
    ~TFileStreamOutput(){
        if (m_file!=0) fclose(m_file);
    }
};

int main(int argc, const char * argv[]){
    if (argc!=4) {
        std::cout<<"patch command parameter:\n oldFileName diffFileName outNewFileName\n";
        return 0;
    }
    const char* oldFileName=argv[1];
    const char* diffFileName=argv[2];
    const char* outNewFileName=argv[3];
    std::cout<<"old :\"" <<oldFileName<< "\"\ndiff:\""<<diffFileName<<"\"\nout :\""<<outNewFileName<<"\"\n";
    clock_t time0=clock();
    {
        TFileStreamInput oldData(oldFileName);
        TFileStreamInput diffData(diffFileName);
        const hpatch_StreamPos_t newDataSize=readNewDataSize(diffData);
        TFileStreamOutput newData(outNewFileName,newDataSize);

        clock_t time1=clock();
        if (!patch_stream(&newData, &oldData, &diffData)){
            std::cout<<"  patch_stream run error!!!\n";
            exit(3);
        }
        clock_t time2=clock();
        std::cout<<"  patch_stream ok!\n";
        std::cout<<"oldDataSize : "<<oldData.streamSize<<"\ndiffDataSize: "<<diffData.streamSize<<"\nnewDataSize : "<<newDataSize<<"\n";
        std::cout<<"\npatch   time:"<<(time2-time1)*(1000.0/CLOCKS_PER_SEC)<<" ms\n";
    }
    clock_t time3=clock();
    std::cout<<"all run time:"<<(time3-time0)*(1000.0/CLOCKS_PER_SEC)<<" ms\n";
    return 0;
}

#endif

