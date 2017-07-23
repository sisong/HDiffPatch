//HPatchZ.cpp
// patch with decompress plugin
//
/*
 This is the HDiffPatch copyright.

 Copyright (c) 2012-2017 HouSisong All Rights Reserved.

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

struct TFileStreamInput:public hpatch_TStreamInput{
    explicit TFileStreamInput(const char* fileName):m_file(0),m_fpos(0){
        m_file=fopen(fileName, "rb");
        if (m_file==0) exit(1);
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
        hpatch_StreamPos_t curPos=readFromPos;
        if (fileStreamInput.m_fpos!=curPos){
            setFilePos64(fileStreamInput.m_file, curPos);
        }
        size_t readed=fread(out_data, 1, (size_t)(out_data_end-out_data), fileStreamInput.m_file);
        assert(readed==(TUInt)(out_data_end-out_data));
        fileStreamInput.m_fpos=curPos+readed;
        return (long)readed;
    }
    FILE*               m_file;
    hpatch_StreamPos_t  m_fpos;
    ~TFileStreamInput(){
        if (m_file!=0) fclose(m_file);
    }
};

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

//===== select decompress plugin =====
#define _CompressPlugin_zlib
#define _CompressPlugin_bz2
//#define _CompressPlugin_lzma

#include "decompress_plugin_demo.h"

//diffFile need create by HDiffZ
int main(int argc, const char * argv[]){
    if (argc!=4) {
        std::cout<<"HPatchZ command parameter:\n oldFileName diffFileName outNewFileName\n";
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
        hpatch_compressedDiffInfo diffInfo;
        if (!compressedDiffInfo(&diffInfo,&diffData)){
            std::cout<<"  compressedDiffInfo() run error!!!\n";
            exit(2);
        }
        if (diffInfo.oldDataSize!=oldData.streamSize){
            std::cout<<"  savedOldDataSize != oldFileSize error!!!\n";
            exit(2);
        }
        
        hpatch_TDecompress* decompressPlugin=0;
        if (diffInfo.compressedCount>0){
#ifdef  _CompressPlugin_zlib
            if (zlibDecompressPlugin.is_can_open(&zlibDecompressPlugin,diffInfo.compressType))
                decompressPlugin=&zlibDecompressPlugin;
#endif
#ifdef  _CompressPlugin_bz2
            if (bz2DecompressPlugin.is_can_open(&bz2DecompressPlugin,diffInfo.compressType))
                decompressPlugin=&bz2DecompressPlugin;
#endif
#ifdef  _CompressPlugin_lzma
            if (lzmaDecompressPlugin.is_can_open(&lzmaDecompressPlugin,diffInfo.compressType))
                decompressPlugin=&lzmaDecompressPlugin;
#endif
            if (!decompressPlugin){
                std::cout<<"  error!!! can no decompress \""<<diffInfo.compressType<<"\" type code\n";
                exit(2);
            }
            std::cout<<"HPatchZ with decompress \""<<diffInfo.compressType<<"\" type code\n";
        }else{
            decompressPlugin=hpatch_kNodecompressPlugin;
            std::cout<<"HPatchZ not need decompress plugin\n";
        }

        TFileStreamOutput newData(outNewFileName,diffInfo.newDataSize);

        clock_t time1=clock();
        if (!patch_decompress(&newData, &oldData, &diffData,decompressPlugin)){
            std::cout<<"  patch_decompress() run error!!!\n";
            exit(3);
        }
        clock_t time2=clock();
        std::cout<<"  patch_decompress() ok!\n";
        std::cout<<"oldDataSize : "<<oldData.streamSize<<"\ndiffDataSize: "<<diffData.streamSize
                 <<"\nnewDataSize : "<<diffInfo.newDataSize<<"\n";
        std::cout<<"\nHPatchZ time:"<<(time2-time1)*(1000.0/CLOCKS_PER_SEC)<<" ms\n";
    }
    clock_t time3=clock();
    std::cout<<"all run time:"<<(time3-time0)*(1000.0/CLOCKS_PER_SEC)<<" ms\n";
    return 0;
}

