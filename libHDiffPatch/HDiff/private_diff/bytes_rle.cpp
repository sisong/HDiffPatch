//bytes_rle.cpp
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

#include "bytes_rle.h"
#include <assert.h>
#include "pack_uint.h"
#include "../../HPatch/patch_private.h"
namespace hdiff_private{

namespace {
    
    typedef unsigned char TByte;
    typedef size_t TUInt;

    static void rle_pushSame(std::vector<TByte>& out_ctrl,std::vector<TByte>& out_code,
                             TByte cur,TUInt count){
        assert(count>0);
        enum TByteRleType type;
        if (cur==0)
            type=kByteRleType_rle0;
        else if (cur==255)
            type=kByteRleType_rle255;
        else
            type=kByteRleType_rle;
        const TUInt packCount=(TUInt)(count-1);
        packUIntWithTag(out_ctrl,packCount,type,kByteRleType_bit);
        if (type==kByteRleType_rle)
            out_code.push_back(cur);
    }

    static void rle_pushNotSame(std::vector<TByte>& out_ctrl,std::vector<TByte>& out_code,
                                const TByte* byteStream,TUInt count){
        assert(count>0);
        if (count==1){
            rle_pushSame(out_ctrl,out_code,(*byteStream),1);
            return;
        }

        packUIntWithTag(out_ctrl,(TUInt)(count-1), kByteRleType_unrle,kByteRleType_bit);
        out_code.insert(out_code.end(),byteStream,byteStream+count);
    }

    inline static const TByte* rle_getEqualEnd(const TByte* cur,const TByte* src_end,TByte value){
        while (cur!=src_end) {
            if (*cur!=value)
                return cur;
            ++cur;
        }
        return src_end;
    }
    
}//end namespace

    void bytesRLE_save(std::vector<TByte>& out_ctrlBuf,std::vector<TByte>& out_codeBuf,
                       const TByte* src,const TByte* src_end,int rle_parameter){
        assert(rle_parameter>=kRle_bestSize);
        assert(rle_parameter<=kRle_bestUnRleSpeed);
        const TUInt kRleMinSameSize=rle_parameter+1;//增大则压缩率变小,解压稍快.

        const TByte* notSame=src;
        while (src!=src_end) {
            //find equal length
            TByte value=*src;
            const TByte* eqEnd=rle_getEqualEnd(src+1,src_end,value);
            const TUInt sameCount=(TUInt)(eqEnd-src);
            if ( (sameCount>kRleMinSameSize) || (  (sameCount==kRleMinSameSize)
                                                 &&( (value==0)||(value==255) ) ) ){//可以压缩.
                if (notSame!=src){
                    rle_pushNotSame(out_ctrlBuf,out_codeBuf,notSame,(TUInt)(src-notSame));
                }
                rle_pushSame(out_ctrlBuf,out_codeBuf,value, sameCount);

                src+=sameCount;
                notSame=src;
            }else{
                src=eqEnd;
            }
        }
        if (notSame!=src_end){
            rle_pushNotSame(out_ctrlBuf,out_codeBuf,notSame,(TUInt)(src_end-notSame));
        }
    }


void bytesRLE_save(std::vector<TByte>& out_code,
                   const TByte* src,const TByte* src_end,int rle_parameter){
    std::vector<TByte> ctrlBuf;
    std::vector<TByte> codeBuf;

    bytesRLE_save(ctrlBuf,codeBuf,src,src_end,rle_parameter);
    packUInt(out_code,(TUInt)ctrlBuf.size());
    out_code.insert(out_code.end(),ctrlBuf.begin(),ctrlBuf.end());
    out_code.insert(out_code.end(),codeBuf.begin(),codeBuf.end());
}

}//namespace hdiff_private
