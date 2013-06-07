//bytes_rle.cpp
//create by housisong
//快速解压的一个通用字节流压缩rle算法.
//
/*
 Copyright (c) 2012-2013 HouSisong All Rights Reserved.
 
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
#include "assert.h"
#include "../diff_base.h"

namespace hdiffpatch {
namespace {
    
    class TBytesZiper_rle{
    public:
        static void save(std::vector<TByte>& out_code,const TByte* src,const TByte* src_end,int rle_parameter){
            assert(rle_parameter>=TBytesRle::kRle_bestSize);
            assert(rle_parameter<=TBytesRle::kRle_bestUnRleSpeed);
            const int kRleMinZipSize=rle_parameter; //增大则压缩率变小,解压稍快.
            const int kRleMinSameSize=kRleMinZipSize+1;
            std::vector<TByte> codeBuf;
            std::vector<TByte> ctrlBuf;
            
            const TByte* notSame=src;
            while (src!=src_end) {
                //find equal length
                TByte value=*src;
                const TByte* eqEnd=getEqualEnd(src+1,src_end,value);
                const int sameCount=(int)(eqEnd-src);
                if ( (sameCount>kRleMinSameSize) || ( (sameCount==kRleMinSameSize)&&( (value==0)||(value==255) ) ) ){//可以压缩.
                    if (notSame!=src){
                        pushNotSame(codeBuf,ctrlBuf,notSame,(TInt32)(src-notSame));
                    }
                    pushSame(codeBuf,ctrlBuf,value, sameCount);
                    
                    src+=sameCount;
                    notSame=src;
                }else{
                    src=eqEnd;
                }
            }
            if (notSame!=src_end){
                pushNotSame(codeBuf,ctrlBuf,notSame,(TInt32)(src_end-notSame));
            }
            
            hdiffpatch::pack32Bit(out_code,(TInt32)ctrlBuf.size());
            out_code.insert(out_code.end(),ctrlBuf.begin(),ctrlBuf.end());
            out_code.insert(out_code.end(),codeBuf.begin(),codeBuf.end());
        }
        
    private:
        static void pushSame(std::vector<TByte>& out_code,std::vector<TByte>& out_ctrl,TByte cur,TInt32 count){
            assert(count>0);
            TByteRleType type;
            if (cur==0)
                type=kByteRleType_rle0;
            else if (cur==255)
                type=kByteRleType_rle255;
            else
                type=kByteRleType_rle;
            const TInt32 packCount=count-1;
            hdiffpatch::pack32BitWithTag(out_ctrl,packCount,type,kByteRleType_bit);
            if (type==kByteRleType_rle)
                out_code.push_back(cur);
        }
        
        static void pushNotSame(std::vector<TByte>& out_code,std::vector<TByte>& out_ctrl,const TByte* byteStream,TInt32 count){
            assert(count>0);
            if (count==1){
                pushSame(out_code,out_ctrl,*byteStream,1);
                return;
            }
            
            hdiffpatch::pack32BitWithTag(out_ctrl,count-1, kByteRleType_unrle,kByteRleType_bit);
            out_code.insert(out_code.end(),byteStream,byteStream+count);
        }
        
        inline static const TByte* getEqualEnd(const TByte* cur,const TByte* src_end,TByte value){
            while (cur!=src_end) {
                if (*cur!=value)
                    return cur;
                ++cur;
            }
            return src_end;
        }
    };

}
    
}//end namespace
    
void TBytesRle::save(std::vector<unsigned char>& out_code,const unsigned char* src,const unsigned char* src_end,int rle_parameter){
    hdiffpatch::TBytesZiper_rle::save(out_code, src, src_end,rle_parameter);
}
    

