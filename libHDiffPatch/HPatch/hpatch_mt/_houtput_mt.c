//  _houtput_mt.c
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
#include "_houtput_mt.h"
#include "_patch_private_mt.h"
#if (_IS_USED_MULTITHREAD)

typedef struct houtput_mt_t{
    hpatch_TStreamOutput        base;
    const hpatch_TStreamOutput* base_stream;
    hpatch_TWorkBuf*            curDataBuf;
    volatile hpatch_StreamPos_t curWritePos;
    hpatch_StreamPos_t          curOutedPos;
    hpatch_mt_base_t            mt_base;
} houtput_mt_t;

    static hpatch_BOOL houtput_mt_write_(const struct hpatch_TStreamOutput* stream,hpatch_StreamPos_t writeToPos,
                                        const unsigned char* data,const unsigned char* data_end);

hpatch_inline static
hpatch_BOOL _houtput_mt_init(houtput_mt_t* self,struct hpatch_mt_t* h_mt,hpatch_TWorkBuf* freeBufList,
                             hpatch_size_t workBufSize,const hpatch_TStreamOutput* base_stream,hpatch_StreamPos_t curWritePos){
    memset(self,0,sizeof(*self));
    assert(freeBufList);
    if (!_hpatch_mt_base_init(&self->mt_base,h_mt,freeBufList,workBufSize)) return hpatch_FALSE;
    self->base.streamImport=self;
    self->base.streamSize=base_stream->streamSize;
    self->base.write=houtput_mt_write_;
    self->base_stream=base_stream;
    self->curWritePos=curWritePos;
    self->curOutedPos=curWritePos;
    return hpatch_TRUE;
}
static void _houtput_mt_free(houtput_mt_t* self){
    if (self==0) return;
    self->base.streamImport=0;
    _hpatch_mt_base_free(&self->mt_base);
}

hpatch_force_inline static
hpatch_BOOL _houtput_mt_writeAData(houtput_mt_t* self,hpatch_TWorkBuf* data){
    hpatch_StreamPos_t writePos=self->curWritePos;
    self->curWritePos+=data->data_size;
    return self->base_stream->write(self->base_stream,writePos,TWorkBuf_data(data),TWorkBuf_data_end(data));
}

static void houtput_thread_(int threadIndex,void* workData){
    houtput_mt_t* self=(houtput_mt_t*)workData;
    hpatch_BOOL _isOnError=hpatch_FALSE;
    while ((!hpatch_mt_isOnError(self->mt_base.h_mt))&(!_isOnError)&(self->curWritePos<self->base.streamSize)){
        hpatch_TWorkBuf* datas=hpatch_mt_base_onceWaitAllBufs_(&self->mt_base,(hpatch_TWorkBuf**)&self->mt_base.dataBufList,&_isOnError);
        
        while (datas){
            if (_houtput_mt_writeAData(self,datas)){
                hpatch_TWorkBuf* next=datas->next;
                hpatch_mt_base_pushABufAtHead_(&self->mt_base,(hpatch_TWorkBuf**)&self->mt_base.freeBufList,datas,&_isOnError);
                datas=next;
            }else{
                hpatch_mt_base_setOnError_(&self->mt_base);
                _isOnError=hpatch_TRUE;
                break;
            }
        }
    }

    hpatch_mt_base_aThreadEnd_(&self->mt_base);
}

static hpatch_BOOL houtput_mt_write_(const struct hpatch_TStreamOutput* stream,hpatch_StreamPos_t writeToPos,
                                     const unsigned char* data,const unsigned char* data_end){
    houtput_mt_t* self=(houtput_mt_t*)stream->streamImport;
    hpatch_BOOL _isOnError=hpatch_FALSE; 
    const hpatch_BOOL isNeedFlush=(writeToPos+(size_t)(data_end-data)==self->base.streamSize);
    if (self->curOutedPos!=writeToPos) return hpatch_FALSE;
    self->curOutedPos+=(data_end-data);
    if (self->curOutedPos>self->base.streamSize) return hpatch_FALSE;
    while ((!_isOnError)&(data<data_end)){
        if (hpatch_mt_isOnError(self->mt_base.h_mt)) { _isOnError=hpatch_TRUE; break; }
        if (self->curDataBuf==0){
            self->curDataBuf=hpatch_mt_base_onceWaitABuf_(&self->mt_base,(hpatch_TWorkBuf**)&self->mt_base.freeBufList,&_isOnError);
            if (self->curDataBuf)
                self->curDataBuf->data_size=0;
        }

        if ((self->curDataBuf!=0)&(!_isOnError)){
            size_t writeLen=self->mt_base.workBufSize-self->curDataBuf->data_size;
            writeLen=(writeLen<(size_t)(data_end-data))?writeLen:(size_t)(data_end-data);
            memcpy(TWorkBuf_data_end(self->curDataBuf),data,writeLen);
            self->curDataBuf->data_size+=writeLen;
            data+=writeLen;
            if ((self->mt_base.workBufSize==self->curDataBuf->data_size)|((data==data_end)&isNeedFlush)){
                hpatch_mt_base_pushABufAtEnd_(&self->mt_base,(hpatch_TWorkBuf**)&self->mt_base.dataBufList,self->curDataBuf,&_isOnError);
                self->curDataBuf=0;
            }
        }
    }
    return (!_isOnError);
}

size_t houtput_mt_t_memSize(){
    return sizeof(houtput_mt_t);
}

hpatch_TStreamOutput* houtput_mt_open(void* pmem,size_t memSize,struct hpatch_mt_t* h_mt,struct hpatch_TWorkBuf* freeBufList,
                                      hpatch_size_t workBufSize,const hpatch_TStreamOutput* base_stream,hpatch_StreamPos_t curWritePos){
    houtput_mt_t* self=pmem;
    if (memSize<houtput_mt_t_memSize()) return 0;
    if (!_houtput_mt_init(self,h_mt,freeBufList,workBufSize,base_stream,curWritePos))
        goto _on_error;

    if (!hpatch_mt_base_aThreadBegin_(&self->mt_base,houtput_thread_,self))
        goto _on_error;

    return &self->base;

_on_error:
    _houtput_mt_free(self);
    return 0;
}

hpatch_BOOL houtput_mt_close(hpatch_TStreamOutput* houtput_mt_stream){
    hpatch_BOOL result;
    houtput_mt_t* self=0;
    if (!houtput_mt_stream) return hpatch_TRUE;
    self=(houtput_mt_t*)houtput_mt_stream->streamImport;
    if (!self) return hpatch_TRUE;
    houtput_mt_stream->streamImport=0;

    result=(!self->mt_base.isOnError)&(self->curWritePos==self->base.streamSize);
    _houtput_mt_free(self);
    return result;
}

#endif //_IS_USED_MULTITHREAD
