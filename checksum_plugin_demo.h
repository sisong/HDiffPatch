//checksum_plugin_demo.h
//  checksum plugin demo for HDiffz\HPatchz
/*
 The MIT License (MIT)
 Copyright (c) 2018-2021 HouSisong
 
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
#ifndef HPatch_checksum_plugin_demo_h
#define HPatch_checksum_plugin_demo_h
//checksum plugin demo:
//  crc32ChecksumPlugin
//  adler32ChecksumPlugin
//  adler64ChecksumPlugin
//  fadler32ChecksumPlugin
//  fadler64ChecksumPlugin
//  fadler128ChecksumPlugin
//  md5ChecksumPlugin
//  blake3ChecksumPlugin
//  sha256ChecksumPlugin
//  sha512ChecksumPlugin
//  xxh3ChecksumPlugin
//  xxh128ChecksumPlugin

#include "libHDiffPatch/HPatch/checksum_plugin.h"

#ifndef _IsNeedIncludeDefaultChecksumHead
#   define _IsNeedIncludeDefaultChecksumHead 1
#endif

#define __out_checksum4__(out_code,i0,v,shr) {    \
    (out_code)[i0  ]=(unsigned char)((v)>>(shr));   \
    (out_code)[i0+1]=(unsigned char)((v)>>(shr+8)); \
    (out_code)[i0+2]=(unsigned char)((v)>>(shr+16));\
    (out_code)[i0+3]=(unsigned char)((v)>>(shr+24));\
}
#define __out_checksum4(out_code,v)     __out_checksum4__(out_code,0,v,0)
#define __out_checksum8(out_code,v)  {  __out_checksum4__(out_code,0,v,0); \
                                        __out_checksum4__(out_code,4,v,32); }


#ifdef  _ChecksumPlugin_crc32
#if (_IsNeedIncludeDefaultChecksumHead)
# ifndef _IS_USED_LIBDEFLATE_CRC32
#   define _IS_USED_LIBDEFLATE_CRC32    0
#endif
#if (_IS_USED_LIBDEFLATE_CRC32)
#   include "libdeflate.h" // https://github.com/ebiggers/libdeflate
# else
#   include "zlib.h" // http://zlib.net/  https://github.com/madler/zlib
# endif
#endif
#if (_IS_USED_LIBDEFLATE_CRC32)
#   define _crc32 libdeflate_crc32
#else
#   define _crc32 crc32
#endif
static const char* _crc32_checksumType(void){
    static const char* type="crc32";
    return type;
}
static size_t _crc32_checksumByteSize(void){
    return sizeof(hpatch_uint32_t);
}
static hpatch_checksumHandle _crc32_open(hpatch_TChecksum* plugin){
    return malloc(sizeof(hpatch_uint32_t));
}
static void _crc32_close(hpatch_TChecksum* plugin,hpatch_checksumHandle handle){
    if (handle) free(handle);
}
static void  _crc32_begin(hpatch_checksumHandle handle){
    hpatch_uint32_t* pv=(hpatch_uint32_t*)handle;
    *pv=(hpatch_uint32_t)_crc32(0,0,0);
}
static void _crc32_append(hpatch_checksumHandle handle,
                          const unsigned char* part_data,const unsigned char* part_data_end){
    hpatch_uint32_t* pv=(hpatch_uint32_t*)handle;
#if (_IS_USED_LIBDEFLATE_CRC32)
    *pv=_crc32(*pv,part_data,part_data_end-part_data);
#else
    uLong v=*pv;
    while (part_data!=part_data_end) {
        size_t dataSize=(size_t)(part_data_end-part_data);
        uInt  len=~(uInt)0;
        //assert(len>0);
        if (len>dataSize)
            len=(uInt)dataSize;
        v=_crc32(v,part_data,len);
        part_data+=len;
    }
    *pv=(hpatch_uint32_t)v;
#endif
}
static void _crc32_end(hpatch_checksumHandle handle,
                       unsigned char* checksum,unsigned char* checksum_end){
    hpatch_uint32_t v=*(hpatch_uint32_t*)handle;
    assert(4==checksum_end-checksum);
    __out_checksum4(checksum,v);
}
static hpatch_TChecksum crc32ChecksumPlugin={ _crc32_checksumType,_crc32_checksumByteSize,_crc32_open,
                                              _crc32_close,_crc32_begin,_crc32_append,_crc32_end};
#endif//_ChecksumPlugin_crc32


#ifdef  _ChecksumPlugin_adler32
#   ifndef _ChecksumPlugin_crc32
#       include "libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.h"
#   endif
static const char* _adler32_checksumType(void){
    static const char* type="adler32";
    return type;
}
static size_t _adler32_checksumByteSize(void){
    return sizeof(hpatch_uint32_t);
}
static hpatch_checksumHandle _adler32_open(hpatch_TChecksum* plugin){
    return malloc(sizeof(hpatch_uint32_t));
}
static void _adler32_close(hpatch_TChecksum* plugin,hpatch_checksumHandle handle){
    if (handle) free(handle);
}
static void  _adler32_begin(hpatch_checksumHandle handle){
    hpatch_uint32_t* pv=(hpatch_uint32_t*)handle;
#ifdef _ChecksumPlugin_crc32
    *pv=(hpatch_uint32_t)adler32(0,0,0);
#else
    *pv=adler32_start(0,0);
#endif
}
static void _adler32_append(hpatch_checksumHandle handle,
                          const unsigned char* part_data,const unsigned char* part_data_end){
    hpatch_uint32_t* pv=(hpatch_uint32_t*)handle;
#ifdef _ChecksumPlugin_crc32
    uLong v=*pv;
    while (part_data!=part_data_end) {
        size_t dataSize=(size_t)(part_data_end-part_data);
        uInt  len=~(uInt)0;
        //assert(len>0);
        if (len>dataSize)
            len=(uInt)dataSize;
        v=adler32(v,part_data,len);
        part_data+=len;
    }
    *pv=(hpatch_uint32_t)v;
#else
    *pv=_adler32_append(*pv,part_data,(size_t)(part_data_end-part_data));
#endif
}
static void _adler32_end(hpatch_checksumHandle handle,
                       unsigned char* checksum,unsigned char* checksum_end){
    hpatch_uint32_t v=*(hpatch_uint32_t*)handle;
    assert(4==checksum_end-checksum);
    __out_checksum4(checksum,v);
}
static hpatch_TChecksum adler32ChecksumPlugin={ _adler32_checksumType,_adler32_checksumByteSize,_adler32_open,
                                                _adler32_close,_adler32_begin,_adler32_append,_adler32_end};
#endif//_ChecksumPlugin_adler32


#ifdef  _ChecksumPlugin_adler64
#include "libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.h"
static const char* _adler64_checksumType(void){
    static const char* type="adler64";
    return type;
}
static size_t _adler64_checksumByteSize(void){
    return sizeof(hpatch_uint64_t);
}
static hpatch_checksumHandle _adler64_open(hpatch_TChecksum* plugin){
    return malloc(sizeof(hpatch_uint64_t));
}
static void _adler64_close(hpatch_TChecksum* plugin,hpatch_checksumHandle handle){
    if (handle) free(handle);
}
static void  _adler64_begin(hpatch_checksumHandle handle){
    hpatch_uint64_t* pv=(hpatch_uint64_t*)handle;
    *pv=adler64_start(0,0);
}
static void _adler64_append(hpatch_checksumHandle handle,
                            const unsigned char* part_data,const unsigned char* part_data_end){
    hpatch_uint64_t* pv=(hpatch_uint64_t*)handle;
    *pv=adler64_append(*pv,part_data,part_data_end-part_data);
}
static void _adler64_end(hpatch_checksumHandle handle,
                         unsigned char* checksum,unsigned char* checksum_end){
    hpatch_uint64_t v=*(hpatch_uint64_t*)handle;
    assert(8==checksum_end-checksum);
    __out_checksum8(checksum,v);
}
static hpatch_TChecksum adler64ChecksumPlugin={ _adler64_checksumType,_adler64_checksumByteSize,_adler64_open,
                                                _adler64_close,_adler64_begin,_adler64_append,_adler64_end};
#endif//_ChecksumPlugin_adler64


#ifdef  _ChecksumPlugin_fadler32
#include "libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.h"
static const char* _fadler32_checksumType(void){
    static const char* type="fadler32";
    return type;
}
static size_t _fadler32_checksumByteSize(void){
    return sizeof(hpatch_uint32_t);
}
static hpatch_checksumHandle _fadler32_open(hpatch_TChecksum* plugin){
    return malloc(sizeof(hpatch_uint32_t));
}
static void _fadler32_close(hpatch_TChecksum* plugin,hpatch_checksumHandle handle){
    if (handle) free(handle);
}
static void  _fadler32_begin(hpatch_checksumHandle handle){
    hpatch_uint32_t* pv=(hpatch_uint32_t*)handle;
    *pv=*pv=fast_adler32_start(0,0);
}
static void _fadler32_append(hpatch_checksumHandle handle,
                             const unsigned char* part_data,const unsigned char* part_data_end){
    hpatch_uint32_t* pv=(hpatch_uint32_t*)handle;
    *pv=fast_adler32_append(*pv,part_data,part_data_end-part_data);
}
static void _fadler32_end(hpatch_checksumHandle handle,
                         unsigned char* checksum,unsigned char* checksum_end){
    hpatch_uint32_t v=*(hpatch_uint32_t*)handle;
    assert(4==checksum_end-checksum);
    __out_checksum4(checksum,v);
}
static hpatch_TChecksum fadler32ChecksumPlugin={ _fadler32_checksumType,_fadler32_checksumByteSize,_fadler32_open,
                                                 _fadler32_close,_fadler32_begin,_fadler32_append,_fadler32_end};
#endif//_ChecksumPlugin_fadler32


#ifdef  _ChecksumPlugin_fadler64
#include "libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.h"
static const char* _fadler64_checksumType(void){
    static const char* type="fadler64";
    return type;
}
static size_t _fadler64_checksumByteSize(void){
    return sizeof(hpatch_uint64_t);
}
static hpatch_checksumHandle _fadler64_open(hpatch_TChecksum* plugin){
    return malloc(sizeof(hpatch_uint64_t));
}
static void _fadler64_close(hpatch_TChecksum* plugin,hpatch_checksumHandle handle){
    if (handle) free(handle);
}
static void  _fadler64_begin(hpatch_checksumHandle handle){
    hpatch_uint64_t* pv=(hpatch_uint64_t*)handle;
    *pv=fast_adler64_start(0,0);
}
static void _fadler64_append(hpatch_checksumHandle handle,
                            const unsigned char* part_data,const unsigned char* part_data_end){
    hpatch_uint64_t* pv=(hpatch_uint64_t*)handle;
    *pv=fast_adler64_append(*pv,part_data,part_data_end-part_data);
}
static void _fadler64_end(hpatch_checksumHandle handle,
                         unsigned char* checksum,unsigned char* checksum_end){
    hpatch_uint64_t v=*(hpatch_uint64_t*)handle;
    assert(8==checksum_end-checksum);
    __out_checksum8(checksum,v);
}
static hpatch_TChecksum fadler64ChecksumPlugin={ _fadler64_checksumType,_fadler64_checksumByteSize,_fadler64_open,
                                                 _fadler64_close,_fadler64_begin,_fadler64_append,_fadler64_end};
#endif//_ChecksumPlugin_fadler64


#ifdef  _ChecksumPlugin_fadler128
#include "libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.h"
static const char* _fadler128_checksumType(void){
    static const char* type="fadler128";
    return type;
}
static size_t _fadler128_checksumByteSize(void){
    return sizeof(adler128_t);
}
static hpatch_checksumHandle _fadler128_open(hpatch_TChecksum* plugin){
    return malloc(sizeof(adler128_t));
}
static void _fadler128_close(hpatch_TChecksum* plugin,hpatch_checksumHandle handle){
    if (handle) free(handle);
}
static void  _fadler128_begin(hpatch_checksumHandle handle){
    adler128_t* pv=(adler128_t*)handle;
    *pv=fast_adler128_start(0,0);
}
static void _fadler128_append(hpatch_checksumHandle handle,
                             const unsigned char* part_data,const unsigned char* part_data_end){
    adler128_t* pv=(adler128_t*)handle;
    *pv=fast_adler128_append(*pv,part_data,part_data_end-part_data);
}
static void _fadler128_end(hpatch_checksumHandle handle,
                          unsigned char* checksum,unsigned char* checksum_end){
    const adler128_t* pv=(const adler128_t*)handle;
    assert(16==checksum_end-checksum);
    __out_checksum8(checksum,pv->adler);
    __out_checksum8(checksum+8,pv->sum);
}
static hpatch_TChecksum fadler128ChecksumPlugin={ _fadler128_checksumType,_fadler128_checksumByteSize,
                            _fadler128_open,_fadler128_close,_fadler128_begin,_fadler128_append,_fadler128_end};
#endif//_ChecksumPlugin_fadler128


#ifdef  _ChecksumPlugin_md5
#if (_IsNeedIncludeDefaultChecksumHead)
#   include "md5.h" // https://sourceforge.net/projects/libmd5-rfc  https://github.com/sisong/libmd5
#endif
static const char* _md5_checksumType(void){
    static const char* type="md5";
    return type;
}
static size_t _md5_checksumByteSize(void){
    return 16;
}
static hpatch_checksumHandle _md5_open(hpatch_TChecksum* plugin){
    return malloc(sizeof(md5_state_t));
}
static void _md5_close(hpatch_TChecksum* plugin,hpatch_checksumHandle handle){
    if (handle) free(handle);
}
static void  _md5_begin(hpatch_checksumHandle handle){
    md5_state_t* ps=(md5_state_t*)handle;
    md5_init(ps);
}
static void _md5_append(hpatch_checksumHandle handle,
                          const unsigned char* part_data,const unsigned char* part_data_end){
    md5_state_t* ps=(md5_state_t*)handle;
    while (part_data!=part_data_end) {
        size_t dataSize=(size_t)(part_data_end-part_data);
        int  len=(int)((~(unsigned int)0)>>1);
        //assert(len>0);
        if ((unsigned int)len>dataSize)
            len=(int)dataSize;
        md5_append(ps,part_data,len);
        part_data+=len;
    }
}
static void _md5_end(hpatch_checksumHandle handle,
                       unsigned char* checksum,unsigned char* checksum_end){
    md5_state_t* ps=(md5_state_t*)handle;
    assert(16==checksum_end-checksum);
    md5_finish(ps,checksum);
}
static hpatch_TChecksum md5ChecksumPlugin={ _md5_checksumType,_md5_checksumByteSize,_md5_open,
                                            _md5_close,_md5_begin,_md5_append,_md5_end};
#endif//_ChecksumPlugin_md5


#ifdef  _ChecksumPlugin_blake3
#if (_IsNeedIncludeDefaultChecksumHead)
#   include "blake3.h" // https://github.com/BLAKE3-team/BLAKE3
#endif
static const char* _blake3_checksumType(void){
    static const char* type="blake3";
    return type;
}
static size_t _blake3_checksumByteSize(void){
    return BLAKE3_OUT_LEN;
}
static hpatch_checksumHandle _blake3_open(hpatch_TChecksum* plugin){
    return malloc(sizeof(blake3_hasher));
}
static void _blake3_close(hpatch_TChecksum* plugin,hpatch_checksumHandle handle){
    if (handle) free(handle);
}
static void  _blake3_begin(hpatch_checksumHandle handle){
    blake3_hasher* ps=(blake3_hasher*)handle;
    blake3_hasher_init(ps);
}
static void _blake3_append(hpatch_checksumHandle handle,
                           const unsigned char* part_data,const unsigned char* part_data_end){
    blake3_hasher* ps=(blake3_hasher*)handle;
    blake3_hasher_update(ps,part_data,(size_t)(part_data_end-part_data));
}
static void _blake3_end(hpatch_checksumHandle handle,
                        unsigned char* checksum,unsigned char* checksum_end){
    blake3_hasher* ps=(blake3_hasher*)handle;
    assert(BLAKE3_OUT_LEN==checksum_end-checksum);
    blake3_hasher_finalize(ps,checksum,(size_t)(checksum_end-checksum));
}
static hpatch_TChecksum blake3ChecksumPlugin={ _blake3_checksumType,_blake3_checksumByteSize,_blake3_open,
                                               _blake3_close,_blake3_begin,_blake3_append,_blake3_end};
#endif//_ChecksumPlugin_blake3


#ifdef  _ChecksumPlugin_curl_md5
#if (_IsNeedIncludeDefaultChecksumHead)
#ifdef __cplusplus
extern "C" {
#endif
#   include "curl_md5.h" // https://github.com/curl/curl/blob/master/lib/curl_md5.h
#ifdef __cplusplus
}
#endif
#endif
static const struct MD5_params* _curl_MD5_params=&Curl_DIGEST_MD5[0];
static const char* _md5_checksumType(void){
    static const char* type="md5";
    return type;
}
static size_t _md5_checksumByteSize(void){
    return _curl_MD5_params->md5_resultlen;
}
static hpatch_checksumHandle _md5_open(hpatch_TChecksum* plugin){
    return malloc(_curl_MD5_params->md5_ctxtsize);
}
static void _md5_close(hpatch_TChecksum* plugin,hpatch_checksumHandle handle){
    if (handle) free(handle);
}
static void  _md5_begin(hpatch_checksumHandle handle){
    CURLcode ret=_curl_MD5_params->md5_init_func(handle);
    assert(ret==CURLE_OK);
}
static void _md5_append(hpatch_checksumHandle handle,
                          const unsigned char* part_data,const unsigned char* part_data_end){
    while (part_data!=part_data_end) {
        size_t dataSize=(size_t)(part_data_end-part_data);
        int  len=(int)((~(unsigned int)0)>>1);
        //assert(len>0);
        if ((unsigned int)len>dataSize)
            len=(int)dataSize;
        _curl_MD5_params->md5_update_func(handle,part_data,len);
        part_data+=len;
    }
}
static void _md5_end(hpatch_checksumHandle handle,
                       unsigned char* checksum,unsigned char* checksum_end){
    assert(_curl_MD5_params->md5_resultlen==checksum_end-checksum);
    _curl_MD5_params->md5_final_func(checksum,handle);
}
static hpatch_TChecksum md5ChecksumPlugin={ _md5_checksumType,_md5_checksumByteSize,_md5_open,
                                            _md5_close,_md5_begin,_md5_append,_md5_end};
#endif//_ChecksumPlugin_curl_md5

#ifdef  _ChecksumPlugin_curl_sha256
#if (_IsNeedIncludeDefaultChecksumHead)
#ifdef __cplusplus
extern "C" {
#endif
#   include "curl_sha256.h" // https://github.com/curl/curl/blob/master/lib/curl_sha256.h
#ifdef __cplusplus
}
#endif
#endif
static const struct HMAC_params* _curl_sha256_params=&Curl_HMAC_SHA256[0];
static const char* _sha256_checksumType(void){
    static const char* type="sha256";
    return type;
}
static size_t _sha256_checksumByteSize(void){
    return _curl_sha256_params->hmac_resultlen;
}
static hpatch_checksumHandle _sha256_open(hpatch_TChecksum* plugin){
    return malloc(_curl_sha256_params->hmac_ctxtsize);
}
static void _sha256_close(hpatch_TChecksum* plugin,hpatch_checksumHandle handle){
    if (handle) free(handle);
}
static void  _sha256_begin(hpatch_checksumHandle handle){
    CURLcode ret=_curl_sha256_params->hmac_hinit(handle);
    assert(ret==CURLE_OK);
}
static void _sha256_append(hpatch_checksumHandle handle,
                           const unsigned char* part_data,const unsigned char* part_data_end){
    while (part_data!=part_data_end) {
        size_t dataSize=(size_t)(part_data_end-part_data);
        int  len=(int)((~(unsigned int)0)>>1);
        //assert(len>0);
        if ((unsigned int)len>dataSize)
            len=(int)dataSize;
        _curl_sha256_params->hmac_hupdate(handle,part_data,len);
        part_data+=len;
    }
}
static void _sha256_end(hpatch_checksumHandle handle,
                       unsigned char* checksum,unsigned char* checksum_end){
    assert(_curl_sha256_params->hmac_resultlen==checksum_end-checksum);
    _curl_sha256_params->hmac_hfinal(checksum,handle);
}
static hpatch_TChecksum sha256ChecksumPlugin={ _sha256_checksumType,_sha256_checksumByteSize,_sha256_open,
                                               _sha256_close,_sha256_begin,_sha256_append,_sha256_end};
#endif//_ChecksumPlugin_curl_sha256


#ifdef  _ChecksumPlugin_mbedtls_md5
#if (_IsNeedIncludeDefaultChecksumHead)
#ifdef __cplusplus
extern "C" {
#endif
#   include "mbedtls/md5.h" // https://github.com/ARMmbed/mbedtls/blob/master/include/mbedtls/md5.h
#ifdef __cplusplus
}
#endif
#endif
static const char* _md5_checksumType(void){
    static const char* type="md5";
    return type;
}
static size_t _md5_checksumByteSize(void){
    return 16;
}
static hpatch_checksumHandle _md5_open(hpatch_TChecksum* plugin){
    return malloc(sizeof(mbedtls_md5_context));
}
static void _md5_close(hpatch_TChecksum* plugin,hpatch_checksumHandle handle){
    if (handle) free(handle);
}
static void _md5_begin(hpatch_checksumHandle handle){
    mbedtls_md5_init((mbedtls_md5_context*)handle);
    mbedtls_md5_starts_ret((mbedtls_md5_context*)handle);
}
static void _md5_append(hpatch_checksumHandle handle,
                        const unsigned char* part_data,const unsigned char* part_data_end){
    mbedtls_md5_update_ret((mbedtls_md5_context*)handle,part_data,part_data_end-part_data);
}
static void _md5_end(hpatch_checksumHandle handle,
                     unsigned char* checksum,unsigned char* checksum_end){
    assert(16==checksum_end-checksum);
    mbedtls_md5_finish_ret((mbedtls_md5_context*)handle,checksum);
}
static hpatch_TChecksum md5ChecksumPlugin={ _md5_checksumType,_md5_checksumByteSize,_md5_open,
                                            _md5_close,_md5_begin,_md5_append,_md5_end};
#endif//_ChecksumPlugin_mbedtls_md5


#ifdef  _ChecksumPlugin_mbedtls_sha256
#if (_IsNeedIncludeDefaultChecksumHead)
#ifdef __cplusplus
extern "C" {
#endif
#   include "mbedtls/sha256.h" // https://github.com/ARMmbed/mbedtls/blob/master/include/mbedtls/sha256.h
#ifdef __cplusplus
}
#endif
#endif
static const char* _sha256_checksumType(void){
    static const char* type="sha256";
    return type;
}
static size_t _sha256_checksumByteSize(void){
    return 32;
}
static hpatch_checksumHandle _sha256_open(hpatch_TChecksum* plugin){
    return malloc(sizeof(mbedtls_sha256_context));
}
static void _sha256_close(hpatch_TChecksum* plugin,hpatch_checksumHandle handle){
    if (handle) free(handle);
}
static void  _sha256_begin(hpatch_checksumHandle handle){
    mbedtls_sha256_init((mbedtls_sha256_context*)handle);
    mbedtls_sha256_starts_ret((mbedtls_sha256_context*)handle,0);
}
static void _sha256_append(hpatch_checksumHandle handle,
                           const unsigned char* part_data,const unsigned char* part_data_end){
    mbedtls_sha256_update_ret((mbedtls_sha256_context*)handle,part_data,part_data_end-part_data);
}
static void _sha256_end(hpatch_checksumHandle handle,
                       unsigned char* checksum,unsigned char* checksum_end){
    assert(32==checksum_end-checksum);
    mbedtls_sha256_finish_ret((mbedtls_sha256_context*)handle,checksum);
}
static hpatch_TChecksum sha256ChecksumPlugin={ _sha256_checksumType,_sha256_checksumByteSize,_sha256_open,
                                               _sha256_close,_sha256_begin,_sha256_append,_sha256_end};
#endif//_ChecksumPlugin_mbedtls_sha256


#ifdef  _ChecksumPlugin_mbedtls_sha512
#if (_IsNeedIncludeDefaultChecksumHead)
#ifdef __cplusplus
extern "C" {
#endif
#   include "mbedtls/sha512.h" // https://github.com/ARMmbed/mbedtls/blob/master/include/mbedtls/sha512.h
#ifdef __cplusplus
}
#endif
#endif
static const char* _sha512_checksumType(void){
    static const char* type="sha512";
    return type;
}
static size_t _sha512_checksumByteSize(void){
    return 64;
}
static hpatch_checksumHandle _sha512_open(hpatch_TChecksum* plugin){
    return malloc(sizeof(mbedtls_sha512_context));
}
static void _sha512_close(hpatch_TChecksum* plugin,hpatch_checksumHandle handle){
    if (handle) free(handle);
}
static void  _sha512_begin(hpatch_checksumHandle handle){
    mbedtls_sha512_init((mbedtls_sha512_context*)handle);
    mbedtls_sha512_starts_ret((mbedtls_sha512_context*)handle,0);
}
static void _sha512_append(hpatch_checksumHandle handle,
                           const unsigned char* part_data,const unsigned char* part_data_end){
    mbedtls_sha512_update_ret((mbedtls_sha512_context*)handle,part_data,part_data_end-part_data);
}
static void _sha512_end(hpatch_checksumHandle handle,
                       unsigned char* checksum,unsigned char* checksum_end){
    assert(64==checksum_end-checksum);
    mbedtls_sha512_finish_ret((mbedtls_sha512_context*)handle,checksum);
}
static hpatch_TChecksum sha512ChecksumPlugin={ _sha512_checksumType,_sha512_checksumByteSize,_sha512_open,
                                               _sha512_close,_sha512_begin,_sha512_append,_sha512_end};
#endif//_ChecksumPlugin_mbedtls_sha512


#ifdef  _ChecksumPlugin_xxh3
#if (_IsNeedIncludeDefaultChecksumHead)
#ifdef __cplusplus
extern "C" {
#endif
#   include "xxh3.h" // https://github.com/Cyan4973/xxHash/blob/master/xxh3.h
#ifdef __cplusplus
}
#endif
#endif
static const char* _xxh3_checksumType(void){
    static const char* type="xxh3";
    return type;
}
static size_t _xxh3_checksumByteSize(void){
    return 8;
}
static hpatch_checksumHandle _xxh3_open(hpatch_TChecksum* plugin){
    return XXH3_createState();
}
static void _xxh3_close(hpatch_TChecksum* plugin,hpatch_checksumHandle handle){
    if (handle){
        XXH_errorcode ret=XXH3_freeState((XXH3_state_t*)handle);
        assert(ret==XXH_OK);
    }
}
static void _xxh3_begin(hpatch_checksumHandle handle){
    XXH_errorcode ret=XXH3_64bits_reset((XXH3_state_t*)handle);
    assert(ret==XXH_OK);
}
static void _xxh3_append(hpatch_checksumHandle handle,
                           const unsigned char* part_data,const unsigned char* part_data_end){
    XXH_errorcode ret=XXH3_64bits_update((XXH3_state_t*)handle,part_data,part_data_end-part_data);
    assert(ret==XXH_OK);
}
static void _xxh3_end(hpatch_checksumHandle handle,
                       unsigned char* checksum,unsigned char* checksum_end){
    assert(8==checksum_end-checksum);
    XXH64_hash_t h64=XXH3_64bits_digest((XXH3_state_t*)handle);
    XXH_writeLE64(checksum,h64);
}
static hpatch_TChecksum xxh3ChecksumPlugin={ _xxh3_checksumType,_xxh3_checksumByteSize,_xxh3_open,
                                             _xxh3_close,_xxh3_begin,_xxh3_append,_xxh3_end};
#endif//_ChecksumPlugin_xxh3


#ifdef  _ChecksumPlugin_xxh128
#if (_IsNeedIncludeDefaultChecksumHead)
#ifdef __cplusplus
extern "C" {
#endif
#   include "xxh3.h" // https://github.com/Cyan4973/xxHash/blob/master/xxh3.h
#ifdef __cplusplus
}
#endif
#endif
static const char* _xxh128_checksumType(void){
    static const char* type="xxh128";
    return type;
}
static size_t _xxh128_checksumByteSize(void){
    return 16;
}
static hpatch_checksumHandle _xxh128_open(hpatch_TChecksum* plugin){
    return XXH3_createState();
}
static void _xxh128_close(hpatch_TChecksum* plugin,hpatch_checksumHandle handle){
    if (handle){
        XXH_errorcode ret=XXH3_freeState((XXH3_state_t*)handle);
        assert(ret==XXH_OK);
    }
}
static void _xxh128_begin(hpatch_checksumHandle handle){
    XXH_errorcode ret=XXH3_128bits_reset((XXH3_state_t*)handle);
    assert(ret==XXH_OK);
}
static void _xxh128_append(hpatch_checksumHandle handle,
                           const unsigned char* part_data,const unsigned char* part_data_end){
    XXH_errorcode ret=XXH3_128bits_update((XXH3_state_t*)handle,part_data,part_data_end-part_data);
    assert(ret==XXH_OK);
}
static void _xxh128_end(hpatch_checksumHandle handle,
                       unsigned char* checksum,unsigned char* checksum_end){
    assert(16==checksum_end-checksum);
    XXH128_hash_t h128=XXH3_128bits_digest((XXH3_state_t*)handle);
    XXH_writeLE64(checksum,h128.low64);
    XXH_writeLE64(checksum+8,h128.high64);
}
static hpatch_TChecksum xxh128ChecksumPlugin={ _xxh128_checksumType,_xxh128_checksumByteSize,_xxh128_open,
                                               _xxh128_close,_xxh128_begin,_xxh128_append,_xxh128_end};
#endif//_ChecksumPlugin_xxh128

#endif
