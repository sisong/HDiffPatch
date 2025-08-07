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
#if (_IS_USED_MULTITHREAD)

typedef struct hcache_old_mt_t{
    hpatch_TStreamInput         base;
    const hpatch_TStreamInput*  old_stream;
    struct hpatch_mt_t*         h_mt;
    size_t                      workBufSize;
    hpatch_TWorkBuf*            curDataBuf;
    size_t                      curDataBuf_pos;
    sspatch_coversListener_t    coversListener;
    sspatch_coversListener_t*   nextCoverlistener;
    volatile hpatch_StreamPos_t   leaveCoverCount;
    const volatile unsigned char* covers_cache;
    const volatile unsigned char* covers_cacheEnd;
    volatile hpatch_TWorkBuf*   freeBufList;
    volatile hpatch_TWorkBuf*   dataBufList;
    volatile hpatch_BOOL        isOnError;
#if (defined(_DEBUG) || defined(DEBUG))
    volatile hpatch_BOOL        threadIsRunning;
#endif
    HLocker     _locker;
    HCondvar    _waitCondvar;
}hcache_old_mt_t;


    static hpatch_BOOL _hcache_old_mt_read(const struct hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                           unsigned char* out_data,unsigned char* out_data_end);
    static void _hcache_old_mt_onStepCovers(struct sspatch_coversListener_t* listener,
                                            const unsigned char* covers_cache,const unsigned char* covers_cacheEnd);
    static void _hcache_old_mt_onStepCoversReset(struct sspatch_coversListener_t* listener,hpatch_StreamPos_t leaveCoverCount);

hpatch_inline static
hpatch_BOOL _hcache_old_mt_init(hcache_old_mt_t* self,struct hpatch_mt_t* h_mt,hpatch_TWorkBuf* freeBufList,
                                const hpatch_TStreamInput* old_stream,sspatch_coversListener_t* nextCoverlistener){
    memset(self,0,sizeof(*self));
    assert(freeBufList);
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
    self->h_mt=h_mt;
    self->old_stream=old_stream;
    self->nextCoverlistener=nextCoverlistener;
    self->leaveCoverCount=~(hpatch_StreamPos_t)0;
    self->freeBufList=freeBufList;
    self->workBufSize=hpatch_mt_workBufSize(h_mt);

    self->_locker=c_locker_new();
    self->_waitCondvar=c_condvar_new();
    return (self->_locker!=0)&(self->_waitCondvar!=0);
}

static void _hcache_old_mt_free(hcache_old_mt_t* self){
    if (self==0) return;
#if (defined(_DEBUG) || defined(DEBUG))
    if (self->_locker) c_locker_enter(self->_locker);
    assert(!self->threadIsRunning);
    if (self->_locker) c_locker_leave(self->_locker);
#endif
    self->base.streamImport=0;
    _thread_obj_free(c_condvar_delete,self->_waitCondvar);
    _thread_obj_free(c_locker_delete,self->_locker);
}

static void hcache_old_mt_setOnError_(hcache_old_mt_t* self) {
    hpatch_BOOL isNeedSetOnError=hpatch_FALSE;
    c_locker_enter(self->_locker);
    isNeedSetOnError=(!self->isOnError);
    if (isNeedSetOnError){
        self->isOnError=hpatch_TRUE;
        c_condvar_signal(self->_waitCondvar);
    }
    c_locker_leave(self->_locker);
    if (isNeedSetOnError)
        hpatch_mt_setOnError(self->h_mt);
}


    static hpatch_BOOL _read_data(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                  hpatch_TWorkBuf* wbuf,size_t read_len){
        unsigned char* out_data=TWorkBuf_data_end(wbuf);
        return stream->read(stream,readFromPos,out_data,out_data+read_len);
    }


    #define _check_therr(func) { \
        if (!(func)) { hcache_old_mt_setOnError_(self); \
                       _isOnError=hpatch_TRUE; \
                       break; } }

static void hcache_old_thread_(int threadIndex,void* workData){
    hcache_old_mt_t* self=(hcache_old_mt_t*)workData;
    while (!hpatch_mt_isOnFinish(self->h_mt)){//covers_cache loop
        unsigned char* covers_cache=0;
        unsigned char* covers_cacheEnd=0;
        hpatch_BOOL _isOnError;
        hpatch_BOOL _isGot_covers_cache=hpatch_FALSE;
        c_locker_enter(self->_locker);
        if (!self->isOnError){
            covers_cache=(unsigned char*)self->covers_cache;
            covers_cacheEnd=(unsigned char*)self->covers_cacheEnd;
            _isGot_covers_cache=(covers_cache!=0)|(self->leaveCoverCount==0);
            if (!_isGot_covers_cache){
                c_condvar_wait(self->_waitCondvar,self->_locker);
            }else{
                self->covers_cache=0;
                self->covers_cacheEnd=0;
            }
        }
        _isOnError=self->isOnError;
        c_locker_leave(self->_locker);
        if (_isOnError) break; //exit loop by error
        if (_isGot_covers_cache&(self->nextCoverlistener!=0))
            self->nextCoverlistener->onStepCovers(self->nextCoverlistener,covers_cache,covers_cacheEnd);
        if (_isGot_covers_cache&(covers_cache==0)) break; //exit loop by all done
        if (covers_cache==0) continue; //need covers_cache
        assert(_isGot_covers_cache);

        {//got covers_cache
            hpatch_TWorkBuf* wbuf=0;
            sspatch_covers_t covers;
            sspatch_covers_init(&covers);
            sspatch_covers_setCoversCache(&covers,covers_cache,covers_cacheEnd);
            while (sspatch_covers_isHaveNextCover(&covers)){//cover loop
                hpatch_StreamPos_t coverLen;
                hpatch_StreamPos_t coverPos;
                _check_therr(sspatch_covers_nextCover(&covers));
                coverLen=covers.cover.length;
                coverPos=covers.cover.oldPos;
                _check_therr((coverPos>self->old_stream->streamSize)|
                             (coverLen>(hpatch_StreamPos_t)(self->old_stream->streamSize-coverPos)));
                while (coverLen){//buf loop
                    size_t readLen;
                    if (wbuf==0){
                        c_locker_enter(self->_locker);
                        if (!self->isOnError){
                            wbuf=TWorkBuf_popABuf(&self->freeBufList);
                            if (wbuf==0)
                                c_condvar_wait(self->_waitCondvar,self->_locker);
                        }
                        _isOnError=self->isOnError;
                        c_locker_leave(self->_locker);
                        if (_isOnError) break; //exit read loop by error
                        if (wbuf==0) continue; //need buf
                        wbuf->data_size=0;
                    }

                    readLen=self->workBufSize-wbuf->data_size;
                    if (readLen>coverLen) readLen=(size_t)coverLen;
                    _check_therr(_read_data(self->old_stream,coverPos,wbuf,readLen));
                    wbuf->data_size+=readLen;
                    coverPos+=readLen;
                    coverLen-=readLen;
                    if (wbuf->data_size==self->workBufSize){
                        c_locker_enter(self->_locker);
                        TWorkBuf_pushABufAtEnd(&self->dataBufList,wbuf);
                        c_condvar_signal(self->_waitCondvar);
                        c_locker_leave(self->_locker);
                        wbuf=0;
                    }
                }//end buf loop
                if (_isOnError) break; //exit loop by error
            }//end cover loop
            if (wbuf){
                c_locker_enter(self->_locker);
                if (wbuf->data_size)
                    TWorkBuf_pushABufAtEnd(&self->dataBufList,wbuf);
                else
                    TWorkBuf_pushABufAtHead(&self->freeBufList,wbuf);
                c_condvar_signal(self->_waitCondvar);
                c_locker_leave(self->_locker);
                wbuf=0;
            }
        }
        if (_isOnError) break; //exit loop by error
    }//end covers_cache loop

    hpatch_mt_onThreadEnd(self->h_mt);
#if (defined(_DEBUG) || defined(DEBUG))
    c_locker_enter(self->_locker);
    self->threadIsRunning=hpatch_FALSE;
    c_locker_leave(self->_locker);
#endif
}

static void _hcache_old_mt_onStepCovers(struct sspatch_coversListener_t* listener,
                                        const unsigned char* covers_cache,const unsigned char* covers_cacheEnd){
    hcache_old_mt_t* self=(hcache_old_mt_t*)listener->import;
    c_locker_enter(self->_locker);
    assert(self->curDataBuf==0);
    assert(self->covers_cache==0);
    self->covers_cache=covers_cache;
    self->covers_cacheEnd=covers_cacheEnd;
    if (covers_cache==0)
        self->leaveCoverCount=0;
    c_condvar_signal(self->_waitCondvar);
    c_locker_leave(self->_locker);
}
static void _hcache_old_mt_onStepCoversReset(struct sspatch_coversListener_t* listener,hpatch_StreamPos_t leaveCoverCount){
    hcache_old_mt_t* self=(hcache_old_mt_t*)listener->import;
    c_locker_enter(self->_locker);
    assert(self->curDataBuf==0);
    assert(self->covers_cache==0);
    self->leaveCoverCount=leaveCoverCount;
    c_locker_leave(self->_locker);
    if ((self->nextCoverlistener)&&(self->nextCoverlistener->onStepCoversReset))
        self->nextCoverlistener->onStepCoversReset(self->nextCoverlistener,leaveCoverCount);
}

static hpatch_BOOL _hcache_old_mt_read(const struct hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                       unsigned char* out_data,unsigned char* out_data_end){
    hcache_old_mt_t* self=(hcache_old_mt_t*)stream->streamImport;
    hpatch_BOOL result=hpatch_TRUE;
    while (result&(out_data<out_data_end)){
        if (self->curDataBuf){
            size_t readLen=self->curDataBuf->data_size-self->curDataBuf_pos;
            readLen=(readLen<(size_t)(out_data_end-out_data))?readLen:(size_t)(out_data_end-out_data);
            memcpy(out_data,TWorkBuf_data(self->curDataBuf)+self->curDataBuf_pos,readLen);
            self->curDataBuf_pos+=readLen;
            out_data+=readLen;
            if (self->curDataBuf_pos==self->curDataBuf->data_size){
                c_locker_enter(self->_locker);
                TWorkBuf_pushABufAtHead(&self->freeBufList,self->curDataBuf);
                c_condvar_signal(self->_waitCondvar);
                result=(!self->isOnError);
                c_locker_leave(self->_locker);
                self->curDataBuf=0;
            }
        }else{
            c_locker_enter(self->_locker);
            self->curDataBuf=TWorkBuf_popABuf(&self->dataBufList);
            if (self->curDataBuf==0)
                c_condvar_wait(self->_waitCondvar,self->_locker);
            result=(!self->isOnError);
            c_locker_leave(self->_locker);
        }
    }
    return result;
}


size_t hcache_old_mt_t_size(){
    return sizeof(hcache_old_mt_t);
}

hpatch_TStreamInput* hcache_old_mt_open(void* pmem,size_t memSize,struct hpatch_mt_t* h_mt,struct hpatch_TWorkBuf* freeBufList,
                                        const hpatch_TStreamInput* old_stream,sspatch_coversListener_t** out_coversListener,
                                        sspatch_coversListener_t* nextCoverlistener){
    hcache_old_mt_t* self=(hcache_old_mt_t*)pmem;
    if (memSize<hcache_old_mt_t_size()) return 0;
    if (nextCoverlistener) assert(nextCoverlistener->onStepCovers);
    if (!_hcache_old_mt_init(self,h_mt,freeBufList,old_stream,nextCoverlistener))
        goto _on_error;

    //start a thread to read
    if (!hpatch_mt_beforeThreadBegin(self->h_mt))
        goto _on_error;
#if (defined(_DEBUG) || defined(DEBUG))
    self->threadIsRunning=hpatch_TRUE;
#endif
    if (!c_thread_parallel(1,hcache_old_thread_,self,hpatch_FALSE,0)){
        hpatch_mt_onThreadEnd(self->h_mt);
    #if (defined(_DEBUG) || defined(DEBUG))
        self->threadIsRunning=hpatch_FALSE;
    #endif
        goto _on_error;
    }

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

    result=(!self->isOnError)&(self->curDataBuf==0)&(self->covers_cache==0)
            &((self->leaveCoverCount==0)|(self->leaveCoverCount==~(hpatch_StreamPos_t)0));
    _hcache_old_mt_free(self);
    return result;
}

#endif //_IS_USED_MULTITHREAD
