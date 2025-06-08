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


typedef struct TInplaceSets{
    size_t      extraSafeSize;          //extra memory for inplace-patch; patch used sum memory size=extraSafeSize+decompressMemSize+cacheSize;
    bool        isCanExtendCover;       //now always is true
} TInplaceSets;
static inline bool isInplaceASets(const TInplaceSets& sets) { return sets.extraSafeSize==0; }
static inline bool isInplaceBSets(const TInplaceSets& sets) { return sets.extraSafeSize>0; }

//create a diff(delta) data for inplace-patch; 
void create_inplace_lite_diff(const hpi_byte* newData,const hpi_byte* newData_end,
                              const hpi_byte* oldData,const hpi_byte* oldData_end,
                              std::vector<hpi_byte>& out_inplace_lite_diff,const TInplaceSets& inplaceSets,
                              const hdiffi_TCompress* compressPlugin,int kMinSingleMatchScore=kLiteMatchScore_default,
                              bool isUseBigCacheMatch=false,size_t threadNum=1);

static inline //inplace A format, a very simplified diffFile format;
void create_inplaceA_lite_diff(const hpi_byte* newData,const hpi_byte* newData_end,
                               const hpi_byte* oldData,const hpi_byte* oldData_end,
                               std::vector<hpi_byte>& out_inplace_lite_diff,
                               const hdiffi_TCompress* compressPlugin,int kMinSingleMatchScore=kLiteMatchScore_default,
                               bool isUseBigCacheMatch=false,size_t threadNum=1){
    const TInplaceSets kInplaceASets={0,true};
    create_inplace_lite_diff(newData,newData_end,oldData,oldData_end,out_inplace_lite_diff,kInplaceASets,
                             compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,threadNum);
}
static inline //inplace B format, enable extra space(memory or disk);
void create_inplaceB_lite_diff(const hpi_byte* newData,const hpi_byte* newData_end,
                               const hpi_byte* oldData,const hpi_byte* oldData_end,
                               std::vector<hpi_byte>& out_inplace_lite_diff,size_t extraSafeSize,
                               const hdiffi_TCompress* compressPlugin,int kMinSingleMatchScore=kLiteMatchScore_default,
                               bool isUseBigCacheMatch=false,size_t threadNum=1){
    const TInplaceSets kInplaceBSets={extraSafeSize,true};
    create_inplace_lite_diff(newData,newData_end,oldData,oldData_end,out_inplace_lite_diff,kInplaceBSets,
                             compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,threadNum);
}

#endif //HDiff_for_hpatch_lite_h
