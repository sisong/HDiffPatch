//  compress_parallel.cpp
//  parallel compress for HDiffz
/*
 The MIT License (MIT)
 Copyright (c) 2019 HouSisong
 
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
#include "compress_parallel.h"
#if (_IS_USED_MULTITHREAD)
#include <stdio.h>
#include <exception>
#include <vector>
#include "libParallel/parallel_channel.h"
#include "libHDiffPatch/HDiff/private_diff/mem_buf.h"


#define check(value) { \
    if (!(value)) throw std::runtime_error("parallel_compress_blocks() check "#value" error!"); }



struct TBlockCompressor {
    hdiff_compressBlockHandle       handle;
    inline explicit TBlockCompressor():handle(0) { }
    inline ~TBlockCompressor() { if ((_pc!=0)&&(handle!=0)) _pc->closeBlockCompressor(_pc,handle); }
    inline void open(hdiff_TParallelCompress* pc){
        _pc=pc;
        handle=pc->openBlockCompressor(pc);
        check(handle!=0);
    }
    hdiff_TParallelCompress*  _pc;
};

struct TWorkData{
    hdiff_TParallelCompress*        pc;
    size_t                          blockSize;
    size_t                          threadMemSize;
    const hdiff_TStreamOutput*      out_code;
    const hdiff_TStreamInput*       in_data;
    
    bool                            isInError;
    hpatch_StreamPos_t              inDataPos;
    hpatch_StreamPos_t              outCodePos;
    hpatch_StreamPos_t              outBlockIndex;
    std::vector<TBlockCompressor>   blockCompressors;
    hdiff_private::TAutoMem         mem;
    CHLocker                        ioLocker;
    int                             chanWaitCount;
    CChannel                        chanForWaitWrite;
    CChannel                        chanForWorkEnd;
};

void _threadRunCallBack(int threadIndex,void* _workData){
    #define _check_br(value){ \
        if (!(value)) { fprintf(stderr,"parallel_compress_blocks() check "#value" error!\n"); \
        wd.isInError=true; wd.chanForWaitWrite.close(); wd.chanForWorkEnd.close(); break; } }
    TWorkData& wd=*(TWorkData*)_workData;
    hdiff_compressBlockHandle cbhandle=wd.blockCompressors[threadIndex].handle;
    unsigned char* dataBuf=wd.mem.data()+threadIndex*wd.threadMemSize;
    unsigned char* codeBuf=dataBuf+wd.blockSize;
    unsigned char* codeBufEnd=dataBuf+wd.threadMemSize;
    while (true) {
        size_t readLen=wd.blockSize;
        hpatch_StreamPos_t blockIndex;
        {//read data
            CAutoLocker _autoLoker(wd.ioLocker.locker);
            if (wd.isInError) break;
            blockIndex=wd.inDataPos/wd.blockSize;
            if (readLen+wd.inDataPos>wd.in_data->streamSize)
                readLen=(size_t)(wd.in_data->streamSize-wd.inDataPos);
            if (readLen==0) break;
            _check_br(wd.in_data->read(wd.in_data,wd.inDataPos,dataBuf,dataBuf+readLen));
            wd.inDataPos+=readLen;
        }
        //compress
        size_t codeLen=wd.pc->compressBlock(wd.pc,cbhandle,blockIndex,
                                            codeBuf,codeBufEnd,dataBuf,dataBuf+readLen);
        while (true) {//write code
            {//write or wait ?
                CAutoLocker _autoLoker(wd.ioLocker.locker);
                if (wd.isInError) break;
                _check_br(codeLen>0);
                //write
                const bool isWrite=(blockIndex==wd.outBlockIndex);
                if (isWrite){
                    _check_br(wd.out_code->write(wd.out_code,wd.outCodePos,codeBuf,codeBuf+codeLen));
                    wd.outCodePos+=codeLen;
                    ++wd.outBlockIndex;
                }
                //wake all
                for (int i=0; i<wd.chanWaitCount; ++i)
                    wd.chanForWaitWrite.send((TChanData)(1+(size_t)i),true);
                wd.chanWaitCount=0;
                //continue work or wait
                if (isWrite)
                    break;//continue work
                else
                    ++wd.chanWaitCount; //continue wait
            }
            //wait
            wd.chanForWaitWrite.accept(true);
        }
    }
    wd.chanForWorkEnd.send((TChanData)(1+(size_t)threadIndex),true); //can't send null
    #undef _check_br
}


hpatch_StreamPos_t parallel_compress_blocks(hdiff_TParallelCompress* pc,
                                            int threadNum,size_t blockSize,
                                            const hdiff_TStreamOutput* out_code,
                                            const hdiff_TStreamInput*  in_data){
    assert(blockSize>0);
    assert(threadNum>0);
    if (threadNum<1) threadNum=1;
    hpatch_StreamPos_t blockCount=(in_data->streamSize+blockSize-1)/blockSize;
    if ((hpatch_StreamPos_t)threadNum>blockCount) threadNum=(int)blockCount;
    try {
        TWorkData workData;
        workData.isInError=false;
        workData.chanWaitCount=0;
        workData.pc=pc;
        workData.blockSize=blockSize;
        workData.out_code=out_code;
        workData.outCodePos=0;
        workData.outBlockIndex=0;
        workData.in_data=in_data;
        workData.inDataPos=0;
        workData.threadMemSize=(size_t)(blockSize + pc->maxCompressedSize(blockSize));
        workData.mem.realloc(workData.threadMemSize*threadNum);
        workData.blockCompressors.resize(threadNum);
        for (int t=0; t<threadNum; ++t)
            workData.blockCompressors[t].open(pc);
        
        thread_parallel(threadNum,_threadRunCallBack,&workData,hpatch_TRUE,0);
        
        for (int t=0;t<threadNum; ++t) //wait all thread end
            workData.chanForWorkEnd.accept(true);
        return workData.isInError?0:workData.outCodePos;
    } catch (const std::exception& err) {
        fprintf(stderr,"parallel_compress_blocks run error! %s\n",err.what());
        return 0; //error
    }
}

#endif// _IS_USED_MULTITHREAD
