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
#include "../../../HPatch/patch_types.h"
namespace hdiff_private{
typedef hpatch_TCover   TCover;

static inline void setCover(TCover& cover,hpatch_StreamPos_t oldPos,hpatch_StreamPos_t newPos,hpatch_StreamPos_t length) {
                                          cover.oldPos=oldPos; cover.newPos=newPos; cover.length=length; }

struct TCovers{
    void*       _covers;
    size_t      _coverCount;
    bool        _isCover32;
    inline TCovers(void* covers,size_t coverCount,bool isCover32)
    :_covers(covers),_coverCount(coverCount),_isCover32(isCover32){}
    inline size_t coverCount()const{ return _coverCount; }
    inline void covers(size_t index,TCover* out_cover)const{
        if (_isCover32) {
            const hpatch_TCover32& c32=((const hpatch_TCover32*)_covers)[index];
            out_cover->oldPos=c32.oldPos;
            out_cover->newPos=c32.newPos;
            out_cover->length=c32.length;
        }else{
            *out_cover=((const hpatch_TCover*)_covers)[index];
        }
    }
    virtual void addCover(const TCover& cover){} //default unsupport
};

class TCoversBuf:public TCovers{
public:
    inline TCoversBuf(hpatch_StreamPos_t dataSize0,hpatch_StreamPos_t dataSize1)
    :TCovers(0,0,(dataSize0|dataSize1)<((hpatch_StreamPos_t)1<<32)){}

    virtual void addCover(const TCover& cover){
        ++_coverCount;
        if (_isCover32) {
            hpatch_TCover32 c32;
            c32.oldPos=(hpatch_uint32_t)cover.oldPos;
            c32.newPos=(hpatch_uint32_t)cover.newPos;
            c32.length=(hpatch_uint32_t)cover.length;
            m_covers_limit.push_back(c32);
            _covers=m_covers_limit.data();
        }else{
            m_covers_larger.push_back(cover);
            _covers=m_covers_larger.data();
        }
    }
private:
    std::vector<hpatch_TCover32>    m_covers_limit;
    std::vector<TCover>             m_covers_larger;
};

}//namespace hdiff_private
#endif /* icover_h */
