// match_inplace.h
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
#ifndef hdiff_match_inplace_h
#define hdiff_match_inplace_h
#include "diff_for_hpatch_lite.h"

    struct TMatchInplace:public ILiteDiffListener {
        const size_t oldSize;
        const size_t newSize;
        const TInplaceSets inplaceSets;
        explicit TMatchInplace(size_t _oldSize,size_t _newSize,const TInplaceSets& _inplaceSets);

        struct TCover:public hpatch_TCover{
            inline explicit TCover(){}
            inline explicit TCover(const hpatch_TCover& c){ oldPos=c.oldPos; newPos=c.newPos; length=c.length; next=0;  }
            inline explicit TCover(hpatch_StreamPos_t _oldPos,hpatch_StreamPos_t _newPos,hpatch_StreamPos_t _length){ oldPos=_oldPos; newPos=_newPos; length=_length; next=0;  }
            TCover*     next;
        };
        std::vector<size_t> _researchIndexList;
        std::vector<TCover> researchedCovers;
        size_t              researchCount;
        hpatch_StreamPos_t  researchLength;
        hpatch_StreamPos_t  inExtraSafeLength;
        hpatch_StreamPos_t  curExtraSafeSize;
    };

#endif //hdiff_match_inplace_h