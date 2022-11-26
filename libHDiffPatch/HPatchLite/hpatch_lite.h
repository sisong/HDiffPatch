//patch_lite.h
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

#ifdef __cplusplus
}
#endif
#endif //_hpatch_lite_h
