//
//  mt_by_queue.h
//  sync_client
//  Created by housisong on 2019-09-29.
/*
 The MIT License (MIT)
 Copyright (c) 2019-2019 HouSisong
 
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
#ifndef mt_by_queue_h
#define mt_by_queue_h
#include "../../libParallel/parallel_channel.h"
#if (_IS_USED_MULTITHREAD)
#include <vector>

struct TMt_by_queue {
    const int    threadNum;
    const size_t workCount;
    inline explicit TMt_by_queue(int _threadNum,size_t _workCount,bool _isOutputNeedQueue=true)
    :threadNum(_threadNum),workCount(_workCount),isOutputNeedQueue(_isOutputNeedQueue),
    finishedThreadNum(0),inputWorkIndex(0){
        if (isOutputNeedQueue) {  threadChannels.resize(_threadNum);
                                  threadWorkIndexs.resize(_threadNum,0); } }
    inline void finish(){
        TAutoLocker _auto_locker(this);
        ++finishedThreadNum;
    }
    void waitAllFinish(){
        while(true){
            {   TAutoLocker _auto_locker(this);
                if (finishedThreadNum==threadNum) break;
            }//else
            this_thread_yield();
        }
    }
    struct TAutoLocker{
        inline TAutoLocker(TMt_by_queue* _mt)
            :mt(_mt){ if (mt) locker_enter(mt->mtdataLocker.locker); }
        inline ~TAutoLocker(){ if (mt) locker_leave(mt->mtdataLocker.locker); }
        TMt_by_queue* mt;
    };
    
    bool getWork(int threadIndex,size_t workIndex){
        TAutoLocker _auto_locker(this);
        if (workIndex==inputWorkIndex){
            threadWorkIndexs[threadIndex]=workIndex;
            if (isOutputNeedQueue&&(workIndex==0))
                wakeupChannel(threadChannels[threadIndex],0);
            ++inputWorkIndex;
            return true;
        }else{
            return false;
        }
    }

    struct TAutoInputLocker{//for get work by index
        inline TAutoInputLocker(TMt_by_queue* _mt)
            :mt(_mt){ if (mt) locker_enter(mt->readLocker.locker); }
        inline ~TAutoInputLocker(){ if (mt) locker_leave(mt->readLocker.locker); }
        TMt_by_queue* mt;
    };
    struct TAutoOutputLocker{//for Output Queue by workIndex
        inline TAutoOutputLocker(TMt_by_queue* _mt,int threadIndex,size_t _workIndex)
        :mt(_mt),workIndex(_workIndex){
            if (mt) mt->wait(threadIndex,workIndex); }
        inline ~TAutoOutputLocker(){ if (mt) mt->wakeup(workIndex+1); }
        TMt_by_queue* mt;
        size_t        workIndex;
    };
private:
    bool      isOutputNeedQueue;
    int       finishedThreadNum;
    size_t    inputWorkIndex;
    CHLocker  mtdataLocker;
    CHLocker  readLocker;
    std::vector<CChannel> threadChannels;
    std::vector<size_t>   threadWorkIndexs;
    
    bool wait(int threadIndex,size_t workWorkIndex){
        assert(isOutputNeedQueue);
        if (!isOutputNeedQueue) return true;
        CChannel& channel=threadChannels[threadIndex];
        TChanData cData=channel.accept(true);//wait
        if (cData==0) return false; //error
        size_t outputWorkIndex=(size_t)cData-1;
        return workWorkIndex==outputWorkIndex;
    }
    void wakeup(size_t outputWorkIndex){
        if (!isOutputNeedQueue) return;
        if (outputWorkIndex>=workCount) return;
        int threadIndex=getThread(outputWorkIndex);
        wakeupChannel(threadChannels[threadIndex],outputWorkIndex);
    }
    inline void wakeupChannel(CChannel& channel,size_t outputWorkIndex){
        if (outputWorkIndex>=workCount) return;
        TChanData cData=(TChanData)(size_t)(outputWorkIndex+1);
        channel.send(cData,true);
    }
    
    int getThread(size_t workIndex){
        while(true){
            {   TAutoLocker _auto_locker(this);
                for (int i=0;i<threadNum;++i)
                    if (threadWorkIndexs[i]==workIndex) return i;
            }//else
            this_thread_yield();
        }
    }
};
#endif
#endif // mt_by_queue_h
