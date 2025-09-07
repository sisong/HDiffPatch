//  _hinput_mt.c
//  hpatch
/*
 The MIT License (MIT)
 Copyright (c) 2025 HouSisong
 
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
#include "_hinput_mt.h"
#include "_patch_private_mt.h"
#if (_IS_USED_MULTITHREAD)

typedef struct hinput_mt_t{
    hpatch_TStreamInput         base;
    const hpatch_TStreamInput*  base_stream;
    hpatch_TWorkBuf*            curDataBuf;
    size_t                      curDataBuf_pos;
    hpatch_StreamPos_t          curReadPos;
    hpatch_TDecompress*         decompressPlugin;
    hpatch_decompressHandle*    decompressHandle;
    volatile hpatch_StreamPos_t curOutedPos;
    hpatch_mt_base_t            mt_base;
} hinput_mt_t;

    static hpatch_BOOL hinput_mt_read_(const struct hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                       unsigned char* out_data,unsigned char* out_data_end);

hpatch_inline static
hpatch_BOOL _hinput_mt_init(hinput_mt_t* self,struct hpatch_mt_t* h_mt,hpatch_TWorkBuf* freeBufList,hpatch_size_t workBufSize,
                            const hpatch_TStreamInput* base_stream,hpatch_StreamPos_t curReadPos,hpatch_StreamPos_t endReadPos,
                            hpatch_TDecompress* decompressPlugin,hpatch_StreamPos_t uncompressedSize){
    memset(self,0,sizeof(*self));
    assert(endReadPos<=base_stream->streamSize);
    assert(freeBufList);
    if (!_hpatch_mt_base_init(&self->mt_base,h_mt,freeBufList,workBufSize)) return hpatch_FALSE;
    self->base.streamImport=self;
    self->base.streamSize=decompressPlugin?(curReadPos+uncompressedSize):endReadPos;
    self->base.read=hinput_mt_read_;
    self->base_stream=base_stream;
    self->curReadPos=curReadPos;
    self->decompressPlugin=decompressPlugin;
    self->curOutedPos=curReadPos;
    if (decompressPlugin)
        self->decompressHandle=decompressPlugin->open(decompressPlugin,uncompressedSize,
                                                      base_stream,curReadPos,endReadPos);
    return (self->decompressPlugin==0)|(self->decompressHandle!=0);
}
static void _hinput_mt_free(hinput_mt_t* self){
    if (self==0) return;
    self->base.streamImport=0;
    if (self->decompressHandle) { self->decompressPlugin->close(self->decompressPlugin,self->decompressHandle); self->decompressHandle=0; }
    _hpatch_mt_base_free(&self->mt_base);
}

static hpatch_BOOL _hinput_mt_readAData(hinput_mt_t* self,hpatch_TWorkBuf* data){
    hpatch_StreamPos_t readPos=self->curReadPos;
    size_t readLen=self->mt_base.workBufSize;
    if (readPos+readLen>self->base.streamSize)
        readLen=(size_t)(self->base.streamSize-readPos);
    self->curReadPos+=readLen;
    data->data_size=readLen;
    if (self->decompressHandle)
        return self->decompressPlugin->decompress_part(self->decompressHandle,TWorkBuf_data(data),TWorkBuf_data_end(data));
    else
        return self->base_stream->read(self->base_stream,readPos,TWorkBuf_data(data),TWorkBuf_data_end(data));
}

static void hinput_thread_(int threadIndex,void* workData){
    hinput_mt_t* self=(hinput_mt_t*)workData;
    hpatch_BOOL _isOnError=hpatch_FALSE;
    while ((!hpatch_mt_isOnFinish(self->mt_base.h_mt))&(!_isOnError)&(self->curReadPos<self->base.streamSize)){
        hpatch_TWorkBuf* wbuf=hpatch_mt_base_onceWaitABuf_(&self->mt_base,(hpatch_TWorkBuf**)&self->mt_base.freeBufList,&_isOnError);
        
        if ((wbuf!=0)&(!_isOnError)){
            if (_hinput_mt_readAData(self,wbuf)){
                hpatch_mt_base_pushABufAtEnd_(&self->mt_base,(hpatch_TWorkBuf**)&self->mt_base.dataBufList,wbuf,&_isOnError);
            }else{
                hpatch_mt_base_setOnError_(&self->mt_base);
                _isOnError=hpatch_TRUE;
            }
        }
    }

    hpatch_mt_base_aThreadEnd_(&self->mt_base);
}
static hpatch_BOOL hinput_mt_read_(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                   unsigned char* out_data,unsigned char* out_data_end){
    hinput_mt_t* self=(hinput_mt_t*)stream->streamImport;
    if (self->curOutedPos!=readFromPos) return hpatch_FALSE;
    self->curOutedPos+=(out_data_end-out_data);
    if (self->curOutedPos>self->base.streamSize) return hpatch_FALSE;
    _DEF_hinput_mt_base_read();
}

size_t hinput_mt_t_memSize(){
    return sizeof(hinput_mt_t);
}
hpatch_TStreamInput* hinput_dec_mt_open(void* pmem,size_t memSize,struct hpatch_mt_t* h_mt,struct hpatch_TWorkBuf* freeBufList,hpatch_size_t workBufSize,
                                        const hpatch_TStreamInput* base_stream,hpatch_StreamPos_t curReadPos,hpatch_StreamPos_t endReadPos,
                                        hpatch_TDecompress* decompressPlugin,hpatch_StreamPos_t uncompressedSize){
    hinput_mt_t* self=(hinput_mt_t*)pmem;
    if (memSize<hinput_mt_t_memSize()) return 0;
    if (!_hinput_mt_init(self,h_mt,freeBufList,workBufSize,base_stream,curReadPos,endReadPos,decompressPlugin,uncompressedSize))
        goto _on_error;

    if (!hpatch_mt_base_aThreadBegin_(&self->mt_base,hinput_thread_,self))
        goto _on_error;

    return &self->base;

_on_error:
    _hinput_mt_free(self);
    return 0;
}

hpatch_TStreamInput* hinput_mt_open(void* pmem,size_t memSize,struct hpatch_mt_t* h_mt,hpatch_TWorkBuf* freeBufList,hpatch_size_t workBufSize,
                                    const hpatch_TStreamInput* base_stream,hpatch_StreamPos_t curReadPos,hpatch_StreamPos_t endReadPos){
    return hinput_dec_mt_open(pmem,memSize,h_mt,freeBufList,workBufSize,base_stream,curReadPos,endReadPos,0,0);
}

hpatch_BOOL hinput_mt_close(hpatch_TStreamInput* hinput_mt_stream){
    hpatch_BOOL result;
    hinput_mt_t* self=0;
    if (!hinput_mt_stream) return hpatch_TRUE;
    self=(hinput_mt_t*)hinput_mt_stream->streamImport;
    if (!self) return hpatch_TRUE;
    hinput_mt_stream->streamImport=0;

    result=(!self->mt_base.isOnError)&(self->curOutedPos==self->base.streamSize);
    _hinput_mt_free(self);
    return result;
}

#endif //_IS_USED_MULTITHREAD
