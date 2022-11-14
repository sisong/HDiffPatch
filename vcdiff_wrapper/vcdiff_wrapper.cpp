// vcdiff_wrapper.cpp
// HDiffPatch
/*
 The MIT License (MIT)
 Copyright (c) 2022 HouSisong
 
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
#include "vcdiff_wrapper.h"
#include "vcpatch_code_table.h"
#include "../libHDiffPatch/HDiff/match_block.h"
#include "../libHDiffPatch/HDiff/private_diff/mem_buf.h"
#include "../libHDiffPatch/HDiff/private_diff/limit_mem_diff/stream_serialize.h"
#include "../libHDiffPatch/HPatch/patch.h"
#include <stdexcept>  //std::runtime_error
#define _check(value,info) { if (!(value)) { throw std::runtime_error(info); } }
static const unsigned char kVcDiffType[3]={('V'|(1<<7)),('C'|(1<<7)),('D'|(1<<7))};
#define kVcDiffVersion 0

namespace hdiff_private{

static void _getSrcWindow(const TCovers& covers,hpatch_StreamPos_t* out_srcPos,hpatch_StreamPos_t* out_srcEnd){
    const size_t count=covers.coverCount();
    if (count==0){
        *out_srcPos=0;
        *out_srcEnd=0;
        return;
    }
    hpatch_StreamPos_t srcPos=~(hpatch_StreamPos_t)0;
    hpatch_StreamPos_t srcEnd=0;
    for (size_t i=0;i<count;++i){
        TCover c; covers.covers(i,&c);
        hpatch_StreamPos_t pos=c.oldPos;
        srcPos=(pos<srcPos)?pos:srcPos;
        pos+=c.length;
        srcEnd=(pos<srcEnd)?srcEnd:pos;
    }
    *out_srcPos=srcPos;
    *out_srcEnd=srcEnd;
}


static const hpatch_byte _code_NULL_MODE = 255;
static inline bool _code_is_empty(hpatch_byte type) { return type==vcdiff_code_NOOP; }
struct vccode_t{
    hpatch_byte         type;
    hpatch_byte         mode;
    hpatch_StreamPos_t  size;
    hpatch_StreamPos_t  addr;
    inline vccode_t(){ set_empty(); }
    inline bool is_empty()const { return _code_is_empty(type); }
    inline void set_empty(){
        type=vcdiff_code_NOOP;
        mode=_code_NULL_MODE;
        size=hpatch_kNullStreamPos;
        addr=hpatch_kNullStreamPos;
    }
    inline void set_add_code(hpatch_StreamPos_t _size){
        type=vcdiff_code_ADD;
        mode=_code_NULL_MODE;
        size=_size;
        addr=hpatch_kNullStreamPos;
    }
    inline void set_copy_code(hpatch_StreamPos_t _size,hpatch_byte _mode,hpatch_StreamPos_t _addr){
        type=vcdiff_code_COPY;
        mode=_mode;
        size=_size;
        addr=_addr;
    }
};

struct vc_encoder{
    vc_encoder(std::vector<unsigned char>& inst,std::vector<unsigned char>& addr)
    :near_array(&_self_hear_near_array[2]),near_index(0),here(0),out_inst(inst),out_addr(addr){
        memset(same_array,0,sizeof(same_array));
        memset(_self_hear_near_array,0,sizeof(_self_hear_near_array));
    }
    
    void encode(const TCovers& covers,hpatch_StreamPos_t targetLen,
                hpatch_StreamPos_t srcWindowPos,hpatch_StreamPos_t srcWindowLen){
        assert(here==0);
        const size_t count=covers.coverCount();
        for (size_t i=0;i<=count;++i){
            TCover c;
            if (i<count){
                covers.covers(i,&c);
            }else{
                c.oldPos=srcWindowPos;
                c.newPos=targetLen;
                c.length=0;
            }
            if (here<c.newPos){
                curCode.set_add_code(c.newPos-here);
                emit_code();
                here=c.newPos;
            }
            if (c.length>0){
                hpatch_StreamPos_t addr=c.oldPos-srcWindowPos;
                hpatch_StreamPos_t subAddr=addr;
                hpatch_byte mode=_get_mode(srcWindowLen,&subAddr);
                curCode.set_copy_code(c.length,mode,subAddr);
                emit_code();
                here=c.newPos+c.length;
                vcdiff_update_addr(same_array,near_array,&near_index,addr);
            }
        }
        assert(curCode.is_empty());
        emit_code();
        assert(prevCode.is_empty());
    }
private:
    hpatch_StreamPos_t same_array[vcdiff_s_same*256];
    hpatch_StreamPos_t _self_hear_near_array[2+vcdiff_s_near];
    hpatch_StreamPos_t* near_array;
    hpatch_StreamPos_t near_index;
    hpatch_StreamPos_t here;
    std::vector<unsigned char>& out_inst;
    std::vector<unsigned char>& out_addr;
    vccode_t  prevCode;
    vccode_t  curCode;

    inline void _emit_inst(hpatch_byte inst){
        out_inst.push_back(inst);
    }
    inline void _emit_addr(hpatch_byte mode,hpatch_StreamPos_t addr){
        if (mode<(2+vcdiff_s_near))
            packUInt(out_addr,addr);
        else
            out_addr.push_back((hpatch_byte)addr);
    }
    inline void _emit_size(hpatch_StreamPos_t size){
        packUInt(out_inst,size);
    }
    hpatch_byte _get_mode(hpatch_StreamPos_t srcWindowLen,hpatch_StreamPos_t* paddr){
        hpatch_StreamPos_t addr=*paddr;
        _self_hear_near_array[1]=addr*2-(srcWindowLen+here);
        hpatch_StreamPos_t minSubAddr=addr;
        hpatch_byte        minSubi=0;
        for (hpatch_byte i=0;i<(2+vcdiff_s_near);i++){
            hpatch_StreamPos_t subAddr=addr-_self_hear_near_array[i];
            if (subAddr<=127){
                *paddr=subAddr;
                return i;
            }
            if (subAddr<minSubAddr){
                minSubAddr=subAddr;
                minSubi=i;
            }
        }
        size_t samei=(size_t)(addr%(vcdiff_s_same*256));
        if (same_array[samei]==addr){
            *paddr=samei%256;
            return (hpatch_byte)(samei/256)+(2+vcdiff_s_near);
        }
        *paddr=minSubAddr;
        return minSubi;
    }

    void emit_code(){
        const hpatch_byte _type_pos=curCode.type;
        if (prevCode.is_empty())
            std::swap(prevCode,curCode);
        if (prevCode.is_empty()) return;
        if (curCode.is_empty()&&(!_code_is_empty(_type_pos))) return;
        switch (prevCode.type){
            case vcdiff_code_ADD: {
                if ((prevCode.size<=4)&&(curCode.type==vcdiff_code_COPY)){ // encode 2 type?
                    if ((curCode.size==4)&&(curCode.mode>=6)){
                        _emit_inst((hpatch_byte)(235+(curCode.mode-6)*4+(prevCode.size-1)));
                        _emit_addr(curCode.mode,curCode.addr);
                        prevCode.set_empty();
                        curCode.set_empty();
                        break;
                    }else if ((2>=(hpatch_StreamPos_t)(curCode.size-4))&&(curCode.mode<6)){
                        _emit_inst((hpatch_byte)(163+curCode.mode*12+(prevCode.size-1)*3+(curCode.size-4)));
                        _emit_addr(curCode.mode,curCode.addr);
                        prevCode.set_empty();
                        curCode.set_empty();
                        break;
                    }
                }
                //encode 1 type
                if (prevCode.size<=17) {
                    _emit_inst((hpatch_byte)(prevCode.size+1));
                }else{
                    _emit_inst(1);
                    _emit_size(prevCode.size);
                }
                prevCode=curCode;
                curCode.set_empty();
            } break;
            case vcdiff_code_COPY: {
                if ((prevCode.size==4)&&(curCode.size==1)&&(curCode.type==vcdiff_code_ADD)){ // encode 2 type?
                    _emit_inst(247+prevCode.mode);
                    _emit_addr(prevCode.mode,prevCode.addr);
                    prevCode.set_empty();
                    curCode.set_empty();
                    break;
                }
                //encode 1 type
                if (14>=(hpatch_StreamPos_t)(prevCode.size-4)){ // 4--18
                    _emit_inst((hpatch_byte)(19+prevCode.mode*16+(prevCode.size-3)));
                }else{
                    _emit_inst(19+prevCode.mode*16);
                    _emit_size(prevCode.size);
                }
                _emit_addr(prevCode.mode,prevCode.addr);
                prevCode=curCode;
                curCode.set_empty();
            } break;
        }
    }
};


static inline void _flushBuf(TDiffStream& outDiff,std::vector<unsigned char>& buf){
    outDiff.pushBack(buf.data(),buf.size());
    buf.clear();
}

static void serialize_vcdiff(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                             const TCovers& covers,const hpatch_TStreamOutput* out_diff,
                             const vcdiff_TCompress* compressPlugin,bool isZeroSubDiff=false){
    std::vector<unsigned char> buf;
    TDiffStream outDiff(out_diff);
    {//head
        pushBack(buf,kVcDiffType,sizeof(kVcDiffType));
        buf.push_back(kVcDiffVersion);
        assert((compressPlugin==0)||(compressPlugin->compress_type==kVcDiff_compressorID_no));
        buf.push_back(0);// Hdr_Indicator: No compression, no custom code table
        _flushBuf(outDiff,buf);
    }
    
    hpatch_StreamPos_t srcPos,srcEnd;
    {//only one window
        {
            _getSrcWindow(covers,&srcPos,&srcEnd);
            if (srcPos<srcEnd){
                buf.push_back(VCD_SOURCE); //Win_Indicator
                packUInt(buf,(hpatch_StreamPos_t)(srcEnd-srcPos));
                packUInt(buf,srcPos);//srcPos
            }else{
                buf.push_back(0); //Win_Indicator, no src window
            }
            _flushBuf(outDiff,buf);
        }
        const hpatch_StreamPos_t targetLen=newData->streamSize;
        hpatch_StreamPos_t deltaLen=targetLen+(targetLen/8)+256;
        TPlaceholder deltaLen_pos=outDiff.packUInt_pos(deltaLen); //need update deltaLen!
        const hpatch_StreamPos_t deltaDataBeginPos=outDiff.getWritedPos();
        const hpatch_StreamPos_t dataLen=TNewDataDiffStream::getDataSize(covers,targetLen);
        hpatch_StreamPos_t Delta_Indicator_pos;
        {
            packUInt(buf,targetLen);
            assert((compressPlugin==0)||(compressPlugin->compress_type==kVcDiff_compressorID_no));
            buf.push_back(0);// Delta_Indicator
            _flushBuf(outDiff,buf);
            Delta_Indicator_pos=outDiff.getWritedPos()-1;
        }

        {
            std::vector<unsigned char> inst;
            std::vector<unsigned char> addr;
            {
                vc_encoder encoder(inst,addr);
                encoder.encode(covers,targetLen,srcPos,srcEnd-srcPos);
            }

            packUInt(buf,dataLen);
            packUInt(buf,inst.size());
            packUInt(buf,addr.size());
            _flushBuf(outDiff,buf);
            deltaLen=(outDiff.getWritedPos()-deltaDataBeginPos) + dataLen+inst.size()+addr.size();
            outDiff.packUInt_update(deltaLen_pos,deltaLen);

            TNewDataDiffStream _newDataDiff(covers,newData);
            outDiff.pushStream(&_newDataDiff);
            outDiff.pushBack(inst.data(),inst.size());
            outDiff.pushBack(addr.data(),addr.size());
        }
    }
}

void _create_vcdiff(const unsigned char* newData,const unsigned char* cur_newData_end,const unsigned char* newData_end,
                    const unsigned char* oldData,const unsigned char* cur_oldData_end,const unsigned char* oldData_end,
                    const hpatch_TStreamOutput* out_diff,const vcdiff_TCompress* compressPlugin,
                    int kMinSingleMatchScore,bool isUseBigCacheMatch,
                    ICoverLinesListener* coverLinesListener,size_t threadNum){
    std::vector<hpatch_TCover_sz> covers;
    get_match_covers_by_sstring(newData,cur_newData_end,oldData,cur_oldData_end,covers,
                                kMinSingleMatchScore,isUseBigCacheMatch,coverLinesListener,threadNum);
    const TCovers _covers((void*)covers.data(),covers.size(),
                          sizeof(*covers.data())==sizeof(hpatch_TCover32));
    
    hdiff_TStreamInput newStream;
    hdiff_TStreamInput oldStream;
    mem_as_hStreamInput(&newStream,newData,newData_end);
    mem_as_hStreamInput(&oldStream,oldData,oldData_end);
    
    serialize_vcdiff(&newStream,&oldStream,_covers,out_diff,compressPlugin);
}

}//end namespace hdiff_private

using namespace hdiff_private;

void create_vcdiff(const unsigned char* newData,const unsigned char* newData_end,
                   const unsigned char* oldData,const unsigned char* oldData_end,
                   const hpatch_TStreamOutput* out_diff,const vcdiff_TCompress* compressPlugin,
                   int kMinSingleMatchScore,bool isUseBigCacheMatch,
                   ICoverLinesListener* coverLinesListener,size_t threadNum){
    _create_vcdiff(newData,newData_end,newData_end,oldData,oldData_end,oldData_end,
                   out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,
                   coverLinesListener,threadNum);
}
void create_vcdiff(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                   const hpatch_TStreamOutput* out_diff,const vcdiff_TCompress* compressPlugin,
                   int kMinSingleMatchScore,bool isUseBigCacheMatch,
                   ICoverLinesListener* coverLinesListener,size_t threadNum){
    TAutoMem oldAndNewData;
    loadOldAndNewStream(oldAndNewData,oldData,newData);
    size_t old_size=oldData?(size_t)oldData->streamSize:0;
    unsigned char* pOldData=oldAndNewData.data();
    unsigned char* pNewData=pOldData+old_size;
    unsigned char* pNewDataEnd=pNewData+(size_t)newData->streamSize;
    _create_vcdiff(pNewData,pNewDataEnd,pNewDataEnd,pOldData,pOldData+old_size,pOldData+old_size,
                   out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,
                   coverLinesListener,threadNum);
}

void create_vcdiff_stream(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                          const hpatch_TStreamOutput* out_diff,const vcdiff_TCompress* compressPlugin,
                          size_t kMatchBlockSize,size_t threadNum){
    TCoversBuf covers(newData->streamSize,oldData->streamSize);
    get_match_covers_by_block(newData,oldData,&covers,kMatchBlockSize,threadNum);
    serialize_vcdiff(newData,oldData,covers,out_diff,compressPlugin,true);
}


void create_vcdiff_block(unsigned char* newData,unsigned char* newData_end,
                         unsigned char* oldData,unsigned char* oldData_end,
                         const hpatch_TStreamOutput* out_diff,const vcdiff_TCompress* compressPlugin,
                         int kMinSingleMatchScore,bool isUseBigCacheMatch,
                         size_t matchBlockSize,size_t threadNum){
    if (matchBlockSize==0){
        _create_vcdiff(newData,newData_end,newData_end,oldData,oldData_end,oldData_end,
                       out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,0,threadNum);
        return;
    }
    TCoversOptimMB<TMatchBlock> coversOp(newData,newData_end,oldData,oldData_end,matchBlockSize,threadNum);
    _create_vcdiff(newData,coversOp.matchBlock->newData_end_cur,newData_end,
                   oldData,coversOp.matchBlock->oldData_end_cur,oldData_end,
                   out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,&coversOp,threadNum);   
}
void create_vcdiff_block(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                         const hpatch_TStreamOutput* out_diff,const vcdiff_TCompress* compressPlugin,
                         int kMinSingleMatchScore,bool isUseBigCacheMatch,
                         size_t matchBlockSize,size_t threadNum){
    TAutoMem oldAndNewData;
    loadOldAndNewStream(oldAndNewData,oldData,newData);
    size_t old_size=oldData?(size_t)oldData->streamSize:0;
    unsigned char* pOldData=oldAndNewData.data();
    unsigned char* pNewData=pOldData+old_size;
    create_vcdiff_block(pNewData,pNewData+(size_t)newData->streamSize,pOldData,pOldData+old_size,
                        out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,
                        matchBlockSize,threadNum);
}

bool check_vcdiff(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                  const hpatch_TStreamInput* diffStream,hpatch_TDecompress* decompressPlugin){
    const size_t kACacheBufSize=hpatch_kFileIOBufBetterSize;
    TAutoMem _cache(kACacheBufSize*(1+5));
    _TCheckOutNewDataStream out_newData(newData,_cache.data(),kACacheBufSize);
    _test_rt(vcpatch_with_cache(&out_newData,oldData,diffStream,decompressPlugin,hpatch_TRUE,
                                _cache.data()+kACacheBufSize,_cache.data_end()));
    _test_rt(out_newData.isWriteFinish());
    return true;
}
bool check_vcdiff(const unsigned char* newData,const unsigned char* newData_end,
                  const unsigned char* oldData,const unsigned char* oldData_end,
                  const unsigned char* diffData,const unsigned char* diffData_end,
                  hpatch_TDecompress* decompressPlugin){
    hdiff_TStreamInput newStream;
    hdiff_TStreamInput oldStream;
    hdiff_TStreamInput diffStream;
    mem_as_hStreamInput(&newStream,newData,newData_end);
    mem_as_hStreamInput(&oldStream,oldData,oldData_end);
    mem_as_hStreamInput(&diffStream,diffData,diffData_end);
    return check_vcdiff(&newStream,&oldStream,&diffStream,decompressPlugin);
}