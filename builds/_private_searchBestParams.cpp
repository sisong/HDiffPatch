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
#include <time.h>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include "../libHDiffPatch/HDiff/diff.h"
#include "../libHDiffPatch/HPatch/patch.h"
#include "../libHDiffPatch/HDiff/private_diff/suffix_string.h"
#include "../../zlib/zlib.h"

typedef unsigned char   TByte;
typedef unsigned int    TUInt32;
typedef ptrdiff_t       TInt;

void readFile(std::vector<TByte>& data,const char* fileName){
    std::ifstream file(fileName);
    file.seekg(0,std::ios::end);
    std::streampos file_length=file.tellg();
    file.seekg(0,std::ios::beg);
    size_t needRead=(size_t)file_length;
    if ((file_length<0)||((std::streamsize)needRead!=(std::streamsize)file_length)) {
        file.close();
        exit(1);
    }
    data.resize(needRead);
    file.read((char*)&data[0], needRead);
    std::streamsize readed=file.gcount();
    file.close();
    if ((std::streamsize)needRead!=readed)  exit(1);
}

void writeFile(const std::vector<TByte>& data,const char* fileName){
    std::ofstream file(fileName);
    file.write((const char*)&data[0], data.size());
    file.close();
}

int zip_compress(unsigned char* out_data,unsigned char* out_data_end,const unsigned char* src,const unsigned char* src_end,int zip_parameter){
    const unsigned char* _zipSrc=&src[0];
    unsigned char* _zipDst=&out_data[0];
    
    z_stream c_stream;
    c_stream.zalloc = (alloc_func)0;
    c_stream.zfree = (free_func)0;
    c_stream.opaque = (voidpf)0;
    c_stream.next_in = (Bytef*)_zipSrc;
    c_stream.avail_in = (int)(src_end-src);
    c_stream.next_out = (Bytef*)_zipDst;
    c_stream.avail_out = (unsigned int)(out_data_end-out_data);
    int ret = deflateInit2(&c_stream, zip_parameter,Z_DEFLATED, 31,8, Z_DEFAULT_STRATEGY);
    if(ret != Z_OK)
    {
        std::cout <<"|"<<"deflateInit2 error "<<std::endl;
        return 0;
    }
    ret = deflate(&c_stream, Z_FINISH);
    if (ret != Z_STREAM_END)
    {
        deflateEnd(&c_stream);
        std::cout <<"|"<<"ret != Z_STREAM_END err="<< ret <<std::endl;
        return 0;
    }
    
    int zipLen = (int)c_stream.total_out;
    ret = deflateEnd(&c_stream);
    if (ret != Z_OK)
    {
        std::cout <<"|"<<"deflateEnd error "<<std::endl;
        return 0;
    }
    return zipLen;
}


int zip_decompress(unsigned char* out_data,unsigned char* out_data_end,const unsigned char* zip_code,const unsigned char* zip_code_end){
    #define CHUNK (256*1024)
    
    int ret;
    unsigned int have;
    z_stream strm;
    unsigned char out[CHUNK];
    int totalsize = 0;
    
    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    
    ret = inflateInit2(&strm, 31);
    
    if (ret != Z_OK)
        return ret;
    
    strm.avail_in = (int)(zip_code_end-zip_code);
    strm.next_in = (unsigned char*)zip_code;
    
    /* run inflate() on input until output buffer not full */
    do {
        strm.avail_out = CHUNK;
        strm.next_out = out;
        ret = inflate(&strm, Z_NO_FLUSH);
        switch (ret)
        {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR; /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                inflateEnd(&strm);
                return ret;
        }
        
        have = CHUNK - strm.avail_out;
        memcpy(out_data + totalsize,out,have);
        totalsize += have;
        assert(out_data+totalsize<=out_data_end);
    } while (strm.avail_out == 0);
    
    /* clean up and return */
    inflateEnd(&strm);
    assert( ret == Z_STREAM_END );
    return true;
}

struct THDiffPrivateParams{
    int kMinMatchLength;
    int kMinSingleMatchLength;
    std::string asString()const{
        std::stringstream str;
        str<<kMinMatchLength; str<<'\t';
        str<<kMinSingleMatchLength;
        
        return str.str();
    }
};

struct TDiffInfo{
    TSuffixString       sstring;
    std::vector<TByte>  oldData;
    std::vector<TByte>  newData;
    std::string         oldFileName;
    size_t              oldFileSize;
    std::string         newFileName;
    size_t              newFileSize;
    THDiffPrivateParams kP;
    size_t              diffSize;
    size_t              zipSize;
    std::string asString()const{
        std::stringstream str;
        
        str<<getFileName(oldFileName); str<<'\t';
        str<<getFileName(newFileName); str<<'\t';
        str<<oldFileSize; str<<'\t';
        str<<newFileSize; str<<'\t';
        str<<kP.asString(); str<<'\t';
        str<<diffSize; str<<'\t';
        str<<zipSize;
        
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

void doDiff(TDiffInfo& di){
    extern void __hdiff_private__create_diff(const TByte* newData,const TByte* newData_end,
                                             const TByte* oldData,const TByte* oldData_end,
                                             std::vector<TByte>& out_diff,const void* kDiffParams,const TSuffixString* sstring);
    
    if (di.sstring.SASize()==0){
        readFile(di.oldData,di.oldFileName.c_str());
        readFile(di.newData,di.newFileName.c_str());
        di.oldFileSize=di.oldData.size();
        di.newFileSize=di.newData.size();
        const TByte* oldData_begin=0; if (!di.oldData.empty()) oldData_begin=&di.oldData[0];
        di.sstring.resetSuffixString(oldData_begin,oldData_begin+di.oldData.size());
    }
    
    std::vector<TByte> diffData;
    TUInt32 newDataSize=(TUInt32)di.newFileSize;
    diffData.push_back(newDataSize);
    diffData.push_back(newDataSize>>8);
    diffData.push_back(newDataSize>>16);
    diffData.push_back(newDataSize>>24);
    
    const TByte* newData_begin=0; if (!di.newData.empty()) newData_begin=&di.newData[0];
    const TByte* oldData_begin=0; if (!di.oldData.empty()) oldData_begin=&di.oldData[0];
    __hdiff_private__create_diff(newData_begin,newData_begin+di.newData.size(),
                                 oldData_begin,oldData_begin+di.oldData.size(),
                                 diffData,&di.kP,&di.sstring);

    if (!check_diff(newData_begin,newData_begin+di.newData.size(),
                    oldData_begin,oldData_begin+di.oldData.size(),
                    &diffData[0]+4, &diffData[0]+diffData.size())){
        std::cout<<"\ncheck diff data error!!!\n";
        exit(1);
    }
    di.diffSize=diffData.size();
    
    std::vector<TByte> zipData;
    zipData.resize(diffData.size()*1.2+1024);
    size_t zipCodeSize=zip_compress(&zipData[0], &zipData[0]+zipData.size(), &diffData[0], &diffData[0]+diffData.size(), Z_BEST_COMPRESSION);
    assert((zipCodeSize>0)&&(zipCodeSize<=zipData.size()));
    di.zipSize=zipCodeSize;
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
    
    for (int kMinMatchLength=6; kMinMatchLength<=12; ++kMinMatchLength) {
        for (int kMinSingleMatchLength=kMinMatchLength; kMinSingleMatchLength<=35; ++kMinSingleMatchLength) {
        {//for (int kExtendMinSameRatio=0.40f*kFixedFloatSmooth_base; kExtendMinSameRatio<=0.55f*kFixedFloatSmooth_base;kExtendMinSameRatio+=0.01f*kFixedFloatSmooth_base) {

        double sumDiffR=0;
        double sumZipDiffR=0;
        size_t sumOldSize=0;
        size_t sumNewSize=0;
        size_t sumDiffSize=0;
        size_t sumZipDiffSize=0;
        for (size_t doi=0; doi<DiList.size(); ++doi) {
            TDiffInfo& curDi=DiList[doi];
            curDi.kP.kMinMatchLength=kMinMatchLength;
            curDi.kP.kMinSingleMatchLength=kMinSingleMatchLength;
            //curDi.kP.kExtendMinSameRatio=kExtendMinSameRatio;
            
            doDiff(curDi);
            double curDiffRi=curDi.diffSize*1.0/curDi.newFileSize;
            sumDiffR+=curDiffRi;
            double curZipDiffRi=curDi.zipSize*1.0/curDi.newFileSize;
            sumZipDiffR+=curZipDiffRi;
            sumNewSize+=curDi.newFileSize;
            sumOldSize+=curDi.oldFileSize;
            sumDiffSize+=curDi.diffSize;
            sumZipDiffSize+=curDi.zipSize;
            //std::cout<<curDi.asString()<<"\t"<<curDiffRi<<"\n";
        }
        
        const double curDiffR=sumDiffR/kDoCount;
        const double curZipDiffR=sumZipDiffR/kDoCount;
        {
            TDiffInfo curDi;
            curDi.oldFileName="";
            curDi.newFileName="";
            curDi.kP.kMinMatchLength=kMinMatchLength;
            curDi.kP.kMinSingleMatchLength=kMinSingleMatchLength;
            curDi.oldFileSize=sumOldSize;
            curDi.newFileSize=sumNewSize;
            curDi.diffSize=sumDiffSize;
            curDi.zipSize=sumZipDiffSize;
            if (curDiffR<=bestDiffR){
                bestDiffR=curDiffR;
                std::cout<<"***";
            }
            std::cout<<curDi.asString()<<"\t"<<curDiffR<<"\t"<<curZipDiffR;
            if (curZipDiffR<=bestZipDiffR){
                bestZipDiffR=curZipDiffR;
                std::cout<<"   ***";
            }
            std::cout<<"\n";
        }
    }}}
}


int main(int argc, const char * argv[]){
    if ((argc<3)||((argc-1)%2!=0)) {
        throw argc;
    }
    std::vector<std::string> fileNames;
    for (int i=1; i<argc; ++i) {
        fileNames.push_back(argv[i]);
    }

    getBestHDiffPrivateParams(fileNames);
    
    std::cout<<"\nok!\n";
    return 0;
}


