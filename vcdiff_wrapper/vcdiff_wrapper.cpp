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
#include <algorithm>  //std::inplace_merge
#include <string>
#define _check(value,info) { if (!(value)) { throw std::runtime_error(info); } }
static const hpatch_byte kVcDiffType[3]={('V'|(1<<7)),('C'|(1<<7)),('D'|(1<<7))};
#define kVcDiffVersion 0

static const char* kHDiffzAppHead="$hdiffz-VCDIFF&";
static const char* kHDiffzAppHead_version="#a";

static std::string getHDiffzAppHead(const vcdiff_TCompress* compressPlugin){
    std::string str(kHDiffzAppHead);
    if (compressPlugin&&compressPlugin->compress)
        str+=compressPlugin->compress->compressType();
    return str+kHDiffzAppHead_version;
}

namespace hdiff_private{

static size_t fixRedundentEncoding(std::vector<hpatch_byte>& buf,hpatch_StreamPos_t buf_pos,
                                   const TPlaceholder& update_pos,hpatch_StreamPos_t updateLen){
    const size_t pkSize=hpatch_packUInt_size(updateLen);
    const size_t upkSize=(size_t)update_pos.size();
    assert(pkSize<=upkSize);
    const size_t retSize=upkSize-pkSize;
    if (retSize==0) return 0;
    // [             buf             ]
    //          [update_pos-buf_pos]
    //              [    pkSize    ]
    size_t upki=(size_t)(update_pos.pos-buf_pos);
    size_t pki=upki+retSize;
    hpatch_byte* bufpki=buf.data()+pki;
    hpatch_byte* bufpki_end=bufpki+pkSize;
    _check(hpatch_packUInt(&bufpki,bufpki_end,updateLen),"update packed len error!");
    _check(bufpki==bufpki_end,"update packed len error!");
    memmove(buf.data()+retSize,buf.data(),upki);
    return retSize;
}

static void _getSrcWindow(const TCovers& covers,size_t coveri,size_t coveriEnd,
                          hpatch_StreamPos_t* out_srcPos,hpatch_StreamPos_t* out_srcEnd){
    if (coveri==coveriEnd){
        *out_srcPos=0;
        *out_srcEnd=0;
        return;
    }
    hpatch_StreamPos_t srcPos=~(hpatch_StreamPos_t)0;
    hpatch_StreamPos_t srcEnd=0;
    for (size_t i=coveri;i<coveriEnd;++i){
        TCover c; covers.covers(i,&c);
        hpatch_StreamPos_t pos=c.oldPos;
        srcPos=(pos<srcPos)?pos:srcPos;
        pos+=c.length;
        srcEnd=(pos<srcEnd)?srcEnd:pos;
    }
    *out_srcPos=srcPos;
    *out_srcEnd=srcEnd;
}

static hpatch_StreamPos_t _getTargetWindow(hpatch_StreamPos_t targetPos,hpatch_StreamPos_t targetPosEnd,
                                           const TCovers& covers,size_t coveri,size_t* coveriEnd,size_t kMaxTargetWindowsSize){
    hpatch_StreamPos_t targetLen=0;
    const size_t count=covers.coverCount();
    *coveriEnd=coveri;
    for (size_t i=coveri;i<=count;++i){
        TCover c; 
        if (i<count){
            covers.covers(i,&c);
        }else{
            c.oldPos=hpatch_kNullStreamPos;
            c.newPos=targetPosEnd;
            c.length=0;
        }
        hpatch_StreamPos_t pos=c.newPos;
        assert(pos>=targetPos);
        if (pos>targetPos){
            hpatch_StreamPos_t targetInc=(pos-targetPos);
            if (targetLen+targetInc<kMaxTargetWindowsSize){
                targetLen+=targetInc;
                targetPos+=targetInc;
            }else{
                targetLen=kMaxTargetWindowsSize;
                break;//ok
            }
        }

        if (i<count){
            hpatch_StreamPos_t targetInc=c.length;
            if (targetLen+targetInc<=kMaxTargetWindowsSize){
                *coveriEnd=i+1;
                targetLen+=targetInc;
                targetPos+=targetInc;
            }else{
                assert(targetLen>0);
                break;//ok
            }
        }
    }
    return targetLen;
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
    vc_encoder(std::vector<hpatch_byte>& inst,std::vector<hpatch_byte>& addr)
    :near_array(&_self_hear_near_array[2]),near_index(0),here(0),out_inst(inst),out_addr(addr){
        inst.clear();
        addr.clear();
        memset(same_array,0,sizeof(same_array));
        memset(_self_hear_near_array,0,sizeof(_self_hear_near_array));
    }
    
    void encode(const TCovers& covers,size_t coveri,
                hpatch_StreamPos_t targetPos,hpatch_StreamPos_t targetLen,
                hpatch_StreamPos_t srcWindowPos,hpatch_StreamPos_t srcWindowLen){
        size_t count=covers.coverCount();
        const hpatch_StreamPos_t targetEnd=targetPos+targetLen;
        here=targetPos;
        for (size_t i=coveri;i<=count;++i){
            TCover c;
            if (i<count){
                covers.covers(i,&c);
            }else{
                c.oldPos=hpatch_kNullStreamPos;
                c.newPos=targetPos+targetLen;
                c.length=0;
            }

            if (here<c.newPos){
                hpatch_StreamPos_t targetInc=c.newPos-here;
                if (here+targetInc>targetEnd)
                    targetInc=targetEnd-here;
                if (targetInc>0){
                    curCode.set_add_code(targetInc);
                    emit_code();
                    here+=targetInc;
                }
                if (here==targetEnd)
                    break; //ok
            }
            if (c.length>0){
                hpatch_StreamPos_t targetInc=c.length;
                if (here+targetInc>targetEnd)
                    break; //ok
                hpatch_StreamPos_t addr=c.oldPos-srcWindowPos;
                hpatch_StreamPos_t subAddr=addr;
                hpatch_byte mode=_get_mode(targetPos,srcWindowLen,&subAddr);
                curCode.set_copy_code(targetInc,mode,subAddr);
                emit_code();
                here+=targetInc;
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
    std::vector<hpatch_byte>& out_inst;
    std::vector<hpatch_byte>& out_addr;
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
    hpatch_byte _get_mode(hpatch_StreamPos_t targetPos,hpatch_StreamPos_t srcWindowLen,hpatch_StreamPos_t* paddr){
        hpatch_StreamPos_t addr=*paddr;
        _self_hear_near_array[1]=addr*2-(srcWindowLen+here-targetPos);
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
                assert(prevCode.size>0);
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


static inline void _flushBuf(TDiffStream& outDiff,std::vector<hpatch_byte>& buf){
    outDiff.pushBack(buf.data(),buf.size());
    buf.clear();
}

struct TVcCompress{
    inline TVcCompress():encoder(0),compressPlugin(0){}
    inline ~TVcCompress(){ if (encoder) compressPlugin->compress_close(compressPlugin->compress,encoder); }
    vcdiff_compressHandle   encoder;
    const vcdiff_TCompress* compressPlugin;
};

static bool compressVcDiffData(std::vector<hpatch_byte>& out_code,TVcCompress* compress,const hpatch_TStreamInput* data){
    hpatch_StreamPos_t uncompressSize=data->streamSize;
    const size_t _kMinDataSizeByCompress=32;
    if (uncompressSize>_kMinDataSizeByCompress){
        out_code.clear();
        packUInt(out_code,uncompressSize);
        const size_t pkSize=out_code.size();

        const vcdiff_TCompress* compressPlugin=compress->compressPlugin;
        const hpatch_BOOL isWriteHead=(compress->encoder==0);
        if (compress->encoder==0){
            compress->encoder=compressPlugin->compress_open(compressPlugin->compress);
            _check(compress->encoder!=0,"compressVcDiffData() compressPlugin->compress_open");
        }

        hpatch_StreamPos_t maxCodeSize=compressPlugin->compress->maxCompressedSize(data->streamSize);
        assert(maxCodeSize+pkSize==(size_t)(maxCodeSize+pkSize));
        out_code.resize(pkSize+(size_t)maxCodeSize);
        hpatch_TStreamOutput codeStream;
        mem_as_hStreamOutput(&codeStream,out_code.data()+pkSize,out_code.data()+out_code.size());
        hpatch_StreamPos_t codeSize=compressPlugin->compress_encode(compress->encoder,&codeStream,data,
                                                                    isWriteHead,hpatch_FALSE);
        _check(codeSize>0,"compressVcDiffData() compressPlugin->compress_encode");
        out_code.resize(pkSize+(size_t)codeSize); //ok
        return true;
    }else{
        assert(uncompressSize==(size_t)uncompressSize);
        out_code.resize((size_t)uncompressSize);
        _check(data->read(data,0,out_code.data(),out_code.data()+out_code.size()),
               "compressVcDiffData() data->read");
        return false;
    }
}



static void serialize_vcdiff(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                             const TCovers& covers,const hpatch_TStreamOutput* out_diff,
                             const vcdiff_TCompress* compressPlugin,size_t kMaxTargetWindowsSize){
    std::vector<hpatch_byte> buf;
    TDiffStream outDiff(out_diff);
    {//head
        pushBack(buf,kVcDiffType,sizeof(kVcDiffType));
        buf.push_back(kVcDiffVersion);
        const bool isHaveCompresser=(compressPlugin!=0)&&(compressPlugin->compress_type!=kVcDiff_compressorID_no);
        if (!isHaveCompresser)
            compressPlugin=0;
        hpatch_byte Hdr_Indicator = ((isHaveCompresser?1:0)<<0) // VCD_DECOMPRESS
                                  | (0<<1)  // VCD_CODETABLE, no custom code table
                                  | (1<<2); // VCD_APPHEADER
        buf.push_back(Hdr_Indicator); 
        if (isHaveCompresser)//VCD_DECOMPRESS
            buf.push_back(compressPlugin->compress_type);
        { //VCD_APPHEADER   // always add HDiffzAppHead tag to out_diff
            const std::string HDiffzAppHead=getHDiffzAppHead(compressPlugin);
            packUInt(buf,HDiffzAppHead.size());
            pushCStr(buf,HDiffzAppHead.c_str());
        }
        _flushBuf(outDiff,buf);
    }
    
    const hpatch_StreamPos_t targetPosEnd=newData->streamSize;
    hpatch_StreamPos_t targetPos=0;
    size_t             coveri=0;
    std::vector<hpatch_byte> inst;
    std::vector<hpatch_byte> addr;
    std::vector<hpatch_byte> addr_z;
    TVcCompress compressList[3];
    compressList[0].compressPlugin=compressPlugin; 
    compressList[1].compressPlugin=compressPlugin; 
    compressList[2].compressPlugin=compressPlugin;        
    hpatch_StreamPos_t srcPos,srcEnd;
    _getSrcWindow(covers,0,covers.coverCount(),&srcPos,&srcEnd);
    while (targetPos<targetPosEnd){
        size_t coveriEnd;
        const hpatch_StreamPos_t targetLen=_getTargetWindow(targetPos,targetPosEnd,covers,coveri,&coveriEnd,kMaxTargetWindowsSize);
        {//used same one lagre srcWindowSize
            if (srcPos<srcEnd){
                buf.push_back(VCD_SOURCE); //Win_Indicator
                packUInt(buf,(hpatch_StreamPos_t)(srcEnd-srcPos));
                packUInt(buf,srcPos);//srcPos
            }else{
                buf.push_back(0); //Win_Indicator, no src window
            }
            _flushBuf(outDiff,buf);
        }
        {
            vc_encoder encoder(inst,addr);
            encoder.encode(covers,coveri,targetPos,targetLen,srcPos,srcEnd-srcPos);
        }
        
        TNewDataDiffStream _newDataDiff(covers,newData,coveri,targetPos,targetPos+targetLen);
        if (compressPlugin==0){
            hpatch_StreamPos_t deltaLen = hpatch_packUInt_size(targetLen)+1+hpatch_packUInt_size(_newDataDiff.streamSize)
                                         +hpatch_packUInt_size(inst.size())+hpatch_packUInt_size(addr.size())
                                         +_newDataDiff.streamSize+inst.size()+addr.size();
            packUInt(buf,deltaLen);
            packUInt(buf,targetLen);
            buf.push_back(0); //Delta_Indicator
            packUInt(buf,_newDataDiff.streamSize);
            packUInt(buf,inst.size());
            packUInt(buf,addr.size());
            _flushBuf(outDiff,buf);

            outDiff.pushStream(&_newDataDiff);
            outDiff.pushBack(inst.data(),inst.size());
            outDiff.pushBack(addr.data(),addr.size());
        }else{
            hpatch_TStreamInput _instStream;
            hpatch_TStreamInput _addrStream;
            mem_as_hStreamInput(&_instStream,inst.data(),inst.data()+inst.size());
            mem_as_hStreamInput(&_addrStream,addr.data(),addr.data()+addr.size());

            bool addr_is_z=compressVcDiffData(addr_z,&compressList[0],&_addrStream);
            std::vector<hpatch_byte>& inst_z=addr;
            bool inst_is_z=compressVcDiffData(inst_z,&compressList[1],&_instStream);
            std::vector<hpatch_byte>& data_z=inst;
            bool data_is_z=compressVcDiffData(data_z,&compressList[2],&_newDataDiff);
            hpatch_byte Delta_Indicator=((data_is_z?1:0)<<0)|((inst_is_z?1:0)<<1)|((addr_is_z?1:0)<<2);
            hpatch_StreamPos_t deltaLen = hpatch_packUInt_size(targetLen)+1+hpatch_packUInt_size(data_z.size())
                                         +hpatch_packUInt_size(inst_z.size())+hpatch_packUInt_size(addr_z.size())
                                         +(hpatch_StreamPos_t)data_z.size()+inst_z.size()+addr_z.size();
            packUInt(buf,deltaLen);
            packUInt(buf,targetLen);
            buf.push_back(Delta_Indicator);
            packUInt(buf,data_z.size());
            packUInt(buf,inst_z.size());
            packUInt(buf,addr_z.size());
            _flushBuf(outDiff,buf);

            outDiff.pushBack(data_z.data(),data_z.size());
            outDiff.pushBack(inst_z.data(),inst_z.size());
            outDiff.pushBack(addr_z.data(),addr_z.size());
        }
        coveri=coveriEnd;
        targetPos+=targetLen;
    } //window loop
    assert(coveri==covers.coverCount());
    assert(targetPos==targetPosEnd);
}

    template<class _TCover,class _TLen>
    static void _clipCovers(std::vector<_TCover>& covers,_TLen limitMaxCoverLen){
        const size_t cCount=covers.size();
        for (size_t i=0;i<cCount;++i){
            _TCover& c=covers[i];
            if (c.length>limitMaxCoverLen){
                _TLen allLen=c.length;
                size_t clip=(size_t)(((hpatch_StreamPos_t)allLen+limitMaxCoverLen-1)/limitMaxCoverLen);
                _TLen clipLen=(_TLen)(((hpatch_StreamPos_t)allLen+clip-1)/clip);
                _TCover nc=c;
                c.length=clipLen;
                for (size_t j=1;j<clip;++j){
                    nc.oldPos+=clipLen;
                    nc.newPos+=clipLen;
                    allLen-=clipLen;
                    nc.length=(j+1<clip)?clipLen:allLen;
                    covers.push_back(nc);
                }
            }
        }
        if (covers.size()>cCount){
            std::inplace_merge(covers.data(),covers.data()+cCount,covers.data()+covers.size(),
                               hdiff_private::cover_cmp_by_new_t<_TCover>());
        }
    }

void _create_vcdiff(const hpatch_byte* newData,const hpatch_byte* cur_newData_end,const hpatch_byte* newData_end,
                    const hpatch_byte* oldData,const hpatch_byte* cur_oldData_end,const hpatch_byte* oldData_end,
                    const hpatch_TStreamOutput* out_diff,const vcdiff_TCompress* compressPlugin,
                    int kMinSingleMatchScore,bool isUseBigCacheMatch,
                    ICoverLinesListener* coverLinesListener,size_t threadNum){
    std::vector<hpatch_TCover_sz> covers;
    const bool isCanExtendCover=false;
    get_match_covers_by_sstring(newData,cur_newData_end,oldData,cur_oldData_end,covers,
                                kMinSingleMatchScore,isUseBigCacheMatch,coverLinesListener,
                                threadNum,isCanExtendCover);
    _clipCovers(covers,(size_t)vcdiff_kMaxTargetWindowsSize/2);
    const TCovers _covers((void*)covers.data(),covers.size(),
                          sizeof(*covers.data())==sizeof(hpatch_TCover32));
    
    hdiff_TStreamInput newStream;
    hdiff_TStreamInput oldStream;
    mem_as_hStreamInput(&newStream,newData,newData_end);
    mem_as_hStreamInput(&oldStream,oldData,oldData_end);
    
    serialize_vcdiff(&newStream,&oldStream,_covers,out_diff,compressPlugin,vcdiff_kMaxTargetWindowsSize);
}

}//end namespace hdiff_private

using namespace hdiff_private;

void create_vcdiff(const hpatch_byte* newData,const hpatch_byte* newData_end,
                   const hpatch_byte* oldData,const hpatch_byte* oldData_end,
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
    hpatch_byte* pOldData=oldAndNewData.data();
    hpatch_byte* pNewData=pOldData+old_size;
    hpatch_byte* pNewDataEnd=pNewData+(size_t)newData->streamSize;
    _create_vcdiff(pNewData,pNewDataEnd,pNewDataEnd,pOldData,pOldData+old_size,pOldData+old_size,
                   out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,
                   coverLinesListener,threadNum);
}

void create_vcdiff_stream(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                          const hpatch_TStreamOutput* out_diff,const vcdiff_TCompress* compressPlugin,
                          size_t kMatchBlockSize,const hdiff_TMTSets_s* mtsets){
    TCoversBuf covers(newData->streamSize,oldData->streamSize);
    get_match_covers_by_block(newData,oldData,&covers,kMatchBlockSize,mtsets);
    if (covers._isCover32)
        _clipCovers(covers.m_covers_limit,(hpatch_uint32_t)vcdiff_kMaxTargetWindowsSize/2);
    else
        _clipCovers(covers.m_covers_larger,(hpatch_StreamPos_t)vcdiff_kMaxTargetWindowsSize/2);
    covers.update();
    serialize_vcdiff(newData,oldData,covers,out_diff,compressPlugin,vcdiff_kMaxTargetWindowsSize);
}


void create_vcdiff_block(hpatch_byte* newData,hpatch_byte* newData_end,
                         hpatch_byte* oldData,hpatch_byte* oldData_end,
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
    hpatch_byte* pOldData=oldAndNewData.data();
    hpatch_byte* pNewData=pOldData+old_size;
    create_vcdiff_block(pNewData,pNewData+(size_t)newData->streamSize,pOldData,pOldData+old_size,
                        out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,
                        matchBlockSize,threadNum);
}

bool check_vcdiff(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                  const hpatch_TStreamInput* diffStream,hpatch_TDecompress* decompressPlugin){
    const size_t kACacheBufSize=hdiff_kFileIOBufBestSize;
    TAutoMem _cache(kACacheBufSize*(1+5));
    _TCheckOutNewDataStream out_newData(newData,_cache.data(),kACacheBufSize);
    _test_rt(vcpatch_with_cache(&out_newData,oldData,diffStream,decompressPlugin,hpatch_TRUE,
                                _cache.data()+kACacheBufSize,_cache.data_end()));
    _test_rt(out_newData.isWriteFinish());
    return true;
}
bool check_vcdiff(const hpatch_byte* newData,const hpatch_byte* newData_end,
                  const hpatch_byte* oldData,const hpatch_byte* oldData_end,
                  const hpatch_byte* diffData,const hpatch_byte* diffData_end,
                  hpatch_TDecompress* decompressPlugin){
    hdiff_TStreamInput newStream;
    hdiff_TStreamInput oldStream;
    hdiff_TStreamInput diffStream;
    mem_as_hStreamInput(&newStream,newData,newData_end);
    mem_as_hStreamInput(&oldStream,oldData,oldData_end);
    mem_as_hStreamInput(&diffStream,diffData,diffData_end);
    return check_vcdiff(&newStream,&oldStream,&diffStream,decompressPlugin);
}