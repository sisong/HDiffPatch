//hpatch_lite_types_private.h
//
/*
 The MIT License (MIT)
 Copyright (c) 2020-2022 HouSisong All Rights Reserved.
*/
#ifndef _hpatch_lite_types_private_h
#define _hpatch_lite_types_private_h
#include "hpatch_lite_types.h"
#ifdef __cplusplus
extern "C" {
#endif

#ifndef _IS_NEED_SHARE_hpi_TInputCache
#   define  _IS_NEED_SHARE_hpi_TInputCache  0
#endif

typedef struct _hpi_TInputCache{
    hpi_size_t      cache_begin;
    hpi_size_t      cache_end;
    hpi_byte*       cache_buf;
    hpi_TInputStreamHandle  inputStream;
    hpi_TInputStream_read   read_code;
} _hpi_TInputCache;

static hpi_force_inline 
hpi_BOOL _hpi_cache_success_finish(const _hpi_TInputCache* self){ return (self->cache_end!=0); }

#if (_IS_NEED_SHARE_hpi_TInputCache)
    hpi_fast_uint8 _hpi_cache_read_1byte(struct _hpi_TInputCache* self);
    hpi_BOOL       _hpi_cache_update(struct _hpi_TInputCache* self);
#endif

#ifdef __cplusplus
}
#endif
#endif //_hpatch_lite_types_private_h
