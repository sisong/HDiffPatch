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
#include <assert.h>
#include <vector>
#include "private_diff/suffix_string.h"
#include "private_diff/bytes_rle.h"
#include "private_diff/pack_uint.h"
#include "../HPatch/patch.h"


namespace{
    
    typedef unsigned char TByte;
    typedef size_t        TUInt;
    typedef ptrdiff_t     TInt;
    static const int kMinTrustMatchLength=1024*8;  //(贪婪)选定该覆盖线(优化一些速度).
    static const int kUnLinkLength=6;              //搜索时不能合并的代价.
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
    std::vector<TOldCover>  cover;          //选出的覆盖线.
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
static bool getBestMatch(const TSuffixString& sstring,const TByte* newData,const TByte* newData_end,
                         TInt* out_pos,TInt* out_length,int kMinMatchLength){
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
    *out_length=bestLength;
    return (bestLength>=kMinMatchLength);
}

//粗略估算区域内当作覆盖时的可能收益.
static TInt getCoverScore(TInt newPos,TInt oldPos,TInt length,const TDiffData& diff){
    if ((oldPos<0)||(oldPos+length>(diff.oldData_end-diff.oldData)))
        return -kUnLinkLength;
    TInt eqScore=0;
    bool eq_state=true;
    for (TInt i=0; i<length; ++i) {
        if (diff.newData[newPos+i]==diff.oldData[oldPos+i]){
            ++eqScore;
            if (eq_state) //认为切换是有成本的;
                ++eqScore;
            eq_state=true;
        }else{
            eq_state=false;
        }
    }
    return (eqScore+1)>>1;
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
    TInt lastLinkScore=getCoverScore(matchCover.newPos,linkOldPos,matchCover.length,diff);
    if ((lastLinkScore<=0)|(lastLinkScore+kUnLinkLength<=matchCover.length))
        return false;
    TInt len=lastCover.length+linkSpaceLength+lastLinkScore*2/3;//扩展大部分,剩下的可能扩展留给extend_cover.
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
    TInt lastScore=getCoverScore(lastCover.newPos,lastCover.oldPos,lastCover.length,diff);
    TInt matchLinkEqLength=getCoverScore(lastCover.newPos,linkOldPos,lastCover.length,diff);
    if (lastScore<=matchLinkEqLength)
        lastCover.oldPos=linkOldPos;
}

//寻找合适的覆盖线.
static void search_cover(TDiffData& diff,const TSuffixString& sstring,int kMinMatchLength){
    if (sstring.SASize()<=0) return;
    const TInt maxSearchNewPos=(diff.newData_end-diff.newData)-kMinMatchLength;
    TInt newPos=0;
    TOldCover lastCover(0,0,0);
    while (newPos<=maxSearchNewPos) {
        TInt matchOldPos=0;
        TInt matchEqLength=0;
        bool isMatched=getBestMatch(sstring,diff.newData+newPos,diff.newData_end,
                                    &matchOldPos,&matchEqLength,kMinMatchLength);
        if (!isMatched){
            ++newPos;//下一个需要匹配的字符串(逐位置匹配速度会比较慢).
            continue;
        }//else matched
        
        TOldCover matchCover(newPos,matchOldPos,matchEqLength);
        if (tryLinkExtend(lastCover,matchCover,diff)){//use link
            if (diff.cover.empty())
                diff.cover.push_back(lastCover);
            else
                diff.cover.back()=lastCover;
        }else{ //use match
            if (!diff.cover.empty())//尝试共线;
                tryCollinear(diff.cover.back(),matchCover,diff);
            diff.cover.push_back(matchCover);
        }
        lastCover=diff.cover.back();
        newPos=std::max(newPos+1,lastCover.newPos+lastCover.length);
    }
}

//选择合适的覆盖线,去掉不合适的.
static void select_cover(TDiffData& diff,int kMinSingleMatchLength){
    std::vector<TOldCover>&  cover=diff.cover;

    const TInt coverSize_old=(TInt)cover.size();
    TInt insertIndex=0;
    for (TInt i=0;i<coverSize_old;++i){
        if (cover[i].oldPos<0) continue;//处理已经del的.
        bool isNeedSave=cover[i].length>=kMinTrustMatchLength;//足够长.
        if (!isNeedSave){//向前合并可能.
            if ((insertIndex>0)&&(cover[insertIndex-1].isCanLink(cover[i])))
                isNeedSave=true;
        }
        if ((!isNeedSave)&&(i+1<coverSize_old)){//查询向后合并可能link
            if (cover[i].isCanLink(cover[i+1])){//右邻直接判断.
                isNeedSave=true;
            }
        }
        if (!isNeedSave){//单覆盖是否保留.
            TInt linkScore=getCoverScore(cover[i].newPos,(TInt)(diff.oldData_end-diff.oldData),
                                            cover[i].length,diff);
            isNeedSave=(cover[i].length-linkScore>=kMinSingleMatchLength);
        }

        if (isNeedSave){
            if ((insertIndex>0)&&(cover[insertIndex-1].isCanLink(cover[i]))){//link合并.
                cover[insertIndex-1].Link(cover[i]);
            }else{
                cover[insertIndex]=cover[i];
                ++insertIndex;
            }
        }
    }
    cover.resize(insertIndex);
}

    
    //得到可以扩展位置的长度.
    static TInt getCanExtendLength(TInt oldPos,TInt newPos,int inc,
                                   TInt newPos_min,TInt newPos_end,const TDiffData& diff){
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
             &&(newPos>=newPos_min)&&(newPos<newPos_end); ++length,oldPos+=inc,newPos+=inc) {
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
    std::vector<TOldCover>&  cover=diff.cover;

    TInt lastNewPos=0;
    for (TInt i=0; i<(TInt)cover.size(); ++i) {
        TInt newPos_next=(TInt)(diff.newData_end-diff.newData);
        if (i+1<(TInt)cover.size())
            newPos_next=cover[i+1].newPos;
        TOldCover& curCover=cover[i];
        //向前延伸.
        TInt extendLength_front=getCanExtendLength(curCover.oldPos-1,curCover.newPos-1,
                                                   -1,lastNewPos,newPos_next,diff);
        if (extendLength_front>0){
            curCover.oldPos-=extendLength_front;
            curCover.newPos-=extendLength_front;
            curCover.length+=extendLength_front;
        }
        //向后延伸.
        TInt extendLength_back=getCanExtendLength(curCover.oldPos+curCover.length,
                                                  curCover.newPos+curCover.length,
                                                  1,lastNewPos,newPos_next,diff);
        if (extendLength_back>0){
            curCover.length+=extendLength_back;
        }

        lastNewPos=curCover.newPos+curCover.length;
    }
}

//用覆盖线得到差异数据.
static void sub_cover(TDiffData& diff){
    std::vector<TOldCover>& cover=diff.cover;
    const TByte*            newData=diff.newData;
    const TByte*            oldData=diff.oldData;

    diff.newDataSubDiff.resize(0);
    diff.newDataSubDiff.resize(diff.newData_end-diff.newData,0);
    TByte*  newDataSubDiff=diff.newDataSubDiff.empty()?0:&diff.newDataSubDiff[0];
    diff.newDataDiff.resize(0);
    diff.newDataDiff.reserve((diff.newData_end-diff.newData)>>2);
    
    TInt newPos_end=0;
    for (TInt i=0;i<(TInt)cover.size();++i){
        const TInt newPos=cover[i].newPos;
        if (newPos>newPos_end)
            pushBack(diff.newDataDiff,newData+newPos_end,newData+newPos);
        const TInt oldPos=cover[i].oldPos;
        const TInt length=cover[i].length;
        for (TInt i=0; i<length;++i)
            newDataSubDiff[i+newPos]=(newData[i+newPos]-oldData[i+oldPos]);
        newPos_end=newPos+length;
    }
    if (newPos_end<diff.newData_end-newData)
        pushBack(diff.newDataDiff,newData+newPos_end,diff.newData_end);
}

//diff结果序列化输出.
static void serialize_diff(const TDiffData& diff,std::vector<TByte>& out_diff){
    const TUInt ctrlCount=(TUInt)diff.cover.size();
    std::vector<TByte> length_buf;
    std::vector<TByte> inc_newPos_buf;
    std::vector<TByte> inc_oldPos_buf;
    {
        TInt oldPosBack=0;
        TInt newPos_end=0;
        for (TUInt i=0; i<ctrlCount; ++i) {
            packUInt(length_buf, diff.cover[i].length);
            assert(diff.cover[i].newPos>=newPos_end);
            packUInt(inc_newPos_buf,diff.cover[i].newPos-newPos_end); //save inc_newPos
            if (diff.cover[i].oldPos>=oldPosBack){ //save inc_oldPos
                packUIntWithTag(inc_oldPos_buf,diff.cover[i].oldPos-oldPosBack, 0, 1);
            }else{
                packUIntWithTag(inc_oldPos_buf,oldPosBack-diff.cover[i].oldPos, 1, 1);
            }
            oldPosBack=diff.cover[i].oldPos;
            newPos_end=diff.cover[i].newPos+diff.cover[i].length;
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
    const TUInt ctrlCount=(TUInt)diff.cover.size();
    std::vector<TByte> cover_buf;
    {
        TInt oldPos_end=0;
        TInt newPos_end=0;
        for (TUInt i=0; i<ctrlCount; ++i) {
            if (diff.cover[i].oldPos>=oldPos_end){ //save inc_oldPos
                packUIntWithTag(cover_buf,diff.cover[i].oldPos-oldPos_end, 0, 1);
            }else{
                packUIntWithTag(cover_buf,oldPos_end-diff.cover[i].oldPos, 1, 1);
            }
            assert(diff.cover[i].newPos>=newPos_end);
            packUInt(cover_buf,diff.cover[i].newPos-newPos_end); //save inc_newPos
            packUInt(cover_buf,diff.cover[i].length);
            oldPos_end=diff.cover[i].oldPos+diff.cover[i].length;//!
            newPos_end=diff.cover[i].newPos+diff.cover[i].length;
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
    if (!compress_cover_buf.empty()||!compress_rle_ctrlBuf.empty()
        ||!compress_rle_codeBuf.empty()||!compress_newDataDiff.empty()){
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
        int kMinMatchLength;
        int kMinSingleMatchLength;
    };
    
static void get_diff(const TByte* newData,const TByte* newData_end,
                     const TByte* oldData,const TByte* oldData_end,
                     TDiffData&   out_diff,
                     const THDiffPrivateParams *kDiffParams=0,
                     const TSuffixString* sstring=0){
    
    int kMinMatchLength       = 8; //最小搜寻覆盖长度.
    int kMinSingleMatchLength =24; //最小独立覆盖长度.
    if (kDiffParams){
        kMinMatchLength      =kDiffParams->kMinMatchLength;
        kMinSingleMatchLength=kDiffParams->kMinSingleMatchLength;
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
    
    search_cover(diff,*sstring,kMinMatchLength);
    sstring=0;
    _sstring_default.clear();
    
    extend_cover(diff);//先尝试扩展.
    select_cover(diff,kMinSingleMatchLength);
    extend_cover(diff);//select_cover会删除一些覆盖线,所以重新扩展.
    sub_cover(diff);
}
    
}//end namespace


//for test
void __hdiff_private__create_diff(const TByte* newData,const TByte* newData_end,
                                  const TByte* oldData,const TByte* oldData_end,
                                  std::vector<TByte>& out_diff,
                                  const void* _kDiffParams,
                                  const TSuffixString* sstring){
    TDiffData diff;
    get_diff(newData,newData_end,oldData,oldData_end,diff,(const THDiffPrivateParams*)_kDiffParams,sstring);
    serialize_diff(diff,out_diff);
}

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
    
    hpatch_TStreamOutput newStream;
    hpatch_TStreamInput  oldStream;
    hpatch_TStreamInput  diffStream;
    memory_as_outputStream(&newStream,testNewData0,testNewData0+testNewData.size());
    memory_as_inputStream(&oldStream,oldData,oldData_end);
    memory_as_inputStream(&diffStream,diff,diff_end);
    
    if (!patch_decompress(&newStream,&oldStream,&diffStream,decompressPlugin))
        return false;
    for (TUInt i=0; i<(TUInt)testNewData.size(); ++i) {
        if (testNewData[i]!=newData[i])
            return false;
    }
    return true;
}


