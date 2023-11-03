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

namespace{
#ifdef __cplusplus
extern "C" {
#endif
    struct TWorkBuf{
        struct TWorkBuf*    next;
        hpatch_StreamPos_t  workIndex;
        size_t              dictSize;
        size_t              uncompressedSize;
        size_t              compressedSize;
        unsigned char       buf[1];
    };
#ifdef __cplusplus
}
#endif

struct TDict{
    unsigned char*  buf;
    size_t          size;
};

struct TBlockCompressor {
    hdiff_compressBlockHandle       handle;
    inline explicit TBlockCompressor():handle(0) { }
    inline ~TBlockCompressor() { if ((_pc!=0)&&(handle!=0)) _pc->closeBlockCompressor(_pc,handle); }
    inline void open(hdiff_TParallelCompress* pc){
        _pc=pc;
        handle=pc->openBlockCompressor(pc);
        if (handle==0) throw std::runtime_error("parallel_compress_blocks() openBlockCompressor() error!");
    }
    hdiff_TParallelCompress*  _pc;
};

struct TMt:public TMtByChannel{
    hdiff_TParallelCompress*        pc;
    size_t                          blockDictSize;
    size_t                          blockSize;
    size_t                          threadMemSize;
    const hpatch_TStreamOutput*     out_code;
    const hpatch_TStreamInput*      in_data;
    hpatch_StreamPos_t              blockCount;
    
    hpatch_StreamPos_t              outCodePos;
    hpatch_StreamPos_t              curReadBlockIndex;
    hpatch_StreamPos_t              curWriteBlockIndex;
    std::vector<TBlockCompressor>   blockCompressors;
    CHLocker                        readLocker;
    CHLocker                        writeLocker;
    TDict                           dict;
    TWorkBuf*                       dataBufList;
};

void _threadRunCallBack(int threadIndex,void* _workData){
    #define _check_br(value){ \
        if (!(value)) { LOG_ERR("parallel_compress_blocks() check "#value" error!\n"); \
            mt.on_error(); break; } }
    TMt& mt=*(TMt*)_workData;
    hdiff_compressBlockHandle cbhandle=mt.blockCompressors[threadIndex].handle;
    TMtByChannel::TAutoThreadEnd __auto_thread_end(mt);
    while (true) {
        TWorkBuf* workBuf=(TWorkBuf*)mt.work_chan.accept(true);
        if (workBuf==0) break; //finish
        unsigned char* dictBufEnd;
        unsigned char* dataBufEnd;
        {//read data
            CAutoLocker _autoLoker(mt.readLocker.locker);
            if (mt.curReadBlockIndex>=mt.blockCount) break;
            workBuf->dictSize=mt.dict.size;
            memcpy(workBuf->buf,mt.dict.buf,mt.dict.size);
            const hpatch_StreamPos_t inDataPos=mt.curReadBlockIndex*mt.blockSize;
            if (mt.curReadBlockIndex+1<mt.blockCount)
                workBuf->uncompressedSize=mt.blockSize;
            else
                workBuf->uncompressedSize=(size_t)(mt.in_data->streamSize-inDataPos);
            dictBufEnd=workBuf->buf+workBuf->dictSize;
            dataBufEnd=dictBufEnd+workBuf->uncompressedSize;
            _check_br(mt.in_data->read(mt.in_data,inDataPos,dictBufEnd,dataBufEnd));
            workBuf->workIndex=mt.curReadBlockIndex;
            ++mt.curReadBlockIndex;
            if (mt.curReadBlockIndex<mt.blockCount){//update dict
                if (workBuf->uncompressedSize+workBuf->dictSize>=mt.blockDictSize)
                    mt.dict.size=mt.blockDictSize;
                else
                    mt.dict.size=workBuf->uncompressedSize+workBuf->dictSize;
                memcpy(mt.dict.buf,dataBufEnd-mt.dict.size,mt.dict.size);
            }
        }
        //compress
        workBuf->compressedSize=mt.pc->compressBlock(mt.pc,cbhandle,workBuf->workIndex,mt.blockCount,
                                                     dataBufEnd,workBuf->buf+mt.threadMemSize,
                                                     workBuf->buf,dictBufEnd,dataBufEnd);
        _check_br(workBuf->compressedSize>0);
        {//write or push
            CAutoLocker _autoLoker(mt.writeLocker.locker);
            //push to list
            TWorkBuf** insertBuf=&mt.dataBufList;
            while ((*insertBuf)&&((*insertBuf)->workIndex<workBuf->workIndex))
                insertBuf=&((*insertBuf)->next);
            workBuf->next=*insertBuf;
            *insertBuf=workBuf;
            //write
            while (mt.dataBufList&&(mt.dataBufList->workIndex==mt.curWriteBlockIndex)){
                //pop from list
                workBuf=mt.dataBufList;
                mt.dataBufList=mt.dataBufList->next;
                //write
                const unsigned char* codeBuf=workBuf->buf+workBuf->dictSize+workBuf->uncompressedSize;
                _check_br(mt.out_code->write(mt.out_code,mt.outCodePos,codeBuf,codeBuf+workBuf->compressedSize));
                mt.outCodePos+=workBuf->compressedSize;
                ++mt.curWriteBlockIndex;
                _check_br(mt.work_chan.send(workBuf,true));
            }
        }
    }
    #undef _check_br
}

} // namespace

hpatch_StreamPos_t parallel_compress_blocks(hdiff_TParallelCompress* pc,
                                            int threadNum,size_t blockDictSize,size_t blockSize,
                                            const hpatch_TStreamOutput* out_code,
                                            const hpatch_TStreamInput*  in_data){
    assert(blockSize>0);
    assert(threadNum>0);
    if (threadNum<1) threadNum=1;
    hpatch_StreamPos_t blockCount=(in_data->streamSize+blockSize-1)/blockSize;
    if ((hpatch_StreamPos_t)threadNum>blockCount) threadNum=(int)blockCount;
    const size_t workBufCount=threadNum+(threadNum+2)/3;
    const size_t threadMemSize=(size_t)(blockDictSize+blockSize+pc->maxCompressedSize(blockSize));
    try {
        hdiff_private::TAutoMem  mem;
        TMt workData;
        workData.pc=pc;
        workData.blockCount=blockCount;
        workData.blockDictSize=blockDictSize;
        workData.blockSize=blockSize;
        workData.out_code=out_code;
        workData.outCodePos=0;
        workData.curReadBlockIndex=0;
        workData.curWriteBlockIndex=0;
        workData.in_data=in_data;
        workData.threadMemSize=threadMemSize;
        workData.blockCompressors.resize(threadNum);
        for (int t=0; t<threadNum; ++t)
            workData.blockCompressors[t].open(pc);
        mem.realloc(workData.threadMemSize*workBufCount + blockDictSize);
        unsigned char* pbuf=mem.data();
        for (size_t i=0; i<workBufCount; ++i){
            if (!workData.work_chan.send(pbuf,true))
                throw std::runtime_error("parallel_compress_blocks() workData.data_chan.send() error!");
            pbuf+=threadMemSize;
        }
        workData.dict.buf=pbuf;
        workData.dict.size=0;
        workData.dataBufList=0;
        workData.start_threads(threadNum,_threadRunCallBack,&workData,hpatch_TRUE);
        workData.wait_all_thread_end();
        return workData.is_on_error()?0:workData.outCodePos;
    } catch (const std::exception& e) {
        LOG_ERR("parallel_compress_blocks run error! %s\n",e.what());
        return 0; //error
    }
}

#endif// _IS_USED_MULTITHREAD
