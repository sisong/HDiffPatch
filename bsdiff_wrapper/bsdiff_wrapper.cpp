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
#ifdef _MSC_VER 
#   pragma warning (disable : 4146)
#endif
#define _check(value,info) { if (!(value)) { throw std::runtime_error(info); } }
static const char* kBsDiffVersionType="BSDIFF40";
static const char* kEsBsDiffVersionType = "ENDSLEY/BSDIFF43";

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
        explicit TCtrlStream(const TCovers& _covers)
        :curPos(0),curi(0),bufi(0),covers(_covers){
            streamImport=this;
            read=_read;
            streamSize=(covers.coverCount()-1)*(hpatch_StreamPos_t)(3*8);
            buf.reserve(hdiff_kFileIOBufBestSize);
        }
    private:
        hpatch_StreamPos_t curPos;
        size_t curi;
        size_t bufi;
        const TCovers& covers;
        std::vector<unsigned char> buf;
        void _updateBuf(){
            buf.clear();
            bufi=0;
            while ((curi+1<covers.coverCount())&&((buf.size()+3*8)<=hdiff_kFileIOBufBestSize)){
                TCover c,cnext;
                covers.covers(curi++,&c);
                covers.covers(curi,&cnext);
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


    struct TInterlaceStream:public hpatch_TStreamInput{
        explicit TInterlaceStream(const TCovers& _covers,TCtrlStream& _ctrlStream,
                                  TNewDataSubDiffStream& _subStream,TNewDataDiffStream& _diffStream)
        :curPos(0),curi(0),covers(_covers),
        curCtrlPos(0),curSubPos(0),curDiffPos(0),curCtrlLen(0),curSubLen(0),curDiffLen(0),
        ctrlStream(_ctrlStream),subStream(_subStream),diffStream(_diffStream){
            streamImport=this;
            read=_read;
            streamSize=ctrlStream.streamSize+subStream.streamSize+diffStream.streamSize;
        }
    private:
        hpatch_StreamPos_t  curPos;
        size_t              curi;
        const TCovers&      covers;
        hpatch_StreamPos_t curCtrlPos;
        hpatch_StreamPos_t curSubPos;
        hpatch_StreamPos_t curDiffPos;
        size_t             curCtrlLen;
        size_t             curSubLen;
        size_t             curDiffLen;
        TCtrlStream&            ctrlStream;
        TNewDataSubDiffStream&  subStream;
        TNewDataDiffStream&     diffStream;
        void _update(){
            if (curi+1<covers.coverCount()){
                TCover c,cnext;
                covers.covers(curi++,&c);
                covers.covers(curi,&cnext);
                curCtrlLen=3*8;
                curDiffLen=cnext.newPos-(c.newPos+c.length);
                curSubLen=c.length;
            }
        }
        hpatch_BOOL readTo(hpatch_StreamPos_t readFromPos,unsigned char* out_data,unsigned char* out_data_end){
            if (readFromPos!=curPos) 
                return hpatch_FALSE;
            size_t readLen=out_data_end-out_data;
            curPos+=readLen;
            while (readLen>0){
                if (curCtrlLen+curSubLen+curDiffLen==0){
                    _update();
                    _check(curCtrlLen+curSubLen+curDiffLen>0,"TInterlaceStream _update");
                }
                
                hpatch_StreamPos_t*  dataPos;
                size_t*              dataLen;
                hpatch_TStreamInput* dataStream;
                if (curCtrlLen>0){
                    dataPos=&curCtrlPos;
                    dataLen=&curCtrlLen;
                    dataStream=&ctrlStream;
                }else if (curSubLen>0){
                    dataPos=&curSubPos;
                    dataLen=&curSubLen;
                    dataStream=&subStream;
                }else{
                    assert(curDiffLen>0);
                    dataPos=&curDiffPos;
                    dataLen=&curDiffLen;
                    dataStream=&diffStream;
                }

                size_t cLen=(*dataLen);
                if (cLen>readLen) cLen=readLen;
                _check(dataStream->read(dataStream,*dataPos,out_data,out_data+cLen),"TInterlaceStream dataStream->read")
                out_data+=cLen;
                readLen-=cLen;
                (*dataPos)+=cLen;
                (*dataLen)-=cLen;
            }
            return hpatch_TRUE;
        }
        static hpatch_BOOL _read(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                 unsigned char* out_data,unsigned char* out_data_end){
            TInterlaceStream* self=(TInterlaceStream*)stream->streamImport;
            return self->readTo(readFromPos,out_data,out_data_end);
        }
    };

static void serialize_bsdiff(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                             const TCovers& covers,const hpatch_TStreamOutput* out_diff,
                             const hdiff_TCompress* compressPlugin,bool isEndsleyBsdiff,bool isZeroSubDiff=false){
    std::vector<unsigned char> buf;
    TDiffStream outDiff(out_diff);
    size_t ctrlDataSize_pos;
    {//head
        buf.clear();
        pushCStr(buf,isEndsleyBsdiff?kEsBsDiffVersionType:kBsDiffVersionType);
        ctrlDataSize_pos=buf.size();
        if (!isEndsleyBsdiff){
            pushUInt64(buf,0); //ctrlDataSize
            pushUInt64(buf,0); //subDataSize
        }
        pushUInt64(buf,newData->streamSize);
        outDiff.pushBack(buf.data(),buf.size());
    }

    if (isEndsleyBsdiff){ 
        // endsley/bsdiff
        TCtrlStream             ctrlStream(covers);
        TNewDataSubDiffStream   subStream(newData,oldData,covers,true,isZeroSubDiff);
        TNewDataDiffStream      diffStream(covers,newData);

        TInterlaceStream interlaceStream(covers,ctrlStream,subStream,diffStream);
        outDiff.pushStream(&interlaceStream,compressPlugin,true);
        return;
    }
    //else bsdiff4
    
    {//ctrl data
        TCtrlStream ctrlStream(covers);
        hpatch_StreamPos_t ctrlDataSize=outDiff.pushStream(&ctrlStream,compressPlugin,true);

        buf.clear();
        pushUInt64(buf,ctrlDataSize);//update ctrlDataSize
        _check(out_diff->write(out_diff,ctrlDataSize_pos,buf.data(),
                               buf.data()+buf.size()),"serialize_bsdiff() out_diff->write");
    }

    {//sub data
        TNewDataSubDiffStream subStream(newData,oldData,covers,true,isZeroSubDiff);
        hpatch_StreamPos_t subDataSize=outDiff.pushStream(&subStream,compressPlugin,true);

        size_t subDataSize_pos=ctrlDataSize_pos+8;
        buf.clear();
        pushUInt64(buf,subDataSize);//update subDataSize
        _check(out_diff->write(out_diff,subDataSize_pos,buf.data(),
                               buf.data()+buf.size()),"serialize_bsdiff() out_diff->write");
    }

    {//copy data
        TNewDataDiffStream diffStream(covers,newData);
        outDiff.pushStream(&diffStream,compressPlugin,true);
    }
}

template<class _TCover,class _TSize>
static void _to_bsdiff_covers(std::vector<_TCover>& covers,_TSize newSize){
    if (covers.empty()||(covers[0].newPos!=0)||(covers[0].oldPos!=0)){//begin cover
        _TCover lc;
        lc.newPos=0;
        lc.oldPos=0;
        lc.length=0;
        covers.insert(covers.begin(),lc);
    }
    {//last cover
        _TSize lastOldPos=0;
        if (!covers.empty()){
            const _TCover& lc=covers[covers.size()-1];
            lastOldPos=lc.oldPos+lc.length;
        }
        _TCover lc;
        lc.newPos=newSize;
        lc.oldPos=lastOldPos;
        lc.length=0;
        covers.push_back(lc);
    }
}

void _create_bsdiff(const unsigned char* newData,const unsigned char* cur_newData_end,const unsigned char* newData_end,
                    const unsigned char* oldData,const unsigned char* cur_oldData_end,const unsigned char* oldData_end,
                    const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                    bool isEndsleyBsdiff,int kMinSingleMatchScore,bool isUseBigCacheMatch,
                    ICoverLinesListener* coverLinesListener,size_t threadNum){
    std::vector<hpatch_TCover_sz> covers;
    get_match_covers_by_sstring(newData,cur_newData_end,oldData,cur_oldData_end,covers,
                                kMinSingleMatchScore,isUseBigCacheMatch,coverLinesListener,threadNum);

    _to_bsdiff_covers(covers,(size_t)(newData_end-newData));
    const TCovers _covers((void*)covers.data(),covers.size(),
                          sizeof(*covers.data())==sizeof(hpatch_TCover32));
    
    hdiff_TStreamInput newStream;
    hdiff_TStreamInput oldStream;
    mem_as_hStreamInput(&newStream,newData,newData_end);
    mem_as_hStreamInput(&oldStream,oldData,oldData_end);
    
    serialize_bsdiff(&newStream,&oldStream,_covers,out_diff,compressPlugin,isEndsleyBsdiff);
}

}//end namespace hdiff_private

using namespace hdiff_private;

void create_bsdiff(const unsigned char* newData,const unsigned char* newData_end,
                   const unsigned char* oldData,const unsigned char* oldData_end,
                   const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                   bool isEndsleyBsdiff,int kMinSingleMatchScore,bool isUseBigCacheMatch,
                   ICoverLinesListener* coverLinesListener,size_t threadNum){
    _create_bsdiff(newData,newData_end,newData_end,oldData,oldData_end,oldData_end,
                   out_diff,compressPlugin,isEndsleyBsdiff,kMinSingleMatchScore,isUseBigCacheMatch,
                   coverLinesListener,threadNum);
}
void create_bsdiff(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                   const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                   bool isEndsleyBsdiff,int kMinSingleMatchScore,bool isUseBigCacheMatch,
                   ICoverLinesListener* coverLinesListener,size_t threadNum){
    TAutoMem oldAndNewData;
    loadOldAndNewStream(oldAndNewData,oldData,newData);
    size_t old_size=oldData?(size_t)oldData->streamSize:0;
    unsigned char* pOldData=oldAndNewData.data();
    unsigned char* pNewData=pOldData+old_size;
    unsigned char* pNewDataEnd=pNewData+(size_t)newData->streamSize;
    _create_bsdiff(pNewData,pNewDataEnd,pNewDataEnd,pOldData,pOldData+old_size,pOldData+old_size,
                   out_diff,compressPlugin,isEndsleyBsdiff,kMinSingleMatchScore,isUseBigCacheMatch,
                   coverLinesListener,threadNum);
}

void create_bsdiff_stream(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                          const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                          bool isEndsleyBsdiff,size_t kMatchBlockSize,const hdiff_TMTSets_s* mtsets){
    TCoversBuf covers(newData->streamSize,oldData->streamSize);
    get_match_covers_by_block(newData,oldData,&covers,kMatchBlockSize,mtsets);
    if (covers._isCover32)
        _to_bsdiff_covers(covers.m_covers_limit,(hpatch_uint32_t)newData->streamSize);
    else
        _to_bsdiff_covers(covers.m_covers_larger,newData->streamSize);
    covers.update();
    serialize_bsdiff(newData,oldData,covers,out_diff,compressPlugin,isEndsleyBsdiff,true);
}


void create_bsdiff_block(unsigned char* newData,unsigned char* newData_end,
                         unsigned char* oldData,unsigned char* oldData_end,
                         const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                         bool isEndsleyBsdiff,int kMinSingleMatchScore,bool isUseBigCacheMatch,
                         size_t matchBlockSize,size_t threadNum){
    if (matchBlockSize==0){
        _create_bsdiff(newData,newData_end,newData_end,oldData,oldData_end,oldData_end,
                       out_diff,compressPlugin,isEndsleyBsdiff,kMinSingleMatchScore,isUseBigCacheMatch,0,threadNum);
        return;
    }
    TCoversOptimMB<TMatchBlock> coversOp(newData,newData_end,oldData,oldData_end,matchBlockSize,threadNum);
    _create_bsdiff(newData,coversOp.matchBlock->newData_end_cur,newData_end,
                   oldData,coversOp.matchBlock->oldData_end_cur,oldData_end,
                   out_diff,compressPlugin,isEndsleyBsdiff,kMinSingleMatchScore,isUseBigCacheMatch,&coversOp,threadNum);   
}
void create_bsdiff_block(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                         const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                         bool isEndsleyBsdiff,int kMinSingleMatchScore,bool isUseBigCacheMatch,
                         size_t matchBlockSize,size_t threadNum){
    TAutoMem oldAndNewData;
    loadOldAndNewStream(oldAndNewData,oldData,newData);
    size_t old_size=oldData?(size_t)oldData->streamSize:0;
    unsigned char* pOldData=oldAndNewData.data();
    unsigned char* pNewData=pOldData+old_size;
    create_bsdiff_block(pNewData,pNewData+(size_t)newData->streamSize,pOldData,pOldData+old_size,
                        out_diff,compressPlugin,isEndsleyBsdiff,kMinSingleMatchScore,isUseBigCacheMatch,
                        matchBlockSize,threadNum);
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