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
class hdiff_TStreamCompress;

struct TCompressedStreamInput:public hpatch_TStreamInput{
    TCompressedStreamInput(const hpatch_TStreamInput* _stream,
                           hdiff_TStreamCompress* _compressPlugin);
private:
    const hpatch_TStreamInput*  data_stream;
    hdiff_TStreamCompress*      compressPlugin;
    void*                       compresser;
    hpatch_TStreamOutput        code_stream;
    std::vector<unsigned char>  _data_buf;
    std::vector<unsigned char>  _code_buf;
    hpatch_StreamPos_t          curReadPos;
    size_t                      curCodePos;
    enum { kReadBufSize = 1024*16 };
    
    static long _read(hpatch_TStreamInputHandle streamHandle,
                      const hpatch_StreamPos_t readFromPos,
                      unsigned char* out_data,unsigned char* out_data_end);
    static long _write_code(hpatch_TStreamOutputHandle streamHandle,
                            const hpatch_StreamPos_t writeToPos,
                            const unsigned char* code,const unsigned char* code_end);
};

struct TCoversStream:public hpatch_TStreamInput{
    TCoversStream(const TCovers& _covers,hpatch_StreamPos_t cover_buf_size);
    static hpatch_StreamPos_t getDataSize(const TCovers& covers);
private:
    const TCovers&              covers;
    std::vector<unsigned char>  _code_buf;
    size_t                      curCodePos;
    size_t                      curCodePos_end;
    size_t                      readedCoverCount;
    hpatch_StreamPos_t          lastOldEnd;
    hpatch_StreamPos_t          lastNewEnd;
    enum { kCodeBufSize = 1024*16 };
    
    static long _read(hpatch_TStreamInputHandle streamHandle,
                      const hpatch_StreamPos_t readFromPos,
                      unsigned char* out_data,unsigned char* out_data_end);
};

struct TNewDataDiffStream:public hpatch_TStreamInput{
    TNewDataDiffStream(const TCovers& _covers,const hpatch_TStreamInput* _newData,
                       hpatch_StreamPos_t newDataDiff_size)
    :covers(_covers),newData(_newData),
    curNewPos(0),curNewPos_end(0),lastNewEnd(0),readedCoverCount(0){
        this->streamHandle=this;
        this->streamSize=newDataDiff_size;
        this->read=_read;
    }
    static hpatch_StreamPos_t getDataSize(const TCovers& covers,hpatch_StreamPos_t newDataSize);
private:
    const hpatch_TStreamInput*  newData;
    const TCovers&              covers;
    hpatch_StreamPos_t          curNewPos;
    hpatch_StreamPos_t          curNewPos_end;
    hpatch_StreamPos_t          lastNewEnd;
    size_t                      readedCoverCount;
    static long _read(hpatch_TStreamInputHandle streamHandle,
                      const hpatch_StreamPos_t readFromPos,
                      unsigned char* out_data,unsigned char* out_data_end);
};

struct TPlaceholder{
    hpatch_StreamPos_t pos;
    hpatch_StreamPos_t pos_end;
    inline TPlaceholder(hpatch_StreamPos_t _pos,hpatch_StreamPos_t _pos_end)
        :pos(_pos),pos_end(_pos_end){ }
};

struct TDiffStream{
    explicit TDiffStream(hpatch_TStreamOutput* _out_diff,const TCovers& _covers)
    :out_diff(_out_diff),covers(_covers),writePos(0){ }
    
    void pushBack(const unsigned char* src,size_t n);
    void packUInt(hpatch_StreamPos_t uValue);
    inline TPlaceholder packUInt_pos(hpatch_StreamPos_t uValue){
        hpatch_StreamPos_t pos=writePos;
        packUInt(uValue);
        return TPlaceholder(pos,writePos);
    }
    void packUInt_update(const TPlaceholder& pos,hpatch_StreamPos_t uValue);
    
    void pushStream(const hpatch_TStreamInput*  stream,
                    hpatch_StreamPos_t          kLimitReadedSize,
                    hdiff_TStreamCompress*      compressPlugin,
                    const TPlaceholder&         update_compress_sizePos);
private:
    hpatch_TStreamOutput*  out_diff;
    const  TCovers&        covers;
    hpatch_StreamPos_t     writePos;
    std::vector<unsigned char> _temp_buf;
    
    void _packUInt_limit(hpatch_StreamPos_t uValue,size_t limitOutSize);
    
    //stream->read can return currently readed data size,return <0 error
    hpatch_StreamPos_t _pushStream(const hpatch_TStreamInput* stream,
                                   hpatch_StreamPos_t kLimitReadedSize);
};

#endif
