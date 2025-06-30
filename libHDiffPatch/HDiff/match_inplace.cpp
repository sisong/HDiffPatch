// match_inplace.cpp
// hdiff
/*
 The MIT License (MIT)
 Copyright (c) 2023 HouSisong
 
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
#include "match_inplace.h"
#include <algorithm> //std::min std::max std::sort
using namespace hdiff_private;

namespace{
    typedef TMatchInplace::TCover   TCover;
    static const hpatch_StreamPos_t _kMinCoverLen     =24;
    static const int _kMaxMatchDeepForResearch = kDefaultMaxMatchDeepForLimit*2;

    static inline bool coverInExtraSafeSize(const hpatch_TCover& cover,hpatch_StreamPos_t kExtraSafeSize){
        return cover.oldPos+kExtraSafeSize>=cover.newPos;
    }

    static inline TInplaceSets _limitSets(size_t oldSize, size_t newSize, const TInplaceSets& inplaceSets) {
        TInplaceSets limitedSets=inplaceSets;
        limitedSets.extraSafeSize=std::min(limitedSets.extraSafeSize,newSize);
        return limitedSets;
    }

//ICoverLinesListener
static bool _search_cover_limit(ICoverLinesListener* listener,const void* pcovers,size_t coverCount,bool isCover32){
    TMatchInplace* self=(TMatchInplace*)listener;
    const hpatch_StreamPos_t kExtraSafeSize=self->inplaceSets.extraSafeSize;
    std::vector<size_t> researchIndexList;
    TRefCovers covers(pcovers,coverCount,isCover32);
    hpatch_StreamPos_t researchLength=0;
    hpatch_StreamPos_t inExtraSafeLength=0;
    for (size_t i=0;i<coverCount;++i){
        hpatch_TCover curCover=covers[i];
        if (coverInExtraSafeSize(curCover,kExtraSafeSize)){//ok
            inExtraSafeLength+=curCover.length;
        }else{
            researchLength+=curCover.length;
            researchIndexList.push_back(i);
        }
    }
    self->inExtraSafeLength=inExtraSafeLength;
    self->researchLength=researchLength;
    self->researchCount=researchIndexList.size();
    self->_researchIndexList.swap(researchIndexList);
    return (!self->_researchIndexList.empty());
}


    struct TDiffSearchCoverListener :public IDiffSearchCoverListener{
        const size_t kExtraSafeSize;
        inline explicit TDiffSearchCoverListener(size_t _extraMemSize):kExtraSafeSize(_extraMemSize){ 
            limitCover=_limitCover;
            limitCover_front=_limitCover_front;
        }
        static bool _limitCover(struct IDiffSearchCoverListener* listener,
                                const hpatch_TCover* cover,hpatch_StreamPos_t* out_leaveLen){
            TDiffSearchCoverListener* self=(TDiffSearchCoverListener*)listener;
            bool isOk=(cover->length>=_kMinCoverLen)&&coverInExtraSafeSize(*cover,self->kExtraSafeSize);
            if (out_leaveLen) *out_leaveLen=isOk?cover->length:0;
            return isOk;
        }
        static void _limitCover_front(struct IDiffSearchCoverListener* listener,
                                      const hpatch_TCover* front_cover,hpatch_StreamPos_t* out_leaveLen){
            TDiffSearchCoverListener* self=(TDiffSearchCoverListener*)listener;
            assert(coverInExtraSafeSize(*front_cover,self->kExtraSafeSize));
            *out_leaveLen=front_cover->length;
        }
    };
static void _research_cover(ICoverLinesListener* listener,IDiffResearchCover* diffi,const void* pcovers,size_t coverCount,bool isCover32){
    TMatchInplace* self=(TMatchInplace*)listener;
    TRefCovers covers(pcovers,coverCount,isCover32);
    TDiffSearchCoverListener diffSearchCoverListener(self->inplaceSets.extraSafeSize);
    std::vector<size_t> researchIndexList;
    researchIndexList.swap(self->_researchIndexList);
    std::vector<TCover> researchedCovers;
    for (size_t i=0;i<researchIndexList.size();++i){
        size_t limitCoverIndex=researchIndexList[i];
        hpatch_TCover cover=covers[limitCoverIndex];
        if (!researchedCovers.empty())
            researchedCovers[i]=TCover(cover);
        diffi->researchCover(diffi,&diffSearchCoverListener,limitCoverIndex,0,0,cover.length);
    }
    self->researchedCovers.swap(researchedCovers);
}

static int _get_max_match_deep(const ICoverLinesListener* listener){
    return _kMaxMatchDeepForResearch;
}

static void _search_cover_finish(ICoverLinesListener* listener,void* pcovers,size_t* pcoverCount,bool isCover32,
                                 hpatch_StreamPos_t* newSize,hpatch_StreamPos_t* oldSize){
    TMatchInplace* self=(TMatchInplace*)listener;
    const hpatch_size_t kExtraSafeSize=self->inplaceSets.extraSafeSize;

    //check covers
    hpatch_StreamPos_t minExtraSafeSize=0;
    TRefCovers _covers(pcovers,*pcoverCount,isCover32);
    for (size_t i=0;i<_covers.size();++i){
        hpatch_TCover cover=_covers[i];
        if (!coverInExtraSafeSize(cover,kExtraSafeSize))
            throw std::runtime_error("TMatchInplace limit cover by extraSafeSize ERROR!");
        if (cover.oldPos<cover.newPos)
            minExtraSafeSize=std::max(minExtraSafeSize,(hpatch_StreamPos_t)(cover.newPos-cover.oldPos));
    }
    if (minExtraSafeSize>kExtraSafeSize)
       throw std::runtime_error("TMatchInplace extraSafeSize value ERROR!");
    self->curExtraSafeSize=minExtraSafeSize;
}

static bool _getInplacePatchExtraSafeSize(ILiteDiffListener* listener,hpi_size_t* out_extraSafeSize){
    const TMatchInplace* self=(const TMatchInplace*)listener;
    if (self->inplaceSets.isCompatibleLiteDiff)
        return false; //not save extraSafeSize
    *out_extraSafeSize=(hpi_size_t)self->curExtraSafeSize;
    return true; //save extraSafeSize
}

}//namespace


TMatchInplace::TMatchInplace(size_t _oldSize,size_t _newSize,const TInplaceSets& _inplaceSets)
:oldSize(_oldSize),newSize(_newSize),inplaceSets(_limitSets(_oldSize,_newSize,_inplaceSets)){
    this->curExtraSafeSize=inplaceSets.extraSafeSize;
    this->getInplacePatchExtraSafeSize=0;
    ICoverLinesListener* cl=this;
    memset(cl,0,sizeof(*cl));
    this->search_cover_limit=_search_cover_limit;
    this->research_cover=_research_cover;
    this->get_max_match_deep=_get_max_match_deep;
    this->search_cover_finish=_search_cover_finish;
    this->getInplacePatchExtraSafeSize=_getInplacePatchExtraSafeSize;
}

void create_inplace_lite_diff(const hpi_byte* newData,const hpi_byte* newData_end,
                              const hpi_byte* oldData,const hpi_byte* oldData_end,
                              std::vector<hpi_byte>& out_inplace_lite_diff,const TInplaceSets& inplaceSets,
                              const hdiffi_TCompress* compressPlugin,int kMinSingleMatchScore,
                              bool isUseBigCacheMatch,size_t threadNum){
    TMatchInplace matchInplace(oldData_end-oldData,newData_end-newData,inplaceSets);
    create_lite_diff(newData,newData_end,oldData,oldData_end,out_inplace_lite_diff,compressPlugin,
                     kMinSingleMatchScore,isUseBigCacheMatch,&matchInplace,threadNum);

#ifdef _DEBUG
    printf("\nDEBUG TMatchInplace info:\n"
           "    cur extraSafeSize    : %" PRIu64 "\n",matchInplace.curExtraSafeSize);
    printf("    extraSafe data length: %" PRIu64 "\n",matchInplace.inExtraSafeLength);
    printf("    research  cover count: %" PRIu64 "\n",(hpatch_StreamPos_t)matchInplace.researchCount);
    printf("    research  data length: %" PRIu64 "\n\n",matchInplace.researchLength);
#endif
}
