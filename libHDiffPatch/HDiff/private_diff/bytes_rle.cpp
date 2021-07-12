//bytes_rle.cpp
//
/*
 The MIT License (MIT)
 Copyright (c) 2012-2021 HouSisong

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

#include "bytes_rle.h"
#include <assert.h>
#include "pack_uint.h"
#include "../../HPatch/patch_private.h"
namespace hdiff_private{

namespace {
    const size_t kStepSize=1024*16;
    
    typedef unsigned char TByte;
    typedef hpatch_StreamPos_t TUInt;

    static void rle_pushSame(std::vector<TByte>& out_ctrl,std::vector<TByte>& out_code,
                             TByte cur,TUInt count){
        assert(count>0);
        while (count>kMaxBytesRleLen){
            rle_pushSame(out_ctrl,out_code,cur,kMaxBytesRleLen);
            count-=kMaxBytesRleLen;
        }
        enum TByteRleType type;
        if (cur==0)
            type=kByteRleType_rle0;
        else if (cur==255)
            type=kByteRleType_rle255;
        else
            type=kByteRleType_rle;
        const TUInt packCount=(TUInt)(count-1);
        packUIntWithTag(out_ctrl,packCount,type,kByteRleType_bit);
        if (type==kByteRleType_rle)
            out_code.push_back(cur);
    }

    static inline void rle_pushNotSameCtrl(std::vector<TByte>& out_ctrl,TUInt count){
        packUIntWithTag(out_ctrl,(TUInt)(count-1),kByteRleType_unrle,kByteRleType_bit);
    }

    struct TStreamRLE{
        std::vector<unsigned char>&  ctrlBuf;
        std::vector<unsigned char>&  codeBuf;
        TByte       sameCur;
        size_t      sameCount;
        size_t      notSameCount;
        const size_t kRleMinSameSize;
        inline explicit TStreamRLE(std::vector<unsigned char>& _ctrlBuf,
                                   std::vector<unsigned char>& _codeBuf,int rle_parameter)
        :ctrlBuf(_ctrlBuf),codeBuf(_codeBuf),sameCur(0),
        sameCount(0),notSameCount(0),kRleMinSameSize(rle_parameter+1){
            assert((rle_parameter>=kRle_bestSize)&&(rle_parameter<=kRle_bestUnRleSpeed)); }
        void append(const unsigned char* appendData,const unsigned char* appendData_end){
            if (sameCount==0){
                if (appendData!=appendData_end){
                    sameCur=*appendData++;
                    sameCount=1;
                }else{
                    return;
                }
            }
            while (appendData!=appendData_end){
                TByte cur=*appendData++;
                if (cur==sameCur){
                    ++sameCount;
                }else{
                    endSame();
                    sameCur=cur;
                    sameCount=1;
                }
            }
        }
        inline void endSame(){
            size_t sign=((size_t)(253-(size_t)(TByte)(sameCur-1))) >> (sizeof(size_t)*8-1);
            if (sameCount+sign>kRleMinSameSize){//can rle
                endNoSame();
                rle_pushSame(ctrlBuf,codeBuf,sameCur,sameCount);
                sameCount=0;
            }else{
                notSameCount+=sameCount;
                while (sameCount--)
                    codeBuf.push_back(sameCur);
            }
        }
        inline void endNoSame(){
            if (notSameCount>1){
                rle_pushNotSameCtrl(ctrlBuf,notSameCount);
                notSameCount=0;
            }else if (notSameCount==1){
                notSameCount=0;
                TByte cur=codeBuf.back();
                codeBuf.pop_back();
                rle_pushSame(ctrlBuf,codeBuf,cur,1);
            }
        }
        inline void finishAppend(){
            endSame();
            endNoSame();
        }
    };
    
}//end namespace
    
void bytesRLE_save(std::vector<TByte>& out_ctrlBuf,std::vector<TByte>& out_codeBuf,
                    const TByte* src,const TByte* src_end,int rle_parameter){
    TStreamRLE rle(out_ctrlBuf,out_codeBuf,rle_parameter);
    rle.append(src,src_end);
    rle.finishAppend();
}

void bytesRLE_save(std::vector<unsigned char>& out_ctrlBuf,std::vector<unsigned char>& out_codeBuf,
                   const hpatch_TStreamInput* src,int rle_parameter){
    TStreamRLE rle(out_ctrlBuf,out_codeBuf,rle_parameter);
    TByte buf[kStepSize];
    hpatch_StreamPos_t readPos=0;
    while (readPos<src->streamSize){
        size_t len=kStepSize;
        if (len+readPos>src->streamSize)
            len=(size_t)(src->streamSize-readPos);
        if (!src->read(src,readPos,buf,buf+len))
            throw std::runtime_error("bytesRLE_save() src->read()");
        rle.append(buf,buf+len);
        readPos+=len;
    }
    rle.finishAppend();
}

void bytesRLE_save(std::vector<TByte>& out_code,
                   const TByte* src,const TByte* src_end,int rle_parameter){
    std::vector<TByte> ctrlBuf;
    std::vector<TByte> codeBuf;

    bytesRLE_save(ctrlBuf,codeBuf,src,src_end,rle_parameter);
    packUInt(out_code,(TUInt)ctrlBuf.size());
    pushBack(out_code,ctrlBuf);
    pushBack(out_code,codeBuf);
}

void bytesRLE_save(std::vector<TByte>& out_code,const hpatch_TStreamInput* src,int rle_parameter){
    std::vector<TByte> ctrlBuf;
    std::vector<TByte> codeBuf;

    bytesRLE_save(ctrlBuf,codeBuf,src,rle_parameter);
    packUInt(out_code,(TUInt)ctrlBuf.size());
    pushBack(out_code,ctrlBuf);
    pushBack(out_code,codeBuf);
}

    
    enum TLastType{
        lastType_same,
        lastType_v,
    };
    
    inline TLastType getLastType(const TSingleStreamRLE0& self){
        if (self.uncompressData.empty()){
            return lastType_same;
        }else{
            assert(self.len0==0);
            return lastType_v;
        }
    }
    
    inline void _out_uncompressData(TSingleStreamRLE0& self){
        size_t saved=0;
        while (self.uncompressData.size()-saved>kMaxBytesRleLen){
            packUInt(self.fixed_code, kMaxBytesRleLen);
            pushBack(self.fixed_code, self.uncompressData.data()+saved,self.uncompressData.data()+saved+kMaxBytesRleLen);
            packUInt(self.fixed_code,0);
            saved+=kMaxBytesRleLen;
        }
        packUInt(self.fixed_code, self.uncompressData.size()-saved);
        pushBack(self.fixed_code, self.uncompressData.data()+saved,self.uncompressData.data()+self.uncompressData.size());
        self.uncompressData.clear();
    }
    inline hpatch_StreamPos_t _out_uncompressData_codeSize(hpatch_StreamPos_t dataSize){
        hpatch_StreamPos_t codeSize=0;
        hpatch_StreamPos_t saved=0;
        while (dataSize-saved>kMaxBytesRleLen){
            codeSize+=hpatch_packUInt_size(kMaxBytesRleLen);
            codeSize+=kMaxBytesRleLen;
            codeSize+=hpatch_packUInt_size(0);
            saved+=kMaxBytesRleLen;
        }
        codeSize+=hpatch_packUInt_size(dataSize-saved);
        codeSize+=(hpatch_StreamPos_t)(dataSize-saved);
        return codeSize;
    }

    inline void _out_0Data(TSingleStreamRLE0& self){
        hpatch_StreamPos_t saved=0;
        while (self.len0-saved>kMaxBytesRleLen){
            packUInt(self.fixed_code,kMaxBytesRleLen);
            packUInt(self.fixed_code,0);
            saved+=kMaxBytesRleLen;
        }
        packUInt(self.fixed_code,(hpatch_StreamPos_t)(self.len0-saved));
        self.len0=0;
    }
    inline hpatch_StreamPos_t _out_0Data_codeSize(hpatch_StreamPos_t len0){
        hpatch_StreamPos_t codeSize=0;
        hpatch_StreamPos_t saved=0;
        while (len0-saved>kMaxBytesRleLen){
            codeSize+=hpatch_packUInt_size(kMaxBytesRleLen);
            codeSize+=hpatch_packUInt_size(0);
            saved+=kMaxBytesRleLen;
        }
        codeSize+=hpatch_packUInt_size((hpatch_StreamPos_t)(len0-saved));
        return codeSize;
    }


    static void _maxCodeSize(TLastType& lastType,hpatch_StreamPos_t& curLen0,hpatch_StreamPos_t& curLenv,
                             hpatch_StreamPos_t& fixedLen,const unsigned char* appendData,const unsigned char* appendData_end){
        while (appendData!=appendData_end) {
            unsigned char v;
            size_t sLen0=0;
            do {
                v=*appendData++;
                if (0==v)
                    ++sLen0;
                else
                    break;
            }while (appendData!=appendData_end);
            if (sLen0>0){
                if (lastType==lastType_v){
                    fixedLen += _out_uncompressData_codeSize(curLenv);
                    curLenv = 0;
                }
                curLen0+=sLen0;
                lastType=lastType_same;
                if (v==0)
                    break;//appendData==appendData_end
            }
            if (lastType==lastType_same){
                fixedLen += _out_0Data_codeSize(curLen0);
                curLen0 = 0;
            }
            ++curLenv;
            lastType=lastType_v;
        }
    }
    static void _maxCodeSize_end(TLastType& lastType,hpatch_StreamPos_t& curLen0,
                                 hpatch_StreamPos_t& curLenv,hpatch_StreamPos_t& fixedLen){
        if (curLenv>0){
            fixedLen += _out_uncompressData_codeSize(curLenv);
            curLenv=0;
        }
        if (curLen0>0){
            fixedLen += _out_0Data_codeSize(curLen0);
            curLen0=0;
        }
    }
    
    hpatch_StreamPos_t TSingleStreamRLE0::maxCodeSize(const unsigned char* appendData,const unsigned char* appendData_end) const{
        TLastType lastType=getLastType(*this);
        hpatch_StreamPos_t curLen0=this->len0;
        hpatch_StreamPos_t curLenv=this->uncompressData.size();
        hpatch_StreamPos_t fixedLen=this->fixed_code.size();
        _maxCodeSize(lastType,curLen0,curLenv,fixedLen,appendData,appendData_end);
        _maxCodeSize_end(lastType,curLen0,curLenv,fixedLen);
        return fixedLen;
    }
    
    hpatch_StreamPos_t TSingleStreamRLE0::maxCodeSizeByZeroLen(hpatch_StreamPos_t appendZeroLen) const{
        hpatch_StreamPos_t fixedLen=this->fixed_code.size();
        if (appendZeroLen==0) return fixedLen;
        TLastType lastType=getLastType(*this);
        hpatch_StreamPos_t curLen0=this->len0;
        hpatch_StreamPos_t curLenv=this->uncompressData.size();
        curLen0+=appendZeroLen;
        if (lastType==lastType_v){
            fixedLen += _out_uncompressData_codeSize(curLenv);
            curLenv = 0;
        }
        lastType=lastType_same;
        _maxCodeSize_end(lastType,curLen0,curLenv,fixedLen);
        return fixedLen;
    }

    hpatch_StreamPos_t TSingleStreamRLE0::maxCodeSize(const hpatch_TStreamInput* appendData) const{
        TLastType lastType=getLastType(*this);
        hpatch_StreamPos_t curLen0=this->len0;
        hpatch_StreamPos_t curLenv=this->uncompressData.size();
        hpatch_StreamPos_t fixedLen=this->fixed_code.size();
        
        TByte buf[kStepSize];
        hpatch_StreamPos_t readPos=0;
        while (readPos<appendData->streamSize){
            size_t len=kStepSize;
            if (len+readPos>appendData->streamSize)
                len=(size_t)(appendData->streamSize-readPos);
            if (!appendData->read(appendData,readPos,buf,buf+len))
                throw std::runtime_error("TSingleStreamRLE0::maxCodeSize() appendData->read()");
            _maxCodeSize(lastType,curLen0,curLenv,fixedLen,buf,buf+len);
            readPos+=len;
        }
        _maxCodeSize_end(lastType,curLen0,curLenv,fixedLen);
        return fixedLen;
    }
    
    
    void TSingleStreamRLE0::appendByZeroLen(hpatch_StreamPos_t appendZeroLen){
        if (appendZeroLen){
            TLastType lastType=getLastType(*this);
            if (lastType==lastType_v)
                _out_uncompressData(*this);
            len0+=appendZeroLen;
        }
    }

    void TSingleStreamRLE0::append(const unsigned char* appendData,const unsigned char* appendData_end){
        TLastType lastType=getLastType(*this);
        while (appendData!=appendData_end) {
            unsigned char v;
            size_t sLen0=0;
            do {
                v=*appendData++;
                if (0==v)
                    ++sLen0;
                else
                    break;
            }while (appendData!=appendData_end);
            if (sLen0>0){
                if (lastType==lastType_v)
                    _out_uncompressData(*this);
                len0+=sLen0;
                lastType=lastType_same;
                if (v==0)
                    break;//appendData==appendData_end
            }
            if (lastType==lastType_same)
                _out_0Data(*this);
            uncompressData.push_back(v);
            lastType=lastType_v;
        }
    }
    
    void TSingleStreamRLE0::append(const hpatch_TStreamInput* appendData){
        TByte buf[kStepSize];
        hpatch_StreamPos_t readPos=0;
        while (readPos<appendData->streamSize){
            size_t len=kStepSize;
            if (len+readPos>appendData->streamSize)
                len=(size_t)(appendData->streamSize-readPos);
            if (!appendData->read(appendData,readPos,buf,buf+len))
                throw std::runtime_error("TSingleStreamRLE0::append() appendData->read()");
            append(buf,buf+len);
            readPos+=len;
        }
    }
    
    void TSingleStreamRLE0::finishAppend(){
        if ((fixed_code.empty())||(len0>0))
            _out_0Data(*this);
        if (!uncompressData.empty())
            _out_uncompressData(*this);
    }
}//namespace hdiff_private
