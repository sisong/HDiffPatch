//  _hpatch_mt.c
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
#include "_hpatch_mt.h"
#include "_patch_private_mt.h"
#if (_IS_USED_MULTITHREAD)

typedef struct hpatch_mt_t{
    size_t                  threadNum;
    HCondvar*               condvarList;
    volatile hpatch_BOOL    isOnFinish;
    volatile unsigned int   runningThreads;
    hpatch_mt_base_t        mt_base;
} hpatch_mt_t;

hpatch_force_inline static
hpatch_BOOL _hpatch_mt_init(hpatch_mt_t* self,size_t threadNum) {
    memset(self,0,sizeof(*self));
    self->threadNum=threadNum;
    self->condvarList=(HCondvar*)(self+1);
    memset(self->condvarList,0,threadNum*sizeof(HCondvar));
    if (!_hpatch_mt_base_init(&self->mt_base,self,0)) return hpatch_FALSE;
    return hpatch_TRUE;
}
static void _hpatch_mt_free(hpatch_mt_t* self) {
    if (!self) return;
    _hpatch_mt_base_free(&self->mt_base);
#if (defined(_DEBUG) || defined(DEBUG))
    {
        size_t i;
        for (i=0;i<self->threadNum;++i)
            assert(self->condvarList[i]==0);
    }
#endif
}

static void _hpatch_mt_condvarList_broadcast(struct hpatch_mt_t* self){
    size_t i;
    for (i=0;i<self->threadNum;++i){
        if (self->condvarList[i])
            c_condvar_broadcast(self->condvarList[i]);
    }
}
hpatch_BOOL hpatch_mt_registeCondvar(struct hpatch_mt_t* self,HCondvar waitCondvar){
    size_t i;
    assert(waitCondvar!=0);
    for (i=0;i<self->threadNum;++i){
        if (self->condvarList[i]==0){
            self->condvarList[i]=waitCondvar;
            return hpatch_TRUE;
        }
    }
    assert(hpatch_FALSE);
    return hpatch_FALSE;
}
hpatch_BOOL hpatch_mt_unregisteCondvar(struct hpatch_mt_t* self,HCondvar waitCondvar){
    size_t i;
    assert(waitCondvar!=0);
    for (i=0;i<self->threadNum;++i){
        if (self->condvarList[i]==waitCondvar){
            self->condvarList[i]=0;
            return hpatch_TRUE;
        }
    }
    assert(hpatch_FALSE);
    return hpatch_FALSE;
}

static void _hpatch_mt_setOnError(hpatch_mt_t* self) {
    if (!self->mt_base.isOnError){
        self->isOnFinish=hpatch_TRUE;
        self->mt_base.isOnError=hpatch_TRUE;
        _hpatch_mt_condvarList_broadcast(self);
    }
}

size_t hpatch_mt_t_memSize(){
    return sizeof(hpatch_mt_t);
}
hpatch_mt_t* hpatch_mt_open(void* pmem,size_t memSumSize,size_t threadNum,size_t workBufCount,size_t workBufNodeSize){
    size_t i;
    hpatch_mt_t* self=(hpatch_mt_t*)pmem;
    hpatch_byte* temp_cache=((hpatch_byte*)self)+hpatch_mt_t_memSize()+threadNum*sizeof(HCondvar);
    if (memSumSize<hpatch_mt_t_memSize()+threadNum*sizeof(HCondvar)+workBufCount*workBufNodeSize) return 0;
    if (workBufCount) assert(workBufNodeSize>sizeof(hpatch_TWorkBuf));
    if (!_hpatch_mt_init(self,threadNum)){
        _hpatch_mt_free(self);
        return 0;
    }
    self->mt_base.workBufSize=(workBufCount>0)?workBufNodeSize-sizeof(hpatch_TWorkBuf):0;
    for (i=0;i<workBufCount;i++,temp_cache+=workBufNodeSize){
        hpatch_TWorkBuf* workBuf=(hpatch_TWorkBuf*)temp_cache;
        TWorkBuf_pushABufAtHead(&self->mt_base.freeBufList,workBuf);
    }
    return self;
}
size_t hpatch_mt_workBufSize(const hpatch_mt_t* self){
    return self->mt_base.workBufSize;
}

hpatch_TWorkBuf* hpatch_mt_popFreeWorkBuf_fast(struct hpatch_mt_t* self,size_t needBufCount){
    hpatch_TWorkBuf* bufList=0;
    if (self->mt_base.isOnError) return 0;
    while (needBufCount--){
        hpatch_TWorkBuf* workBuf=TWorkBuf_popABuf(&self->mt_base.freeBufList);
        assert(workBuf);
        if (workBuf){
            TWorkBuf_pushABufAtHead(&bufList,workBuf);
        }else{
            self->mt_base.isOnError=hpatch_TRUE;
            return 0;
        }
    }
    return bufList;
}

hpatch_BOOL hpatch_mt_beforeThreadBegin(hpatch_mt_t* self){
    hpatch_BOOL result;
    c_locker_enter(self->mt_base._locker);
    result=(!self->mt_base.isOnError);
    if (result)
        ++self->runningThreads;
    c_locker_leave(self->mt_base._locker);
    return result;
}
void hpatch_mt_onThreadEnd(hpatch_mt_t* self){
    c_locker_enter(self->mt_base._locker);
    assert(self->runningThreads>0);
    if (self->runningThreads>0){
        --self->runningThreads;
        if (self->runningThreads==0)
            c_condvar_signal(self->mt_base._waitCondvar);
    }else{
        LOG_ERR("hpatch_mt_t runningThreads logic error");
        _hpatch_mt_setOnError(self);
    }
    c_locker_leave(self->mt_base._locker);
}

hpatch_BOOL hpatch_mt_isOnFinish(hpatch_mt_t* self){
    hpatch_BOOL result;
    c_locker_enter(self->mt_base._locker);
    result=self->isOnFinish;
    c_locker_leave(self->mt_base._locker);
    return result;
}
hpatch_BOOL hpatch_mt_isOnError(hpatch_mt_t* self){
    hpatch_BOOL result;
    c_locker_enter(self->mt_base._locker);
    result=self->mt_base.isOnError;
    c_locker_leave(self->mt_base._locker);
    return result;
}

void hpatch_mt_setOnError(struct hpatch_mt_t* self){
    c_locker_enter(self->mt_base._locker);
    _hpatch_mt_setOnError(self);
    c_locker_leave(self->mt_base._locker);
}

void hpatch_mt_waitAllThreadEnd(hpatch_mt_t* self,hpatch_BOOL isOnError){
    c_locker_enter(self->mt_base._locker);
    if (isOnError){
        _hpatch_mt_setOnError(self);
    }else if (!self->isOnFinish){
        self->isOnFinish=hpatch_TRUE;
        _hpatch_mt_condvarList_broadcast(self);
    }
    while (self->runningThreads){
        c_condvar_wait(self->mt_base._waitCondvar,self->mt_base._locker);
    }
    c_locker_leave(self->mt_base._locker);
}
hpatch_BOOL hpatch_mt_close(hpatch_mt_t* self,hpatch_BOOL isOnError){
    if (!self) return !isOnError;
    hpatch_mt_waitAllThreadEnd(self,isOnError);
    isOnError|=self->mt_base.isOnError;
    _hpatch_mt_free(self);
    return (!isOnError);
}

#endif //_IS_USED_MULTITHREAD
