//diff.cpp
//
/*
 The MIT License (MIT)
 Copyright (c) 2012-2018 HouSisong

 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation
 files (the "Software"), to deal in the Software without
 restriction, including without limitation the rights to use,
 copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following
 conditions:

 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
*/

#include "diff.h"
#include <string.h> //strlen memcmp
#include <stdio.h>  //fprintf
#include <algorithm> //std::max
#include <vector>
#include "private_diff/suffix_string.h"
#include "private_diff/bytes_rle.h"
#include "private_diff/compress_detect.h"
#include "private_diff/pack_uint.h"
#include "private_diff/mem_buf.h"
#include "../HPatch/patch.h"
#include "../HPatch/patch_private.h"
#include "private_diff/limit_mem_diff/covers.h"
#include "private_diff/limit_mem_diff/digest_matcher.h"
#include "private_diff/limit_mem_diff/stream_serialize.h"
using namespace hdiff_private;

static void getCovers_by_stream(const hpatch_TStreamInput*  newData,
                                const hpatch_TStreamInput*  oldData,
                                size_t kMatchBlockSize,bool kIsSkipSameRange,
                                TCovers& out_covers);


static const char* kHDiffVersionType  ="HDIFF13";
static const char* kHDiffSFVersionType="HDIFFSF20";

#define checki(value,info) { if (!(value)) { throw std::runtime_error(info); } }
#define check(value) checki(value,"check "#value" error!")

namespace{
    
    typedef unsigned char TByte;
    typedef size_t        TUInt;
    typedef ptrdiff_t     TInt;
    static const int kMaxLinkSpaceLength=(1<<9)-1; //跨覆盖线合并时,允许合并的最远距离.
    
    //覆盖线.
    struct TOldCover {
        TInt   oldPos;
        TInt   newPos;
        TInt   length;
        inline TOldCover():oldPos(0),newPos(0),length(0) { }
        inline TOldCover(TInt _oldPos,TInt _newPos,TInt _length)
            :oldPos(_oldPos),newPos(_newPos),length(_length) { }
        inline TOldCover(const TOldCover& cover)
            :oldPos(cover.oldPos),newPos(cover.newPos),length(cover.length) { }
        
        inline bool isCanLink(const TOldCover& next)const{//覆盖线是否可以连接.
            return isCollinear(next)&&(linkSpaceLength(next)<=kMaxLinkSpaceLength);
        }
        inline bool isCollinear(const TOldCover& next)const{//覆盖线是否在同一条直线上.
            return (oldPos-next.oldPos==newPos-next.newPos);
        }
        inline TInt linkSpaceLength(const TOldCover& next)const{//覆盖线间的间距.
            return next.oldPos-(oldPos+length);
        }
        inline void Link(const TOldCover& next){//共线的2覆盖线合并链接.
            assert(isCollinear(next));
            assert(oldPos<=next.oldPos);
            length = (next.oldPos-oldPos)+next.length;
        }
    };


struct TDiffData{
    const TByte*            newData;
    const TByte*            newData_end;
    const TByte*            oldData;
    const TByte*            oldData_end;
    std::vector<TOldCover>  covers;         //选出的覆盖线.
    std::vector<TByte>      newDataDiff;    //集中储存newData中没有被覆盖线覆盖住的字节数据.
    std::vector<TByte>      newDataSubDiff; //newData中的每个数值减去对应的cover线条的oldData数值和newDataDiff的数值.
};


//查找相等的字符串长度.
static TInt getEqualLength(const TByte* x,const TByte* x_end,
                           const TByte* y,const TByte* y_end){
    const TInt xLen=(TInt)(x_end-x);
    const TInt yLen=(TInt)(y_end-y);
    const TInt maxEqLen=(xLen<yLen)?xLen:yLen;
    for (TInt i=0; i<maxEqLen; ++i) {
        if (x[i]!=y[i])
            return i;
    }
    return maxEqLen;
}

//得到最长的一个匹配长度和其位置.
static TInt getBestMatch(TInt* out_pos,const TSuffixString& sstring,
                         const TByte* newData,const TByte* newData_end){
    TInt sai=sstring.lower_bound(newData,newData_end);
    
    const TByte* src_begin=sstring.src_begin();
    const TByte* src_end=sstring.src_end();
    TInt bestLength=-1;
    TInt bestOldPos=-1;
    for (TInt i=sai-1; i<=sai; ++i) {
        if ((i<0)||(i>=(src_end-src_begin))) continue;
        TInt curOldPos=sstring.SA(i);
        TInt curLength=getEqualLength(newData,newData_end,src_begin+curOldPos,src_end);
        if (curLength>bestLength){
            bestLength=curLength;
            bestOldPos=curOldPos;
        }
    }
    *out_pos=bestOldPos;
    return  bestLength;
}

    
    //粗略估算覆盖线的控制数据成本;
    inline static TInt getCoverCtrlCost(const TOldCover& cover,const TOldCover& lastCover){
        static const int kUnLinkOtherScore=0;//0--2
        return _getIntCost<TInt,TUInt>((TInt)(cover.oldPos-lastCover.oldPos))
             + _getUIntCost((TUInt)cover.length)
             + _getUIntCost((TUInt)(cover.newPos-lastCover.newPos)) + kUnLinkOtherScore;
    }
    
    //粗略估算 区域内当作覆盖时的可能存储成本.
    inline static bool checkGetCoverCost(TInt* out_cost,TInt newPos,TInt oldPos,
                                         TInt length,const TDiffData& diff){
        if ((oldPos<0)||(oldPos+length>(diff.oldData_end-diff.oldData)))
            return false;
        *out_cost=(TInt)getRegionRleCost(diff.newData+newPos,length,diff.oldData+oldPos);
        return true;
    }
    inline static TInt getCoverCost(const TOldCover& cover,const TDiffData& diff){
        return (TInt)getRegionRleCost(diff.newData+cover.newPos,cover.length,diff.oldData+cover.oldPos);
    }
    
//尝试延长lastCover来完全代替matchCover;
static bool tryLinkExtend(TOldCover& lastCover,const TOldCover& matchCover,const TDiffData& diff){
    if (lastCover.length<=0) return false;
    const TInt linkSpaceLength=(matchCover.newPos-(lastCover.newPos+lastCover.length));
    if ((linkSpaceLength>kMaxLinkSpaceLength)||(matchCover.newPos==0))
        return false;
    if (lastCover.isCollinear(matchCover)){//已经共线;
        lastCover.Link(matchCover);
        return true;
    }
    TInt linkOldPos=lastCover.oldPos+lastCover.length+linkSpaceLength;
    TInt matchCost=getCoverCtrlCost(matchCover,lastCover);
    TInt lastLinkCost;
    if (!checkGetCoverCost(&lastLinkCost,matchCover.newPos,linkOldPos,matchCover.length,diff)) return false;
    if (lastLinkCost>matchCost)
        return false;
    TInt len=lastCover.length+linkSpaceLength+(matchCover.length*2/3);//扩展大部分,剩下的可能扩展留给extend_cover.
    len+=getEqualLength(diff.newData+lastCover.newPos+len,diff.newData_end,
                        diff.oldData+lastCover.oldPos+len,diff.oldData_end);
    while ((len>0) && (diff.newData[lastCover.newPos+len-1]
                       !=diff.oldData[lastCover.oldPos+len-1])) {
        --len;
    }
    lastCover.length=len;
    return true;
}
    
//尝试设置lastCover为matchCover所在直线的延长线,实现共线(增加合并可能等);
static void tryCollinear(TOldCover& lastCover,const TOldCover& matchCover,const TDiffData& diff){
    if (lastCover.length<=0) return;
    if (lastCover.isCollinear(matchCover)) return; //已经共线;
    TInt linkOldPos=matchCover.oldPos-(matchCover.newPos-lastCover.newPos);
    TInt lastCost=getCoverCost(lastCover,diff);
    TInt matchLinkCost;
    if (!checkGetCoverCost(&matchLinkCost,lastCover.newPos,linkOldPos,lastCover.length,diff)) return;
    if (lastCost>=matchLinkCost)
        lastCover.oldPos=linkOldPos;
}

//寻找合适的覆盖线.
static void search_cover(TDiffData& diff,const TSuffixString& sstring){
    if (sstring.SASize()<=0) return;
    static const int kMinMatchScore = 2; //最小搜寻覆盖收益.
    const TInt maxSearchNewPos=(diff.newData_end-diff.newData)-kMinMatchScore;
    TInt newPos=0;
    TOldCover lastCover(0,0,0);
    while (newPos<=maxSearchNewPos) {
        TInt matchOldPos=0;
        TInt matchEqLength=getBestMatch(&matchOldPos,sstring,diff.newData+newPos,diff.newData_end);
        TOldCover matchCover(matchOldPos,newPos,matchEqLength);
        if (matchEqLength-getCoverCtrlCost(matchCover,lastCover)<kMinMatchScore){
            ++newPos;//下一个需要匹配的字符串(逐位置匹配速度会比较慢).
            continue;
        }//else matched
        
        if (tryLinkExtend(lastCover,matchCover,diff)){//use link
            if (diff.covers.empty())
                diff.covers.push_back(lastCover);
            else
                diff.covers.back()=lastCover;
        }else{ //use match
            if (!diff.covers.empty())//尝试共线;
                tryCollinear(diff.covers.back(),matchCover,diff);
            diff.covers.push_back(matchCover);
        }
        lastCover=diff.covers.back();
        newPos=std::max(newPos+1,lastCover.newPos+lastCover.length);//选出的cover不允许重叠,这可能不是最优策略;
    }
}

//选择合适的覆盖线,去掉不合适的.
static void select_cover(TDiffData& diff,int kMinSingleMatchScore){
    std::vector<TOldCover>&  covers=diff.covers;
    TCompressDetect  nocover_detect;
    TCompressDetect  cover_detect;

    TOldCover lastCover(0,0,0);
    const TInt coverSize_old=(TInt)covers.size();
    TInt insertIndex=0;
    for (TInt i=0;i<coverSize_old;++i){
        if (covers[i].oldPos<0) continue;//处理已经del的.
        bool isNeedSave=false;
        if (!isNeedSave){//向前合并可能.
            if ((insertIndex>0)&&(covers[insertIndex-1].isCanLink(covers[i])))
                isNeedSave=true;
        }
        if (i+1<coverSize_old){//查询向后合并可能link
            for (TInt j=i+1;j<coverSize_old; ++j) {
                if (!covers[i].isCanLink(covers[j])) break;
                covers[i].Link(covers[j]);
                covers[j].oldPos=-1;//del
            }
        }
        if (!isNeedSave){//单覆盖是否保留.
            TInt noCoverCost=nocover_detect.cost(diff.newData+covers[i].newPos,covers[i].length);
            TInt coverCost=cover_detect.cost(diff.newData+covers[i].newPos,covers[i].length,
                                             diff.oldData+covers[i].oldPos);
            TInt coverSorce=noCoverCost-coverCost-getCoverCtrlCost(covers[i],lastCover);
            isNeedSave=(coverSorce>=kMinSingleMatchScore);
        }
        
        if (isNeedSave){
            if ((insertIndex>0)&&(covers[insertIndex-1].isCanLink(covers[i]))){//link合并.
                covers[insertIndex-1].Link(covers[i]);
                cover_detect.add_chars(diff.newData+lastCover.newPos+lastCover.length,
                                       covers[insertIndex-1].length-lastCover.length,
                                       diff.oldData+lastCover.oldPos+lastCover.length);
            }else{
                covers[insertIndex++]=covers[i];
                nocover_detect.add_chars(diff.newData+lastCover.newPos+lastCover.length,
                                         covers[i].newPos-(lastCover.newPos+lastCover.length));
                cover_detect.add_chars(diff.newData+covers[i].newPos,covers[i].length,
                                       diff.oldData+covers[i].oldPos);
            }
            lastCover=covers[insertIndex-1];
        }
    }
    covers.resize(insertIndex);
}
    
    typedef size_t TFixedFloatSmooth; //定点数.
    static const TFixedFloatSmooth kFixedFloatSmooth_base=1024;//定点数小数点位置.

    //得到可以扩展位置的长度.
    static TInt getCanExtendLength(TInt oldPos,TInt newPos,int inc,TInt newPos_min,TInt lastNewEnd,
                                   const TDiffData& diff,const TFixedFloatSmooth kExtendMinSameRatio){
        static const unsigned int kSmoothLength=4;

        TFixedFloatSmooth curBestSameRatio=0;
        TInt curBestLength=0;
        TUInt curSameCount=0;
        const TFixedFloatSmooth kLimitSameCount=(~(TFixedFloatSmooth)0)/kFixedFloatSmooth_base;
        for (TUInt length=1; (oldPos>=0)&&(oldPos<(diff.oldData_end-diff.oldData))
             &&(newPos>=newPos_min)&&(newPos<lastNewEnd); ++length,oldPos+=inc,newPos+=inc) {
            if (diff.oldData[oldPos]==diff.newData[newPos]){
                ++curSameCount;
                
                if (curSameCount>= kLimitSameCount) break; //for curSameCount*kFixedFloatSmooth_base
                const TFixedFloatSmooth curSameRatio= (curSameCount*kFixedFloatSmooth_base)
                                                      /(length+kSmoothLength);
                if (curSameRatio>=curBestSameRatio){
                    curBestSameRatio=curSameRatio;
                    curBestLength=length;
                }
            }
        }
        if ((curBestSameRatio<kExtendMinSameRatio)||(curBestLength<=2)){
            curBestLength=0;
        }
        
        return curBestLength;
    }

//尝试延长覆盖区域.
static void extend_cover(TDiffData& diff,const TFixedFloatSmooth kExtendMinSameRatio){
    std::vector<TOldCover>&  covers=diff.covers;

    TInt lastNewEnd=0;
    for (TInt i=0; i<(TInt)covers.size(); ++i) {
        TInt newPos_next=(TInt)(diff.newData_end-diff.newData);
        if (i+1<(TInt)covers.size())
            newPos_next=covers[i+1].newPos;
        TOldCover& curCover=covers[i];
        //向前延伸.
        TInt extendLength_front=getCanExtendLength(curCover.oldPos-1,curCover.newPos-1,
                                                   -1,lastNewEnd,newPos_next,diff,kExtendMinSameRatio);
        if (extendLength_front>0){
            curCover.oldPos-=extendLength_front;
            curCover.newPos-=extendLength_front;
            curCover.length+=extendLength_front;
        }
        //向后延伸.
        TInt extendLength_back=getCanExtendLength(curCover.oldPos+curCover.length,
                                                  curCover.newPos+curCover.length,
                                                  1,lastNewEnd,newPos_next,diff,kExtendMinSameRatio);
        if (extendLength_back>0){
            curCover.length+=extendLength_back;
        }

        lastNewEnd=curCover.newPos+curCover.length;
    }
}

    template<class _TCover,class _TInt>
    static void assert_cover_safe(const _TCover& cover,_TInt lastNewEnd,_TInt newSize,_TInt oldSize){
        check(cover.length>0);
        check(cover.newPos>=lastNewEnd);
        check(cover.newPos<newSize);
        check(cover.newPos+cover.length>0);
        check(cover.newPos+cover.length<=newSize);
        check(cover.oldPos>=0);
        check(cover.oldPos<oldSize);
        check(cover.oldPos+cover.length>0);
        check(cover.oldPos+cover.length<=oldSize);
    }

//用覆盖线得到差异数据.
static void sub_cover(TDiffData& diff){
    std::vector<TOldCover>& covers=diff.covers;
    const TByte*            newData=diff.newData;
    const TByte*            oldData=diff.oldData;

    diff.newDataSubDiff.resize(0);
    diff.newDataSubDiff.resize(diff.newData_end-diff.newData,0);
    TByte*  newDataSubDiff=diff.newDataSubDiff.data();
    diff.newDataDiff.resize(0);
    diff.newDataDiff.reserve((diff.newData_end-diff.newData)>>2);
    
    TInt lastNewEnd=0;
    for (TInt i=0;i<(TInt)covers.size();++i){
        assert_cover_safe(covers[i],lastNewEnd,
                          diff.newData_end-diff.newData,diff.oldData_end-diff.oldData);
        const TInt newPos=covers[i].newPos;
        if (newPos>lastNewEnd)
            pushBack(diff.newDataDiff,newData+lastNewEnd,newData+newPos);
        const TInt oldPos=covers[i].oldPos;
        const TInt length=covers[i].length;
        for (TInt si=0; si<length;++si)
            newDataSubDiff[si+newPos]=(newData[si+newPos]-oldData[si+oldPos]);
        lastNewEnd=newPos+length;
    }
    if (lastNewEnd<diff.newData_end-newData)
        pushBack(diff.newDataDiff,newData+lastNewEnd,diff.newData_end);
}

//diff结果序列化输出.
static void serialize_diff(const TDiffData& diff,std::vector<TByte>& out_diff){
    const TUInt coverCount=(TUInt)diff.covers.size();
    std::vector<TByte> length_buf;
    std::vector<TByte> inc_newPos_buf;
    std::vector<TByte> inc_oldPos_buf;
    {
        TInt oldPosBack=0;
        TInt lastNewEnd=0;
        for (TUInt i=0; i<coverCount; ++i) {
            packUInt(length_buf, (TUInt)diff.covers[i].length);
            assert(diff.covers[i].newPos>=lastNewEnd);
            packUInt(inc_newPos_buf,(TUInt)(diff.covers[i].newPos-lastNewEnd)); //save inc_newPos
            if (diff.covers[i].oldPos>=oldPosBack){ //save inc_oldPos
                packUIntWithTag(inc_oldPos_buf,(TUInt)(diff.covers[i].oldPos-oldPosBack), 0, 1);
            }else{
                packUIntWithTag(inc_oldPos_buf,(TUInt)(oldPosBack-diff.covers[i].oldPos), 1, 1);//sub safe
            }
            oldPosBack=diff.covers[i].oldPos;
            lastNewEnd=diff.covers[i].newPos+diff.covers[i].length;
        }
    }

    packUInt(out_diff, (TUInt)coverCount);
    packUInt(out_diff, (TUInt)length_buf.size());
    packUInt(out_diff, (TUInt)inc_newPos_buf.size());
    packUInt(out_diff, (TUInt)inc_oldPos_buf.size());
    packUInt(out_diff, (TUInt)diff.newDataDiff.size());
    pushBack(out_diff,length_buf);
    pushBack(out_diff,inc_newPos_buf);
    pushBack(out_diff,inc_oldPos_buf);
    pushBack(out_diff,diff.newDataDiff);
    
    const TByte* newDataSubDiff=diff.newDataSubDiff.data();
    bytesRLE_save(out_diff,newDataSubDiff,
                  newDataSubDiff+diff.newDataSubDiff.size(),kRle_bestSize);
}
    
    
    static void do_compress(std::vector<TByte>& out_code,const std::vector<TByte>& data,
                            const hdiff_TCompress* compressPlugin,bool isMustCompress=false){
        out_code.clear();
        if (!compressPlugin) return;
        if (data.empty()) return;
        hpatch_StreamPos_t maxCodeSize=compressPlugin->maxCompressedSize(data.size());
        if ((maxCodeSize<=data.size())||(maxCodeSize!=(size_t)maxCodeSize)) return; //error
        out_code.resize((size_t)maxCodeSize);
        const TByte* data0=&data[0];
        TByte* out_code0=&out_code[0];
        size_t codeSize=hdiff_compress_mem(compressPlugin,out_code0,out_code0+out_code.size(),
                                           data0,data0+data.size());
        if ((codeSize>0)&&(isMustCompress||(codeSize<data.size())))
            out_code.resize(codeSize); //ok
        else
            out_code.clear();//error or cancel
    }
    inline static void pushCompressCode(std::vector<TByte>& out_diff,
                                        const std::vector<TByte>& compress_code,
                                        const std::vector<TByte>& data){
        if (compress_code.empty())
            pushBack(out_diff,data);
        else
            pushBack(out_diff,compress_code);
    }
    
    template<class T>
    static void _outType(std::vector<TByte>& out_data,T* compressPlugin,const char* versionType=kHDiffVersionType){
        //type version
        pushCStr(out_data,versionType);
        pushCStr(out_data,"&");
        {//compressType
            const char* compressType="";
            if (compressPlugin)
                compressType=compressPlugin->compressType();
            size_t compressTypeLen=strlen(compressType);
            check(compressTypeLen<=hpatch_kMaxPluginTypeLength);
            check(0==strchr(compressType,'&'));
            pushCStr(out_data,compressType);
        }
        const TByte _cstrEndTag='\0';//c string end tag
        pushBack(out_data,&_cstrEndTag,(&_cstrEndTag)+1);
    }
    
static void serialize_compressed_diff(const TDiffData& diff,std::vector<TByte>& out_diff,
                                      const hdiff_TCompress* compressPlugin){
    const TUInt coverCount=(TUInt)diff.covers.size();
    std::vector<TByte> cover_buf;
    {
        TInt lastOldEnd=0;
        TInt lastNewEnd=0;
        for (TUInt i=0; i<coverCount; ++i) {
            if (diff.covers[i].oldPos>=lastOldEnd){ //save inc_oldPos
                packUIntWithTag(cover_buf,(TUInt)(diff.covers[i].oldPos-lastOldEnd), 0, 1);
            }else{
                packUIntWithTag(cover_buf,(TUInt)(lastOldEnd-diff.covers[i].oldPos), 1, 1);//sub safe
            }
            assert(diff.covers[i].newPos>=lastNewEnd);
            packUInt(cover_buf,(TUInt)(diff.covers[i].newPos-lastNewEnd)); //save inc_newPos
            packUInt(cover_buf,(TUInt)diff.covers[i].length);
            lastOldEnd=diff.covers[i].oldPos+diff.covers[i].length;//! +length
            lastNewEnd=diff.covers[i].newPos+diff.covers[i].length;
        }
    }
    
    std::vector<TByte> rle_ctrlBuf;
    std::vector<TByte> rle_codeBuf;
    const TByte* newDataSubDiff=diff.newDataSubDiff.data();
    bytesRLE_save(rle_ctrlBuf,rle_codeBuf,newDataSubDiff,
                  newDataSubDiff+diff.newDataSubDiff.size(),kRle_bestSize);
    
    std::vector<TByte> compress_cover_buf;
    std::vector<TByte> compress_rle_ctrlBuf;
    std::vector<TByte> compress_rle_codeBuf;
    std::vector<TByte> compress_newDataDiff;
    do_compress(compress_cover_buf,cover_buf,compressPlugin);
    do_compress(compress_rle_ctrlBuf,rle_ctrlBuf,compressPlugin);
    do_compress(compress_rle_codeBuf,rle_codeBuf,compressPlugin);
    do_compress(compress_newDataDiff,diff.newDataDiff,compressPlugin);

    _outType(out_diff,compressPlugin);
    const TUInt newDataSize=(TUInt)(diff.newData_end-diff.newData);
    const TUInt oldDataSize=(TUInt)(diff.oldData_end-diff.oldData);
    packUInt(out_diff, newDataSize);
    packUInt(out_diff, oldDataSize);
    packUInt(out_diff, coverCount);
    packUInt(out_diff, (TUInt)cover_buf.size());
    packUInt(out_diff, (TUInt)compress_cover_buf.size());
    packUInt(out_diff, (TUInt)rle_ctrlBuf.size());
    packUInt(out_diff, (TUInt)compress_rle_ctrlBuf.size());
    packUInt(out_diff, (TUInt)rle_codeBuf.size());
    packUInt(out_diff, (TUInt)compress_rle_codeBuf.size());
    packUInt(out_diff, (TUInt)diff.newDataDiff.size());
    packUInt(out_diff, (TUInt)compress_newDataDiff.size());
    
    pushCompressCode(out_diff,compress_cover_buf,cover_buf);
    pushCompressCode(out_diff,compress_rle_ctrlBuf,rle_ctrlBuf);
    pushCompressCode(out_diff,compress_rle_codeBuf,rle_codeBuf);
    pushCompressCode(out_diff,compress_newDataDiff,diff.newDataDiff);
}
    
    static void search_cover_by_stream(TDiffData& diff,size_t kMatchBlockSize,bool isSkipSameRange){
        hdiff_TStreamInput oldData;
        mem_as_hStreamInput(&oldData,diff.oldData,diff.oldData_end);
        hdiff_TStreamInput newData;
        mem_as_hStreamInput(&newData,diff.newData,diff.newData_end);
        TCovers covers(oldData.streamSize,newData.streamSize);
        getCovers_by_stream(&newData,&oldData,kMatchBlockSize,isSkipSameRange,covers);
       
        size_t coverCount=covers.coverCount();
        diff.covers.resize(coverCount);
        TOldCover* outCover=diff.covers.data();
        for (size_t i=0;i<coverCount;++i,++outCover){
            TCover cover;
            covers.covers(i,&cover);
            outCover->oldPos=(TInt)cover.oldPos;
            outCover->newPos=(TInt)cover.newPos;
            outCover->length=(TInt)cover.length;
        }
    }
    
static void get_diff(const TByte* newData,const TByte* newData_end,
                     const TByte* oldData,const TByte* oldData_end,
                     TDiffData&   out_diff,int kMinSingleMatchScore,
                     const TSuffixString* sstring=0, bool isDoSubCover=true,
                     bool _is_search_cover_by_stream=false){
    assert(newData<=newData_end);
    assert(oldData<=oldData_end);
    TDiffData& diff=out_diff;
    diff.newData=newData;
    diff.newData_end=newData_end;
    diff.oldData=oldData;
    diff.oldData_end=oldData_end;
    
    if (_is_search_cover_by_stream){
        search_cover_by_stream(diff,8,true);
    }else{
        TSuffixString _sstring_default(0,0);
        if (sstring==0){
            _sstring_default.resetSuffixString(oldData,oldData_end);
            sstring=&_sstring_default;
        }
        search_cover(diff,*sstring);
        sstring=0;
        _sstring_default.clear();
    }
    
    TFixedFloatSmooth kExtendMinSameRatio=kMinSingleMatchScore*36+254;
    if  (kExtendMinSameRatio<200) kExtendMinSameRatio=200;
    if (kExtendMinSameRatio>800) kExtendMinSameRatio=800;
    
    extend_cover(diff,kExtendMinSameRatio);//先尝试扩展.
    select_cover(diff,kMinSingleMatchScore);
    extend_cover(diff,kExtendMinSameRatio);//select_cover会删除一些覆盖线,所以重新扩展.
    if (isDoSubCover) sub_cover(diff);
}
    
}//end namespace


void create_diff(const TByte* newData,const TByte* newData_end,
                 const TByte* oldData,const TByte* oldData_end,
                 std::vector<TByte>& out_diff,
                 int kMinSingleMatchScore){
    TDiffData diff;
    get_diff(newData,newData_end,oldData,oldData_end,diff,kMinSingleMatchScore);
    serialize_diff(diff,out_diff);
}

bool check_diff(const TByte* newData,const TByte* newData_end,
                const TByte* oldData,const TByte* oldData_end,
                const TByte* diff,const TByte* diff_end){
    TAutoMem updateNewData(newData_end-newData);
    TByte* updateNew0=updateNewData.data();
    if (!patch(updateNew0,updateNew0+updateNewData.size(),
               oldData,oldData_end, diff,diff_end)) return false;
    return (0==memcmp(updateNew0,newData,updateNewData.size()));
}

void create_compressed_diff(const TByte* newData,const TByte* newData_end,
                            const TByte* oldData,const TByte* oldData_end,
                            std::vector<TByte>& out_diff,
                            const hdiff_TCompress* compressPlugin,int kMinSingleMatchScore){
    TDiffData diff;
    get_diff(newData,newData_end,oldData,oldData_end,diff,kMinSingleMatchScore);
    serialize_compressed_diff(diff,out_diff,compressPlugin);
}

bool check_compressed_diff(const TByte* newData,const TByte* newData_end,
                           const TByte* oldData,const TByte* oldData_end,
                           const TByte* diff,const TByte* diff_end,
                           hpatch_TDecompress* decompressPlugin){
    TAutoMem updateNewData(newData_end-newData);
    TByte* updateNew0=updateNewData.data();
    if (!patch_decompress_mem(updateNew0,updateNew0+updateNewData.size(),
                              oldData,oldData_end, diff,diff_end, decompressPlugin)) return false;
    return (0==memcmp(updateNew0,newData,updateNewData.size()));
}


static void _flush_step_code(std::vector<TByte> &buf, std::vector<TByte> &step_bufCover, std::vector<TByte> &step_bufData,
                             hdiff_private::TSangileStreamRLE0 &step_bufRle,size_t& curMaxStepMemSize) {
    step_bufRle.finishAppend();
    packUInt(buf,step_bufCover.size()); //general saved data
    packUInt(buf,step_bufRle.curCodeSize());
    size_t bufSize_back=buf.size();
    pushBack(buf,step_bufCover);            step_bufCover.clear();
    pushBack(buf,step_bufRle.fixed_code);   step_bufRle.clear();
    size_t curStepMemSize=buf.size()-bufSize_back;
    pushBack(buf,step_bufData);             step_bufData.clear();
    if (curMaxStepMemSize<curStepMemSize)
        curMaxStepMemSize=curStepMemSize;
}

static void serialize_single_compressed_diff(TDiffData& diff,std::vector<TByte>& out_diff,
                                             const hdiff_TCompress* compressPlugin,size_t patchStepMemSize){
    check(patchStepMemSize>=hpatch_kStreamCacheSize);
    std::vector<TOldCover>& covers=diff.covers;
    const TUInt newDataSize=(TUInt)(diff.newData_end-diff.newData);
    const TUInt oldDataSize=(TUInt)(diff.oldData_end-diff.oldData);
    if (covers.empty()){
        covers.push_back(TOldCover(0,newDataSize,0));
    }else{
        const TOldCover& back=covers[covers.size()-1];
        if ((TUInt)back.newPos+(TUInt)back.length<newDataSize){
            covers.push_back(TOldCover(back.oldPos+back.length,newDataSize,0));
        }
    }
    size_t curMaxStepMemSize=0;
    std::vector<TByte> buf;
    {
        TInt lastOldEnd=0;
        TInt lastNewEnd=0;
        TUInt curNewDiff=0;
        std::vector<TByte> step_bufCover;
        TSangileStreamRLE0 step_bufRle;
        std::vector<TByte> step_bufData;
        TUInt i=0;
        while ( i<covers.size()) {
            const size_t step_bufCover_backSize=step_bufCover.size();
            
            const TOldCover& cover=covers[i];
            const TByte* subDiff=diff.newDataSubDiff.data()+cover.newPos;
            if (cover.oldPos>=lastOldEnd){ //save inc_oldPos
                packUIntWithTag(step_bufCover,(TUInt)(cover.oldPos-lastOldEnd), 0, 1);
            }else{
                packUIntWithTag(step_bufCover,(TUInt)(lastOldEnd-cover.oldPos), 1, 1);//sub safe
            }
            TInt backNewLen=cover.newPos-lastNewEnd;
            assert(backNewLen>=0);
            packUInt(step_bufCover,(TUInt)backNewLen); //save inc_newPos
            packUInt(step_bufCover,cover.length);
            
            const TUInt curMaxNeedSize = step_bufCover.size() + step_bufRle.maxCodeSize(subDiff,subDiff+cover.length);
            if (curMaxNeedSize<=patchStepMemSize){ //append
                step_bufRle.append(subDiff,subDiff+cover.length);
                if (backNewLen>0){
                    const TByte* newDataDiff=diff.newDataDiff.data()+curNewDiff;
                    pushBack(step_bufData,newDataDiff,newDataDiff+backNewLen);
                    curNewDiff+=backNewLen;
                }

                //next i
                lastOldEnd=cover.oldPos+cover.length;//! +length
                lastNewEnd=cover.newPos+cover.length;
                ++i;
            }else{
                if (step_bufCover_backSize+step_bufRle.curCodeSize()>=(patchStepMemSize/2)){//flush step
                    step_bufCover.resize(step_bufCover_backSize);
                    _flush_step_code(buf,step_bufCover,step_bufData,step_bufRle,curMaxStepMemSize);
                    continue;  // old i!
                }else{ //clip one cover to two cover
                    TOldCover& cover_l=covers[i];
                    TUInt clen=cover_l.length;
                    while (1) {
                        clen=clen*3/4;
                        check(clen>0); // stepMemSize error
                        const TUInt _curMaxNeedSize = step_bufCover.size() + step_bufRle.maxCodeSize(subDiff,subDiff+clen);
                        if (_curMaxNeedSize<=patchStepMemSize)
                            break;
                    }
                    TOldCover cover_r=cover_l;
                    cover_l.length=clen;
                    cover_r.length-=clen;
                    cover_r.oldPos+=clen;
                    cover_r.newPos+=clen;
                    covers.insert(covers.begin()+(i+1),cover_r);
                    
                    step_bufCover.resize(step_bufCover_backSize);
                    continue;  // old i!
                }
            }
        }
        check(diff.newDataDiff.size()==curNewDiff);
        if (!step_bufCover.empty())
            _flush_step_code(buf,step_bufCover,step_bufData,step_bufRle,curMaxStepMemSize);
    }
    assert(curMaxStepMemSize<=patchStepMemSize);
    
    std::vector<TByte> compress_buf;
    do_compress(compress_buf,buf,compressPlugin);
    
    _outType(out_diff,compress_buf.empty()?0:compressPlugin,kHDiffSFVersionType);
    packUInt(out_diff, newDataSize);
    packUInt(out_diff, oldDataSize);
    packUInt(out_diff, covers.size());
    packUInt(out_diff, curMaxStepMemSize);
    packUInt(out_diff, (TUInt)buf.size());
    packUInt(out_diff, (TUInt)compress_buf.size());
    
    pushCompressCode(out_diff,compress_buf,buf);
}

void create_single_compressed_diff(const TByte* newData,const TByte* newData_end,
                                   const TByte* oldData,const TByte* oldData_end,
                                   std::vector<unsigned char>& out_diff,ICoverLinesListener* listener,const hdiff_TCompress* compressPlugin,
                                   int kMinSingleMatchScore,size_t patchStepMemSize,bool _is_search_cover_by_stream){
    TDiffData diff;
    get_diff(newData,newData_end,oldData,oldData_end,diff,kMinSingleMatchScore,0,false,_is_search_cover_by_stream);
    if (listener){
        std::vector<hpatch_TCover> _covers(diff.covers.size());
        for (size_t i=0; i<diff.covers.size(); ++i) {
            _covers[i].oldPos=diff.covers[i].oldPos;
            _covers[i].newPos=diff.covers[i].newPos;
            _covers[i].length=diff.covers[i].length;
        }
        hpatch_StreamPos_t newDataSize=(size_t)(diff.newData_end-diff.newData);
        hpatch_StreamPos_t oldDataSize=(size_t)(diff.oldData_end-diff.oldData);
        size_t coverCount=_covers.size();
        listener->coverLines(listener,_covers.data(),&coverCount,&newDataSize,&oldDataSize);
        assert(coverCount<=_covers.size());
        diff.covers.resize(coverCount);
        diff.newData_end=diff.newData+newDataSize;
        diff.oldData_end=diff.oldData+oldDataSize;
        for (size_t i=0; i<diff.covers.size(); ++i){
            diff.covers[i].oldPos=(TInt)_covers[i].oldPos;
            diff.covers[i].newPos=(TInt)_covers[i].newPos;
            diff.covers[i].length=(TInt)_covers[i].length;
        }
    }
    sub_cover(diff);
    serialize_single_compressed_diff(diff,out_diff,compressPlugin,patchStepMemSize);
}

static hpatch_BOOL _check_single_onDiffInfo(struct sspatch_listener_t* listener,
                                            const hpatch_singleCompressedDiffInfo* info,
                                            hpatch_TDecompress** out_decompressPlugin,
                                            unsigned char** out_temp_cache,
                                            unsigned char** out_temp_cacheEnd){
    size_t memSize=(size_t)(info->stepMemSize+hpatch_kStreamCacheSize*3);
    *out_temp_cache=(unsigned char*)malloc(memSize);
    *out_temp_cacheEnd=(*out_temp_cache)+memSize;   
    *out_decompressPlugin=(hpatch_TDecompress*)listener->import;
    return hpatch_TRUE;
}
static void _check_single_onPatchFinish(struct sspatch_listener_t* listener,
                                        unsigned char* temp_cache, unsigned char* temp_cacheEnd){
    if (temp_cache) free(temp_cache);
}

bool check_single_compressed_diff(const TByte* newData,const TByte* newData_end,
                                  const TByte* oldData,const TByte* oldData_end,
                                  const TByte* diff,const TByte* diff_end,
                                  hpatch_TDecompress* decompressPlugin){
    sspatch_listener_t listener={0};
    listener.import=decompressPlugin;
    listener.onDiffInfo=_check_single_onDiffInfo;
    listener.onPatchFinish=_check_single_onPatchFinish;
    TAutoMem updateNewData(newData_end-newData);
    TByte* updateNew0=updateNewData.data();
    if (!patch_single_stream_by_mem(&listener,updateNew0,updateNew0+updateNewData.size(),
                                    oldData,oldData_end,diff,diff_end)) return false;
    return (0==memcmp(updateNew0,newData,updateNewData.size()));
}


#define _test(value) { if (!(value)) { fprintf(stderr,"patch check "#value" error!\n");  return hpatch_FALSE; } }

bool check_compressed_diff_stream(const hpatch_TStreamInput*  newData,
                                  const hpatch_TStreamInput*  oldData,
                                  const hpatch_TStreamInput*  compressed_diff,
                                  hpatch_TDecompress* decompressPlugin){

    struct _TCheckOutNewDataStream:public hpatch_TStreamOutput{
        explicit _TCheckOutNewDataStream(const hpatch_TStreamInput*  _newData,
                                         TByte* _buf,size_t _bufSize)
        :newData(_newData),writedLen(0),buf(_buf),bufSize(_bufSize){
            streamImport=this;
            streamSize=newData->streamSize;
            read_writed=0;
            write=_write_check;
        }
        static hpatch_BOOL _write_check(const hpatch_TStreamOutput* stream,hpatch_StreamPos_t writeToPos,
                                        const TByte* data,const TByte* data_end){
            _TCheckOutNewDataStream* self=(_TCheckOutNewDataStream*)stream->streamImport;
            _test(self->writedLen==writeToPos);
            self->writedLen+=(size_t)(data_end-data);
            _test(self->writedLen<=self->streamSize);
            
            hpatch_StreamPos_t readPos=writeToPos;
            while (data<data_end) {
                size_t readLen=(size_t)(data_end-data);
                if (readLen>self->bufSize) readLen=self->bufSize;
                _test(self->newData->read(self->newData,readPos,self->buf,self->buf+readLen));
                _test(0==memcmp(data,self->buf,readLen));
                data+=readLen;
                readPos+=readLen;
            }
            return hpatch_TRUE;
        }
        bool isWriteFinish()const{ return writedLen==newData->streamSize; }
        const hpatch_TStreamInput*  newData;
        hpatch_StreamPos_t          writedLen;
        TByte*                      buf;
        size_t                      bufSize;
    };
    
    const size_t kACacheBufSize=hpatch_kFileIOBufBetterSize;
    TAutoMem _cache(kACacheBufSize*8);
    _TCheckOutNewDataStream out_newData(newData,_cache.data(),kACacheBufSize);
    _test(patch_decompress_with_cache(&out_newData,oldData,compressed_diff,decompressPlugin,
                                             _cache.data()+kACacheBufSize,_cache.data_end()));
    _test(out_newData.isWriteFinish());
    return true;
}
#undef _test

//for test
void __hdiff_private__create_compressed_diff(const TByte* newData,const TByte* newData_end,
                                             const TByte* oldData,const TByte* oldData_end,
                                             std::vector<TByte>& out_diff,
                                             const hdiff_TCompress* compressPlugin,int kMinSingleMatchScore,
                                             const TSuffixString* sstring){
    TDiffData diff;
    get_diff(newData,newData_end,oldData,oldData_end,diff,kMinSingleMatchScore,sstring);
    serialize_compressed_diff(diff,out_diff,compressPlugin);
}


//======================
#include "private_diff/limit_mem_diff/digest_matcher.h"
#include "private_diff/limit_mem_diff/stream_serialize.h"

static void getCovers_by_stream(const hpatch_TStreamInput*  newData,
                                const hpatch_TStreamInput*  oldData,
                                size_t kMatchBlockSize,bool kIsSkipSameRange,
                                TCovers& out_covers){
    {
        TDigestMatcher matcher(oldData,kMatchBlockSize,kIsSkipSameRange);
        matcher.search_cover(newData,&out_covers);
    }
    {//check cover
        TCover cover;
        hpatch_StreamPos_t lastNewEnd=0;
        size_t coverCount=out_covers.coverCount();
        for (size_t i=0;i<coverCount;++i){
            out_covers.covers(i,&cover);
            assert_cover_safe(cover,lastNewEnd,newData->streamSize,oldData->streamSize);
            lastNewEnd=cover.newPos+cover.length;
        }
    }
    //todo: + extend_cover_stream ?
}

static void stream_serialize(const hpatch_TStreamInput*  newData,
                             hpatch_StreamPos_t          oldDataSize,
                             const hpatch_TStreamOutput* out_diff,
                             const hdiff_TCompress* compressPlugin,
                             const TCovers& covers){
    
    std::vector<TByte> rle_ctrlBuf;
    std::vector<TByte> rle_codeBuf;
    {//empty rle //todo: suport rle data
        if (newData->streamSize>0)
          packUIntWithTag(rle_ctrlBuf,newData->streamSize-1,kByteRleType_rle0,kByteRleType_bit);
        assert(rle_codeBuf.empty());
    }
    
    TDiffStream outDiff(out_diff);
    {//type
        std::vector<TByte> out_type;
        _outType(out_type,compressPlugin);
        outDiff.pushBack(out_type.data(),out_type.size());
    }
    outDiff.packUInt(newData->streamSize);
    outDiff.packUInt(oldDataSize);
    outDiff.packUInt(covers.coverCount());
    const hpatch_StreamPos_t cover_buf_size=TCoversStream::getDataSize(covers);
    outDiff.packUInt(cover_buf_size);
    TPlaceholder compress_cover_buf_sizePos=
        outDiff.packUInt_pos(compressPlugin?cover_buf_size:0); //compress_cover_buf size
    outDiff.packUInt(rle_ctrlBuf.size());//rle_ctrlBuf size
    outDiff.packUInt(0);//compress_rle_ctrlBuf size
    outDiff.packUInt(rle_codeBuf.size());//rle_codeBuf size
    outDiff.packUInt(0);//compress_rle_codeBuf size
    const hpatch_StreamPos_t newDataDiff_size=
                                TNewDataDiffStream::getDataSize(covers,newData->streamSize);
    outDiff.packUInt(newDataDiff_size);
    TPlaceholder compress_newDataDiff_sizePos=
        outDiff.packUInt_pos(compressPlugin?newDataDiff_size:0); //compress_newDataDiff size
    
    {//save covers
        TCoversStream cover_buf(covers,cover_buf_size);
        outDiff.pushStream(&cover_buf,compressPlugin,compress_cover_buf_sizePos);
    }
    {//rle
        outDiff.pushBack(rle_ctrlBuf.data(),rle_ctrlBuf.size());
        outDiff.pushBack(rle_codeBuf.data(),rle_codeBuf.size());
    }
    {//save newDataDiff
        TNewDataDiffStream newDataDiff(covers,newData,newDataDiff_size);
        outDiff.pushStream(&newDataDiff,compressPlugin,compress_newDataDiff_sizePos);
    }
}

void create_compressed_diff_stream(const hpatch_TStreamInput*  newData,
                                   const hpatch_TStreamInput*  oldData,
                                   const hpatch_TStreamOutput* out_diff,
                                   const hdiff_TCompress* compressPlugin,size_t kMatchBlockSize){
    const bool isSkipSameRange=(compressPlugin!=0);
    TCovers covers(oldData->streamSize,newData->streamSize);
    getCovers_by_stream(newData,oldData,kMatchBlockSize,isSkipSameRange,covers);
    stream_serialize(newData,oldData->streamSize,out_diff,compressPlugin,covers);
}


void resave_compressed_diff(const hpatch_TStreamInput*  in_diff,
                            hpatch_TDecompress*         decompressPlugin,
                            const hpatch_TStreamOutput* out_diff,
                            const hdiff_TCompress*      compressPlugin){
    _THDiffzHead              head;
    hpatch_compressedDiffInfo diffInfo;
    assert(in_diff!=0);
    assert(in_diff->read!=0);
    assert(out_diff!=0);
    assert(out_diff->write!=0);
    
    {//read head
        checki(read_diffz_head(&diffInfo,&head,in_diff),
               "resave_compressed_diff() read_diffz_head() error!");
        checki((decompressPlugin!=0)||(diffInfo.compressedCount<=0),
               "resave_compressed_diff() decompressPlugin null error!");
        if ((decompressPlugin)&&(diffInfo.compressedCount>0)){
            checki(decompressPlugin->is_can_open(diffInfo.compressType),
                   "resave_compressed_diff() decompressPlugin cannot open compressed data error!");
        }
    }
    
    TDiffStream outDiff(out_diff);
    {//type
        std::vector<TByte> out_type;
        _outType(out_type,compressPlugin);
        outDiff.pushBack(out_type.data(),out_type.size());
    }
    {//copy other
        TStreamClip clip(in_diff,head.typesEndPos,head.compressSizeBeginPos);
        outDiff.pushStream(&clip);
    }
    outDiff.packUInt(head.cover_buf_size);
    TPlaceholder compress_cover_buf_sizePos=
        outDiff.packUInt_pos(compressPlugin?head.cover_buf_size:0);//compress_cover_buf size
    outDiff.packUInt(head.rle_ctrlBuf_size);//rle_ctrlBuf size
    TPlaceholder compress_rle_ctrlBuf_sizePos=
        outDiff.packUInt_pos(compressPlugin?head.rle_ctrlBuf_size:0);//compress_rle_ctrlBuf size
    outDiff.packUInt(head.rle_codeBuf_size);//rle_codeBuf size
    TPlaceholder compress_rle_codeBuf_sizePos=
        outDiff.packUInt_pos(compressPlugin?head.rle_codeBuf_size:0);//compress_rle_codeBuf size
    outDiff.packUInt(head.newDataDiff_size);
    TPlaceholder compress_newDataDiff_sizePos=
        outDiff.packUInt_pos(compressPlugin?head.newDataDiff_size:0);//compress_newDataDiff size
    
    {//save covers
        TStreamClip clip(in_diff,head.headEndPos,head.coverEndPos,
                         (head.compress_cover_buf_size>0)?decompressPlugin:0,head.cover_buf_size);
        outDiff.pushStream(&clip,compressPlugin,compress_cover_buf_sizePos);
    }
    hpatch_StreamPos_t diffPos0=head.coverEndPos;
    {//save rle ctrl
        bool isCompressed=(head.compress_rle_ctrlBuf_size>0);
        hpatch_StreamPos_t bufSize=isCompressed?head.compress_rle_ctrlBuf_size:head.rle_ctrlBuf_size;
        TStreamClip clip(in_diff,diffPos0,diffPos0+bufSize,
                         isCompressed?decompressPlugin:0,head.rle_ctrlBuf_size);
        outDiff.pushStream(&clip,compressPlugin,compress_rle_ctrlBuf_sizePos);
        diffPos0+=bufSize;
    }
    {//save rle code
        bool isCompressed=(head.compress_rle_codeBuf_size>0);
        hpatch_StreamPos_t bufSize=isCompressed?head.compress_rle_codeBuf_size:head.rle_codeBuf_size;
        TStreamClip clip(in_diff,diffPos0,diffPos0+bufSize,
                         isCompressed?decompressPlugin:0,head.rle_codeBuf_size);
        outDiff.pushStream(&clip,compressPlugin,compress_rle_codeBuf_sizePos);
        diffPos0+=bufSize;
    }
    {//save newDataDiff
        bool isCompressed=(head.compress_newDataDiff_size>0);
        hpatch_StreamPos_t bufSize=isCompressed?head.compress_newDataDiff_size:head.newDataDiff_size;
        TStreamClip clip(in_diff,diffPos0,diffPos0+bufSize,
                         isCompressed?decompressPlugin:0,head.newDataDiff_size);
        outDiff.pushStream(&clip,compressPlugin,compress_newDataDiff_sizePos);
        diffPos0+=bufSize;
    }
}
