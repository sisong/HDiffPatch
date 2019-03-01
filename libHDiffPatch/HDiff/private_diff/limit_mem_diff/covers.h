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

class TCovers{
public:
    inline TCovers(hpatch_StreamPos_t oldSize,hpatch_StreamPos_t newSize)
    :m_is32((oldSize|newSize)<((hpatch_StreamPos_t)1<<32)){ }
    
    inline void addCover(const TCover& cover){
        if (m_is32) {
            m_covers_limit.push_back((hpatch_uint32_t)cover.oldPos);
            m_covers_limit.push_back((hpatch_uint32_t)cover.newPos);
            m_covers_limit.push_back((hpatch_uint32_t)cover.length);
        }else{
            m_covers_larger.push_back(cover);
        }
    }
    
    inline size_t coverCount()const{
        return m_is32?(m_covers_limit.size()/3):m_covers_larger.size();
    }
    
    inline void covers(size_t index,TCover* out_cover)const{
        if (m_is32) {
            size_t i=index*3;
            out_cover->oldPos=m_covers_limit[i  ];
            out_cover->newPos=m_covers_limit[i+1];
            out_cover->length=m_covers_limit[i+2];
        }else{
            *out_cover=m_covers_larger[index];
        }
    }
private:
    std::vector<hpatch_uint32_t>    m_covers_limit;
    std::vector<TCover>             m_covers_larger;
    const  bool                     m_is32;
};

}//namespace hdiff_private
#endif /* icover_h */
