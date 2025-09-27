//  covers.h
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
#ifndef covers_h
#define covers_h
#include <vector>
#include <algorithm> //std::sort
#include "../../../HDiff/diff_types.h"
namespace hdiff_private{
typedef hpatch_TCover   TCover;

static inline void setCover(TCover& cover,hpatch_StreamPos_t oldPos,hpatch_StreamPos_t newPos,hpatch_StreamPos_t length) {
                                          cover.oldPos=oldPos; cover.newPos=newPos; cover.length=length; }

// input & output covers
//  must overwrite push_cover for output covers
struct TCovers{
    TCover*     _covers;
    size_t      _coverCount;
    inline TCovers(TCover* covers,size_t coverCount)
    :_covers(covers),_coverCount(coverCount){}
    inline size_t size()const{ return _coverCount; }
    inline const hpatch_TCover& operator[](size_t index)const{
        return _covers[index];
    }
    inline hpatch_TCover& operator[](size_t index){
        return _covers[index];
    }
};

template<class _TCover>
static void tm_collate_covers(std::vector<_TCover>& covers){
    if (covers.size()<=1) return;
    std::sort(covers.begin(),covers.end(),cover_cmp_by_new_t<_TCover>());
    size_t backi=0;
    for (size_t i=1;i<covers.size();++i){
        if (covers[i].newPos<covers[backi].newPos+covers[backi].length){
            if (covers[i].newPos+covers[i].length>covers[backi].newPos+covers[backi].length){
                if (cover_is_collinear(covers[i],covers[backi])){//insert i part to backi,del i
                    covers[backi].length=covers[i].newPos+covers[i].length-covers[backi].newPos;
                }else{//del backi part, save i
                    covers[backi].length=covers[i].newPos-covers[backi].newPos;
                    if (covers[backi].length>=kCoverMinMatchLen)
                        ++backi;
                    covers[backi]=covers[i];
                }
            } //else del i
        }else if ((covers[i].newPos==covers[backi].newPos+covers[backi].length)
                &&(covers[i].oldPos==covers[backi].oldPos+covers[backi].length)){
            covers[backi].length+=covers[i].length; //insert i all to backi,del i
        }else{ //save i
            ++backi;
            covers[backi]=covers[i];
        }
    }
    covers.resize(backi+1);
}

class TCoversBuf:public hpatch_TOutputCovers{
public:
    inline TCoversBuf(){
        push_cover=_push_cover;
        collate_covers=_collate_covers;
    }
private:
    static hpatch_BOOL _push_cover(struct hpatch_TOutputCovers* out_covers,const TCover* cover){
        TCoversBuf* self=(TCoversBuf*)out_covers;
        self->vec_covers.push_back(*cover);
        return hpatch_TRUE;
    }
    static void _collate_covers(struct hpatch_TOutputCovers* out_covers){
        TCoversBuf* self=(TCoversBuf*)out_covers;
        tm_collate_covers(self->vec_covers);
    }
public:
    std::vector<TCover>     vec_covers;
};

}//namespace hdiff_private
#endif /* icover_h */
