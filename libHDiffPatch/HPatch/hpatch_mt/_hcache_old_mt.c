//  _hcache_old_mt.c
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
#include "_hcache_old_mt.h"
#include "_patch_private_mt.h"
#include "../patch_private.h"
#if (_IS_USED_MULTITHREAD)

typedef struct hcache_old_mt_t{
    hpatch_TStreamInput         base;
    const hpatch_TStreamInput*  old_stream;
    hpatch_TWorkBuf*            curDataBuf;
    size_t                      curDataBuf_pos;
    sspatch_coversListener_t    coversListener;
    sspatch_coversListener_t*   nextCoverlistener;
    volatile hpatch_StreamPos_t   leaveCoverCount;
    const volatile unsigned char* covers_cache;
    const volatile unsigned char* covers_cacheEnd;
    hpatch_BOOL                 isOnStepCoversInThread;
    hpatch_mt_base_t            mt_base;
}hcache_old_mt_t;

static void _hcache_old_mt_free(hcache_old_mt_t* self){
    if (self==0) return;
    self->base.streamImport=0;
    _hpatch_mt_base_free(&self->mt_base);
}

    static hpatch_force_inline
    hpatch_BOOL _read_data(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                           hpatch_TWorkBuf* wbuf,size_t read_len){
        unsigned char* out_data=TWorkBuf_data_end(wbuf);
        return stream->read(stream,readFromPos,out_data,out_data+read_len);
    }


    #define _check_therr(func) { \
        if (!(func)) { hpatch_mt_base_setOnError_(&self->mt_base); \
                       _isOnError=hpatch_TRUE; \
                       break; } }

static void hcache_old_thread_(int threadIndex,void* workData){
    hcache_old_mt_t* self=(hcache_old_mt_t*)workData;
    sspatch_covers_t covers;
    sspatch_covers_init(&covers);
    while (!hpatch_mt_isOnFinish(self->mt_base.h_mt)){//covers_cache loop
        unsigned char* covers_cache=0;
        unsigned char* covers_cacheEnd=0;
        hpatch_BOOL _isOnError;
        hpatch_BOOL _isGot_covers_cache=hpatch_FALSE;
        c_locker_enter(self->mt_base._locker);
        _isOnError=self->mt_base.isOnError;
        if (!_isOnError){
            covers_cache=(unsigned char*)self->covers_cache;
            covers_cacheEnd=(unsigned char*)self->covers_cacheEnd;
            _isGot_covers_cache=(covers_cache!=0)|(self->leaveCoverCount==0);
            if (!_isGot_covers_cache){
                c_condvar_wait(self->mt_base._waitCondvar,self->mt_base._locker);
                covers_cache=(unsigned char*)self->covers_cache;
                covers_cacheEnd=(unsigned char*)self->covers_cacheEnd;
                _isGot_covers_cache=(covers_cache!=0)|(self->leaveCoverCount==0);
            }
            if (_isGot_covers_cache){
                self->covers_cache=0;
                self->covers_cacheEnd=0;
            }
        }
        c_locker_leave(self->mt_base._locker);
        if (_isOnError) break; //exit loop by error
        if (_isGot_covers_cache&self->isOnStepCoversInThread&(self->nextCoverlistener!=0))
            self->nextCoverlistener->onStepCovers(self->nextCoverlistener,covers_cache,covers_cacheEnd);
        if (_isGot_covers_cache&(covers_cache==0)) break; //exit loop by all done
        if (covers_cache==0) continue; //need covers_cache
        assert(_isGot_covers_cache);

        {//got covers_cache
            hpatch_TWorkBuf* wbuf=0;
            sspatch_covers_setCoversCache(&covers,covers_cache,covers_cacheEnd);
            while (sspatch_covers_isHaveNextCover(&covers)){//cover loop
                hpatch_StreamPos_t coverLen;
                hpatch_StreamPos_t coverPos;
                _check_therr(sspatch_covers_nextCover(&covers));
                coverLen=covers.cover.length;
                coverPos=covers.cover.oldPos;
                _check_therr((coverPos<=self->old_stream->streamSize)&
                             (coverLen<=(hpatch_StreamPos_t)(self->old_stream->streamSize-coverPos)));
                while (coverLen){//buf loop
                    size_t readLen;
                    if (wbuf==0){
                        wbuf=hpatch_mt_base_onceWaitABuf_(&self->mt_base,(hpatch_TWorkBuf**)&self->mt_base.freeBufList,&_isOnError);
                        if (_isOnError) break; //exit read loop by error
                        if (wbuf==0) continue; //need buf
                        wbuf->data_size=0;
                    }

                    readLen=self->mt_base.workBufSize-wbuf->data_size;
                    if (readLen>coverLen) readLen=(size_t)coverLen;
                    _check_therr(_read_data(self->old_stream,coverPos,wbuf,readLen));
                    wbuf->data_size+=readLen;
                    coverPos+=readLen;
                    coverLen-=readLen;
                    if (wbuf->data_size==self->mt_base.workBufSize){
                        hpatch_mt_base_pushABufAtEnd_(&self->mt_base,(hpatch_TWorkBuf**)&self->mt_base.dataBufList,wbuf,&_isOnError);
                        wbuf=0;
                    }
                }//end buf loop
                if (_isOnError) break; //exit loop by error
            }//end cover loop
            if (wbuf){
                hpatch_mt_base_pushABufAtEnd_(&self->mt_base,(hpatch_TWorkBuf**)&self->mt_base.dataBufList,wbuf,&_isOnError);
                wbuf=0;
            }
        }
        if (_isOnError) break; //exit loop by error
    }//end covers_cache loop

    hpatch_mt_base_aThreadEnd_(&self->mt_base);
}

static void _hcache_old_mt_onStepCovers(struct sspatch_coversListener_t* listener,
                                        const unsigned char* covers_cache,const unsigned char* covers_cacheEnd){
    hcache_old_mt_t* self=(hcache_old_mt_t*)listener->import;
    c_locker_enter(self->mt_base._locker);
    assert(self->curDataBuf==0);
    assert(self->covers_cache==0);
    self->covers_cache=covers_cache;
    self->covers_cacheEnd=covers_cacheEnd;
    if (covers_cache==0){
        assert(covers_cacheEnd==0);
        self->leaveCoverCount=0;
    }
    c_condvar_signal(self->mt_base._waitCondvar);
    c_locker_leave(self->mt_base._locker);
    if ((!self->isOnStepCoversInThread)&(self->nextCoverlistener!=0))
        self->nextCoverlistener->onStepCovers(self->nextCoverlistener,covers_cache,covers_cacheEnd);
}
static void _hcache_old_mt_onStepCoversReset(struct sspatch_coversListener_t* listener,hpatch_StreamPos_t leaveCoverCount){
    hcache_old_mt_t* self=(hcache_old_mt_t*)listener->import;
    c_locker_enter(self->mt_base._locker);
    assert(self->curDataBuf==0);
    assert(self->covers_cache==0);
    self->leaveCoverCount=leaveCoverCount;
    c_locker_leave(self->mt_base._locker);
    if ((self->nextCoverlistener)&&(self->nextCoverlistener->onStepCoversReset))
        self->nextCoverlistener->onStepCoversReset(self->nextCoverlistener,leaveCoverCount);
}

static hpatch_BOOL _hcache_old_mt_read(const struct hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                       unsigned char* out_data,unsigned char* out_data_end){
    hcache_old_mt_t* self=(hcache_old_mt_t*)stream->streamImport;
    _DEF_hinput_mt_base_read();
}

hpatch_inline static
hpatch_BOOL _hcache_old_mt_init(hcache_old_mt_t* self,struct hpatch_mt_t* h_mt,hpatch_TWorkBuf* freeBufList,
                                hpatch_size_t workBufSize,const hpatch_TStreamInput* old_stream,
                                sspatch_coversListener_t* nextCoverlistener,hpatch_BOOL isOnStepCoversInThread){
    memset(self,0,sizeof(*self));
    assert(freeBufList);
    if (!_hpatch_mt_base_init(&self->mt_base,h_mt,freeBufList,workBufSize)) return hpatch_FALSE;
    self->base.streamImport=self;
    self->base.streamSize=old_stream->streamSize;
    self->base.read=_hcache_old_mt_read;
    self->coversListener.import=self;
    self->coversListener.onStepCovers=_hcache_old_mt_onStepCovers;
#if (defined(_DEBUG) || defined(DEBUG))
    if (hpatch_TRUE)
#else
    if (nextCoverlistener&&nextCoverlistener->onStepCoversReset)
#endif
        self->coversListener.onStepCoversReset=_hcache_old_mt_onStepCoversReset;
    self->old_stream=old_stream;
    self->nextCoverlistener=nextCoverlistener;
    self->isOnStepCoversInThread=isOnStepCoversInThread?1:0;
    self->leaveCoverCount=~(hpatch_StreamPos_t)0;
    return hpatch_TRUE;
}


size_t hcache_old_mt_t_memSize(){
    return sizeof(hcache_old_mt_t);
}

hpatch_TStreamInput* hcache_old_mt_open(void* pmem,size_t memSize,struct hpatch_mt_t* h_mt,
                                        struct hpatch_TWorkBuf* freeBufList,hpatch_size_t workBufSize,
                                        const hpatch_TStreamInput* old_stream,sspatch_coversListener_t** out_coversListener,
                                        sspatch_coversListener_t* nextCoverlistener,hpatch_BOOL isOnStepCoversInThread){
    hcache_old_mt_t* self=(hcache_old_mt_t*)pmem;
    if (memSize<hcache_old_mt_t_memSize()) return 0;
    if (nextCoverlistener) assert(nextCoverlistener->onStepCovers);
    if (!_hcache_old_mt_init(self,h_mt,freeBufList,workBufSize,old_stream,nextCoverlistener,isOnStepCoversInThread))
        goto _on_error;

    if (!hpatch_mt_base_aThreadBegin_(&self->mt_base,hcache_old_thread_,self))
        goto _on_error;

    *out_coversListener=&self->coversListener;
    return &self->base;

_on_error:
    _hcache_old_mt_free(self);
    return 0;
}

hpatch_BOOL hcache_old_mt_close(hpatch_TStreamInput* hcache_old_mt_stream){
    hpatch_BOOL result;
    hcache_old_mt_t* self=0;
    if (!hcache_old_mt_stream) return hpatch_TRUE;
    self=(hcache_old_mt_t*)hcache_old_mt_stream->streamImport;
    if (!self) return hpatch_TRUE;
    hcache_old_mt_stream->streamImport=0;

    result=(!self->mt_base.isOnError)&(self->curDataBuf==0)&(self->covers_cache==0)
            &((self->leaveCoverCount==0)|(self->leaveCoverCount==~(hpatch_StreamPos_t)0));
    _hcache_old_mt_free(self);
    return result;
}

#endif //_IS_USED_MULTITHREAD
