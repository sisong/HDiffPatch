//diff.cpp
//
/*
 The MIT License (MIT)
 Copyright (c) 2012-2017 HouSisong

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
#include <string.h> //strlen
#include <assert.h>
#include <vector>
#include "private_diff/suffix_string.h"
#include "private_diff/bytes_rle.h"
#include "private_diff/pack_uint.h"
#include "../HPatch/patch.h"


static const int kDefaultMinMatchScore        = 2; //最小搜寻覆盖收益.
static const int kDefaultMinSingleMatchScore  =12; //最小独立覆盖收益.

namespace{
    
    typedef unsigned char TByte;
    typedef size_t        TUInt;
    typedef ptrdiff_t     TInt;
    static const int kMinTrustMatchLength=1024*8;  //(贪婪)选定该覆盖线(优化一些速度).
    static const int kMaxLinkSpaceLength=(1<<9)-1; //跨覆盖线合并时,允许合并的最远距离.
    
    inline static void pushBack(std::vector<TByte>& out_buf,const TByte* data,const TByte* data_end){
        out_buf.insert(out_buf.end(),data,data_end);
    }
    inline static void pushBack(std::vector<TByte>& out_buf,const std::vector<TByte>& data){
        out_buf.insert(out_buf.end(),data.begin(),data.end());
    }
    
    //覆盖线.
    struct TOldCover {
        TInt   newPos;
        TInt   oldPos;
        TInt   length;
        inline TOldCover():newPos(0),oldPos(0),length(0) { }
        inline TOldCover(TInt _newPos,TInt _oldPos,TInt _length)
            :newPos(_newPos),oldPos(_oldPos),length(_length) { }
        inline TOldCover(const TOldCover& cover)
            :newPos(cover.newPos),oldPos(cover.oldPos),length(cover.length) { }
        
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

    
    static TInt _getUIntCost(TUInt v){
        if ((sizeof(TUInt)>4)&&(v>>56)==0) {
            TInt cost=1;
            TUInt t;
            if ((t=(v>>28))) { v=t; cost+=4; }
            if ((t=(v>>14))) { v=t; cost+=2; }
            if ((t=(v>> 7))) { v=t; ++cost; }
            return cost;
        }else{
            return 9;
        }
    }
    inline static TInt _getIntCost(TInt v){
        return _getUIntCost(2*((v>=0)?v:(-v)));
    }
    
    //粗略估算覆盖线的控制数据成本;
    inline static TInt getCoverCtrlCost(const TOldCover& cover,const TOldCover& lastCover){
        static const int kUnLinkOtherScore=0;
        return _getIntCost(cover.oldPos-lastCover.oldPos) + _getUIntCost(cover.length)
              +_getUIntCost(cover.newPos-lastCover.newPos) + kUnLinkOtherScore;
    }
    
    //粗略估算区域内当作覆盖时的可能存储成本.
    static TInt getCoverCost(TInt newPos,TInt oldPos,TInt length,const TDiffData& diff){
        if ((oldPos<0)||(oldPos+length>(diff.oldData_end-diff.oldData)))
            return length+kMinTrustMatchLength;
        TInt cost=0;
        bool eq_state=true;
        for (TInt i=0; i<length; ++i) {
            if (diff.newData[newPos+i]==diff.oldData[oldPos+i]){
                if (!eq_state) //认为切换是有成本的;
                    ++cost;
                eq_state=true;
            }else{
                eq_state=false;
                ++cost;
            }
        }
        return cost;
    }
    
    inline static TInt getCoverCost(const TOldCover& cover,const TDiffData& diff){
        return getCoverCost(cover.newPos,cover.oldPos,cover.length,diff);
    }
    
    static TInt getNoCoverCost(TInt newPos,TInt length,const TDiffData& diff){
        TInt cost=0;
        bool eq_state=true;
        for (TInt i=0; i<length; ++i) {
            if (diff.newData[newPos+i]==0){
                if (!eq_state) //认为切换是有成本的;
                    ++cost;
                eq_state=true;
            }else{
                eq_state=false;
                ++cost;
            }
        }
        return cost;
    }
    
    inline static TInt getCoverSorce(const TOldCover& cover,const TOldCover& lastCover,const TDiffData& diff){
        return (getNoCoverCost(cover.newPos,cover.length,diff)
              -getCoverCost(cover,diff))-getCoverCtrlCost(cover,lastCover);
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
    TInt lastLinkCost=getCoverCost(matchCover.newPos,linkOldPos,matchCover.length,diff);
    if (lastLinkCost>=matchCost)
        return false;
    TInt len=lastCover.length+linkSpaceLength+matchCover.length*2/3;//扩展大部分,剩下的可能扩展留给extend_cover.
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
    TInt matchLinkCost=getCoverCost(lastCover.newPos,linkOldPos,lastCover.length,diff);
    if (lastCost>=matchLinkCost)
        lastCover.oldPos=linkOldPos;
}

//寻找合适的覆盖线.
static void search_cover(TDiffData& diff,const TSuffixString& sstring,int kMinMatchScore){
    if (sstring.SASize()<=0) return;
    const TInt maxSearchNewPos=(diff.newData_end-diff.newData)-kMinMatchScore;
    TInt newPos=0;
    TOldCover lastCover(0,0,0);
    while (newPos<=maxSearchNewPos) {
        TInt matchOldPos=0;
        TInt matchEqLength=getBestMatch(&matchOldPos,sstring,diff.newData+newPos,diff.newData_end);
        TOldCover matchCover(newPos,matchOldPos,matchEqLength);
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
        newPos=std::max(newPos+1,lastCover.newPos+lastCover.length);
    }
}

//选择合适的覆盖线,去掉不合适的.
static void select_cover(TDiffData& diff,int kMinSingleMatchScore){
    std::vector<TOldCover>&  covers=diff.covers;

    TOldCover lastCover(0,0,0);
    const TInt coverSize_old=(TInt)covers.size();
    TInt insertIndex=0;
    for (TInt i=0;i<coverSize_old;++i){
        if (covers[i].oldPos<0) continue;//处理已经del的.
        bool isNeedSave=covers[i].length>=kMinTrustMatchLength;//足够长.
        if (!isNeedSave){//向前合并可能.
            if ((insertIndex>0)&&(covers[insertIndex-1].isCanLink(covers[i])))
                isNeedSave=true;
        }
        if ((!isNeedSave)&&(i+1<coverSize_old)){//查询向后合并可能link
            if (covers[i].isCanLink(covers[i+1])){//右邻直接判断.
                isNeedSave=true;
            }
        }
        if (!isNeedSave){//单覆盖是否保留.
            isNeedSave=(getCoverSorce(covers[i],lastCover,diff)>=kMinSingleMatchScore);
        }

        if (isNeedSave){
            if ((insertIndex>0)&&(covers[insertIndex-1].isCanLink(covers[i]))){//link合并.
                covers[insertIndex-1].Link(covers[i]);
            }else{
                covers[insertIndex]=covers[i];
                ++insertIndex;
            }
            lastCover=covers[insertIndex-1];
        }
    }
    covers.resize(insertIndex);
}

    
    //得到可以扩展位置的长度.
    static TInt getCanExtendLength(TInt oldPos,TInt newPos,int inc,
                                   TInt newPos_min,TInt lastNewEnd,const TDiffData& diff){
        typedef size_t TFixedFloatSmooth; //定点数.
        static const TFixedFloatSmooth kFixedFloatSmooth_base=1024*8;//定点数小数点位置.
        static const TFixedFloatSmooth kExtendMinSameRatio=
                            (TFixedFloatSmooth)(0.463f*kFixedFloatSmooth_base); //0.40--0.55
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
static void extend_cover(TDiffData& diff){
    std::vector<TOldCover>&  covers=diff.covers;

    TInt lastNewEnd=0;
    for (TInt i=0; i<(TInt)covers.size(); ++i) {
        TInt newPos_next=(TInt)(diff.newData_end-diff.newData);
        if (i+1<(TInt)covers.size())
            newPos_next=covers[i+1].newPos;
        TOldCover& curCover=covers[i];
        //向前延伸.
        TInt extendLength_front=getCanExtendLength(curCover.oldPos-1,curCover.newPos-1,
                                                   -1,lastNewEnd,newPos_next,diff);
        if (extendLength_front>0){
            curCover.oldPos-=extendLength_front;
            curCover.newPos-=extendLength_front;
            curCover.length+=extendLength_front;
        }
        //向后延伸.
        TInt extendLength_back=getCanExtendLength(curCover.oldPos+curCover.length,
                                                  curCover.newPos+curCover.length,
                                                  1,lastNewEnd,newPos_next,diff);
        if (extendLength_back>0){
            curCover.length+=extendLength_back;
        }

        lastNewEnd=curCover.newPos+curCover.length;
    }
}

    static void check_cover_safe(const TOldCover& cover,TInt lastNewEnd,TInt newSize,TInt oldSize){
        if (!(cover.length>0)) throw cover;
        if (!(cover.newPos>=lastNewEnd)) throw cover;
        if (!(cover.newPos<newSize)) throw cover;
        if (!(cover.newPos+cover.length>0)) throw cover;
        if (!(cover.newPos+cover.length<=newSize)) throw cover;
        if (!(cover.oldPos>=0)) throw cover;
        if (!(cover.oldPos<oldSize)) throw cover;
        if (!(cover.oldPos+cover.length>0)) throw cover;
        if (!(cover.oldPos+cover.length<=oldSize)) throw cover;
    }

//用覆盖线得到差异数据.
static void sub_cover(TDiffData& diff){
    std::vector<TOldCover>& covers=diff.covers;
    const TByte*            newData=diff.newData;
    const TByte*            oldData=diff.oldData;

    diff.newDataSubDiff.resize(0);
    diff.newDataSubDiff.resize(diff.newData_end-diff.newData,0);
    TByte*  newDataSubDiff=diff.newDataSubDiff.empty()?0:&diff.newDataSubDiff[0];
    diff.newDataDiff.resize(0);
    diff.newDataDiff.reserve((diff.newData_end-diff.newData)>>2);
    
    TInt lastNewEnd=0;
    for (TInt i=0;i<(TInt)covers.size();++i){
        check_cover_safe(covers[i],lastNewEnd,
                         diff.newData_end-diff.newData,diff.oldData_end-diff.oldData);
        const TInt newPos=covers[i].newPos;
        if (newPos>lastNewEnd)
            pushBack(diff.newDataDiff,newData+lastNewEnd,newData+newPos);
        const TInt oldPos=covers[i].oldPos;
        const TInt length=covers[i].length;
        for (TInt i=0; i<length;++i)
            newDataSubDiff[i+newPos]=(newData[i+newPos]-oldData[i+oldPos]);
        lastNewEnd=newPos+length;
    }
    if (lastNewEnd<diff.newData_end-newData)
        pushBack(diff.newDataDiff,newData+lastNewEnd,diff.newData_end);
}

//diff结果序列化输出.
static void serialize_diff(const TDiffData& diff,std::vector<TByte>& out_diff){
    const TUInt ctrlCount=(TUInt)diff.covers.size();
    std::vector<TByte> length_buf;
    std::vector<TByte> inc_newPos_buf;
    std::vector<TByte> inc_oldPos_buf;
    {
        TInt oldPosBack=0;
        TInt lastNewEnd=0;
        for (TUInt i=0; i<ctrlCount; ++i) {
            packUInt(length_buf, diff.covers[i].length);
            assert(diff.covers[i].newPos>=lastNewEnd);
            packUInt(inc_newPos_buf,diff.covers[i].newPos-lastNewEnd); //save inc_newPos
            if (diff.covers[i].oldPos>=oldPosBack){ //save inc_oldPos
                packUIntWithTag(inc_oldPos_buf,diff.covers[i].oldPos-oldPosBack, 0, 1);
            }else{
                packUIntWithTag(inc_oldPos_buf,oldPosBack-diff.covers[i].oldPos, 1, 1);
            }
            oldPosBack=diff.covers[i].oldPos;
            lastNewEnd=diff.covers[i].newPos+diff.covers[i].length;
        }
    }

    packUInt(out_diff, ctrlCount);
    packUInt(out_diff, (TUInt)length_buf.size());
    packUInt(out_diff, (TUInt)inc_newPos_buf.size());
    packUInt(out_diff, (TUInt)inc_oldPos_buf.size());
    packUInt(out_diff, (TUInt)diff.newDataDiff.size());
    pushBack(out_diff,length_buf);
    pushBack(out_diff,inc_newPos_buf);
    pushBack(out_diff,inc_oldPos_buf);
    pushBack(out_diff,diff.newDataDiff);
    
    const TByte* newDataSubDiff=diff.newDataSubDiff.empty()?0:&diff.newDataSubDiff[0];
    bytesRLE_save(out_diff,newDataSubDiff,
                  newDataSubDiff+diff.newDataSubDiff.size(),kRle_bestSize);
}
    
    
    static void do_compress(std::vector<TByte>& out_code,const std::vector<TByte>& data,
                            const hdiff_TCompress* compressPlugin){
        out_code.clear();
        if (!compressPlugin) return;
        if (data.empty()) return;
        size_t maxCodeSize=compressPlugin->maxCompressedSize(compressPlugin,data.size());
        if (maxCodeSize<=0) return;
        out_code.resize(maxCodeSize);
        const TByte* data0=&data[0];
        TByte* out_code0=&out_code[0];
        size_t codeSize=compressPlugin->compress(compressPlugin,
                                                 out_code0,out_code0+out_code.size(),
                                                 data0,data0+data.size());
        if (codeSize<=0) throw codeSize; //diff error!
        if (codeSize<data.size())
            out_code.resize(codeSize); //ok
        else
            out_code.clear();
    }
    inline static void pushCompressCode(std::vector<TByte>& out_diff,
                                        const std::vector<TByte>& compress_code,
                                        const std::vector<TByte>& data){
        if (compress_code.empty())
            pushBack(out_diff,data);
        else
            pushBack(out_diff,compress_code);
    }
    
static void serialize_compressed_diff(const TDiffData& diff,std::vector<TByte>& out_diff,
                                      const hdiff_TCompress* compressPlugin){
    const TUInt ctrlCount=(TUInt)diff.covers.size();
    std::vector<TByte> cover_buf;
    {
        TInt oldPos_end=0;
        TInt lastNewEnd=0;
        for (TUInt i=0; i<ctrlCount; ++i) {
            if (diff.covers[i].oldPos>=oldPos_end){ //save inc_oldPos
                packUIntWithTag(cover_buf,diff.covers[i].oldPos-oldPos_end, 0, 1);
            }else{
                packUIntWithTag(cover_buf,oldPos_end-diff.covers[i].oldPos, 1, 1);
            }
            assert(diff.covers[i].newPos>=lastNewEnd);
            packUInt(cover_buf,diff.covers[i].newPos-lastNewEnd); //save inc_newPos
            packUInt(cover_buf,diff.covers[i].length);
            oldPos_end=diff.covers[i].oldPos+diff.covers[i].length;//!
            lastNewEnd=diff.covers[i].newPos+diff.covers[i].length;
        }
    }
    
    std::vector<TByte> rle_ctrlBuf;
    std::vector<TByte> rle_codeBuf;
    const TByte* newDataSubDiff=diff.newDataSubDiff.empty()?0:&diff.newDataSubDiff[0];
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

    static const char kVersionType[8+1]="HDIFF13&";
    const char* compressType="";
    if (compressPlugin){
        compressType=compressPlugin->compressType(compressPlugin);
        if (strlen(compressType)>hpatch_kMaxInfoLength) throw compressType; //diff error!
    }
    pushBack(out_diff,(const TByte*)kVersionType,
                      (const TByte*)kVersionType+strlen(kVersionType));
    pushBack(out_diff,(const TByte*)compressType,
                      (const TByte*)compressType+strlen(compressType)+1);//with '\0'
    
    TByte pluginInfo[hpatch_kMaxInfoLength];
    size_t pluginInfoSize=0;
    if ((compressPlugin)&&(compressPlugin->pluginInfoSize)){
        pluginInfoSize=compressPlugin->pluginInfoSize(compressPlugin);
        if (pluginInfoSize>hpatch_kMaxInfoLength) throw pluginInfoSize; //diff error!
        compressPlugin->pluginInfo(compressPlugin,pluginInfo,pluginInfo+pluginInfoSize);
    }
    packUInt(out_diff,pluginInfoSize);
    pushBack(out_diff,pluginInfo,pluginInfo+pluginInfoSize);
    
    const TUInt newDataSize=(TUInt)(diff.newData_end-diff.newData);
    const TUInt oldDataSize=(TUInt)(diff.oldData_end-diff.oldData);
    packUInt(out_diff, newDataSize);
    packUInt(out_diff, oldDataSize);
    packUInt(out_diff, ctrlCount);
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

    struct THDiffPrivateParams{
        int kMinMatchScore;
        int kMinSingleMatchScore;
    };
    
static void get_diff(const TByte* newData,const TByte* newData_end,
                     const TByte* oldData,const TByte* oldData_end,
                     TDiffData&   out_diff,
                     const THDiffPrivateParams *kDiffParams=0,
                     const TSuffixString* sstring=0){
    
    int kMinMatchScore       = kDefaultMinMatchScore;
    int kMinSingleMatchScore  = kDefaultMinSingleMatchScore;
    if (kDiffParams){
        kMinMatchScore      =kDiffParams->kMinMatchScore;
        kMinSingleMatchScore=kDiffParams->kMinSingleMatchScore;
    }
    assert(newData<=newData_end);
    assert(oldData<=oldData_end);
    TSuffixString _sstring_default(0,0);
    if (sstring==0){
        _sstring_default.resetSuffixString(oldData,oldData_end);
        sstring=&_sstring_default;
    }
    
    TDiffData& diff=out_diff;
    diff.newData=newData;
    diff.newData_end=newData_end;
    diff.oldData=oldData;
    diff.oldData_end=oldData_end;
    
    search_cover(diff,*sstring,kMinMatchScore);
    sstring=0;
    _sstring_default.clear();
    
    extend_cover(diff);//先尝试扩展.
    select_cover(diff,kMinSingleMatchScore);
    extend_cover(diff);//select_cover会删除一些覆盖线,所以重新扩展.
    sub_cover(diff);
}
    
}//end namespace


void create_diff(const TByte* newData,const TByte* newData_end,
                 const TByte* oldData,const TByte* oldData_end,
                 std::vector<TByte>& out_diff){
    TDiffData diff;
    get_diff(newData,newData_end,oldData,oldData_end,diff);
    serialize_diff(diff,out_diff);
}

bool check_diff(const TByte* newData,const TByte* newData_end,
                const TByte* oldData,const TByte* oldData_end,
                const TByte* diff,const TByte* diff_end){
    std::vector<TByte> testNewData(newData_end-newData);
    TByte* testNewData0=testNewData.empty()?0:&testNewData[0];
    if (!patch(testNewData0,testNewData0+testNewData.size(),
               oldData,oldData_end,
               diff,diff_end))
        return false;
    for (TUInt i=0; i<(TUInt)testNewData.size(); ++i) {
        if (testNewData[i]!=newData[i])
            return false;
    }
    return true;
}

void create_compressed_diff(const unsigned char* newData,const unsigned char* newData_end,
                            const unsigned char* oldData,const unsigned char* oldData_end,
                            std::vector<unsigned char>& out_diff,
                            const hdiff_TCompress* compressPlugin){
    TDiffData diff;
    get_diff(newData,newData_end,oldData,oldData_end,diff);
    serialize_compressed_diff(diff,out_diff,compressPlugin);
}

bool check_compressed_diff(const unsigned char* newData,const unsigned char* newData_end,
                           const unsigned char* oldData,const unsigned char* oldData_end,
                           const unsigned char* diff,const unsigned char* diff_end,
                           hpatch_TDecompress* decompressPlugin){
    std::vector<TByte> testNewData(newData_end-newData);
    TByte* testNewData0=testNewData.empty()?0:&testNewData[0];
    
    if (!patch_decompress_mem(testNewData0,testNewData0+testNewData.size(),
                              oldData,oldData_end, diff,diff_end,
                              decompressPlugin)) return false;
    for (TUInt i=0; i<(TUInt)testNewData.size(); ++i) {
        if (testNewData[i]!=newData[i])
            return false;
    }
    return true;
}

//for test
void __hdiff_private__create_compressed_diff(const TByte* newData,const TByte* newData_end,
                                             const TByte* oldData,const TByte* oldData_end,
                                             std::vector<TByte>& out_diff,
                                             const hdiff_TCompress* compressPlugin,
                                             const void* _kDiffParams,
                                             const TSuffixString* sstring){
    TDiffData diff;
    get_diff(newData,newData_end,oldData,oldData_end,diff,(const THDiffPrivateParams*)_kDiffParams,sstring);
    serialize_compressed_diff(diff,out_diff,compressPlugin);
}



