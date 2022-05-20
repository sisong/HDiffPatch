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
#include <algorithm> //std::max std::sort
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

static const char* kHDiffVersionType  ="HDIFF13";
static const char* kHDiffSFVersionType="HDIFFSF20";

#define checki(value,info) { if (!(value)) { throw std::runtime_error(info); } }
#define check(value) checki(value,"check "#value" error!")

#if (_SSTRING_FAST_MATCH>0)
static const int kMinMatchLen   = (_SSTRING_FAST_MATCH>5)?_SSTRING_FAST_MATCH:5;
#else
static const int kMinMatchLen   = 5; //最小搜寻相等长度。
#endif
static const int kMinMatchScore = 2; //最小搜寻覆盖收益.

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

struct TDiffLimit{
    IDiffSearchCoverListener* listener;
    size_t      newPos;
    size_t      newEnd;
    size_t      recoverOldPos;
    size_t      recoverOldEnd;
    TCompressDetect& nocover_detect;
    TCompressDetect& cover_detect;
    TOldCover        lastCover_back;
};

 
//得到最长的一个匹配长度和其位置.
static TInt getBestMatch(TInt* out_pos,const TSuffixString& sstring,
                         const TByte* newData,const TByte* newData_end,
                         TInt curNewPos,TDiffLimit* diffLimit=0){
    const TInt kMaxMatchDeepForLimit=6;
    const TInt matchDeep = diffLimit?kMaxMatchDeepForLimit:2;
    const TInt kLimitOldPos=(TInt)(diffLimit?diffLimit->recoverOldPos:0);
    const TInt kLimitOldEnd=(TInt)(diffLimit?diffLimit->recoverOldEnd:sstring.SASize());
    TInt sai=sstring.lower_bound(newData,newData_end);
    
    const TByte* src_begin=sstring.src_begin();
    const TByte* src_end=sstring.src_end();
    TInt bestLength= kMinMatchLen -1;
    TInt bestOldPos=-1;
    bool leftOk = false;
    bool rightOk = false;
    for (TInt mdi= 0; mdi< matchDeep; ++mdi) {
        if (mdi&1){
            if (rightOk) continue;
        } else {
            if (leftOk) continue;
        }
        TInt i = sai + (1-(mdi&1)*2) * ((mdi+1)/2);
        if ((i<0)|(i>=(src_end-src_begin))) continue;
        TInt curOldPos=sstring.SA(i);
        if (diffLimit){
            if ((curOldPos>=kLimitOldPos)&(curOldPos<kLimitOldEnd))
                continue;
        }
        TInt curLength=getEqualLength(newData,newData_end,src_begin+curOldPos,src_end);
        if (curLength>bestLength){
            if (diffLimit){
                hpatch_TCover cover={(size_t)curOldPos,(size_t)curNewPos,(size_t)curLength};
                hpatch_StreamPos_t hitPos;
                diffLimit->listener->limitCover(diffLimit->listener,&cover,&hitPos);
                if (hitPos<=(size_t)bestLength)
                    continue;
                else
                    curLength=(TInt)hitPos;
            }
            bestLength = curLength;
            bestOldPos= curOldPos;
        }
        if (mdi&1){
            rightOk=true; if (leftOk) break;
        } else {
            leftOk=true; if (rightOk) break;
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
    inline static TInt getCoverCost(const TOldCover& cover,const TDiffData& diff){
        return (TInt)getRegionRleCost(diff.newData+cover.newPos,cover.length,diff.oldData+cover.oldPos);
    }
    
//尝试延长lastCover来完全代替matchCover;
static bool tryLinkExtend(TOldCover& lastCover,const TOldCover& matchCover,const TDiffData& diff,TDiffLimit* diffLimit){
    if (lastCover.length<=0) return false;
    const TInt linkSpaceLength=(matchCover.newPos-(lastCover.newPos+lastCover.length));
    assert(linkSpaceLength>=0);
    if (linkSpaceLength>kMaxLinkSpaceLength)
        return false;
    TInt linkOldPos=lastCover.oldPos+lastCover.length+linkSpaceLength;
    if (linkOldPos+matchCover.length>(diff.oldData_end-diff.oldData))
        return false;
    const bool isCollinear=lastCover.isCollinear(matchCover);
    if (diffLimit){
        size_t cnewPos=lastCover.newPos+lastCover.length;
        hpatch_TCover cover={(size_t)(lastCover.oldPos+lastCover.length),cnewPos,
                             (size_t)(linkSpaceLength+(isCollinear?0:matchCover.length))};
        if (!diffLimit->listener->limitCover(diffLimit->listener,&cover,0))
            return false;
    }
    if (isCollinear){//已经共线;
        lastCover.Link(matchCover);
        return true;
    }

    TInt matchCost=getCoverCtrlCost(matchCover,lastCover);
    TInt lastLinkCost=(TInt)getRegionRleCost(diff.newData+matchCover.newPos,matchCover.length,diff.oldData+linkOldPos);
    if (lastLinkCost>matchCost)
        return false;
    TInt len=lastCover.length+linkSpaceLength+(matchCover.length*2/3);//扩展大部分,剩下的可能扩展留给extend_cover.
    len+=getEqualLength(diff.newData+lastCover.newPos+len,diff.newData_end,
                        diff.oldData+lastCover.oldPos+len,diff.oldData_end);
    if (diffLimit){
        TInt limitLen=diffLimit->newEnd-lastCover.newPos;
        len=len<limitLen?len:limitLen;
        TInt safeLen=lastCover.length+linkSpaceLength+matchCover.length;
        if (len>safeLen){
            hpatch_TCover cover={(size_t)(lastCover.oldPos+safeLen),(size_t)(lastCover.newPos+safeLen),
                                 (size_t)(len-safeLen)};
            hpatch_StreamPos_t hitPos;
            diffLimit->listener->limitCover(diffLimit->listener,&cover,&hitPos);
            len=(TInt)(safeLen+hitPos);
        }
    }
    while ((len>0) && (diff.newData[lastCover.newPos+len-1]
                       !=diff.oldData[lastCover.oldPos+len-1])) {
        --len;
    }
    lastCover.length=len;
    return true;
}
    
//尝试设置lastCover为matchCover所在直线的延长线,实现共线(增加合并可能等);
static void tryCollinear(TOldCover& lastCover,const TOldCover& matchCover,const TDiffData& diff,TDiffLimit* diffLimit){
    if (lastCover.length<=0) return;
    if (lastCover.isCollinear(matchCover)) return; //已经共线;
    
    TInt linkOldPos=matchCover.oldPos-(matchCover.newPos-lastCover.newPos);
    if ((linkOldPos<0)||(linkOldPos+lastCover.length>(diff.oldData_end-diff.oldData)))
        return;
    if (diffLimit){
        hpatch_TCover cover={ (size_t)linkOldPos,(size_t)lastCover.newPos,(size_t)lastCover.length};
        if (!diffLimit->listener->limitCover(diffLimit->listener,&cover,0))
            return;
    }
    TInt lastCost=getCoverCost(lastCover,diff);
    TInt matchLinkCost=(TInt)getRegionRleCost(diff.newData+lastCover.newPos,lastCover.length,diff.oldData+linkOldPos);
    if (lastCost>=matchLinkCost)
        lastCover.oldPos=linkOldPos;
}


//寻找合适的覆盖线.
static void search_cover(std::vector<TOldCover>& covers,const TDiffData& diff,
                         const TSuffixString& sstring,TDiffLimit* diffLimit=0){
    if (sstring.SASize()<=0) return;
    TInt newPos=diffLimit?diffLimit->newPos:0;
    const TInt newEnd=diffLimit?diffLimit->newEnd:(diff.newData_end-diff.newData);
    if (newEnd-newPos<=kMinMatchLen) return;
    const TInt maxSearchNewPos=newEnd-kMinMatchLen;

    TOldCover lastCover(0,0,0);
    while (newPos<=maxSearchNewPos) {
        TInt matchOldPos=0;
        TInt matchEqLength=getBestMatch(&matchOldPos,sstring,diff.newData+newPos,diff.newData+newEnd,newPos,diffLimit);
        if (matchEqLength<kMinMatchLen){
            ++newPos;
            continue;
        }
        TOldCover matchCover(matchOldPos,newPos,matchEqLength);
        if (matchEqLength-getCoverCtrlCost(matchCover,lastCover)<kMinMatchScore){
            ++newPos;//下一个需要匹配的字符串(逐位置匹配速度会比较慢).
            continue;
        }//else matched
        
        if (tryLinkExtend(lastCover,matchCover,diff,diffLimit)){//use link
            if (covers.empty())
                covers.push_back(lastCover);
            else
                covers.back()=lastCover;
        }else{ //use match
            if (!covers.empty())//尝试共线;
                tryCollinear(covers.back(),matchCover,diff,diffLimit);
            covers.push_back(matchCover);
        }
        lastCover=covers.back();
        newPos=std::max(newPos+1,lastCover.newPos+lastCover.length);//选出的cover不允许重叠,这可能不是最优策略;
    }
}


//选择合适的覆盖线,去掉不合适的.
static void _select_cover(std::vector<TOldCover>& covers,const TDiffData& diff,int kMinSingleMatchScore,
                          TCompressDetect& nocover_detect,TCompressDetect& cover_detect,TDiffLimit* diffLimit){
    
    TOldCover lastCover(0,0,0);
    if (diffLimit)
        lastCover=diffLimit->lastCover_back;
    const TInt coverSize_old=(TInt)covers.size();
    TInt insertIndex=0;
    for (TInt i=0;i<coverSize_old;++i){
        if (covers[i].length<=0) continue;//处理已经del的.
        bool isNeedSave=false;
        bool isCanLink=false;
        if (!isNeedSave){//向前合并可能.
            if ((insertIndex>0)&&(covers[insertIndex-1].isCanLink(covers[i]))){
                if (diffLimit){
                    const TOldCover& fc=covers[insertIndex-1];
                    hpatch_TCover cover={(size_t)(fc.oldPos+fc.length),(size_t)(fc.newPos+fc.length),
                                         (size_t)fc.linkSpaceLength(covers[i])};
                    if (diffLimit->listener->limitCover(diffLimit->listener,&cover,0)){
                        isCanLink=true;
                        isNeedSave=true;
                    }
                }else{
                    isCanLink=true;
                    isNeedSave=true;
                }
            }
        }
        if (i+1<coverSize_old){//查询向后合并可能link
            for (TInt j=i+1;j<coverSize_old; ++j) {
                if (!covers[i].isCanLink(covers[j])) break;
                if (diffLimit){
                    const TOldCover& fc=covers[i];
                    hpatch_TCover cover={(size_t)(fc.oldPos+fc.length),(size_t)(fc.newPos+fc.length),
                                         (size_t)fc.linkSpaceLength(covers[j])};
                    if (diffLimit->listener->limitCover(diffLimit->listener,&cover,0)){
                        covers[i].Link(covers[j]);
                        covers[j].length=0;//del
                    }else{
                        break;
                    }
                }else{
                    covers[i].Link(covers[j]);
                    covers[j].length=0;//del
                }

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
            if (isCanLink){//link合并.
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

static void select_cover(std::vector<TOldCover>& covers,const TDiffData& diff,
                         int kMinSingleMatchScore,TDiffLimit* diffLimit=0){
    if (diffLimit==0){
        TCompressDetect  nocover_detect;
        TCompressDetect  cover_detect;
        _select_cover(covers,diff,kMinSingleMatchScore,nocover_detect,cover_detect,0);
    }else{
        _select_cover(covers,diff,kMinSingleMatchScore,diffLimit->nocover_detect,diffLimit->cover_detect,diffLimit);
    }
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
static void extend_cover(std::vector<TOldCover>& covers,const TDiffData& diff,
                         const TFixedFloatSmooth kExtendMinSameRatio,TDiffLimit* diffLimit=0){
    TInt lastNewEnd=diffLimit?diffLimit->newPos:0;
    for (TInt i=0; i<(TInt)covers.size(); ++i) {
        TInt newPos_next;
        if (i+1<(TInt)covers.size())
            newPos_next=covers[i+1].newPos;
        else
            newPos_next=diffLimit?diffLimit->newEnd:(TInt)(diff.newData_end-diff.newData);
        TOldCover& curCover=covers[i];
        if (diffLimit){
            TInt limit_front=std::min(curCover.newPos-lastNewEnd,curCover.oldPos);
            if (limit_front>0){
                hpatch_TCover cover={(size_t)(curCover.oldPos-limit_front),
                                     (size_t)(curCover.newPos-limit_front),(size_t)limit_front};
                hpatch_StreamPos_t lenLimit;
                diffLimit->listener->limitCover_front(diffLimit->listener,&cover,&lenLimit);
                lastNewEnd=curCover.newPos-(TInt)lenLimit;
            }

            TInt limit_back=newPos_next-(curCover.newPos+curCover.length);
            if ((curCover.oldPos+curCover.length)+limit_back>(diff.oldData_end-diff.oldData))
                limit_back=(diff.oldData_end-diff.oldData)-(curCover.oldPos+curCover.length);
            if (limit_back>0){
                hpatch_TCover cover={(size_t)(curCover.oldPos+curCover.length),
                                     (size_t)(curCover.newPos+curCover.length),(size_t)limit_back};
                hpatch_StreamPos_t lenLimit;
                diffLimit->listener->limitCover(diffLimit->listener,&cover,&lenLimit);
                newPos_next=(curCover.newPos+curCover.length)+ (TInt)lenLimit;
            }
        }
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

    static void assert_covers_safe(const TCovers& covers,hpatch_StreamPos_t newSize,hpatch_StreamPos_t oldSize){
        hpatch_StreamPos_t lastNewEnd=0;
        for (size_t i=0;i<covers.coverCount();++i){
            TCover cover;
            covers.covers(i,&cover);
            assert_cover_safe(cover,lastNewEnd,newSize,oldSize);
            lastNewEnd=cover.newPos+cover.length;
        }
    }
    static void assert_covers_safe(const std::vector<TOldCover>& covers,hpatch_StreamPos_t newSize,hpatch_StreamPos_t oldSize){
        const TCovers _covers((void*)covers.data(),covers.size(),
                              sizeof(*covers.data())==sizeof(hpatch_TCover32));
        assert_covers_safe(_covers,newSize,oldSize);
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
    
    const TCovers _covers((void*)diff.covers.data(),diff.covers.size(),
                          sizeof(*diff.covers.data())==sizeof(hpatch_TCover32));
    hpatch_TStreamInput _newDataStream;
    mem_as_hStreamInput(&_newDataStream,diff.newData,diff.newData_end);
    TNewDataDiffStream newDataDiffStream(_covers,&_newDataStream);

    packUInt(out_diff, (TUInt)coverCount);
    packUInt(out_diff, (TUInt)length_buf.size());
    packUInt(out_diff, (TUInt)inc_newPos_buf.size());
    packUInt(out_diff, (TUInt)inc_oldPos_buf.size());
    packUInt(out_diff, newDataDiffStream.streamSize);
    pushBack(out_diff,length_buf);
    pushBack(out_diff,inc_newPos_buf);
    pushBack(out_diff,inc_oldPos_buf);
    pushBack(out_diff,&newDataDiffStream);
    
    TNewDataSubDiffStream_mem newDataSubDiff(diff.newData,diff.newData_end,
                                             diff.oldData,diff.oldData_end,_covers);
    bytesRLE_save(out_diff,&newDataSubDiff,kRle_bestSize);
}
    
    inline static void pushCompressCode(std::vector<TByte>& out_diff,
                                        const std::vector<TByte>& compress_code,
                                        const std::vector<TByte>& data){
        if (compress_code.empty())
            pushBack(out_diff,data);
        else
            pushBack(out_diff,compress_code);
    }
    inline static void pushCompressCode(std::vector<TByte>& out_diff,
                                        const std::vector<TByte>& compress_code,
                                        const hpatch_TStreamInput* data){
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
    {
        const TCovers _covers((void*)diff.covers.data(),diff.covers.size(),
                              sizeof(*diff.covers.data())==sizeof(hpatch_TCover32));
        TNewDataSubDiffStream_mem newDataSubDiff(diff.newData,diff.newData_end,
                                                 diff.oldData,diff.oldData_end,_covers);
        bytesRLE_save(rle_ctrlBuf,rle_codeBuf,&newDataSubDiff,kRle_bestSize);
    }
    
    std::vector<TByte> compress_cover_buf;
    std::vector<TByte> compress_rle_ctrlBuf;
    std::vector<TByte> compress_rle_codeBuf;
    std::vector<TByte> compress_newDataDiff;
    do_compress(compress_cover_buf,cover_buf,compressPlugin);
    do_compress(compress_rle_ctrlBuf,rle_ctrlBuf,compressPlugin);
    do_compress(compress_rle_codeBuf,rle_codeBuf,compressPlugin);
    
    const TCovers _covers((void*)diff.covers.data(),diff.covers.size(),
                          sizeof(*diff.covers.data())==sizeof(hpatch_TCover32));
    hpatch_TStreamInput _newDataStream;
    mem_as_hStreamInput(&_newDataStream,diff.newData,diff.newData_end);
    TNewDataDiffStream newDataDiffStream(_covers,&_newDataStream);
    do_compress(compress_newDataDiff,&newDataDiffStream,compressPlugin);

    _outType(out_diff,compressPlugin);
    const TUInt newDataSize=(size_t)(diff.newData_end-diff.newData);
    const TUInt oldDataSize=(size_t)(diff.oldData_end-diff.oldData);
    packUInt(out_diff, newDataSize);
    packUInt(out_diff, oldDataSize);
    packUInt(out_diff, coverCount);
    packUInt(out_diff, (TUInt)cover_buf.size());
    packUInt(out_diff, (TUInt)compress_cover_buf.size());
    packUInt(out_diff, (TUInt)rle_ctrlBuf.size());
    packUInt(out_diff, (TUInt)compress_rle_ctrlBuf.size());
    packUInt(out_diff, (TUInt)rle_codeBuf.size());
    packUInt(out_diff, (TUInt)compress_rle_codeBuf.size());
    packUInt(out_diff, newDataDiffStream.streamSize);
    packUInt(out_diff, (TUInt)compress_newDataDiff.size());
            
    pushCompressCode(out_diff,compress_cover_buf,cover_buf);
    pushCompressCode(out_diff,compress_rle_ctrlBuf,rle_ctrlBuf);
    pushCompressCode(out_diff,compress_rle_codeBuf,rle_codeBuf);
    pushCompressCode(out_diff,compress_newDataDiff,&newDataDiffStream);
}
    

static void dispose_cover(std::vector<TOldCover>& covers,const TDiffData& diff,
                         int kMinSingleMatchScore,TDiffLimit* diffLimit=0){
    TFixedFloatSmooth kExtendMinSameRatio=kMinSingleMatchScore*36+254;
    if  (kExtendMinSameRatio<200) kExtendMinSameRatio=200;
    if (kExtendMinSameRatio>800) kExtendMinSameRatio=800;

    extend_cover(covers,diff,kExtendMinSameRatio,diffLimit);//先尝试扩展.
    select_cover(covers,diff,kMinSingleMatchScore,diffLimit);
    extend_cover(covers,diff,kExtendMinSameRatio,diffLimit);//select_cover会删除一些覆盖线,所以重新扩展.
}

static const hpatch_StreamPos_t _kNullCoverHitEndPos =~(hpatch_StreamPos_t)0;
struct TDiffResearchCover:public IDiffResearchCover{
    TDiffResearchCover(TDiffData& diff_,const TSuffixString& sstring_,int kMinSingleMatchScore_)
        :diff(diff_),sstring(sstring_),kMinSingleMatchScore(kMinSingleMatchScore_),
        limitCoverIndex_back(~(size_t)0),limitCoverHitEndPos_back(_kNullCoverHitEndPos){ researchCover=_researchCover; }

    void _researchRange(TDiffLimit* diffLimit){
        search_cover(curCovers,diff,sstring,diffLimit);
          if (curCovers.empty()) return;
        dispose_cover(curCovers,diff,kMinSingleMatchScore,diffLimit);
        if (curCovers.empty()) return;
        reCovers.insert(reCovers.end(),curCovers.begin(),curCovers.end());
        curCovers.clear();
    }

    inline void endResearchCover(){
        if (limitCoverHitEndPos_back!=_kNullCoverHitEndPos){
            TOldCover& cover=diff.covers[limitCoverIndex_back];
            cover.oldPos+=(TInt)limitCoverHitEndPos_back;
            cover.newPos+=(TInt)limitCoverHitEndPos_back;
            cover.length-=(TInt)limitCoverHitEndPos_back;
            limitCoverHitEndPos_back=_kNullCoverHitEndPos;
        }
    }
    inline void doResearchCover(IDiffSearchCoverListener* listener,size_t limitCoverIndex,
                                hpatch_StreamPos_t endPosBack,hpatch_StreamPos_t hitPos,hpatch_StreamPos_t hitLen){
        if (limitCoverIndex_back!=limitCoverIndex)
            endResearchCover();
        limitCoverIndex_back=limitCoverIndex;
        limitCoverHitEndPos_back=hitPos+hitLen;
        
        const TOldCover& cover=diff.covers[limitCoverIndex];
        TOldCover lastCover_back(0,0,0);
        if (endPosBack<hitPos){
            lastCover_back.oldPos=cover.oldPos+(TInt)endPosBack;
            lastCover_back.newPos=cover.newPos+(TInt)endPosBack;
            lastCover_back.length=(TInt)(hitPos-endPosBack);
            reCovers.push_back(lastCover_back);
        }else {
            assert(endPosBack==hitPos);
            if (limitCoverIndex>0)
                lastCover_back=diff.covers[limitCoverIndex-1];
            if ((!reCovers.empty())&&(reCovers.back().newPos>lastCover_back.newPos))
                lastCover_back=reCovers.back();
        }

        TDiffLimit diffLimit={listener,cover.newPos+(size_t)hitPos,cover.newPos+(size_t)(hitPos+hitLen),
                              cover.oldPos+(size_t)hitPos,cover.oldPos+(size_t)(hitPos+hitLen),   
                              nocover_detect,cover_detect,lastCover_back};
        _researchRange(&diffLimit);
    }

    static void _researchCover(struct IDiffResearchCover* diffi,struct IDiffSearchCoverListener* listener,size_t limitCoverIndex,
                               hpatch_StreamPos_t endPosBack,hpatch_StreamPos_t hitPos,hpatch_StreamPos_t hitLen){
        TDiffResearchCover* self=(TDiffResearchCover*)diffi;
        self->doResearchCover(listener,limitCoverIndex,endPosBack,hitPos,hitLen);
    }
    
    void researchFinish(){
        endResearchCover();
        size_t insert=0;
        for (size_t i=0;i<diff.covers.size();++i){
            if (diff.covers[i].length>0)
                diff.covers[insert++]=diff.covers[i];
        }
        diff.covers.resize(insert);
        diff.covers.insert(diff.covers.end(),reCovers.begin(),reCovers.end());
        std::inplace_merge(diff.covers.begin(),diff.covers.begin()+insert,
                           diff.covers.end(),cover_cmp_by_new_t<TOldCover>());
    }

    TDiffData&              diff;
    const TSuffixString&    sstring;
    int                     kMinSingleMatchScore;
    std::vector<TOldCover>  reCovers;
    std::vector<TOldCover>  curCovers;
    size_t                  limitCoverIndex_back;
    hpatch_StreamPos_t      limitCoverHitEndPos_back;
    TCompressDetect  nocover_detect;
    TCompressDetect  cover_detect;
};

struct TDiffInsertCover:public IDiffInsertCover{
    inline TDiffInsertCover(std::vector<TOldCover>& _covers)
    :covers(_covers){
        insertCover=_insertCover;
    }
    static void* _insertCover(IDiffInsertCover* diffi,const void* pInsertCovers,size_t insertCoverCount,bool insertIsCover32){
        TDiffInsertCover* self=(TDiffInsertCover*)diffi;
        return self->_insertCover(pInsertCovers,insertCoverCount,insertIsCover32);
    }
    void* _insertCover(const void* pInsertCovers,size_t insertCoverCount,bool insertIsCover32){
        const bool isCover32=sizeof(*covers.data())==sizeof(hpatch_TCover32);
        if (insertIsCover32==isCover32){
            covers.insert(covers.end(),(const TOldCover*)pInsertCovers,
                          ((const TOldCover*)pInsertCovers)+insertCoverCount);
        }else{
            size_t oldSize=covers.size();
            covers.resize(oldSize +insertCoverCount);
            for (size_t i=0;i<insertCoverCount;i++){
                if (insertIsCover32){
                    const hpatch_TCover32& s=((const hpatch_TCover32*)pInsertCovers)[i];
                    covers[oldSize+i]=TOldCover((TInt)s.oldPos,(TInt)s.newPos,(TInt)s.length);
                }else{
                    const hpatch_TCover& s=((const hpatch_TCover*)pInsertCovers)[i];
                    covers[oldSize+i]=TOldCover((TInt)s.oldPos,(TInt)s.newPos,(TInt)s.length);
                }
            }
        }
        return covers.data();
    }
    std::vector<TOldCover>& covers;
};
  

static void get_diff(const TByte* newData,const TByte* newData_end,
                     const TByte* oldData,const TByte* oldData_end,
                     TDiffData&   out_diff,int kMinSingleMatchScore,
                     bool isUseBigCacheMatch,ICoverLinesListener* listener=0,
                     const TSuffixString* sstring=0){
    assert(newData<=newData_end);
    assert(oldData<=oldData_end);
    TDiffData& diff=out_diff;
    diff.newData=newData;
    diff.newData_end=newData_end;
    diff.oldData=oldData;
    diff.oldData_end=oldData_end;
    
    const bool isCover32=sizeof(*diff.covers.data())==sizeof(hpatch_TCover32);
    if (!isCover32) 
        assert(sizeof(*diff.covers.data())==sizeof(hpatch_TCover));
    {
        TSuffixString _sstring_default(isUseBigCacheMatch);
        if (sstring==0){
            _sstring_default.resetSuffixString(oldData,oldData_end);
            sstring=&_sstring_default;
        }
        search_cover(diff.covers,diff,*sstring);
        dispose_cover(diff.covers,diff,kMinSingleMatchScore);
        assert_covers_safe(diff.covers,diff.newData_end-diff.newData,diff.oldData_end-diff.oldData);
        if (listener&&listener->search_cover_limit&&
                listener->search_cover_limit(listener,diff.covers.data(),diff.covers.size(),isCover32)){
            TDiffResearchCover diffResearchCover(diff,*sstring,kMinSingleMatchScore);
            listener->research_cover(listener,&diffResearchCover,diff.covers.data(),diff.covers.size(),isCover32);
            diffResearchCover.researchFinish();
        }
        sstring=0;
        _sstring_default.clear();
    }
    if (listener&&listener->insert_cover){
        TDiffInsertCover diffInsertCover(diff.covers);
        hpatch_StreamPos_t newDataSize=(size_t)(diff.newData_end-diff.newData);
        hpatch_StreamPos_t oldDataSize=(size_t)(diff.oldData_end-diff.oldData);
        listener->insert_cover(listener,&diffInsertCover,diff.covers.data(),diff.covers.size(),isCover32,
                               &newDataSize,&oldDataSize);
        diff.newData_end=diff.newData+(size_t)newDataSize;
        diff.oldData_end=diff.oldData+(size_t)oldDataSize;
        assert_covers_safe(diff.covers,diff.newData_end-diff.newData,diff.oldData_end-diff.oldData);
    }
    if (listener&&listener->search_cover_finish){
        hpatch_StreamPos_t newDataSize=(size_t)(diff.newData_end-diff.newData);
        hpatch_StreamPos_t oldDataSize=(size_t)(diff.oldData_end-diff.oldData);
        size_t newCoverCount=diff.covers.size();
        listener->search_cover_finish(listener,diff.covers.data(),&newCoverCount,isCover32,
                                      &newDataSize,&oldDataSize);
        check(newCoverCount<=diff.covers.size());
        diff.covers.resize(newCoverCount);
        diff.newData_end=diff.newData+(size_t)newDataSize;
        diff.oldData_end=diff.oldData+(size_t)oldDataSize;
    }
    if (listener){
        assert_covers_safe(diff.covers,diff.newData_end-diff.newData,diff.oldData_end-diff.oldData);
    }
}
    
}//end namespace


void create_diff(const TByte* newData,const TByte* newData_end,
                 const TByte* oldData,const TByte* oldData_end,
                 std::vector<TByte>& out_diff,
                 int kMinSingleMatchScore,bool isUseBigCacheMatch){
    TDiffData diff;
    get_diff(newData,newData_end,oldData,oldData_end,diff,
             kMinSingleMatchScore,isUseBigCacheMatch);
    serialize_diff(diff,out_diff);
}

void create_compressed_diff(const TByte* newData,const TByte* newData_end,
                            const TByte* oldData,const TByte* oldData_end,
                            std::vector<TByte>& out_diff,const hdiff_TCompress* compressPlugin,
                            int kMinSingleMatchScore,bool isUseBigCacheMatch,ICoverLinesListener* listener){
    TDiffData diff;
    get_diff(newData,newData_end,oldData,oldData_end,diff,
             kMinSingleMatchScore,isUseBigCacheMatch,listener);
    serialize_compressed_diff(diff,out_diff,compressPlugin);
}

void create_compressed_diff(const TByte* newData,const TByte* newData_end,
                            const TByte* oldData,const TByte* oldData_end,
                            const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                            int kMinSingleMatchScore,bool isUseBigCacheMatch,ICoverLinesListener* listener){
    std::vector<unsigned char> _out_diff;
    create_compressed_diff(newData,newData_end,oldData,oldData_end,_out_diff,
                           compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,listener);
    checki(out_diff->write(out_diff,0,_out_diff.data(),_out_diff.data()+_out_diff.size()),"create_compressed_diff() out_diff->write");
}

static void serialize_single_compressed_diff(const hpatch_TStreamInput* newStream,const hpatch_TStreamInput* oldStream,
                                             bool isZeroSubDiff,const TCovers& covers,const hpatch_TStreamOutput* out_diff,
                                             const hdiff_TCompress* compressPlugin,size_t patchStepMemSize){
    check(patchStepMemSize>=hpatch_kStreamCacheSize);
    TStepStream stepStream(newStream,oldStream,isZeroSubDiff,covers,patchStepMemSize);
    
    TDiffStream outDiff(out_diff);
    {//type
        std::vector<TByte> out_type;
        _outType(out_type,compressPlugin,kHDiffSFVersionType);
        outDiff.pushBack(out_type.data(),out_type.size());
    }
    outDiff.packUInt(newStream->streamSize);
    outDiff.packUInt(oldStream->streamSize);
    outDiff.packUInt(stepStream.getCoverCount());
    outDiff.packUInt(stepStream.getMaxStepMemSize());
    outDiff.packUInt(stepStream.streamSize);
    TPlaceholder compressed_sizePos=outDiff.packUInt_pos(compressPlugin?stepStream.streamSize:0);
    outDiff.pushStream(&stepStream,compressPlugin,compressed_sizePos);
}

void create_single_compressed_diff(const TByte* newData,const TByte* newData_end,
                                   const TByte* oldData,const TByte* oldData_end,
                                   std::vector<unsigned char>& out_diff,
                                   const hdiff_TCompress* compressPlugin,int kMinSingleMatchScore,
                                   size_t patchStepMemSize,bool isUseBigCacheMatch,
                                   ICoverLinesListener* listener){
    TVectorAsStreamOutput outDiffStream(out_diff);
    create_single_compressed_diff(newData,newData_end,oldData,oldData_end,&outDiffStream,
                                  compressPlugin,kMinSingleMatchScore,patchStepMemSize,
                                  isUseBigCacheMatch,listener);
}

void create_single_compressed_diff(const TByte* newData,const TByte* newData_end,
                                   const TByte* oldData,const TByte* oldData_end,
                                   const hpatch_TStreamOutput* out_diff,
                                   const hdiff_TCompress* compressPlugin,int kMinSingleMatchScore,
                                   size_t patchStepMemSize,bool isUseBigCacheMatch,
                                   ICoverLinesListener* listener){
    TDiffData diff;
    get_diff(newData,newData_end,oldData,oldData_end,diff,
             kMinSingleMatchScore,isUseBigCacheMatch,listener);

    hpatch_TStreamInput _newStream;
    hpatch_TStreamInput _oldStream;
    mem_as_hStreamInput(&_newStream,diff.newData,diff.newData_end);
    mem_as_hStreamInput(&_oldStream,diff.oldData,diff.oldData_end);
    const TCovers _covers((void*)diff.covers.data(),diff.covers.size(),
                          sizeof(*diff.covers.data())==sizeof(hpatch_TCover32));
    serialize_single_compressed_diff(&_newStream,&_oldStream,false,_covers,
                                     out_diff,compressPlugin,patchStepMemSize);
}

void create_single_compressed_diff_stream(const hpatch_TStreamInput*  newData,
                                          const hpatch_TStreamInput*  oldData,
                                          const hpatch_TStreamOutput* out_diff,
                                          const hdiff_TCompress* compressPlugin,
                                          size_t kMatchBlockSize,size_t patchStepMemSize){
    const bool isSkipSameRange=(compressPlugin!=0);
    TCoversBuf covers(newData->streamSize,oldData->streamSize);
    get_match_covers_by_block(newData,oldData,&covers,kMatchBlockSize,isSkipSameRange);
    serialize_single_compressed_diff(newData,oldData,true,covers,
                                     out_diff,compressPlugin,patchStepMemSize);
}


bool check_diff(const TByte* newData,const TByte* newData_end,
                const TByte* oldData,const TByte* oldData_end,
                const TByte* diff,const TByte* diff_end){
    hpatch_TStreamInput  newStream;
    hpatch_TStreamInput  oldStream;
    hpatch_TStreamInput  diffStream;
    mem_as_hStreamInput(&newStream,newData,newData_end);
    mem_as_hStreamInput(&oldStream,oldData,oldData_end);
    mem_as_hStreamInput(&diffStream,diff,diff_end);
    return check_diff(&newStream,&oldStream,&diffStream);
}

bool check_diff(const hpatch_TStreamInput*  newData,
                const hpatch_TStreamInput*  oldData,
                const hpatch_TStreamInput*  diff){
    const size_t kACacheBufSize=hpatch_kFileIOBufBetterSize;
    TAutoMem _cache(kACacheBufSize*(1+8));
    _TCheckOutNewDataStream out_newData(newData,_cache.data(),kACacheBufSize);
    _test_rt(patch_stream_with_cache(&out_newData,oldData,diff,
                                     _cache.data()+kACacheBufSize,_cache.data_end()));
    _test_rt(out_newData.isWriteFinish());
    return true;
}

bool check_compressed_diff(const TByte* newData,const TByte* newData_end,
                           const TByte* oldData,const TByte* oldData_end,
                           const TByte* diff,const TByte* diff_end,
                           hpatch_TDecompress* decompressPlugin){
    hpatch_TStreamInput  newStream;
    hpatch_TStreamInput  oldStream;
    hpatch_TStreamInput  diffStream;
    mem_as_hStreamInput(&newStream,newData,newData_end);
    mem_as_hStreamInput(&oldStream,oldData,oldData_end);
    mem_as_hStreamInput(&diffStream,diff,diff_end);
    return check_compressed_diff(&newStream,&oldStream,&diffStream,decompressPlugin);
}

bool check_compressed_diff(const hpatch_TStreamInput*  newData,
                           const hpatch_TStreamInput*  oldData,
                           const hpatch_TStreamInput*  compressed_diff,
                           hpatch_TDecompress* decompressPlugin){
    const size_t kACacheBufSize=hpatch_kFileIOBufBetterSize;
    TAutoMem _cache(kACacheBufSize*(1+6));
    _TCheckOutNewDataStream out_newData(newData,_cache.data(),kACacheBufSize);
    _test_rt(patch_decompress_with_cache(&out_newData,oldData,compressed_diff,decompressPlugin,
                                         _cache.data()+kACacheBufSize,_cache.data_end()));
    _test_rt(out_newData.isWriteFinish());
    return true;
}

bool check_single_compressed_diff(const TByte* newData,const TByte* newData_end,
                                  const TByte* oldData,const TByte* oldData_end,
                                  const TByte* diff,const TByte* diff_end,
                                  hpatch_TDecompress* decompressPlugin){
    hpatch_TStreamInput  newStream;
    hpatch_TStreamInput  oldStream;
    hpatch_TStreamInput  diffStream;
    mem_as_hStreamInput(&newStream,newData,newData_end);
    mem_as_hStreamInput(&oldStream,oldData,oldData_end);
    mem_as_hStreamInput(&diffStream,diff,diff_end);
    return check_single_compressed_diff(&newStream,&oldStream,&diffStream,decompressPlugin);
}

bool check_single_compressed_diff(const hpatch_TStreamInput* newData,
                                  const TByte* oldData,const TByte* oldData_end,
                                  const hpatch_TStreamInput* diff,
                                  hpatch_TDecompress* decompressPlugin){ 
    hpatch_TStreamInput  oldStream;
    mem_as_hStreamInput(&oldStream,oldData,oldData_end);
    return check_single_compressed_diff(newData,&oldStream,diff,decompressPlugin);
}

    static hpatch_BOOL _check_single_onDiffInfo(struct sspatch_listener_t* listener,
                                                const hpatch_singleCompressedDiffInfo* info,
                                                hpatch_TDecompress** out_decompressPlugin,
                                                unsigned char** out_temp_cache,
                                                unsigned char** out_temp_cacheEnd){
        size_t memSize=(size_t)(info->stepMemSize+hpatch_kStreamCacheSize*3);
        *out_temp_cache=(unsigned char*)malloc(memSize);
        *out_temp_cacheEnd=(*out_temp_cache)+memSize;   
        *out_decompressPlugin=(info->compressType[0]=='\0')?0:(hpatch_TDecompress*)listener->import;
        return hpatch_TRUE;
    }
    static void _check_single_onPatchFinish(struct sspatch_listener_t* listener,
                                            unsigned char* temp_cache, unsigned char* temp_cacheEnd){
        if (temp_cache) free(temp_cache);
    }
bool check_single_compressed_diff(const hpatch_TStreamInput* newData,
                                  const hpatch_TStreamInput* oldData,
                                  const hpatch_TStreamInput* diff,
                                  hpatch_TDecompress* decompressPlugin){
    sspatch_listener_t listener={0};
    listener.import=decompressPlugin;
    listener.onDiffInfo=_check_single_onDiffInfo;
    listener.onPatchFinish=_check_single_onPatchFinish;
        
    const size_t kACacheBufSize=hpatch_kFileIOBufBetterSize;
    TAutoMem _cache(kACacheBufSize*1);
    _TCheckOutNewDataStream out_newData(newData,_cache.data(),kACacheBufSize);

    _test_rt(patch_single_stream(&listener,&out_newData,oldData,diff,0,0));
    _test_rt(out_newData.isWriteFinish());
    return true;
}


//for test
void __hdiff_private__create_compressed_diff(const TByte* newData,const TByte* newData_end,
                                             const TByte* oldData,const TByte* oldData_end,
                                             std::vector<TByte>& out_diff,
                                             const hdiff_TCompress* compressPlugin,int kMinSingleMatchScore,
                                             const TSuffixString* sstring){
    TDiffData diff;
    get_diff(newData,newData_end,oldData,oldData_end,diff,
             kMinSingleMatchScore,false,0,sstring);
    serialize_compressed_diff(diff,out_diff,compressPlugin);
}


//======================

void get_match_covers_by_block(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                               hpatch_TOutputCovers* out_covers,size_t kMatchBlockSize,bool kIsSkipSameRange){
    assert(out_covers->push_cover!=0);
    TDigestMatcher matcher(oldData,kMatchBlockSize,kIsSkipSameRange);
    matcher.search_cover(newData,out_covers);
    //todo: + extend_cover_stream ?
}
void get_match_covers_by_block(const unsigned char* newData,const unsigned char* newData_end,
                               const unsigned char* oldData,const unsigned char* oldData_end,
                               hpatch_TOutputCovers* out_covers,size_t kMatchBlockSize,bool kIsSkipSameRange){
    hdiff_TStreamInput oldData_stream;
    mem_as_hStreamInput(&oldData_stream,oldData,oldData_end);
    hdiff_TStreamInput newData_stream;
    mem_as_hStreamInput(&newData_stream,newData,newData_end);
    get_match_covers_by_block(&newData_stream,&oldData_stream,out_covers,kMatchBlockSize,kIsSkipSameRange);
}

void get_match_covers_by_sstring(const unsigned char* newData,const unsigned char* newData_end,
                                 const unsigned char* oldData,const unsigned char* oldData_end,
                                 std::vector<hpatch_TCover_sz>& out_covers,int kMinSingleMatchScore,
                                 bool isUseBigCacheMatch,ICoverLinesListener* listener){
    TDiffData diff;
    get_diff(newData,newData_end,oldData,oldData_end,diff,
             kMinSingleMatchScore,isUseBigCacheMatch,listener);
    void* pcovers=&diff.covers;
    out_covers.swap(*(std::vector<hpatch_TCover_sz>*)pcovers);
}
void get_match_covers_by_sstring(const unsigned char* newData,const unsigned char* newData_end,
                                 const unsigned char* oldData,const unsigned char* oldData_end,
                                 hpatch_TOutputCovers* out_covers,int kMinSingleMatchScore,
                                 bool isUseBigCacheMatch,ICoverLinesListener* listener){
    std::vector<hpatch_TCover_sz> covers;
    get_match_covers_by_sstring(newData,newData_end,oldData,oldData_end,covers,
                                kMinSingleMatchScore,isUseBigCacheMatch,listener);
    const hpatch_TCover_sz* pcovers=covers.data();
    for (size_t i=0;i<covers.size();++i,++pcovers){
        if (sizeof(*pcovers)==sizeof(hpatch_TCover)){
            out_covers->push_cover(out_covers,(const hpatch_TCover*)pcovers);
        }else{
            hpatch_TCover cover; 
            cover.oldPos=pcovers->oldPos;
            cover.newPos=pcovers->newPos;
            cover.length=pcovers->length;
            out_covers->push_cover(out_covers,&cover);
        }
    }
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
    TCoversBuf covers(newData->streamSize,oldData->streamSize);
    get_match_covers_by_block(newData,oldData,&covers,kMatchBlockSize,isSkipSameRange);
    stream_serialize(newData,oldData->streamSize,out_diff,compressPlugin,covers);
}


void resave_compressed_diff(const hpatch_TStreamInput*  in_diff,
                            hpatch_TDecompress*         decompressPlugin,
                            const hpatch_TStreamOutput* out_diff,
                            const hdiff_TCompress*      compressPlugin,
                            hpatch_StreamPos_t          out_diff_curPos){
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
    
    TDiffStream outDiff(out_diff,out_diff_curPos);
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


void resave_single_compressed_diff(const hpatch_TStreamInput*  in_diff,
                                   hpatch_TDecompress*         decompressPlugin,
                                   const hpatch_TStreamOutput* out_diff,
                                   const hdiff_TCompress*      compressPlugin,
                                   const hpatch_singleCompressedDiffInfo* diffInfo,
                                   hpatch_StreamPos_t          in_diff_curPos,
                                   hpatch_StreamPos_t          out_diff_curPos){
    hpatch_singleCompressedDiffInfo _diffInfo;
    if (diffInfo==0){
        checki(getSingleCompressedDiffInfo(&_diffInfo,in_diff,in_diff_curPos),
               "getSingleCompressedDiffInfo() return fail!");
        diffInfo=&_diffInfo;
    }
    const bool isCompressed=(diffInfo->compressedSize>0);
    if (isCompressed){ //check
        checki(diffInfo->compressedSize+(in_diff_curPos+diffInfo->diffDataPos)==in_diff->streamSize,
               "resave_single_compressed_diff() diffInfo error!");
        checki((decompressPlugin!=0)&&(decompressPlugin->is_can_open(diffInfo->compressType)),
               "resave_single_compressed_diff() decompressPlugin error!");
    }

    TDiffStream outDiff(out_diff,out_diff_curPos);
    {//type & head
        std::vector<TByte> outBuf;
        _outType(outBuf, compressPlugin,kHDiffSFVersionType);
        packUInt(outBuf, diffInfo->newDataSize);
        packUInt(outBuf, diffInfo->oldDataSize);
        packUInt(outBuf, diffInfo->coverCount);
        packUInt(outBuf, diffInfo->stepMemSize);
        packUInt(outBuf, diffInfo->uncompressedSize);
        outDiff.pushBack(outBuf.data(),outBuf.size());
        //no compressedSize
    }
    {//save single stream data
        TStreamClip clip(in_diff,diffInfo->diffDataPos+in_diff_curPos,in_diff->streamSize,
                         isCompressed?decompressPlugin:0,diffInfo->uncompressedSize);
        TPlaceholder compressedSize_pos=outDiff.packUInt_pos(compressPlugin?diffInfo->uncompressedSize:0);
        outDiff.pushStream(&clip,compressPlugin,compressedSize_pos);
    }
}


//----------------------------------------------------------------------------------------------------

#include "diff_for_hpatch_lite.h"
#include "../HPatchLite/hpatch_lite.h"

namespace{
    static const char*      kHPatchLite_versionType="hI";
    static const hpi_byte   kHPatchLite_versionCode=1;

    static inline void hpi_packUInt(std::vector<TByte>& buf,TUInt v){
        check(v==(hpi_pos_t)v);
        packUInt(buf,v);
    }
    static inline void hpi_packUIntWithTag(std::vector<TByte>& buf,TUInt v,TByte tag,TByte bit){
        check(v==(hpi_pos_t)v);
        packUIntWithTag(buf,v,tag,bit);
    }
    
    static inline TByte hpi_getSavedSizeBytes(TUInt size){
        check(size==(hpi_pos_t)size);
        TByte bytes=0; 
        while (size>0){
            ++bytes;
            size>>=8;
        }
        return bytes;
    }
    static inline void hpi_saveSize(std::vector<TByte>& buf,TUInt size){
        check(size==(hpi_pos_t)size);
        while (size>0){
            buf.push_back((TByte)size);
            size>>=8;
        }
    }
    
    static bool _getIs0(const TByte* data,size_t length){
        for (size_t i=0; i<length; ++i) {
            if (data[i]==0)
                continue;
            else
                return false;
        }
        return true;
    }

    static void _getSubDiff(std::vector<TByte>& subDiff,const TDiffData& diff,const TOldCover& cover){
        subDiff.resize(cover.length);
        const TByte* pnew=diff.newData+cover.newPos;
        const TByte* pold=diff.oldData+cover.oldPos;
        for (size_t i=0;i<subDiff.size();++i)
            subDiff[i]=pnew[i]-pold[i];
    }

static void serialize_lite_diff(const TDiffData& diff,std::vector<TByte>& out_diff,
                                const hdiffi_TCompress* compressPlugin){
    const TUInt coverCount=(TUInt)diff.covers.size();
    std::vector<TByte> subDiff;
    std::vector<TByte> buf;
    hpi_packUInt(buf,coverCount);
    const TUInt newSize=(TUInt)(diff.newData_end-diff.newData);
    {
        TUInt lastOldEnd=0;
        TUInt lastNewEnd=0;
        for (TUInt i=0; i<coverCount; ++i) {
            const TOldCover& cover=diff.covers[i];
            hpi_packUInt(buf, cover.length);
            _getSubDiff(subDiff,diff,cover);
            const TByte isNullSubDiff=_getIs0(subDiff.data(),cover.length)?1:0;
            if ((TUInt)cover.oldPos>=lastOldEnd){ //save inc_oldPos
                hpi_packUIntWithTag(buf,(TUInt)(cover.oldPos-lastOldEnd), 0+isNullSubDiff*2,2);
            }else{
                hpi_packUIntWithTag(buf,(TUInt)(lastOldEnd-cover.oldPos), 1+isNullSubDiff*2,2);
            }
            TUInt backNewLen=cover.newPos-lastNewEnd;
            assert(backNewLen>=0);
            hpi_packUInt(buf,(TUInt)backNewLen); //save inc_newPos
            
            if (backNewLen>0){
                const TByte* newDataDiff=diff.newData+lastNewEnd;
                pushBack(buf,newDataDiff,newDataDiff+backNewLen);
            }
            if (!isNullSubDiff){
                pushBack(buf,subDiff.data(),subDiff.data()+cover.length);
            }
            
            lastOldEnd=cover.oldPos+cover.length;
            lastNewEnd=cover.newPos+cover.length;
        }
        
        TUInt backNewLen=(newSize-lastNewEnd);
        check(backNewLen==0);
    }
    
    std::vector<TByte> compress_buf;
    do_compress(compress_buf,buf,compressPlugin->compress);
    out_diff.push_back(kHPatchLite_versionType[0]);
    out_diff.push_back(kHPatchLite_versionType[1]);
    out_diff.push_back(compress_buf.empty()?hpi_compressType_no:compressPlugin->compress_type);
    TUInt savedUncompressSize=compress_buf.empty()?0:buf.size();
    out_diff.push_back((kHPatchLite_versionCode<<6)|
                       (hpi_getSavedSizeBytes(newSize))|
                       (hpi_getSavedSizeBytes(savedUncompressSize)<<3));
    hpi_saveSize(out_diff,newSize);
    hpi_saveSize(out_diff,savedUncompressSize);
    pushBack(out_diff,compress_buf.empty()?buf:compress_buf);
}

} //end namespace

void create_lite_diff(const unsigned char* newData,const unsigned char* newData_end,
                      const unsigned char* oldData,const unsigned char* oldData_end,
                      std::vector<hpi_byte>& out_lite_diff,const hdiffi_TCompress* compressPlugin,
                      int kMinSingleMatchScore,bool isUseBigCacheMatch){
    static const int _kMatchScore_optim4bin=6;
    TDiffData diff;
    get_diff(newData,newData_end,oldData,oldData_end,diff,kMinSingleMatchScore-_kMatchScore_optim4bin,isUseBigCacheMatch);
    hpatch_StreamPos_t oldPosEnd=0;
    hpatch_StreamPos_t newPosEnd=0;
    if (!diff.covers.empty()){
        const TOldCover& c=diff.covers.back();
        oldPosEnd=c.oldPos+c.length;
        newPosEnd=c.newPos+c.length;
    }
    const size_t newSize=newData_end-newData;
    if (newPosEnd<newSize)
        diff.covers.push_back(TOldCover(oldPosEnd,newSize,0));
    serialize_lite_diff(diff,out_lite_diff,compressPlugin);
}

namespace{
struct TPatchiListener:public hpatchi_listener_t{
    hpatch_decompressHandle decompresser;
    hpatch_TDecompress*     decompressPlugin;
    inline TPatchiListener():decompresser(0){}
    inline ~TPatchiListener(){ if (decompresser) decompressPlugin->close(decompressPlugin,decompresser); }
    const hpi_byte* diffData_cur;
    const hpi_byte* diffData_end;
    hpatch_TStreamInput diffStream;
    hpi_pos_t           uncompressSize;
    const hpi_byte* newData_cur;
    const hpi_byte* newData_end;
    const hpi_byte* oldData;
    const hpi_byte* oldData_end;

    static hpi_BOOL _read_diff(hpi_TInputStreamHandle inputStream,hpi_byte* out_data,hpi_size_t* data_size){
        TPatchiListener& self=*(TPatchiListener*)inputStream;
        const hpi_byte* cur=self.diffData_cur;
        size_t d_size=self.diffData_end-cur;
        size_t r_size=*data_size;
        if (r_size>d_size){
            r_size=d_size;
            *data_size=(hpi_size_t)r_size;
        }
        memcpy(out_data,cur,r_size);
        self.diffData_cur=cur+r_size;
        return hpi_TRUE;
    }
    static hpi_BOOL _read_diff_dec(hpi_TInputStreamHandle inputStream,hpi_byte* out_data,hpi_size_t* data_size){
        TPatchiListener& self=*(TPatchiListener*)inputStream;
        hpi_size_t r_size=*data_size;
        if (r_size>self.uncompressSize){
            r_size=(hpi_size_t)self.uncompressSize;
            *data_size=(hpi_size_t)self.uncompressSize;
        }
        if (!self.decompressPlugin->decompress_part(self.decompresser,out_data,out_data+r_size))
            return hpi_FALSE;
        self.uncompressSize-=r_size;
        return hpi_TRUE;
    }
    static hpi_BOOL _write_new(struct hpatchi_listener_t* listener,const hpi_byte* data,hpi_size_t data_size){
        TPatchiListener& self=*(TPatchiListener*)listener;
        if (data_size>(size_t)(self.newData_end-self.newData_cur)) 
            return hpi_FALSE;
        if (0!=memcmp(self.newData_cur,data,data_size))
            return hpi_FALSE;
        self.newData_cur+=data_size;
        return hpi_TRUE;
    }
    static hpi_BOOL _read_old(struct hpatchi_listener_t* listener,hpi_pos_t read_from_pos,hpi_byte* out_data,hpi_size_t data_size){
        TPatchiListener& self=*(TPatchiListener*)listener;
        size_t dsize=self.oldData_end-self.oldData;
        if ((read_from_pos>dsize)|(data_size>(size_t)(dsize-read_from_pos))) return hpi_FALSE;
        memcpy(out_data,self.oldData+(size_t)read_from_pos,data_size);
        return hpi_TRUE;
    }
};
}// end namespace

bool check_lite_diff_open(const hpi_byte* lite_diff,const hpi_byte* lite_diff_end,
                          hpi_compressType* out_compress_type){
    TPatchiListener listener;
    listener.diffData_cur=lite_diff;
    listener.diffData_end=lite_diff_end;
    hpi_pos_t saved_newSize;
    hpi_pos_t saved_uncompressSize;
    if (!hpatch_lite_open(&listener,listener._read_diff,out_compress_type,&saved_newSize,
                          &saved_uncompressSize)) return false;
    return true;
}

bool check_lite_diff(const hpi_byte* newData,const hpi_byte* newData_end,
                     const hpi_byte* oldData,const hpi_byte* oldData_end,
                     const hpi_byte* lite_diff,const hpi_byte* lite_diff_end,
                     hpatch_TDecompress* decompressPlugin){
    TPatchiListener listener;
    listener.diffData_cur=lite_diff;
    listener.diffData_end=lite_diff_end;
    hpi_compressType _compress_type;
    hpi_pos_t saved_newSize;
    hpi_pos_t saved_uncompressSize;
    if (!hpatch_lite_open(&listener,listener._read_diff,&_compress_type,&saved_newSize,
                          &saved_uncompressSize)) return false;
    if (saved_newSize!=(size_t)(newData_end-newData)) return false;
    listener.diff_data=&listener;
    listener.decompressPlugin=(_compress_type!=hpi_compressType_no)?decompressPlugin:0;
    if (listener.decompressPlugin){
        listener.uncompressSize=saved_uncompressSize;
        mem_as_hStreamInput(&listener.diffStream,listener.diffData_cur,lite_diff_end);
        listener.decompresser=decompressPlugin->open(decompressPlugin,saved_uncompressSize,&listener.diffStream,
                                                     0,(size_t)(lite_diff_end-listener.diffData_cur));
        if (listener.decompresser==0) return false;
        listener.read_diff=listener._read_diff_dec;
    }else{
        listener.read_diff=listener._read_diff;
    }
    listener.newData_cur=newData;
    listener.newData_end=newData_end;
    listener.write_new=listener._write_new;
    listener.oldData=oldData;
    listener.oldData_end=oldData_end;
    listener.read_old=listener._read_old;

    const size_t kACacheBufSize=1024*32;
    hdiff_private::TAutoMem _cache(kACacheBufSize);
    
    if (!hpatch_lite_patch(&listener,saved_newSize,_cache.data(),(hpi_size_t)_cache.size()))
        return false;
    return listener.newData_cur==listener.newData_end;
}