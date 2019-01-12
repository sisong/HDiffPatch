//  new_stream.h
//  dir patch
/*
 The MIT License (MIT)
 Copyright (c) 2018-2019 HouSisong
 
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
#ifndef DirPatch_new_stream_h
#define DirPatch_new_stream_h
#include "../../HDiffPatch/libHDiffPatch/HPatch/patch_types.h"

typedef struct INewStreamListener{
    void*         listenerImport;
    hpatch_BOOL (*outNewDir)   (struct INewStreamListener* listener,size_t newPathIndex);
    hpatch_BOOL (*copySameFile)(struct INewStreamListener* listener,size_t oldPathIndex,size_t newPathIndex);
    hpatch_BOOL (*openNewFile) (struct INewStreamListener* listener,size_t newPathIndex,
                                const hpatch_TStreamOutput** out_newFileStream);
    hpatch_BOOL (*closeNewFile)(struct INewStreamListener* listener,const hpatch_TStreamOutput* newFileStream);
} INewStreamListener;

//对外模拟成一个输出流;
//利用samePairList、newRefList等生成新文件和copy文件
typedef struct TNewStream{
    const hpatch_TStreamOutput* stream;
    hpatch_BOOL             isFinish;
//private:
    size_t                  _pathCount;
    const size_t*           _newRefList;
    size_t                  _newRefCount;
    const size_t*           _samePairList;
    size_t                  _samePairCount;
    hpatch_TStreamOutput    _stream;
    
    size_t                  _curPathIndex;
    size_t                  _curNewRefIndex;
    size_t                  _curSamePairIndex;
    hpatch_StreamPos_t      _curWriteToPos;
    hpatch_StreamPos_t      _curWriteToPosEnd;
    const hpatch_TStreamOutput* _curNewFile;
    INewStreamListener*     _listener;
} TNewStream;

hpatch_inline static
void        TNewStream_init(TNewStream* self) { memset(self,0,sizeof(*self)); }
hpatch_BOOL TNewStream_open(TNewStream* self,INewStreamListener* listener,
                            hpatch_StreamPos_t newRefDataSize,size_t newPathCount,
                            const size_t* newRefList,size_t newRefCount,
                            const size_t* samePairList,size_t samePairCount);
hpatch_BOOL TNewStream_close(TNewStream* self);

#endif
