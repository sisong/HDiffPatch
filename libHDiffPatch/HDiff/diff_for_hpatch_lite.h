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
#include <vector>

struct hdiffi_TCompress{
    const hdiff_TCompress* compress;
    hpi_compressType       compress_type;
};
const int kLiteMatchScore_default = 6;

void create_lite_diff(const hpi_byte* newData,const hpi_byte* newData_end,
                      const hpi_byte* oldData,const hpi_byte* oldData_end,
                      std::vector<hpi_byte>& out_lite_diff,const hdiffi_TCompress* compressPlugin,
                      int kMinSingleMatchScore=kLiteMatchScore_default,bool isUseBigCacheMatch=false,
                      ICoverLinesListener* listener=0,size_t threadNum=1);

bool check_lite_diff_open(const hpi_byte* lite_diff,const hpi_byte* lite_diff_end,
                          hpi_compressType* out_compress_type);
bool check_lite_diff(const hpi_byte* newData,const hpi_byte* newData_end,
                     const hpi_byte* oldData,const hpi_byte* oldData_end,
                     const hpi_byte* lite_diff,const hpi_byte* lite_diff_end,
                     hpatch_TDecompress* decompressPlugin);

#endif //HDiff_for_hpatch_lite_h
