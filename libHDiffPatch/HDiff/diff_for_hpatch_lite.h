//diff_for_hpatch_lite.h
//
/*
 The MIT License (MIT)
 Copyright (c) 2020-2022 HouSisong All Rights Reserved.
 */

#ifndef HDiff_for_hpatch_lite_h
#define HDiff_for_hpatch_lite_h
#include "diff_types.h"
#include "../HPatchLite/hpatch_lite_types.h"

struct hdiffi_TCompress:public hdiff_TCompress{
    hpi_compressType    compress_type;
};
const int kLiteMatchScore_default = 4;

void create_lite_diff(const unsigned char* newData,const unsigned char* newData_end,
                      const unsigned char* oldData,const unsigned char* oldData_end,
                      const hpatch_TStreamOutput* out_diff,const hdiffi_TCompress* compressPlugin,
                      int kMinSingleMatchScore=kLiteMatchScore_default,bool isUseBigCacheMatch=false);
bool check_lite_diff(const unsigned char* newData,const unsigned char* newData_end,
                     const unsigned char* oldData,const unsigned char* oldData_end,
                     const unsigned char* lite_diff,const unsigned char* lite_diff_end,
                     hpatch_TDecompress* decompressPlugin);

#endif //HDiff_for_hpatch_lite_h
