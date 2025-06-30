//hpatch_lite.h
//
/*
 The MIT License (MIT)
 Copyright (c) 2020-2022 HouSisong All Rights Reserved.
*/

#ifndef _hpatch_lite_h
#define _hpatch_lite_h
#include "hpatch_lite_types.h"
#ifdef __cplusplus
extern "C" {
#endif

#ifndef _IS_WTITE_NEW_BY_PAGE
#   define _IS_WTITE_NEW_BY_PAGE   0
#endif

//-----------------------------------------------------------------------------------------------------------------
// hpatch_lite by stream: hpatch_lite_open()+hpatch_lite_patch() compiled by Mbed Studio is 662 bytes
//   hdiffpatch v4.2.3, other patcher compiled by Mbed Studio:
//      patch_single_stream() 2356 bytes (hpatch_StreamPos_t=hpatch_uint32_t)
//      patch_decompress_with_cache() 2846 bytes (_IS_NEED_CACHE_OLD_BY_COVERS=0,hpatch_StreamPos_t=hpatch_uint32_t)

//diff_data must created by create_lite_diff()

typedef struct hpatchi_listener_t{
    hpi_TInputStreamHandle  diff_data;
    hpi_TInputStream_read   read_diff;
    //must read data_size data to out_data, from read_from_pos of stream; if read error return hpi_FALSE;
    hpi_BOOL (*read_old)(struct hpatchi_listener_t* listener,hpi_pos_t read_from_pos,hpi_byte* out_data,hpi_size_t data_size);
    //must write data_size data to sequence stream; if write error return hpi_FALSE;
    hpi_BOOL (*write_new)(struct hpatchi_listener_t* listener,const hpi_byte* data,hpi_size_t data_size);
} hpatchi_listener_t;

//hpatch_lite open
// read lite headinfo from diff_data
// if diff_data uncompress(*out_compress_type==hpi_compressType_no), *out_uncompressSize==0;
// if (*out_compress_type!=hpi_compressType_no), you need open compressed data by decompresser 
//      (see https://github.com/sisong/HPatchLite/decompresser_demo.h & https://github.com/sisong/HPatchLite/hpatchi.c)
hpi_BOOL hpatch_lite_open(hpi_TInputStreamHandle diff_data,hpi_TInputStream_read read_diff,
                          hpi_compressType* out_compress_type,hpi_pos_t* out_newSize,hpi_pos_t* out_uncompressSize);
//hpatch_lite patch
//	used temp_cache_size memory + {decompress buffer*1}
//  note: temp_cache_size>=hpi_kMinCacheSize
hpi_BOOL hpatch_lite_patch(hpatchi_listener_t* listener,hpi_pos_t newSize,
                           hpi_byte* temp_cache,hpi_size_t temp_cache_size);


//-----------------------------------------------------------------------------------------------------------------
// hpatch_lite inplace-patch by extra: 
//  hpatchi_inplace_open()+hpatchi_inplaceB() compiled by Mbed Studio is 976 bytes
//  hpatchi_inplace_open()+hpatchi_inplaceB_by_page() compiled by Mbed Studio is 1116 bytes

//inplace-patch open
//  diff_data created by create_inplace_lite_diff() or create_inplaceA_lite_diff() or create_inplaceB_lite_diff();
hpi_BOOL hpatchi_inplace_open(hpi_TInputStreamHandle diff_data,hpi_TInputStream_read read_diff,
                              hpi_compressType* out_compress_type,hpi_pos_t* out_newSize,
                              hpi_pos_t* out_uncompressSize,hpi_size_t* out_extraSafeSize);

//inplace-patch for inplaceA format
//  diff_data created by (create_inplace_lite_diff() with extraSafeSize==0) or create_inplaceA_lite_diff();
static hpi_force_inline hpi_BOOL hpatchi_inplaceA(hpatchi_listener_t* listener,hpi_pos_t newSize,
                                                  hpi_byte* temp_cache,hpi_size_t temp_cache_size){
    return hpatch_lite_patch(listener,newSize,temp_cache,temp_cache_size);
}


#if (_IS_WTITE_NEW_BY_PAGE==0)

//inplace-patch for inplaceB format
//  used extraSafeSize extra memory for safe, prevent writing to old data areas that are still useful;
//  diff_data created by (create_inplace_lite_diff() with extraSafeSize>0) or create_inplaceB_lite_diff();
//  this function is also compatible apply inplaceA format;
//  note: temp_cache_size>=hpi_kMinInplaceCacheSize+extraSafeSize; hpatchi_inplaceB() used hpatch_lite_patch();
hpi_BOOL hpatchi_inplaceB(hpatchi_listener_t* listener,hpi_pos_t newSize,
                          hpi_byte* temp_cache,hpi_size_t extraSafeSize_in_temp_cache,hpi_size_t temp_cache_size);

#else //_IS_WTITE_NEW_BY_PAGE!=0

//same as hpatchi_inplaceB, but write new data by page every time;
//  note: temp_cache_size>=hpi_kMinInplaceCacheSize+extraSafeSize+pageSize;
hpi_BOOL hpatchi_inplaceB_by_page(hpatchi_listener_t* listener,hpi_pos_t newSize,hpi_byte* temp_cache,
                                  hpi_size_t extraSafeSize_in_temp_cache,hpi_size_t pageSize_in_temp_cache,hpi_size_t temp_cache_size);

#endif //_IS_WTITE_NEW_BY_PAGE

#ifdef __cplusplus
}
#endif
#endif //_hpatch_lite_h
