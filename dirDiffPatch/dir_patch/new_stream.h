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
#include "dir_patch_types.h"
#if (_IS_NEED_DIR_DIFF_PATCH)

typedef struct hpatch_INewStreamListener{
    void*       listenerImport;
    hpatch_BOOL (*makeNewDirOrEmptyFile)(struct hpatch_INewStreamListener* listener,size_t newPathIndex);
    hpatch_BOOL (*copySameFile)(struct hpatch_INewStreamListener* listener,size_t newPathIndex,size_t oldPathIndex);
    hpatch_BOOL  (*openNewFile)(struct hpatch_INewStreamListener* listener,size_t newRefIndex,
                                const hpatch_TStreamOutput** out_newFileStream);
    hpatch_BOOL (*closeNewFile)(struct hpatch_INewStreamListener* listener,const hpatch_TStreamOutput* newFileStream);
    void    (*writedNewRefData)(struct hpatch_INewStreamListener* listener,const unsigned char* data,
                                const unsigned char* dataEnd);
    hpatch_BOOL (*writedFinish)(struct hpatch_INewStreamListener* listener);
} hpatch_INewStreamListener;

typedef struct hpatch_TSameFilePair{
    size_t  newIndex;
    size_t  oldIndex;
} hpatch_TSameFilePair;

//对外模拟成一个输出流;
//利用samePairList、newRefList等生成新文件和copy文件
typedef struct hpatch_TNewStream{
    const hpatch_TStreamOutput* stream;
    hpatch_BOOL             isFinish;
//private:
    size_t                  _pathCount;
    const size_t*           _newRefList;
    size_t                  _newRefCount;
    const hpatch_TSameFilePair* _samePairList;
    size_t                  _samePairCount;
    hpatch_TStreamOutput    _stream;
    
    size_t                  _curPathIndex;
    size_t                  _curNewRefIndex;
    size_t                  _curSamePairIndex;
    hpatch_StreamPos_t      _curWriteToPos;
    hpatch_StreamPos_t      _curWriteToPosEnd;
    const hpatch_TStreamOutput* _curNewFile;
    hpatch_INewStreamListener*  _listener;
} hpatch_TNewStream;

hpatch_inline static
void        hpatch_TNewStream_init(hpatch_TNewStream* self) { memset(self,0,sizeof(*self)); }
hpatch_BOOL hpatch_TNewStream_open(hpatch_TNewStream* self,hpatch_INewStreamListener* listener,
                                   hpatch_StreamPos_t newRefDataSize,size_t newPathCount,
                                   const size_t* newRefList,size_t newRefCount,
                                   const hpatch_TSameFilePair* samePairList,size_t samePairCount);
hpatch_BOOL hpatch_TNewStream_closeFileHandles(hpatch_TNewStream* self);
hpatch_BOOL hpatch_TNewStream_close(hpatch_TNewStream* self);

#endif
#endif
