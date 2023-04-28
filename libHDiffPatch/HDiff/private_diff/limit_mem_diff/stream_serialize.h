//stream_serialize.h
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
#ifndef stream_serialize_h
#define stream_serialize_h
#include "covers.h"
#include "../pack_uint.h" //for packUInt_fixSize
#include "../mem_buf.h"
#include "../bytes_rle.h"

struct hdiff_TCompress;
namespace hdiff_private{

struct TCoversStream:public hpatch_TStreamInput{
    TCoversStream(const TCovers& _covers,hpatch_StreamPos_t cover_buf_size);
    ~TCoversStream();
    static hpatch_StreamPos_t getDataSize(const TCovers& covers);
private:
    const TCovers&              covers;
    TAutoMem                    _code_mem;
    size_t                      curCodePos;
    size_t                      curCodePos_end;
    size_t                      readedCoverCount;
    hpatch_StreamPos_t          lastOldEnd;
    hpatch_StreamPos_t          lastNewEnd;
    hpatch_StreamPos_t          _readFromPos_back;
    enum { kCodeBufSize = hdiff_kFileIOBufBestSize };
    
    static hpatch_BOOL _read(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                             unsigned char* out_data,unsigned char* out_data_end);
};


struct TNewDataSubDiffStream:public hpatch_TStreamInput{
    TNewDataSubDiffStream(const hdiff_TStreamInput* _newData,const hdiff_TStreamInput* _oldData,
                          const TCovers& _covers,bool _isOnlySubCover=false,bool _isZeroSubDiff=false);
    inline ~TNewDataSubDiffStream(){ assert(curReadPos==streamSize); }
private:
    hpatch_StreamPos_t curReadNewPos;
    hpatch_StreamPos_t curReadOldPos;
    hpatch_StreamPos_t curReadPos;
    hpatch_StreamPos_t curDataLen;
    size_t nextCoveri;
    const hdiff_TStreamInput* newData;
    const hdiff_TStreamInput* oldData;
    const TCovers& covers;
    const bool isOnlySubCover;
    const bool isZeroSubDiff;
    TAutoMem _cache;
    void initRead();
    void readTo(unsigned char* out_data,unsigned char* out_data_end);
    static hpatch_BOOL _read(const struct hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                             unsigned char* out_data,unsigned char* out_data_end);
};

struct TNewDataSubDiffStream_mem:public TNewDataSubDiffStream{
    TNewDataSubDiffStream_mem(const unsigned char* newData,const unsigned char* newData_end,
                              const unsigned char* oldData,const unsigned char* oldData_end,
                              const TCovers& _covers,bool _isOnlySubCover=false,bool _isZeroSubDiff=false);
private:
    hdiff_TStreamInput mem_newData;
    hdiff_TStreamInput mem_oldData;
};

struct TNewDataDiffStream:public hpatch_TStreamInput{
    inline TNewDataDiffStream(const TCovers& _covers,const hpatch_TStreamInput* _newData,
                              hpatch_StreamPos_t newDataDiff_size)
            :covers(_covers),newData(_newData) { _init(newDataDiff_size); }
    inline TNewDataDiffStream(const TCovers& _covers,const hpatch_TStreamInput* _newData)
        :covers(_covers),newData(_newData) { _init(getDataSize(_covers,_newData->streamSize)); }
    inline TNewDataDiffStream(const TCovers& _covers,const hpatch_TStreamInput* _newData,
                              size_t coveri,hpatch_StreamPos_t newDataPos,hpatch_StreamPos_t newDataPosEnd)
        :covers(_covers),newData(_newData){ _initByRange(coveri,newDataPos,newDataPosEnd); }
    static hpatch_StreamPos_t getDataSize(const TCovers& covers,hpatch_StreamPos_t newDataSize);
private:
    static hpatch_StreamPos_t getDataSizeByRange(const TCovers& covers,size_t coveri,
                                                 hpatch_StreamPos_t newDataPos,hpatch_StreamPos_t newDataPosEnd);
    void _init(hpatch_StreamPos_t newDataDiff_size);
    void _initByRange(size_t coveri,hpatch_StreamPos_t newDataPos,hpatch_StreamPos_t newDataPosEnd);
    const TCovers&              covers;
    const hpatch_TStreamInput*  newData;
    hpatch_StreamPos_t          curNewPos;
    hpatch_StreamPos_t          curNewPos_end;
    hpatch_StreamPos_t          lastNewEnd;
    size_t                      readedCoverCount;
    hpatch_StreamPos_t          _readFromPos_back;

    //by range
    size_t                      _coveri;
    hpatch_StreamPos_t          _newDataPos;

    static hpatch_BOOL _read(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                             unsigned char* out_data,unsigned char* out_data_end);
};

struct TNewDataSubDiffCoverStream:public hpatch_TStreamInput{
    TNewDataSubDiffCoverStream(const hpatch_TStreamInput* _newStream,
                               const hpatch_TStreamInput* _oldStream,bool _isZeroSubDiff);
    void resetCover(const TCover& _cover);
    void resetCoverLen(hpatch_StreamPos_t coverLen);
    const bool isZeroSubDiff;
private:
    hpatch_StreamPos_t inStreamLen;
    size_t curDataLen;
    const hpatch_TStreamInput* newStream;
    const hpatch_TStreamInput* oldStream;
    TCover cover;
    unsigned char* newData;
    unsigned char* oldData;
    TAutoMem _cache;
    void initRead();
    hpatch_BOOL _updateCache(hpatch_StreamPos_t readFromPos);
    hpatch_BOOL readTo(hpatch_StreamPos_t readFromPos,unsigned char* out_data,unsigned char* out_data_end);
    static hpatch_BOOL _read(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                             unsigned char* out_data,unsigned char* out_data_end);
};

class TStreamClip:public hpatch_TStreamInput{
public:
    inline explicit TStreamClip():_decompressHandle(0){ clear(); }
    inline TStreamClip(const hpatch_TStreamInput* stream,
                         hpatch_StreamPos_t clipBeginPos,hpatch_StreamPos_t clipEndPos,
                         hpatch_TDecompress* decompressPlugin=0,hpatch_StreamPos_t uncompressSize=0)
                         :_decompressHandle(0){ reset(stream,clipBeginPos,clipEndPos,decompressPlugin,uncompressSize); }
    void reset(const hpatch_TStreamInput* stream,
               hpatch_StreamPos_t clipBeginPos,hpatch_StreamPos_t clipEndPos,
               hpatch_TDecompress* decompressPlugin=0,hpatch_StreamPos_t uncompressSize=0);
    inline ~TStreamClip() { clear(); }
    inline void clear() { closeDecompressHandle(); streamSize=0; }
private:
    const hpatch_TStreamInput*  _src;
    hpatch_StreamPos_t          _src_begin;
    hpatch_StreamPos_t          _src_end;
    hpatch_TDecompress*         _decompressPlugin;
    hpatch_decompressHandle     _decompressHandle;
    hpatch_StreamPos_t          _read_uncompress_pos;
    void closeDecompressHandle();
    void openDecompressHandle();
    static hpatch_BOOL _clip_read(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                  unsigned char* out_data,unsigned char* out_data_end);
};

struct TStepStream:public hpatch_TStreamInput{
    TStepStream(const hpatch_TStreamInput* newStream,const hpatch_TStreamInput* oldStream,
                bool isZeroSubDiff,const TCovers& covers,size_t patchStepMemSize);
    inline size_t getCoverCount()const{ return endCoverCount; }
    inline size_t getMaxStepMemSize()const{ return endMaxStepMemSize; }
private:
    TNewDataSubDiffCoverStream  subDiff;
    TNewDataDiffStream          newDataDiff;
    const TCovers&  covers;
    const size_t    patchStepMemSize;
    const hpatch_StreamPos_t newDataSize;
    size_t  curCoverCount;
    size_t  endCoverCount;
    size_t  curMaxStepMemSize;
    size_t  endMaxStepMemSize;
    hpatch_StreamPos_t lastOldEnd;
    hpatch_StreamPos_t lastNewEnd;
    std::vector<unsigned char> step_bufCover;
    TSingleStreamRLE0  step_bufRle;
    hpatch_StreamPos_t step_dataDiffSize;
    hpatch_StreamPos_t newDataDiffReadPos;
    bool    isHaveLastCover;
    bool    isHaveLeftCover;
    bool    isHaveRightCover;
    TCover  lastCover;
    TCover  leftCover;
    TCover  rightCover;
    size_t  cur_i;
    TCover  cover;
    const TCover* pCurCover;
    bool    isInInit;
    hpatch_StreamPos_t  sumBufSize_forInit;
    size_t              step_bufCover_size;
    std::vector<unsigned char> step_buf;
    TStreamClip                step_dataDiff;
    hpatch_StreamPos_t readFromPosBack;
    hpatch_StreamPos_t         readBufPos;
    void initStream();
    void beginStep();
    bool doStep();
    void _last_flush_step();
    void _flush_step_code();
    hpatch_BOOL readTo(unsigned char* out_data,unsigned char* out_data_end);
    static hpatch_BOOL _read(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                             unsigned char* out_data,unsigned char* out_data_end);
};


struct TDiffStream{
    explicit TDiffStream(const hpatch_TStreamOutput* _out_diff,
                         hpatch_StreamPos_t out_diff_curPos=0);
    ~TDiffStream();
    
    void pushBack(const unsigned char* src,size_t n);
    size_t packUInt(hpatch_StreamPos_t uValue);
    inline TPlaceholder packUInt_pos(hpatch_StreamPos_t uValue){
        hpatch_StreamPos_t pos=writePos;
        packUInt(uValue);
        return TPlaceholder(pos,writePos);
    }
    void packUInt_update(const TPlaceholder& pos,hpatch_StreamPos_t uValue);
    
    hpatch_StreamPos_t pushStream(const hpatch_TStreamInput* stream,
                                  const hdiff_TCompress*     compressPlugin,
                                  const TPlaceholder&        update_compress_sizePos,
                                  bool isMustCompress=false,const hpatch_StreamPos_t cancelSizeOnCancelCompress=0);
    hpatch_StreamPos_t pushStream(const hpatch_TStreamInput* stream,
                                  const hdiff_TCompress*     compressPlugin,
                                  bool isMustCompress=false,const hpatch_StreamPos_t cancelSizeOnCancelCompress=0){
                TPlaceholder nullPos(0,0); return pushStream(stream,compressPlugin,nullPos,
                                                             isMustCompress,cancelSizeOnCancelCompress); }
    hpatch_StreamPos_t pushStream(const hpatch_TStreamInput* stream){
                _pushStream(stream); return stream->streamSize; }
    hpatch_StreamPos_t getWritedPos()const{ return writePos; }
    void stream_read(const TPlaceholder& pos,hpatch_byte* out_data);
    void stream_update(const TPlaceholder& pos,const hpatch_byte* in_data);
private:
    const hpatch_TStreamOutput*  out_diff;
    hpatch_StreamPos_t     writePos;
    enum{ kBufSize=hdiff_kFileIOBufBestSize };
    TAutoMem               _temp_mem;
    
    void _packUInt_limit(hpatch_StreamPos_t uValue,size_t limitOutSize);
    
    //stream->read can return currently readed data size,return <0 error
    void _pushStream(const hpatch_TStreamInput* stream);
};

struct TVectorAsStreamOutput:public hpatch_TStreamOutput{
    explicit TVectorAsStreamOutput(std::vector<unsigned char>& _dst):dst(_dst){
        this->streamImport=this;
        this->streamSize=hpatch_kNullStreamPos;
        this->read_writed=_read;
        this->write=_write;
    }
    static hpatch_BOOL _write(const hpatch_TStreamOutput* stream,
                            const hpatch_StreamPos_t writeToPos,
                            const unsigned char* data,const unsigned char* data_end){
        TVectorAsStreamOutput* self=(TVectorAsStreamOutput*)stream->streamImport;
        std::vector<unsigned char>& dst=self->dst;
        size_t writeLen=(size_t)(data_end-data);
        if (writeToPos>dst.size()) return hpatch_FALSE;
        if  (dst.size()==writeToPos){
            dst.insert(dst.end(),data,data_end);
        }else{
            if (writeToPos+writeLen!=(size_t)(writeToPos+writeLen)) return hpatch_FALSE;
            if (dst.size()<writeToPos+writeLen)
                dst.resize((size_t)(writeToPos+writeLen));
            memcpy(&dst[(size_t)writeToPos],data,writeLen);
        }
        return hpatch_TRUE;
    }
    static hpatch_BOOL _read(const struct hpatch_TStreamOutput* stream,hpatch_StreamPos_t readFromPos,
                             unsigned char* out_data,unsigned char* out_data_end){
        const TVectorAsStreamOutput* self=(const TVectorAsStreamOutput*)stream->streamImport;
        const std::vector<unsigned char>& src=self->dst;
        size_t readLen=out_data_end-out_data;
        if (readFromPos+readLen>src.size()) return hpatch_FALSE;
        memcpy(out_data,src.data()+(size_t)readFromPos,readLen);
        return hpatch_TRUE;
    }
    std::vector<unsigned char>& dst;
};


#define _test_rt(value) { if (!(value)) { LOG_ERR("check "#value" error!\n");  return hpatch_FALSE; } }

struct _TCheckOutNewDataStream:public hpatch_TStreamOutput{
    _TCheckOutNewDataStream(const hpatch_TStreamInput* _newData,unsigned char* _buf,size_t _bufSize);
    bool isWriteFinish()const{ return writedLen==newData->streamSize; }
private:
    const hpatch_TStreamInput*  newData;
    hpatch_StreamPos_t          writedLen;
    unsigned char*                      buf;
    size_t                      bufSize;
    static hpatch_BOOL _read_writed(const struct hpatch_TStreamOutput* stream,hpatch_StreamPos_t readFromPos,
                                    unsigned char* out_data,unsigned char* out_data_end);
    static hpatch_BOOL _write_check(const hpatch_TStreamOutput* stream,hpatch_StreamPos_t writeToPos,
                                    const unsigned char* data,const unsigned char* data_end);
};


void do_compress(std::vector<unsigned char>& out_code,const hpatch_TStreamInput* data,
                 const hdiff_TCompress* compressPlugin,bool isMustCompress=false);
static inline void do_compress(std::vector<unsigned char>& out_code,const std::vector<unsigned char>& data,
                               const hdiff_TCompress* compressPlugin,bool isMustCompress=false){
    hpatch_TStreamInput dataStream;
    mem_as_hStreamInput(&dataStream,data.data(),data.data()+data.size());
    do_compress(out_code,&dataStream,compressPlugin,isMustCompress);
}

}//namespace hdiff_private
#endif
