//  parallel_channel.cpp
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
#include "parallel_channel.h"

#if (_IS_USED_MULTITHREAD)
#include <assert.h>
#include <deque>

class _CChannel_import{
public:
    explicit _CChannel_import(ptrdiff_t maxDataCount)
    :_locker(0),_wantSendCond(0),_sendCond(0),_acceptCond(0),
    _maxDataCount(maxDataCount),_waitingCount(0),_isClosed(false){
        _locker=locker_new();
        _wantSendCond=condvar_new();
        _sendCond=condvar_new();
        _acceptCond=condvar_new();
    }
    ~_CChannel_import(){
        close();
        while (true) { //wait all thread exit
            if (_waitingCount==0) break;
            {
                CAutoLocker locker(_locker);
                if (_waitingCount==0) break;
            }
            this_thread_yield(); //todo:优化?
        }
        locker_delete(_locker);
        assert(_dataList.empty()); // error, why?
    }
    void close(){
        if (_isClosed) return;
        {
            CAutoLocker locker(_locker);
            _isClosed=true;
        }
        condvar_broadcast(_wantSendCond);
        condvar_broadcast(_sendCond);
        condvar_broadcast(_acceptCond);
    }
    bool is_can_fast_send(bool isWait){
        if (_maxDataCount<0) return true;
        if (_maxDataCount==0) return false;
        while (true) {
            if (_isClosed) return false;
            {
                CAutoLocker locker(_locker);
                if (_isClosed) return false;
                if (_dataList.size()<(size_t)_maxDataCount) {
                    return true;
                }else if(!isWait){
                    return false;
                }//else wait
                ++_waitingCount;
                condvar_wait(_wantSendCond,&locker);
                --_waitingCount;
            }
        }
    }
    
    bool send(TChanData data,bool isWait){
        assert(data!=0);
        while (true) {
            if (_isClosed) return false;
            {
                CAutoLocker locker(_locker);
                if (_isClosed) return false;
                if ((_maxDataCount<=0)||(_dataList.size()<(size_t)_maxDataCount)) {
                    _dataList.push_back(data); break; //ok
                }else if(!isWait){
                    return false;
                }//else wait
                ++_waitingCount;
                condvar_wait(_sendCond,&locker);
                --_waitingCount;
            }
        }
        condvar_signal(_acceptCond);
        if (_maxDataCount==0){ //must wait
            while (true) { //wait _dataList empty
                if (_isClosed) break;
                {
                    CAutoLocker locker(_locker);
                    if (_isClosed) break;
                    if (_dataList.empty()) break;
                }
                this_thread_yield(); //todo:优化;
            }
        }
        return true;
    }
    TChanData accept(bool isWait){
        TChanData result=0;
        while (true) {
            CAutoLocker locker(_locker);
            if (!_dataList.empty()) {
                result=_dataList.front();
                _dataList.pop_front();
                break; //ok
            }else if(_isClosed){
                return 0;
            }else if(!isWait){
                return 0;
            }//else wait
            ++_waitingCount;
            condvar_wait(_acceptCond,&locker);
            --_waitingCount;
        }
        condvar_signal(_sendCond);
        condvar_signal(_wantSendCond);
        return result;
    }
private:
    HLocker     _locker;
    HCondvar    _wantSendCond;
    HCondvar    _sendCond;
    HCondvar    _acceptCond;
    std::deque<TChanData>   _dataList;
    const ptrdiff_t         _maxDataCount;
    volatile size_t         _waitingCount;
    volatile bool           _isClosed;
};


CChannel::CChannel(ptrdiff_t maxDataCount):_import(0){
    _import=new _CChannel_import(maxDataCount);
}
CChannel::~CChannel(){
    delete _import;
}
void CChannel::close(){
    _import->close();
}
bool CChannel::is_can_fast_send(bool isWait){
    return _import->is_can_fast_send(isWait);
}
bool CChannel::send(TChanData data,bool isWait){
    return _import->send(data,isWait);
}
TChanData CChannel::accept(bool isWait){
    return _import->accept(isWait);
}

#endif //_IS_USED_MULTITHREAD
