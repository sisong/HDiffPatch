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

struct TPlaceholder{
    //TODO:
};

struct TCompressedStreamInput:public hpatch_TStreamInput{
    explicit TCompressedStreamInput(const hpatch_TStreamInput* _stream,
                                    hdiff_TStreamCompress* _compressPlugin)
    :stream(_stream),compressPlugin(_compressPlugin){
        //TODO:
    }
private:
    const hpatch_TStreamInput* stream;
    hdiff_TStreamCompress* compressPlugin;
};

struct TCoversStream:public hpatch_TStreamInput{
    //TODO:
};

struct TNewDataDiffStream:public hpatch_TStreamInput{
    //TODO:
};

struct TDiffStream{
    explicit TDiffStream(hpatch_TStreamOutput* _out_diff,const TCovers& _covers)
    :out_diff(_out_diff),covers(_covers),writePos(0){ }
    void pushBack(const unsigned char* src,size_t n){
        //TODO:
    }
    TPlaceholder packUInt(hpatch_StreamPos_t uValue){
        //TODO:
        return TPlaceholder();
    }
    void packUInt_update(const TPlaceholder& pos,hpatch_StreamPos_t uValue){
        //TODO:
    }
    
    hpatch_StreamPos_t getDataSize(hpatch_StreamPos_t newDataSize,
                                   hpatch_StreamPos_t* out_cover_buf_size,
                                   hpatch_StreamPos_t* out_newDataDiff_size){
        //TODO:
        return 0;
    }
    
    
    const hpatch_TStreamInput& getCoverStream(hpatch_StreamPos_t cover_buf_size){
        //todo:
        return coversStream;
    }
    const hpatch_TStreamInput& getNewDataDiffStream(const hpatch_TStreamInput* newData,
                                                    hpatch_StreamPos_t newDataDiff_size){
        //todo:
        return newDataDiffStream;
    }
    
    hpatch_StreamPos_t pushStream(const hpatch_TStreamInput* stream,hpatch_StreamPos_t kLimitReadedSize){
        //TODO:
        return 0;
    }
private:
    hpatch_TStreamOutput*  out_diff;
    const  TCovers&        covers;
    hpatch_StreamPos_t     writePos;
    TCoversStream          coversStream;
    TNewDataDiffStream     newDataDiffStream;
};

#endif
