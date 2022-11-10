// vcdiff_wrapper.cpp
// HDiffPatch
/*
 The MIT License (MIT)
 Copyright (c) 2022 HouSisong
 
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
#include "vcdiff_wrapper.h"
#include "../libHDiffPatch/HDiff/match_block.h"
#include "../libHDiffPatch/HDiff/private_diff/mem_buf.h"
#include "../libHDiffPatch/HDiff/private_diff/limit_mem_diff/stream_serialize.h"
#include "../libHDiffPatch/HPatch/patch.h"
#include <stdexcept>  //std::runtime_error
#define _check(value,info) { if (!(value)) { throw std::runtime_error(info); } }
static const unsigned char kVcDiffType[3]={('V'|(1<<7)),('C'|(1<<7)),('D'|(1<<7))};
#define kVcDiffVersion 0

namespace hdiff_private{

static void serialize_vcdiff(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                             const TCovers& covers,const hpatch_TStreamOutput* out_diff,
                             const vcdiff_TCompress* compressPlugin,bool isZeroSubDiff=false){
    std::vector<unsigned char> buf;
    TDiffStream outDiff(out_diff);
    size_t ctrlDataSize_pos;
    size_t subDataSize_pos;
    {//head
        buf.clear();
        pushBack(buf,kVcDiffType,sizeof(kVcDiffType));
        buf.push_back(kVcDiffVersion);
        buf.push_back(0);// Hdr_Indicator: No compression, no custom code table
        assert((compressPlugin==0)||(compressPlugin->compress_type==kVcDiff_compressorID_no));
        
    }
    //todo:
}

void _create_vcdiff(const unsigned char* newData,const unsigned char* cur_newData_end,const unsigned char* newData_end,
                    const unsigned char* oldData,const unsigned char* cur_oldData_end,const unsigned char* oldData_end,
                    const hpatch_TStreamOutput* out_diff,const vcdiff_TCompress* compressPlugin,
                    int kMinSingleMatchScore,bool isUseBigCacheMatch,
                    ICoverLinesListener* coverLinesListener,size_t threadNum){
    std::vector<hpatch_TCover_sz> covers;
    get_match_covers_by_sstring(newData,cur_newData_end,oldData,cur_oldData_end,covers,
                                kMinSingleMatchScore,isUseBigCacheMatch,coverLinesListener,threadNum);
    const TCovers _covers((void*)covers.data(),covers.size(),
                          sizeof(*covers.data())==sizeof(hpatch_TCover32));
    
    hdiff_TStreamInput newStream;
    hdiff_TStreamInput oldStream;
    mem_as_hStreamInput(&newStream,newData,newData_end);
    mem_as_hStreamInput(&oldStream,oldData,oldData_end);
    
    serialize_vcdiff(&newStream,&oldStream,_covers,out_diff,compressPlugin);
}

}//end namespace hdiff_private

using namespace hdiff_private;

void create_vcdiff(const unsigned char* newData,const unsigned char* newData_end,
                   const unsigned char* oldData,const unsigned char* oldData_end,
                   const hpatch_TStreamOutput* out_diff,const vcdiff_TCompress* compressPlugin,
                   int kMinSingleMatchScore,bool isUseBigCacheMatch,
                   ICoverLinesListener* coverLinesListener,size_t threadNum){
    _create_vcdiff(newData,newData_end,newData_end,oldData,oldData_end,oldData_end,
                   out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,
                   coverLinesListener,threadNum);
}
void create_vcdiff(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                   const hpatch_TStreamOutput* out_diff,const vcdiff_TCompress* compressPlugin,
                   int kMinSingleMatchScore,bool isUseBigCacheMatch,
                   ICoverLinesListener* coverLinesListener,size_t threadNum){
    TAutoMem oldAndNewData;
    loadOldAndNewStream(oldAndNewData,oldData,newData);
    size_t old_size=oldData?(size_t)oldData->streamSize:0;
    unsigned char* pOldData=oldAndNewData.data();
    unsigned char* pNewData=pOldData+old_size;
    unsigned char* pNewDataEnd=pNewData+(size_t)newData->streamSize;
    _create_vcdiff(pNewData,pNewDataEnd,pNewDataEnd,pOldData,pOldData+old_size,pOldData+old_size,
                   out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,
                   coverLinesListener,threadNum);
}

void create_vcdiff_stream(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                          const hpatch_TStreamOutput* out_diff,const vcdiff_TCompress* compressPlugin,
                          size_t kMatchBlockSize,size_t threadNum){
    TCoversBuf covers(newData->streamSize,oldData->streamSize);
    get_match_covers_by_block(newData,oldData,&covers,kMatchBlockSize,threadNum);
    serialize_vcdiff(newData,oldData,covers,out_diff,compressPlugin,true);
}


void create_vcdiff_block(unsigned char* newData,unsigned char* newData_end,
                         unsigned char* oldData,unsigned char* oldData_end,
                         const hpatch_TStreamOutput* out_diff,const vcdiff_TCompress* compressPlugin,
                         int kMinSingleMatchScore,bool isUseBigCacheMatch,
                         size_t matchBlockSize,size_t threadNum){
    if (matchBlockSize==0){
        _create_vcdiff(newData,newData_end,newData_end,oldData,oldData_end,oldData_end,
                       out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,0,threadNum);
        return;
    }
    TCoversOptimMB<TMatchBlock> coversOp(newData,newData_end,oldData,oldData_end,matchBlockSize,threadNum);
    _create_vcdiff(newData,coversOp.matchBlock->newData_end_cur,newData_end,
                   oldData,coversOp.matchBlock->oldData_end_cur,oldData_end,
                   out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,&coversOp,threadNum);   
}
void create_vcdiff_block(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                         const hpatch_TStreamOutput* out_diff,const vcdiff_TCompress* compressPlugin,
                         int kMinSingleMatchScore,bool isUseBigCacheMatch,
                         size_t matchBlockSize,size_t threadNum){
    TAutoMem oldAndNewData;
    loadOldAndNewStream(oldAndNewData,oldData,newData);
    size_t old_size=oldData?(size_t)oldData->streamSize:0;
    unsigned char* pOldData=oldAndNewData.data();
    unsigned char* pNewData=pOldData+old_size;
    create_vcdiff_block(pNewData,pNewData+(size_t)newData->streamSize,pOldData,pOldData+old_size,
                        out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,
                        matchBlockSize,threadNum);
}

bool get_is_vcdiff(const hpatch_TStreamInput* diffData){
    hpatch_VcDiffInfo diffinfo;
    if (!getVcDiffInfo(&diffinfo,diffData))
        return false;
    return true;
}
bool get_is_vcdiff(const unsigned char* diffData,const unsigned char* diffData_end){
    hdiff_TStreamInput diffStream;
    mem_as_hStreamInput(&diffStream,diffData,diffData_end);
    return get_is_vcdiff(&diffStream);
}

bool check_vcdiff(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                  const hpatch_TStreamInput* diffStream,hpatch_TDecompress* decompressPlugin){
    const size_t kACacheBufSize=hpatch_kFileIOBufBetterSize;
    TAutoMem _cache(kACacheBufSize*(1+5));
    _TCheckOutNewDataStream out_newData(newData,_cache.data(),kACacheBufSize);
    _test_rt(vcpatch_with_cache(&out_newData,oldData,diffStream,decompressPlugin,hpatch_TRUE,
                                _cache.data()+kACacheBufSize,_cache.data_end()));
    _test_rt(out_newData.isWriteFinish());
    return true;
}
bool check_vcdiff(const unsigned char* newData,const unsigned char* newData_end,
                  const unsigned char* oldData,const unsigned char* oldData_end,
                  const unsigned char* diffData,const unsigned char* diffData_end,
                  hpatch_TDecompress* decompressPlugin){
    hdiff_TStreamInput newStream;
    hdiff_TStreamInput oldStream;
    hdiff_TStreamInput diffStream;
    mem_as_hStreamInput(&newStream,newData,newData_end);
    mem_as_hStreamInput(&oldStream,oldData,oldData_end);
    mem_as_hStreamInput(&diffStream,diffData,diffData_end);
    return check_vcdiff(&newStream,&oldStream,&diffStream,decompressPlugin);
}