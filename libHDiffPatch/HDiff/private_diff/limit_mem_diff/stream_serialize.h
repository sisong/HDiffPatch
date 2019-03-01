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
struct hdiff_TCompress;
namespace hdiff_private{

struct TCompressedStream:public hpatch_TStreamOutput{
    TCompressedStream(const hpatch_TStreamOutput*  _out_code,
                      hpatch_StreamPos_t _writePos,hpatch_StreamPos_t _kLimitOutCodeSize,
                      const hpatch_TStreamInput*   _in_stream);
    inline bool  is_overLimit()const { return _is_overLimit; }
private:
    const hpatch_TStreamOutput*  out_code;
    hpatch_StreamPos_t           out_pos;
    hpatch_StreamPos_t           out_posLimitEnd;
    const hpatch_TStreamInput*   in_stream;
    hpatch_StreamPos_t           _writeToPos_back;
    bool                         _is_overLimit;
    static hpatch_BOOL _write_code(const hpatch_TStreamOutput* stream,hpatch_StreamPos_t writeToPos,
                                   const unsigned char* data,const unsigned char* data_end);
};

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
    enum { kCodeBufSize = 1024*64 };
    
    static hpatch_BOOL _read(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                             unsigned char* out_data,unsigned char* out_data_end);
};

struct TNewDataDiffStream:public hpatch_TStreamInput{
    TNewDataDiffStream(const TCovers& _covers,const hpatch_TStreamInput* _newData,
                       hpatch_StreamPos_t newDataDiff_size);
    static hpatch_StreamPos_t getDataSize(const TCovers& covers,hpatch_StreamPos_t newDataSize);
private:
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

struct TDiffStream{
    explicit TDiffStream(const hpatch_TStreamOutput* _out_diff);
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
    enum{ kBufSize=1024*64 };
    TAutoMem               _temp_mem;
    
    void _packUInt_limit(hpatch_StreamPos_t uValue,size_t limitOutSize);
    
    //stream->read can return currently readed data size,return <0 error
    void _pushStream(const hpatch_TStreamInput* stream);
};


class TStreamClip:public hpatch_TStreamInput{
public:
    explicit TStreamClip(const hpatch_TStreamInput* stream,
                         hpatch_StreamPos_t clipBeginPos,hpatch_StreamPos_t clipEndPos,
                         hpatch_TDecompress* decompressPlugin=0,hpatch_StreamPos_t uncompressSize=0);
    ~TStreamClip();
private:
    const hpatch_TStreamInput*  _src;
    const hpatch_StreamPos_t    _src_begin;
    const hpatch_StreamPos_t    _src_end;
    hpatch_TDecompress*         _decompressPlugin;
    hpatch_StreamPos_t          _read_uncompress_pos;
    hpatch_decompressHandle     _decompressHandle;
    void closeDecompressHandle();
    void openDecompressHandle();
    static hpatch_BOOL _clip_read(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                  unsigned char* out_data,unsigned char* out_data_end);
};

}//namespace hdiff_private
#endif
