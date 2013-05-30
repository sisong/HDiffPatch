//
//  patch_base.h
//  HPatch
//
/*
 This is the HDiffPatch copyright.
 
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

#ifndef HPatch_patch_base_h
#define HPatch_patch_base_h

typedef unsigned char TByte;
typedef signed   int  TInt32;
typedef unsigned int  TUInt32;

//数据用rle压缩后的包类型2bit
enum TByteRleType{
    kByteRleType_rle0  = 0,    //00表示后面存的压缩0;    (包中不需要字节数据)
    kByteRleType_rle255= 1,    //01表示后面存的压缩255;  (包中不需要字节数据)
    kByteRleType_rle   = 2,    //10表示后面存的压缩数据;  (包中字节数据只需储存一个字节数据)
    kByteRleType_unrle = 3     //11表示后面存的未压缩数据;(包中连续储存多个字节数据)
};
static const int kByteRleType_bit=2;

#endif
