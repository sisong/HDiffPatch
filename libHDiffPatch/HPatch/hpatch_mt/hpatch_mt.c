//  hpatch_mt.c
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
#include "hpatch_mt.h"
#include "../patch_private.h"
#include "../patch.h"

#if (_IS_USED_MULTITHREAD)
#include "_patch_private_mt.h"
#include "_hcache_old_mt.h"
#include "_hinput_mt.h"
#include "_houtput_mt.h"
#include "_hpatch_mt.h"
#ifndef _MT_IS_NEED_LIMIT_SIZE
#   define _MT_IS_NEED_LIMIT_SIZE   1
#endif

    const size_t kAlignSize=sizeof(hpatch_StreamPos_t);
    const size_t kMinBufNodeSize=hpatch_kStreamCacheSize;
    const size_t kBetterBufNodeSize=hpatch_kFileIOBufBetterSize;
    const size_t kObjNodeCount=2;
    static size_t _getObjsMemSize(hpatchMTSets_t mtsets,size_t* pworkBufCount);

    // [writeNew,readOld,readDiff,decompressDiff]
    typedef hpatch_byte disThreads_t[4];
    static const disThreads_t _disThreadsTab_2[4]=
        { {1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1} };
    static const disThreads_t _disThreadsTab_3[6]=
        { {1,1,0,0},{1,0,1,0},{1,0,0,1},{0,1,1,0},{0,1,0,1},{0,0,1,1} };
    static const disThreads_t _disThreadsTab_4[4]=
        { {1,1,1,0},{1,1,0,1},{1,0,1,1},{0,1,1,1} };
    static const disThreads_t _disThreadsTab_5[1]=
        { {1,1,1,1} };

    #define _define_times5(_times) \
        const hpatch_uint64_t _times[5] = { writeNewTime,readOldTime,readDiffTime,decompressTime,patchTime }

    static hpatch_uint64_t _runTimeByDisThreads(const disThreads_t curThreads,const hpatch_uint64_t times[5]) {
        hpatch_uint64_t pT = times[4]; //patch
        hpatch_uint64_t dT = times[3]; //decompress
        hpatch_uint64_t maxT=0;
        if (curThreads[0]) //writeNewTime
            maxT=(times[0]>maxT)?times[0]:maxT;
        else
            pT+=times[0];
        if (curThreads[1]) //readOld
            maxT=(times[1]>maxT)?times[1]:maxT;
        else
            pT+=times[1];
        if (curThreads[2]) //readDiff
            maxT=(times[2]>maxT)?times[2]:maxT;
        else
            dT+=times[2];
        if (curThreads[3]) //decompressDiff
            maxT=(dT>maxT)?dT:maxT;
        else
            pT+=dT;
        return (pT>maxT)?pT:maxT;
    }
    static hpatch_inline hpatch_uint64_t _sumTimeByDisThreads(const disThreads_t curThreads, const hpatch_uint64_t times[5]) {
        return times[0]*curThreads[0]+times[1]*curThreads[1]+times[2]*curThreads[2]+times[3]*curThreads[3];
    }
    
    static hpatch_uint64_t _distributeThread(disThreads_t disThreads,const hpatch_uint64_t times[5],
                                             size_t maxThreadNum,hpatchMTSets_t mtsets){
        size_t i;
        hpatch_uint64_t minTime=~(hpatch_uint64_t)0;
        const disThreads_t* disThreadsTab;
        size_t disThreadsTabCount;
        size_t threadNum=_hpatchMTSets_threadNum(mtsets);
        threadNum=threadNum<maxThreadNum?threadNum:maxThreadNum;
        if (threadNum==2){
            disThreadsTab=_disThreadsTab_2;
            disThreadsTabCount=sizeof(_disThreadsTab_2)/sizeof(disThreads_t);
        }else if (threadNum==3){
            disThreadsTab=_disThreadsTab_3;
            disThreadsTabCount=sizeof(_disThreadsTab_3)/sizeof(disThreads_t);
        }else if (threadNum==4){
            disThreadsTab=_disThreadsTab_4;
            disThreadsTabCount=sizeof(_disThreadsTab_4)/sizeof(disThreads_t);
        }else{
            disThreadsTab=_disThreadsTab_5;
            disThreadsTabCount=sizeof(_disThreadsTab_5)/sizeof(disThreads_t);
        }
        for (i=0;i<disThreadsTabCount;++i){
            size_t        j;
            hpatch_uint64_t curTime;
            disThreads_t  curThreads;
            for (j=0;j<sizeof(disThreads_t)/sizeof(disThreads[0]);++j)
                curThreads[j]=disThreadsTab[i][j]&((hpatch_byte*)&mtsets)[j];
            if (_hpatchMTSets_threadNum(*(hpatchMTSets_t*)curThreads)!=threadNum) continue;
            curTime=_runTimeByDisThreads(curThreads,times);
            if (curTime<=minTime){
                if (curTime==minTime){
                    if (_sumTimeByDisThreads(curThreads,times)<_sumTimeByDisThreads(disThreads,times))
                        continue;
                }
                memcpy(disThreads,curThreads,sizeof(disThreads_t));
                minTime=curTime;
            }
        }
        assert(_hpatchMTSets_threadNum(*(hpatchMTSets_t*)disThreads)==threadNum);
        return minTime;
    }

hpatchMTSets_t _hpatch_getMTSets(hpatch_StreamPos_t newSize,hpatch_StreamPos_t oldSize,hpatch_StreamPos_t diffSize,
                                const hpatch_TDecompress* decompressPlugin,size_t kCacheCount,size_t stepMemSize,
                                size_t temp_cacheSumSize,size_t maxThreadNum,hpatchMTSets_t mtsets){
    hpatch_uint64_t writeNewTime,readOldTime,readDiffTime,decompressTime,patchTime;
    hpatch_uint64_t readOldSpeed=25;
    assert(sizeof(disThreads_t)==sizeof(hpatchMTSets_t));
    assert((2<=maxThreadNum));
#if (_MT_IS_NEED_LIMIT_SIZE)
    if (newSize<1*(1<<20)) mtsets.writeNew_isMT=0;
    if (oldSize<1*(1<<17)) mtsets.readOld_isMT=0;
    if ((diffSize<(newSize+oldSize)/256)|(diffSize<(1<<17))) mtsets.readDiff_isMT=0;
#endif
    if (decompressPlugin==0) mtsets.decompressDiff_isMT=0;
    if (mtsets.readOld_isMT){// is can cache all old?
        size_t workBufCount,objsMemSize,kMinTempCacheSize;
        hpatchMTSets_t _mtsets=mtsets; _mtsets.readOld_isMT=0;
        objsMemSize=_getObjsMemSize(_mtsets,&workBufCount);
        kMinTempCacheSize=objsMemSize+kMinBufNodeSize*(workBufCount+kCacheCount)+(kAlignSize-1);
        if (_patch_is_can_cache_all_old(oldSize,stepMemSize+kMinTempCacheSize,temp_cacheSumSize)){
            mtsets.readOld_isMT=0; //old data will load in memery, no need MT.
            readOldSpeed*=1000; //old in memory
        }
    #if (_IS_NEED_CACHE_OLD_BY_COVERS)
        else{ //is can cache part of old by step?
            kMinTempCacheSize=objsMemSize+kBetterBufNodeSize*(workBufCount+kCacheCount)+(kAlignSize-1);
            if (_patch_step_cache_old_canUsedSize(stepMemSize,kMinTempCacheSize,temp_cacheSumSize)>0){
                mtsets.readOld_isMT=0; // not use MT.
                readOldSpeed*=2; //cache part of old by step
            }
        }
    #endif // _IS_NEED_CACHE_OLD_BY_COVERS
    }else{
        readOldSpeed*=1000; //old in memory
    }
    if (_hpatchMTSets_threadNum(mtsets)<=1) return hpatchMTSets_no;
    { // estimate run time of each part
        hpatch_uint64_t newInDiffSize=diffSize/(decompressPlugin?1:2);
        hpatch_uint64_t decompressSpeed;
        if (decompressPlugin==0)
            decompressSpeed=0; //not used
        else if (decompressPlugin->is_can_open("zstd"))
            decompressSpeed=55;
        else if (decompressPlugin->is_can_open("lzma2")||decompressPlugin->is_can_open("lzma"))
            decompressSpeed=10;
        else if (decompressPlugin->is_can_open("zlib"))
            decompressSpeed=30;
        else if (decompressPlugin->is_can_open("bzip2"))
            decompressSpeed=5;
        else if (decompressPlugin->is_can_open("brotli"))
            decompressSpeed=35;
        else
            decompressSpeed=30/*unknow*/;
        decompressTime=mtsets.decompressDiff_isMT?diffSize/decompressSpeed:0;
        writeNewTime=mtsets.writeNew_isMT?newSize/100:0;
        readDiffTime=mtsets.readDiff_isMT?diffSize/150:0;
        patchTime=newSize/300;
        readOldTime=((oldSize<newInDiffSize?0:oldSize-newInDiffSize)+oldSize/8)/readOldSpeed;
    }
    { // distribute thread
        size_t   i;
        _define_times5(times);
        disThreads_t disThreads={0,0,0,0};
        hpatch_uint64_t minTime=_distributeThread(disThreads,times,maxThreadNum,mtsets);
#if (_MT_IS_NEED_LIMIT_SIZE)
        for (i=0;i<4;++i)
            disThreads[i]=(times[i]>(minTime/128))?disThreads[i]:0;
#endif
        mtsets=*(hpatchMTSets_t*)disThreads;
    }
    if ((decompressPlugin!=0)&(mtsets.readDiff_isMT!=0)&(!mtsets.decompressDiff_isMT)){
        mtsets.readDiff_isMT=0;
        mtsets.decompressDiff_isMT=1;
    }
    return mtsets;
}


typedef struct hpatch_mt_manager_t{
    struct hpatch_mt_t*     h_mt;
    hpatch_TStreamOutput*   newData;
    hpatch_TStreamInput*    oldData;
    hpatch_TStreamInput*    diffData;
    hpatch_TStreamInput*    decDiffData;
} hpatch_mt_manager_t;

static size_t _getObjsMemSize(hpatchMTSets_t mtsets,size_t* pworkBufCount){
    size_t threadNum=_hpatchMTSets_threadNum(mtsets);
    size_t objsMemSize=0;

    *pworkBufCount=kObjNodeCount*(threadNum-1);
    objsMemSize+=_hpatch_align_upper(sizeof(hpatch_mt_manager_t),kAlignSize);
    objsMemSize+=_hpatch_align_upper(hpatch_mt_t_memSize(threadNum),kAlignSize);
    objsMemSize+=mtsets.readDiff_isMT       ?_hpatch_align_upper(hinput_mt_t_memSize(),kAlignSize):0;
    objsMemSize+=mtsets.decompressDiff_isMT ?_hpatch_align_upper(hinput_mt_t_memSize(),kAlignSize):0;
    objsMemSize+=mtsets.readOld_isMT        ?_hpatch_align_upper(hcache_old_mt_t_memSize(),kAlignSize):0;
    objsMemSize+=mtsets.writeNew_isMT       ?_hpatch_align_upper(houtput_mt_t_memSize(),kAlignSize):0;
    return objsMemSize;
}


hpatch_mt_manager_t* hpatch_mt_manager_open(const hpatch_TStreamOutput** pout_newData,
                                            const hpatch_TStreamInput**  poldData,
                                            const hpatch_TStreamInput**  psingleCompressedDiff,
                                            hpatch_StreamPos_t*          pdiffData_pos,
                                            hpatch_StreamPos_t*          pdiffData_posEnd,
                                            hpatch_StreamPos_t           uncompressedSize,
                                            hpatch_TDecompress**         pdecompressPlugin,
                                            size_t                       stepMemSize,
                                            hpatch_byte** ptemp_cache,hpatch_byte** ptemp_cache_end,
                                            sspatch_coversListener_t**   pcoversListener,
                                            hpatch_BOOL                  isOnStepCoversInThread,
                                            size_t                       kCacheCount,
                                            hpatchMTSets_t               mtsets){
    size_t         objsMemSize;
    size_t         workBufCount;
    size_t         workBufNodeSize;
    size_t         kMinTempCacheSize;
    hpatch_byte* temp_cache=*ptemp_cache;
    hpatch_byte* temp_cache_end=*ptemp_cache_end;
    hpatch_byte* wbufsMem=0;
    hpatch_size_t workBufSize;
    hpatch_mt_manager_t* self=0;
    const size_t threadNum=_hpatchMTSets_threadNum(mtsets);
    assert(threadNum>1);
    objsMemSize =_getObjsMemSize(mtsets,&workBufCount);
    kMinTempCacheSize=objsMemSize+kMinBufNodeSize*(workBufCount+kCacheCount)+(kAlignSize-1);
    if (stepMemSize+kMinTempCacheSize>(hpatch_size_t)(temp_cache_end-temp_cache)) return 0;
    if (!mtsets.readOld_isMT){
        if (_patch_is_can_cache_all_old((*poldData)->streamSize,stepMemSize+kMinTempCacheSize,temp_cache_end-temp_cache)){
            size_t cacheAllNeedSize=(size_t)_patch_cache_all_old_needSize((*poldData)->streamSize,0);
            temp_cache_end-=cacheAllNeedSize; // patch_single_stream_diff() will load all old data into memory 
        }
    #if (_IS_NEED_CACHE_OLD_BY_COVERS)
        else{
            size_t cacheStepNeedSize;
            kMinTempCacheSize=objsMemSize+kBetterBufNodeSize*(workBufCount+kCacheCount)+(kAlignSize-1);
            cacheStepNeedSize=_patch_step_cache_old_canUsedSize(stepMemSize,kMinTempCacheSize,temp_cache_end-temp_cache);
            if (cacheStepNeedSize>0)
                temp_cache_end-=cacheStepNeedSize; // patch_single_stream_diff() will cache part of old by step
        }
    #endif // _IS_NEED_CACHE_OLD_BY_COVERS
    }
    {
        size_t memCacheSize=(temp_cache_end-temp_cache)-objsMemSize-stepMemSize;
        workBufNodeSize=memCacheSize/(workBufCount+kCacheCount)/kAlignSize*kAlignSize;
        workBufSize=TWorkBuf_getWorkBufSize(workBufNodeSize);
    }
   
    self=(hpatch_mt_manager_t*)_hpatch_align_upper(temp_cache,kAlignSize);
    memset(self,0,sizeof(*self));
    temp_cache=(hpatch_byte*)_hpatch_align_upper(temp_cache+sizeof(hpatch_mt_manager_t),kAlignSize);

    self->h_mt=hpatch_mt_open(temp_cache,hpatch_mt_t_memSize(threadNum),threadNum);
    if (self->h_mt==0) goto _on_error;
    temp_cache=(hpatch_byte*)_hpatch_align_upper(temp_cache+hpatch_mt_t_memSize(threadNum),kAlignSize);
    wbufsMem=temp_cache;
    temp_cache+=workBufCount*workBufNodeSize;
    if (mtsets.readDiff_isMT){
        self->diffData=hinput_mt_open(temp_cache,hinput_mt_t_memSize(),self->h_mt,
                                      TWorkBuf_allocFreeList(&wbufsMem,kObjNodeCount,workBufNodeSize),workBufSize,
                                      *psingleCompressedDiff,*pdiffData_pos,*pdiffData_posEnd);
        if (self->diffData==0) goto _on_error;
        *psingleCompressedDiff=self->diffData;
        *pdiffData_posEnd=self->diffData->streamSize;
        temp_cache=(hpatch_byte*)_hpatch_align_upper(temp_cache+hinput_mt_t_memSize(),kAlignSize);
    }
    if (mtsets.decompressDiff_isMT){
        assert(*pdecompressPlugin);
        self->decDiffData=hinput_dec_mt_open(temp_cache,hinput_mt_t_memSize(),self->h_mt,
                                            TWorkBuf_allocFreeList(&wbufsMem,kObjNodeCount,workBufNodeSize),workBufSize,
                                             self->diffData?self->diffData:*psingleCompressedDiff,*pdiffData_pos,
                                             self->diffData?self->diffData->streamSize:*pdiffData_posEnd,
                                             *pdecompressPlugin,uncompressedSize);
        if (self->decDiffData==0) goto _on_error;
        *psingleCompressedDiff=self->decDiffData;
        *pdiffData_posEnd=self->decDiffData->streamSize;
        *pdecompressPlugin=0;
        temp_cache=(hpatch_byte*)_hpatch_align_upper(temp_cache+hinput_mt_t_memSize(),kAlignSize);
    }
    if (mtsets.readOld_isMT){
        sspatch_coversListener_t* out_coversListener=0;
        self->oldData=hcache_old_mt_open(temp_cache,hcache_old_mt_t_memSize(),self->h_mt,
                                         TWorkBuf_allocFreeList(&wbufsMem,kObjNodeCount,workBufNodeSize),workBufSize,
                                         *poldData,&out_coversListener,*pcoversListener,isOnStepCoversInThread);
        if (self->oldData==0) goto _on_error;
        *poldData=self->oldData;
        *pcoversListener=out_coversListener;
        temp_cache=(hpatch_byte*)_hpatch_align_upper(temp_cache+hcache_old_mt_t_memSize(),kAlignSize);
    }
    if (mtsets.writeNew_isMT){
        self->newData=houtput_mt_open(temp_cache,houtput_mt_t_memSize(),self->h_mt,
                                      TWorkBuf_allocFreeList(&wbufsMem,kObjNodeCount,workBufNodeSize),workBufSize,
                                      *pout_newData,0);
        if (self->newData==0) goto _on_error;
        *pout_newData=self->newData;
        temp_cache=(hpatch_byte*)_hpatch_align_upper(temp_cache+houtput_mt_t_memSize(),kAlignSize);
    }

    *ptemp_cache=temp_cache;
    return self;

_on_error:
    hpatch_mt_manager_close(self,hpatch_TRUE);
    return 0;
}

#define _mt_obj_free(free_fn,mt_obj) { if (mt_obj) { isOnError|=!free_fn(mt_obj); mt_obj=0; } }
hpatch_BOOL hpatch_mt_manager_close(struct hpatch_mt_manager_t* self,hpatch_BOOL isOnError){
    if (!self) return !isOnError;
    if (self->h_mt) hpatch_mt_waitAllThreadEnd(self->h_mt,isOnError);
    _mt_obj_free(hinput_mt_close,     self->diffData);
    _mt_obj_free(hinput_mt_close,     self->decDiffData);
    _mt_obj_free(hcache_old_mt_close, self->oldData);
    _mt_obj_free(houtput_mt_close,    self->newData);
    if (self->h_mt) { isOnError|=!hpatch_mt_close(self->h_mt,isOnError); self->h_mt=0; }
    return !isOnError;
}


#endif //_IS_USED_MULTITHREAD