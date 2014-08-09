//
//  diff.cpp
//  HDiff
//
/*
 This is the HDiff copyright.
 
 Copyright (c) 2012-2013 HouSisong All Rights Reserved.
 (The MIT License)
 
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
#include "assert.h"
#include "private_diff/suffix_string.h"
#include "private_diff/bytes_rle.h"
#include "../HPatch/patch.h"

namespace hdiffpatch {

//todo:寻找更短的线,允许重叠,(考虑编码代价权重)然后优化一个路径算法.
////////
const int kMinMatchLength=7;            //最小搜寻覆盖长度.
const int kMinSangleMatchLength=17;     //最小独立覆盖长度. 17 //二进制8-10(best)-21 文本: 17-21
const int kUnLinkLength=4;              //搜索时不能合并的代价.
const int kMinTrustMatchLength=1024*8;  //(贪婪)选定该覆盖线(优化一些速度).
const int kMaxLinkSpaceLength=128;      //跨覆盖线合并时,允许合并的最远距离.

struct TOldCover;

struct TDiffData{
    const TByte*            newData;
    const TByte*            newData_end;
    const TByte*            oldData;
    const TByte*            oldData_end;
    std::vector<TOldCover>  cover;          //选出的覆盖线.
    std::vector<TByte>      newDataDiff;    //集中储存newData中没有被覆盖线覆盖住的字节数据.
    std::vector<TByte>      newDataSubDiff; //newData中的每个数值减去对应的cover线条的oldData数值和newDataDiff的数值.
};

//覆盖线.
struct TOldCover {
    TInt32  newPos;
    TInt32  oldPos;
    TInt32  length;
    inline TOldCover():newPos(0),oldPos(0),length(0) { }
    inline TOldCover(TInt32 _newPos,TInt32 _oldPos,TInt32 _length):newPos(_newPos),oldPos(_oldPos),length(_length) { }
    inline TOldCover(const TOldCover& cover):newPos(cover.newPos),oldPos(cover.oldPos),length(cover.length) { }
    
    inline bool isCanLink(const TOldCover& next)const{//覆盖线是否在同一条直线上.
        return ((oldPos-newPos==next.oldPos-next.newPos))&&(linkSpaceLength(next)<=kMaxLinkSpaceLength);
    }
    inline TInt32 linkSpaceLength(const TOldCover& next)const{//覆盖线间的间距.
        return next.oldPos-(oldPos+length);
    }
};

//查找相等的字符串长度.
static TInt32 getEqualLength(const TByte* oldData,const TByte* oldData_end,const TByte* newData,const TByte* newData_end){
    const TInt32 newDataLength=(TInt32)(newData_end-newData);
    TInt32 maxEqualLength=(TInt32)(oldData_end-oldData);
    if (newDataLength<maxEqualLength)
        maxEqualLength=newDataLength;
    for (TInt32 i=0; i<maxEqualLength; ++i) {
        if (oldData[i]!=newData[i])
            return i;
    }
    return maxEqualLength;
}

//得到最好的一个匹配长度和其位置.
static bool getBestMatch(const TSuffixString& sstring,const TByte* newData,const TByte* newData_end,TInt32* out_pos,TInt32* out_length,int kMinMatchLength){
    const char* ssbegin=sstring.ssbegin;
    const char* ssend=sstring.ssend;
    if (ssend-ssbegin<=0) return false;
    TInt32  bestPos=-1;
    TInt32 bestLength=kMinMatchLength-1;
    
    const char* s_newData_end=(const char*)newData_end;
    if (newData_end-newData>kMinTrustMatchLength)
        s_newData_end=(const char*)newData+kMinTrustMatchLength;
    TSuffixIndex sai=sstring.lower_bound((const char*)newData,s_newData_end);

    const TSuffixIndex* SA=&sstring.SA[0];
    for (TInt32 i=sai; i>=sai-1; --i) {
        if ((i<0)||(i>=(ssend-ssbegin))) continue;
        TInt32 curLength=getEqualLength((const TByte*)ssbegin+SA[i],(const TByte*)ssend,newData,newData_end);
        if (curLength>bestLength){
            bestPos=SA[i];
            bestLength=curLength;
        }
    }
    
    bool isMatched=(bestPos>=0);
    if (isMatched){
        *out_pos=bestPos;
        *out_length=bestLength;
    }
    return isMatched;
}

//获得区域内当作覆盖时的收益.
static TInt32 getLinkEqualCount(TInt32 newPos,TInt32 newPos_end,TInt32 oldPos,const TDiffData& diff){
    TInt32 eqCount=0;
    for (TInt32 i=0; i<(newPos_end-newPos); ++i) {
        if (oldPos+i>=(diff.oldData_end-diff.oldData)){
            if (diff.newData[newPos+i]==0)
                ++eqCount;
        }else if (diff.newData[newPos+i]==diff.oldData[oldPos+i]){
            ++eqCount;
        }
    }
    return eqCount;
}


//寻找合适的覆盖线.
static void search_cover(TDiffData& diff){
    const TSuffixString sstring((const char*)diff.oldData,(const char*)diff.oldData_end);
    
    TInt32 newPos=0;
    TInt32 lastOldPos=0;
    TInt32 lastNewPos=0;
    while (diff.newData_end-(diff.newData+newPos)>=kMinMatchLength) {
        TInt32 curOldPos;
        TInt32 curEqLength;
        bool isMatched=getBestMatch(sstring,diff.newData+newPos,diff.newData_end, &curOldPos,&curEqLength,kMinMatchLength);
        if (isMatched){
            assert(curEqLength>=kMinMatchLength);
            const TInt32 lastLinkEqLength=getLinkEqualCount(newPos,newPos+curEqLength,lastOldPos+(newPos-lastNewPos),diff);
            if ((curEqLength<lastLinkEqLength+kUnLinkLength)&&(newPos-lastNewPos>0)){//use link
                const TInt32 linkEqLength=getEqualLength(diff.oldData+lastOldPos+(newPos-lastNewPos), diff.oldData_end,diff.newData+newPos, diff.newData_end);
                const TInt32 linkLength=(newPos-lastNewPos)+linkEqLength;
                if (diff.cover.empty()){
                    diff.cover.push_back(TOldCover(lastNewPos,lastOldPos,linkLength));
                }else{
                    TOldCover& curCover=diff.cover.back();
                    curCover.length+=linkLength;
                }
                newPos+=std::max(linkEqLength,curEqLength);//实际等价于+=curEqLength;
            }else{ //use match
                diff.cover.push_back(TOldCover(newPos,curOldPos,curEqLength));
                newPos+=curEqLength;
            }
            TOldCover& curCover=diff.cover.back();
            lastOldPos=curCover.oldPos+curCover.length;
            lastNewPos=curCover.newPos+curCover.length;
        }else{
            ++newPos;//下一个需要匹配的字符串.
            //匹配不成功时,速度会比较慢.
        }
    }
}
    
//选择合适的覆盖线,去掉不合适的.
static void select_cover(TDiffData& diff){
    std::vector<TOldCover>&  cover=diff.cover;
    
    const TInt32 coverSize_old=(TInt32)cover.size();
    int insertIndex=0;
    for (int i=0;i<coverSize_old;++i){
        if (cover[i].oldPos<0) continue;//处理已经del的.
        bool isNeedSave=cover[i].length>=kMinTrustMatchLength;//足够长.
        if (!isNeedSave){//向前合并可能.
            if ((insertIndex>0)&&(cover[insertIndex-1].isCanLink(cover[i])))
                isNeedSave=true;
        }
        if (i+1<coverSize_old){//查询向后合并可能 处理可能的跳跃link
            if (cover[i].isCanLink(cover[i+1])){//右邻直接判断.
                isNeedSave=true;
            }
        }
        if (!isNeedSave){//单覆盖是否保留.
            const TInt32 linkEqLength=getLinkEqualCount(cover[i].newPos,cover[i].newPos+cover[i].length,(TInt32)(diff.oldData_end-diff.oldData),diff);
            isNeedSave=(cover[i].length-linkEqLength>=kMinSangleMatchLength);
        }
        
        if (isNeedSave){
            if ((insertIndex>0)&&(cover[insertIndex-1].isCanLink(cover[i]))){//link合并.
                cover[insertIndex-1].length=cover[i].length+(cover[i].oldPos-cover[insertIndex-1].oldPos);
            }else{
                cover[insertIndex]=cover[i];
                ++insertIndex;
            }
        }
    }
    cover.resize(insertIndex);
}
    
    
    //得到可以扩展位置的长度.
    static TInt32 getCanExtendLength(TInt32 oldPos,TInt32 newPos,int inc,TInt32 newPos_min,TInt32 newPos_end,const TDiffData& diff){
        //todo:更准确的代价模型?
        const int   kSmoothLength=4;
        typedef TInt32 TFixedFloat10000;
        //const float kMinSameRatio=0.40f;
        const TFixedFloat10000 kMinSameRatio=4000;
        //const float kMinTustSameRatio=0.65f;
        const TFixedFloat10000 kMinTustSameRatio=6500;
        
        //float curBestSameRatio=0;
        TFixedFloat10000 curBestSameRatio=0;
        TInt32 curBestLength=0;
        TInt32 curSameCount=0;
        for (TInt32 length=1; (oldPos>=0)&&(oldPos<(diff.oldData_end-diff.oldData))
             &&(newPos>=newPos_min)&&(newPos<newPos_end); ++length,oldPos+=inc,newPos+=inc) {
            if (diff.oldData[oldPos]==diff.newData[newPos]){
                ++curSameCount;
                
                //const float curSameRatio=((float)curSameCount)/(length+kSmoothLength);
                if (curSameCount>= ((1<<30)/10000)) break; //for curSameCount*10000
                const TFixedFloat10000 curSameRatio=curSameCount*10000/(length+kSmoothLength);
                
                if ((curSameRatio>=curBestSameRatio)||(curSameRatio>=kMinTustSameRatio)){
                    curBestSameRatio=curSameRatio;
                    curBestLength=length;
                }
            }
        }
        if ((curBestSameRatio<kMinSameRatio)||(curBestLength<=2))//ok
            curBestLength=0;
        
        return curBestLength;
    }
    
//尝试延长覆盖区域.
static void extend_cover(TDiffData& diff){
    std::vector<TOldCover>&  cover=diff.cover;
    
    TInt32 lastNewPos=0;
    for (int i=0; i<(int)cover.size(); ++i) {
        TInt32 newPos_next=(TInt32)(diff.newData_end-diff.newData);
        if (i+1<(int)cover.size())
            newPos_next=cover[i+1].newPos;
        TOldCover& curCover=cover[i];
        //向前延伸.
        TInt32 extendLength_front=getCanExtendLength(curCover.oldPos-1,curCover.newPos-1,-1,lastNewPos,newPos_next,diff);
        if (extendLength_front>0){
            curCover.oldPos-=extendLength_front;
            curCover.newPos-=extendLength_front;
            curCover.length+=extendLength_front;
        }
        //向后延伸.
        TInt32 extendLength_back=getCanExtendLength(curCover.oldPos+curCover.length,curCover.newPos+curCover.length,1,lastNewPos,newPos_next,diff);
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
    
    TInt32 newPos_end=0;
    for (int i=0;i<(int)cover.size();++i){
        const TInt32 newPos=cover[i].newPos;
        if (newPos>newPos_end){
            diff.newDataDiff.insert(diff.newDataDiff.end(),newData+newPos_end,newData+newPos);
            diff.newDataSubDiff.resize(newPos,0);
        }
        const TInt32 oldPos=cover[i].oldPos;
        const TInt32 length=cover[i].length;
        for (TInt32 i=0; i<length;++i) {
            diff.newDataSubDiff.push_back(newData[i+newPos]-oldData[i+oldPos]);
        }
        newPos_end=newPos+length;
    }
    if (newPos_end<diff.newData_end-newData){
        diff.newDataDiff.insert(diff.newDataDiff.end(),newData+newPos_end,diff.newData_end);
        diff.newDataSubDiff.resize(diff.newData_end-newData,0);
    }
}

//diff结果序列化输出.
static void serialize_diff(const TDiffData& diff,std::vector<TByte>& out_serializeDiffStream){
    const TInt32 ctrlCount=(TInt32)diff.cover.size();
    std::vector<TByte> length_buf;
    std::vector<TByte> inc_newPos_buf;
    std::vector<TByte> inc_oldPos_buf;
    {
        TInt32 oldPosBack=0;
        TInt32 newPos_end=0;
        for (TInt32 i=0; i<ctrlCount; ++i) {
            pack32Bit(length_buf, diff.cover[i].length);
            const TInt32 inc_newPos=diff.cover[i].newPos-newPos_end;
            assert(inc_newPos>=0);
            pack32Bit(inc_newPos_buf,inc_newPos);
            const TInt32 inc_oldPos=diff.cover[i].oldPos-oldPosBack;
            if (inc_oldPos>=0){
                pack32BitWithTag(inc_oldPos_buf,inc_oldPos, 0, 1);
            }else{
                assert(-inc_oldPos>0);
                pack32BitWithTag(inc_oldPos_buf,-inc_oldPos, 1, 1);
            }
            oldPosBack=diff.cover[i].oldPos;
            newPos_end=diff.cover[i].newPos+diff.cover[i].length;
        }
    }
    
    pack32Bit(out_serializeDiffStream, ctrlCount);
    pack32Bit(out_serializeDiffStream, (TUInt32)length_buf.size());
    pack32Bit(out_serializeDiffStream, (TUInt32)inc_newPos_buf.size());
    pack32Bit(out_serializeDiffStream, (TUInt32)inc_oldPos_buf.size());
    pack32Bit(out_serializeDiffStream, (TUInt32)diff.newDataDiff.size());
    out_serializeDiffStream.insert(out_serializeDiffStream.end(),length_buf.begin(), length_buf.end());
    out_serializeDiffStream.insert(out_serializeDiffStream.end(),inc_newPos_buf.begin(), inc_newPos_buf.end());
    out_serializeDiffStream.insert(out_serializeDiffStream.end(),inc_oldPos_buf.begin(), inc_oldPos_buf.end());
    out_serializeDiffStream.insert(out_serializeDiffStream.end(),diff.newDataDiff.begin(), diff.newDataDiff.end());
    
    const int kBestRleLength=TBytesRle::kRle_bestSize;
    std::vector<TByte> rleData;
    const TByte* newDataSubDiff_begin=0;
    if (!diff.newDataSubDiff.empty()) newDataSubDiff_begin=&diff.newDataSubDiff[0];
    TBytesRle::save(rleData,newDataSubDiff_begin,newDataSubDiff_begin+diff.newDataSubDiff.size(),kBestRleLength);
    out_serializeDiffStream.insert(out_serializeDiffStream.end(),rleData.begin(),rleData.end());
}

}//end namespace

//------------------------------------------------------------------------------------------------------------------------

using namespace hdiffpatch;

void create_diff(const TByte* newData,const TByte* newData_end,const TByte* oldData,const TByte* oldData_end,std::vector<TByte>& out_diff){
    TDiffData diff;
    diff.newData=newData;
    diff.newData_end=newData_end;
    diff.oldData=oldData;
    diff.oldData_end=oldData_end;
    
    search_cover(diff);
    extend_cover(diff);//先尝试扩展.
    select_cover(diff);
    extend_cover(diff);//select_cover会删除一些覆盖线,所以重新扩展.
    sub_cover(diff);
    serialize_diff(diff,out_diff);
}

bool check_diff(const TByte* newData,const TByte* newData_end,const TByte* oldData,const TByte* oldData_end,const TByte* diff,const TByte* diff_end){
    std::vector<TByte> testNewData(newData_end-newData);
    TByte* testNewData_begin=0;
    if (!testNewData.empty()) testNewData_begin=&testNewData[0];
    if (!patch(testNewData_begin,testNewData_begin+testNewData.size(),oldData,oldData_end, diff,diff_end))
        return false;
    for (TInt32 i=0; i<(TInt32)testNewData.size(); ++i) {
        if (testNewData[i]!=newData[i])
            return false;
    }
    return true;
}

