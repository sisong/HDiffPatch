// bsdiff_wrapper.h
// HDiffPatch
/*
 The MIT License (MIT)
 Copyright (c) 2021 HouSisong
 
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
#ifndef hdiff_bsdiff_wrapper_h
#define hdiff_bsdiff_wrapper_h
#include "../libHDiffPatch/HDiff/diff.h"

// create diffFile compatible with bsdiff4
void create_bsdiff(const unsigned char* newData,const unsigned char* newData_end,
                   const unsigned char* oldData,const unsigned char* oldData_end,
                   const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                   bool isEndsleyBsdiff=false,int kMinSingleMatchScore=kMinSingleMatchScore_default,
                   bool isUseBigCacheMatch=false,ICoverLinesListener* coverLinesListener=0,
                   size_t threadNum=1);
void create_bsdiff(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                   const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                   bool isEndsleyBsdiff=false,int kMinSingleMatchScore=kMinSingleMatchScore_default,
                   bool isUseBigCacheMatch=false,ICoverLinesListener* coverLinesListener=0,
                   size_t threadNum=1);

// create diffFile by stream compatible with bsdiff4
void create_bsdiff_stream(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                          const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                          bool isEndsleyBsdiff=false,size_t kMatchBlockSize=kMatchBlockSize_default,const hdiff_TMTSets_s* mtsets=0);

bool check_bsdiff(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                  const hpatch_TStreamInput* diffData,hpatch_TDecompress* decompressPlugin);
bool check_bsdiff(const unsigned char* newData,const unsigned char* newData_end,
                  const unsigned char* oldData,const unsigned char* oldData_end,
                  const unsigned char* diffData,const unsigned char* diffData_end,
                  hpatch_TDecompress* decompressPlugin);

#include "../libHDiffPatch/HDiff/match_block.h"

void create_bsdiff_block(unsigned char* newData,unsigned char* newData_end,
                         unsigned char* oldData,unsigned char* oldData_end,
                         const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                         bool isEndsleyBsdiff=false,int kMinSingleMatchScore=kMinSingleMatchScore_default,
                         bool isUseBigCacheMatch=false,
                         size_t matchBlockSize=kDefaultFastMatchBlockSize,
                         size_t threadNum=1);
void create_bsdiff_block(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                         const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                         bool isEndsleyBsdiff=false,int kMinSingleMatchScore=kMinSingleMatchScore_default,
                         bool isUseBigCacheMatch=false,
                         size_t matchBlockSize=kDefaultFastMatchBlockSize,
                         size_t threadNum=1);

#endif