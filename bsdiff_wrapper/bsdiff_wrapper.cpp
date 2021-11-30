// bsdiff_wrapper.cpp
// HDiffPatch
/*
 The MIT License (MIT)
 Copyright (c) 2021 HouSisong
 
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
#include "bsdiff_wrapper.h"
#include "bspatch_wrapper.h"
#include "../libHDiffPatch/HDiff/match_block.h"
#include "../libHDiffPatch/HDiff/diff.h"
#include "../libHDiffPatch/HDiff/private_diff/mem_buf.h"
#include "../libHDiffPatch/HDiff/private_diff/limit_mem_diff/stream_serialize.h"
#include "../libHDiffPatch/HPatch/patch.h"
#include <stdexcept>  //std::runtime_error
#define _check(value,info) { if (!(value)) { throw std::runtime_error(info); } }
static const char* kBsDiffVersionType="BSDIFF40";

namespace hdiff_private{
    static inline void pushUInt64(std::vector<unsigned char>& buf,hpatch_uint64_t v){ 
        pushUInt(buf,v);
    }
    static inline void pushSInt64(std::vector<unsigned char>& buf,hpatch_uint64_t v){
        if ((v>>63)==1){
            v=-v;
            v|=((hpatch_uint64_t)1)<<63;
        }
        pushUInt64(buf,v);
    }

    struct TCtrlStream:public hpatch_TStreamInput{
        explicit TCtrlStream(const std::vector<hpatch_TCover_sz>& _covers)
        :curPos(0),curi(0),bufi(0),covers(_covers){
            streamImport=this;
            read=_read;
            streamSize=(covers.size()-1)*(hpatch_StreamPos_t)(3*8);
            buf.reserve(hpatch_kFileIOBufBetterSize);
        }
    private:
        hpatch_StreamPos_t curPos;
        size_t curi;
        size_t bufi;
        const std::vector<hpatch_TCover_sz>& covers;
        std::vector<unsigned char> buf;
        void _updateBuf(){
            buf.clear();
            bufi=0;
            while ((curi+1<covers.size())&&((buf.size()+3*8)<=hpatch_kFileIOBufBetterSize)){
                const hpatch_TCover_sz& c=covers[curi++];
                const hpatch_TCover_sz& cnext=covers[curi];
                pushUInt64(buf,c.length);
                pushUInt64(buf,(hpatch_uint64_t)cnext.newPos-(hpatch_uint64_t)(c.newPos+c.length));
                pushSInt64(buf,(hpatch_uint64_t)cnext.oldPos-(hpatch_uint64_t)(c.oldPos+c.length));
            }
        }
        hpatch_BOOL readTo(hpatch_StreamPos_t readFromPos,unsigned char* out_data,unsigned char* out_data_end){
            if (readFromPos!=curPos) 
                return hpatch_FALSE;
            size_t readLen=out_data_end-out_data;
            curPos+=readLen;
            while (readLen>0){
                if (bufi==buf.size()){
                    _updateBuf();
                    _check(bufi<buf.size(),"TCtrlStream readTo");
                } 
                size_t cLen=buf.size()-bufi;
                if (cLen>readLen) cLen=readLen;
                memcpy(out_data,buf.data()+bufi,cLen);
                out_data+=cLen;
                bufi+=cLen;
                readLen-=cLen;
            }
            return hpatch_TRUE;
        }
        static hpatch_BOOL _read(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                 unsigned char* out_data,unsigned char* out_data_end){
            TCtrlStream* self=(TCtrlStream*)stream->streamImport;
            return self->readTo(readFromPos,out_data,out_data_end);
        }
    };

static void serialize_bsdiff(const unsigned char* newData,const unsigned char* newData_end,
                             const unsigned char* oldData,const unsigned char* oldData_end,
                             const std::vector<hpatch_TCover_sz>& covers,
                             const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin){
    std::vector<unsigned char> buf;
    TDiffStream outDiff(out_diff);
    size_t ctrlDataSize_pos;
    size_t subDataSize_pos;
    {//head
        buf.clear();
        pushCStr(buf,kBsDiffVersionType);
        ctrlDataSize_pos=buf.size();
        pushUInt64(buf,0); //ctrlDataSize
        subDataSize_pos=buf.size();
        pushUInt64(buf,0); //subDataSize
        pushUInt64(buf,(size_t)(newData_end-newData));
        outDiff.pushBack(buf.data(),buf.size());
    }
    
    {//ctrl data
        TCtrlStream ctrlStream(covers);
        hpatch_StreamPos_t ctrlDataSize=outDiff.pushStream(&ctrlStream,compressPlugin,true);

        buf.clear();
        pushUInt64(buf,ctrlDataSize);//update ctrlDataSize
        _check(out_diff->write(out_diff,ctrlDataSize_pos,buf.data(),
                               buf.data()+buf.size()),"serialize_bsdiff() out_diff->write");
    }

    const TCovers _covers((void*)covers.data(),covers.size(),
                          sizeof(*covers.data())==sizeof(hpatch_TCover32));
    {//sub data
        TNewDataSubDiffStream_mem subStream(newData,newData_end,oldData,oldData_end,_covers,true);
        hpatch_StreamPos_t subDataSize=outDiff.pushStream(&subStream,compressPlugin,true);

        buf.clear();
        pushUInt64(buf,subDataSize);//update subDataSize
        _check(out_diff->write(out_diff,subDataSize_pos,buf.data(),
                               buf.data()+buf.size()),"serialize_bsdiff() out_diff->write");
    }

    {//copy data
        hpatch_TStreamInput newStream;
        mem_as_hStreamInput(&newStream,newData,newData_end);
        TNewDataDiffStream diffStream(_covers,&newStream);
        outDiff.pushStream(&diffStream,compressPlugin,true);
    }
}


void _create_bsdiff(const unsigned char* newData,const unsigned char* cur_newData_end,const unsigned char* newData_end,
                    const unsigned char* oldData,const unsigned char* cur_oldData_end,const unsigned char* oldData_end,
                    const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                    int kMinSingleMatchScore,bool isUseBigCacheMatch,ICoverLinesListener* coverLinesListener){
    std::vector<hpatch_TCover_sz> covers;
    get_match_covers_by_sstring(newData,cur_newData_end,oldData,cur_oldData_end,covers,
                                kMinSingleMatchScore,isUseBigCacheMatch,coverLinesListener);
    if (covers.empty()||(covers[0].newPos!=0)||(covers[0].oldPos!=0)){//begin cover
        hpatch_TCover_sz lc;
        lc.newPos=0;
        lc.oldPos=0;
        lc.length=0;
        covers.insert(covers.begin(),lc);
    }
    {//last cover
        size_t lastOldPos=0;
        if (!covers.empty()){
            const hpatch_TCover_sz& lc=covers[covers.size()-1];
            lastOldPos=lc.oldPos+lc.length;
        }
        size_t newSize=newData_end-newData;
        hpatch_TCover_sz lc;
        lc.newPos=newSize;
        lc.oldPos=lastOldPos;
        lc.length=0;
        covers.push_back(lc);
    }
    serialize_bsdiff(newData,newData_end,oldData,oldData_end,covers,out_diff,compressPlugin);
}

}//end namespace hdiff_private

using namespace hdiff_private;

void create_bsdiff(const unsigned char* newData,const unsigned char* newData_end,
                   const unsigned char* oldData,const unsigned char* oldData_end,
                   const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                   int kMinSingleMatchScore,bool isUseBigCacheMatch,ICoverLinesListener* coverLinesListener){
    _create_bsdiff(newData,newData_end,newData_end,oldData,oldData_end,oldData_end,
                   out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,coverLinesListener);
}
void create_bsdiff(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                   const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                   int kMinSingleMatchScore,bool isUseBigCacheMatch,ICoverLinesListener* coverLinesListener){
    TAutoMem oldAndNewData;
    loadOldAndNewStream(oldAndNewData,oldData,newData);
    size_t old_size=oldData?(size_t)oldData->streamSize:0;
    unsigned char* pOldData=oldAndNewData.data();
    unsigned char* pNewData=pOldData+old_size;
    unsigned char* pNewDataEnd=pNewData+(size_t)newData->streamSize;
    _create_bsdiff(pNewData,pNewDataEnd,pNewDataEnd,pOldData,pOldData+old_size,pOldData+old_size,
                   out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,coverLinesListener);
}

void create_bsdiff_block(unsigned char* newData,unsigned char* newData_end,
                         unsigned char* oldData,unsigned char* oldData_end,
                         const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                         int kMinSingleMatchScore,bool isUseBigCacheMatch,size_t matchBlockSize){
    if (matchBlockSize==0){
        _create_bsdiff(newData,newData_end,newData_end,oldData,oldData_end,oldData_end,
                       out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,0);
        return;
    }
    TCoversOptimMB<TMatchBlock> coversOp(newData,newData_end,oldData,oldData_end,matchBlockSize);
    _create_bsdiff(newData,coversOp.matchBlock->newData_end_cur,newData_end,
                   oldData,coversOp.matchBlock->oldData_end_cur,oldData_end,
                   out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,&coversOp);   
}
void create_bsdiff_block(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                         const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                         int kMinSingleMatchScore,bool isUseBigCacheMatch,size_t matchBlockSize){
    TAutoMem oldAndNewData;
    loadOldAndNewStream(oldAndNewData,oldData,newData);
    size_t old_size=oldData?(size_t)oldData->streamSize:0;
    unsigned char* pOldData=oldAndNewData.data();
    unsigned char* pNewData=pOldData+old_size;
    create_bsdiff_block(pNewData,pNewData+(size_t)newData->streamSize,pOldData,pOldData+old_size,
                        out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,matchBlockSize);
}

bool get_is_bsdiff(const hpatch_TStreamInput* diffData){
    hpatch_BsDiffInfo diffinfo;
    if (!getBsDiffInfo(&diffinfo,diffData))
        return false;
    return true;
}
bool get_is_bsdiff(const unsigned char* diffData,const unsigned char* diffData_end){
    hdiff_TStreamInput diffStream;
    mem_as_hStreamInput(&diffStream,diffData,diffData_end);
    return get_is_bsdiff(&diffStream);
}

bool check_bsdiff(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                  const hpatch_TStreamInput* diffStream,hpatch_TDecompress* decompressPlugin){
    const size_t kACacheBufSize=hpatch_kFileIOBufBetterSize;
    TAutoMem _cache(kACacheBufSize*(1+5));
    _TCheckOutNewDataStream out_newData(newData,_cache.data(),kACacheBufSize);
    _test_rt(bspatch_with_cache(&out_newData,oldData,diffStream,decompressPlugin,
                                _cache.data()+kACacheBufSize,_cache.data_end()));
    _test_rt(out_newData.isWriteFinish());
    return true;
}
bool check_bsdiff(const unsigned char* newData,const unsigned char* newData_end,
                  const unsigned char* oldData,const unsigned char* oldData_end,
                  const unsigned char* diffData,const unsigned char* diffData_end,
                  hpatch_TDecompress* decompressPlugin){
    hdiff_TStreamInput newStream;
    hdiff_TStreamInput oldStream;
    hdiff_TStreamInput diffStream;
    mem_as_hStreamInput(&newStream,newData,newData_end);
    mem_as_hStreamInput(&oldStream,oldData,oldData_end);
    mem_as_hStreamInput(&diffStream,diffData,diffData_end);
    return check_bsdiff(&newStream,&oldStream,&diffStream,decompressPlugin);
}