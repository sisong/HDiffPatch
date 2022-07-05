//hpatch_lite_type.h
//
/*
 The MIT License (MIT)
 Copyright (c) 2020-2022 HouSisong All Rights Reserved.
*/

#ifndef _hpatch_lite_type_h
#define _hpatch_lite_type_h
#ifdef NDEBUG
# ifndef assert
#   define  assert(expression) ((void)0)
# endif
#else
#   include <assert.h> //assert
#endif
#ifdef __cplusplus
extern "C" {
#endif

#ifndef hpi_byte
    typedef     unsigned char   hpi_byte;
#endif
#ifndef hpi_fast_uint8
    typedef     unsigned int    hpi_fast_uint8; //>= 8bit uint
#endif
#ifndef hpi_BOOL
    typedef     hpi_fast_uint8  hpi_BOOL;
#endif
#define         hpi_FALSE       0
#define         hpi_TRUE        1
#ifndef hpi_size_t
    typedef     unsigned int    hpi_size_t; //memory size type
#endif
#ifndef hpi_pos_t
    typedef     unsigned int    hpi_pos_t; //file size type
#endif

#ifndef hpi_inline
#if (defined(_MSC_VER))
#   define hpi_inline __inline
#else
#   define hpi_inline inline
#endif
#endif
#ifndef hpi_force_inline
#if defined(_MSC_VER)
#   define hpi_force_inline __forceinline
#elif defined(__GNUC__) || defined(__clang__) || defined(__CC_ARM)
#   define hpi_force_inline __attribute__((always_inline)) inline
#elif defined(__ICCARM__)
#   define hpi_force_inline _Pragma("inline=forced")
#else
#   define hpi_force_inline hpi_inline
#endif
#endif

#ifndef hpi_try_inline
//#   define hpi_try_inline hpi_inline
#   define hpi_try_inline
#endif


#ifndef _IS_RUN_MEM_SAFE_CHECK 
#   define _IS_RUN_MEM_SAFE_CHECK 1
#endif

#ifndef hpi_TInputStreamHandle
    typedef void*   hpi_TInputStreamHandle;
#endif

//read (*data_size) data to out_data from sequence stream; if input stream end,set *data_size readed size; if read error return hpi_FALSE;
typedef hpi_BOOL (*hpi_TInputStream_read)(hpi_TInputStreamHandle inputStream,hpi_byte* out_data,hpi_size_t* data_size);

#ifndef hpi_kHeadSize
#   define  hpi_kHeadSize       (2+1+1) //"hI" + hpi_compressType + (versionCode+newSize bytes+uncompressSize bytes) { + newSize + uncompressSize}
#endif

#ifndef hpi_kMinCacheSize
#   define hpi_kMinCacheSize    (1*2)
#endif

typedef enum hpi_compressType{
    hpi_compressType_no=0,
    hpi_compressType_tuz=1, //tinyuz
    hpi_compressType_zlib=2, //deflate format
    hpi_compressType_lzma=3,
    hpi_compressType_lzma2=4,
    hpi_compressType_zstd=5,
    hpi_compressType_bzip2=6,
    hpi_compressType_lz4=7,
    hpi_compressType_brotli=8,
    hpi_compressType_lzham=9,
    //...
} hpi_compressType;

#ifdef __cplusplus
}
#endif
#endif //_hpatch_lite_type_h
