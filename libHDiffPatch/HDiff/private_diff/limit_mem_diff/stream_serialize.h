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
    enum { kCodeBufSize = hpatch_kFileIOBufBetterSize };
    
    static hpatch_BOOL _read(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                             unsigned char* out_data,unsigned char* out_data_end);
};

struct TNewDataDiffStream:public hpatch_TStreamInput{
    inline TNewDataDiffStream(const TCovers& _covers,const hpatch_TStreamInput* _newData,
                              hpatch_StreamPos_t newDataDiff_size):covers(_covers) { _init(_newData,newDataDiff_size); }
    inline TNewDataDiffStream(const TCovers& _covers,const hpatch_TStreamInput* _newData)
        :covers(_covers) { _init(_newData,getDataSize(_covers,_newData->streamSize)); }
    static hpatch_StreamPos_t getDataSize(const TCovers& covers,hpatch_StreamPos_t newDataSize);
private:
    void _init(const hpatch_TStreamInput* _newData,hpatch_StreamPos_t newDataDiff_size);
    const TCovers&              covers;
    const hpatch_TStreamInput*  newData;
    hpatch_StreamPos_t          curNewPos;
    hpatch_StreamPos_t          curNewPos_end;
    hpatch_StreamPos_t          lastNewEnd;
    size_t                      readedCoverCount;
    hpatch_StreamPos_t          _readFromPos_back;
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
    enum { kSubDiffCacheSize = hpatch_kFileIOBufBetterSize*4 };
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
    void packUInt(hpatch_StreamPos_t uValue);
    inline TPlaceholder packUInt_pos(hpatch_StreamPos_t uValue){
        hpatch_StreamPos_t pos=writePos;
        packUInt(uValue);
        return TPlaceholder(pos,writePos);
    }
    void packUInt_update(const TPlaceholder& pos,hpatch_StreamPos_t uValue);
    
    void pushStream(const hpatch_TStreamInput*   stream,
                    const hdiff_TCompress*       compressPlugin,
                    const TPlaceholder&          update_compress_sizePos);
    void pushStream(const hpatch_TStreamInput* stream){
                            TPlaceholder nullPos(0,0); pushStream(stream,0,nullPos); }
    hpatch_StreamPos_t getWritedPos()const{ return writePos; }
private:
    const hpatch_TStreamOutput*  out_diff;
    hpatch_StreamPos_t     writePos;
    enum{ kBufSize=hpatch_kFileIOBufBetterSize };
    TAutoMem               _temp_mem;
    
    void _packUInt_limit(hpatch_StreamPos_t uValue,size_t limitOutSize);
    
    //stream->read can return currently readed data size,return <0 error
    void _pushStream(const hpatch_TStreamInput* stream);
};

struct TVectorAsStreamOutput:public hpatch_TStreamOutput{
    explicit TVectorAsStreamOutput(std::vector<unsigned char>& _dst):dst(_dst){
        this->streamImport=this;
        this->streamSize=~(hpatch_StreamPos_t)0;
        this->read_writed=0;
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
    std::vector<unsigned char>& dst;
};

}//namespace hdiff_private
#endif
