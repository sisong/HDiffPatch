//  parallel_channel.h
/*
 The MIT License (MIT)
 Copyright (c) 2018 HouSisong
 
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

#ifndef parallel_channel_h
#define parallel_channel_h
#include "parallel_import.h"
#if (_IS_USED_MULTITHREAD)
#include <stdint.h> //uint32
#include <assert.h>
#include <stddef.h> //for size_t ptrdiff_t

struct CHLocker{
    HLocker locker;
    inline CHLocker():locker(0) { locker=locker_new(); }
    inline ~CHLocker() { locker_delete(locker); }
};

#if (_IS_USED_CPP11THREAD)
#   include <mutex>
    struct CAutoLocker:public _TLockerBox_name {
        inline CAutoLocker(HLocker _locker)
            :_TLockerBox_name(){ if (_locker) { _TLockerBox_name _t(*(std::mutex*)_locker); _t.swap(*this); }  }
        inline ~CAutoLocker(){ }
    };
#else
    struct CAutoLocker:public TLockerBox {
        inline CAutoLocker(HLocker _locker){ locker=_locker; if (locker) locker_enter(locker); }
        inline ~CAutoLocker(){ if (locker) locker_leave(locker); }
    };
#endif

    //通道交互数据;
    typedef void* TChanData;

    class _CChannel_import;
    //通道;
    class CChannel{
    public:
        explicit CChannel(ptrdiff_t maxDataCount=-1);
        ~CChannel();
        void close();
        bool is_can_fast_send(bool isWait); //mybe not need wait when send
        bool send(TChanData data,bool isWait); //can't send null
        TChanData accept(bool isWait); //result can null;
    private:
        _CChannel_import* _import;
    };

//用通道传递来共享数据;
struct TMtByChannel {
    CChannel read_chan;
    CChannel work_chan;
    CChannel data_chan;

    void on_error(){
        {
            CAutoLocker _auto_locker(_locker.locker);
            if (_is_on_error) return;
            _is_on_error=true;
        }
        closeAndClear();
    }
    
    bool start_threads(int threadCount,TThreadRunCallBackProc threadProc,void* workData,bool isUseThisThread){
        for (int i=0;i<threadCount;++i){
            if (isUseThisThread&&(i==threadCount-1)){
                thread_on(1);
                threadProc(i,workData);
            }else{
                thread_on(1);
                try{
                    thread_parallel(1,threadProc,workData,0,i);
                }catch(...){
                    thread_on(-1);
                    on_error();
                    return false;
                }
            }
        }
        return true;
    }
    inline void thread_end(){
        _end_chan.send((TChanData)1,true);
    }
    struct TAutoThreadEnd{
        inline  explicit TAutoThreadEnd(TMtByChannel& mt) :_mt(mt) {  }
        inline ~TAutoThreadEnd() { _mt.thread_end(); }
        TMtByChannel&  _mt;
    };

    inline  explicit TMtByChannel():_is_on_error(false),_is_thread_on(0) {}
    inline ~TMtByChannel() { closeAndClear(); wait_all_thread_end(); _end_chan.close(); while (_end_chan.accept(false)) {} }
    inline bool is_on_error()const{ CAutoLocker _auto_locker(_locker.locker); return _is_on_error; }
    
    inline void finish(){ // wait all threads exit
        close();
        wait_all_thread_end();
    }
    inline void wait_all_thread_end(){
        while(_is_thread_on){
            --_is_thread_on;
            _end_chan.accept(true);
        }
    }
protected:
    CHLocker  _locker;
    volatile bool _is_on_error;
    
    inline void close() {
        read_chan.close();
        work_chan.close();
        data_chan.close();
    }
    void closeAndClear(){
        close();
        while(read_chan.accept(false)) {}
        while(work_chan.accept(false)) {}
        while(data_chan.accept(false)) {}
    }
private:
    inline void thread_on(int threadNum){
        _is_thread_on+=threadNum;
    }
    volatile size_t _is_thread_on;
    CChannel  _end_chan;
};

#endif //_IS_USED_MULTITHREAD
#endif //parallel_channel_h
