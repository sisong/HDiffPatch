//  patch_types.h
//
/*
 The MIT License (MIT)
 Copyright (c) 2012-2018 HouSisong
 
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

#ifndef HPatch_patch_types_h
#define HPatch_patch_types_h

#include <string.h> //for size_t memset memcpy
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HDIFFPATCH_VERSION_MAJOR    2
#define HDIFFPATCH_VERSION_MINOR    5
#define HDIFFPATCH_VERSION_RELEASE  1

#define _HDIFFPATCH_VERSION          HDIFFPATCH_VERSION_MAJOR.HDIFFPATCH_VERSION_MINOR.HDIFFPATCH_VERSION_RELEASE
#define _HDIFFPATCH_QUOTE(str) #str
#define _HDIFFPATCH_EXPAND_AND_QUOTE(str) _HDIFFPATCH_QUOTE(str)
#define HDIFFPATCH_VERSION_STRING   _HDIFFPATCH_EXPAND_AND_QUOTE(_HDIFFPATCH_VERSION)
    
#if defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
#   include <stdint.h> //for uint32_t,uint64_t
    typedef uint32_t            hpatch_uint32_t;
    typedef uint64_t            hpatch_StreamPos_t;
#else
#   ifdef _MSC_VER
#       if (_MSC_VER >= 1300)
    typedef unsigned __int32    hpatch_uint32_t;
#       else
    typedef unsigned int        hpatch_uint32_t;
#       endif
    typedef unsigned __int64    hpatch_StreamPos_t;
#   else
    typedef unsigned int        hpatch_uint32_t;
    typedef unsigned long long  hpatch_StreamPos_t;
#   endif
#endif
    
#ifdef _MSC_VER
#   define hpatch_inline _inline
#else
#   define hpatch_inline inline
#endif
    
#define _hpatch_align_lower(p,align2pow) (((size_t)(p)) & (~(size_t)((align2pow)-1)))
#define _hpatch_align_upper(p,align2pow) _hpatch_align_lower(((size_t)(p))+((align2pow)-1),align2pow)
    
    typedef void* hpatch_TStreamInputHandle;
    typedef void* hpatch_TStreamOutputHandle;
    
    typedef struct hpatch_TStreamInput{
        hpatch_TStreamInputHandle streamHandle;
        hpatch_StreamPos_t        streamSize;
        //read() must return (out_data_end-out_data), otherwise error
        long                      (*read) (hpatch_TStreamInputHandle streamHandle,
                                           const hpatch_StreamPos_t readFromPos,
                                           unsigned char* out_data,unsigned char* out_data_end);
    } hpatch_TStreamInput;
    
    typedef struct hpatch_TStreamOutput{
        hpatch_TStreamOutputHandle streamHandle;
        hpatch_StreamPos_t         streamSize;
        //write() must return (out_data_end-out_data), otherwise error
        //   first writeToPos==0; the next writeToPos+=(data_end-data)
        long                       (*write)(hpatch_TStreamOutputHandle streamHandle,
                                            const hpatch_StreamPos_t writeToPos,
                                            const unsigned char* data,const unsigned char* data_end);
    } hpatch_TStreamOutput;
    

    #define hpatch_kMaxCompressTypeLength   256
    
    typedef struct hpatch_compressedDiffInfo{
        hpatch_StreamPos_t  newDataSize;
        hpatch_StreamPos_t  oldDataSize;
        int                 compressedCount;//need open hpatch_decompressHandle number
        char                compressType[hpatch_kMaxCompressTypeLength+1];
    } hpatch_compressedDiffInfo;
    
    typedef void*  hpatch_decompressHandle;
    typedef struct hpatch_TDecompress{
        int                (*is_can_open)(struct hpatch_TDecompress* decompressPlugin,
                                          const hpatch_compressedDiffInfo* compressedDiffInfo);
        //error return 0.
        hpatch_decompressHandle  (*open)(struct hpatch_TDecompress* decompressPlugin,
                                         hpatch_StreamPos_t dataSize,
                                         const struct hpatch_TStreamInput* codeStream,
                                         hpatch_StreamPos_t code_begin,
                                         hpatch_StreamPos_t code_end);//codeSize==code_end-code_begin
        void                     (*close)(struct hpatch_TDecompress* decompressPlugin,
                                          hpatch_decompressHandle decompressHandle);
        //decompress_part() must return (out_part_data_end-out_part_data), otherwise error
        long           (*decompress_part)(const struct hpatch_TDecompress* decompressPlugin,
                                          hpatch_decompressHandle decompressHandle,
                                          unsigned char* out_part_data,unsigned char* out_part_data_end);
    } hpatch_TDecompress;

    
    
    
    void mem_as_hStreamInput(hpatch_TStreamInput* out_stream,
                             const unsigned char* mem,const unsigned char* mem_end);
    void mem_as_hStreamOutput(hpatch_TStreamOutput* out_stream,
                              unsigned char* mem,unsigned char* mem_end);
    
    #define hpatch_BOOL   int
    #define hpatch_FALSE  ((int)0)
    #define hpatch_TRUE   ((int)(!hpatch_FALSE))
    
    #define  hpatch_kMaxPackedUIntBytes ((sizeof(hpatch_StreamPos_t)*8+6)/7+1)
    hpatch_BOOL hpatch_packUIntWithTag(unsigned char** out_code,unsigned char* out_code_end,
                                       hpatch_StreamPos_t uValue,unsigned int highTag,const unsigned int kTagBit);
    unsigned int hpatch_packUIntWithTag_size(hpatch_StreamPos_t uValue,const unsigned int kTagBit);
    #define hpatch_packUInt(out_code,out_code_end,uValue) \
                hpatch_packUIntWithTag(out_code,out_code_end,uValue,0,0)

    hpatch_BOOL hpatch_unpackUIntWithTag(const unsigned char** src_code,const unsigned char* src_code_end,
                                         hpatch_StreamPos_t* result,const unsigned int kTagBit);
    #define hpatch_unpackUInt(src_code,src_code_end,result) \
                hpatch_unpackUIntWithTag(src_code,src_code_end,result,0)

    
    typedef struct hpatch_TCover{
        hpatch_StreamPos_t oldPos;
        hpatch_StreamPos_t newPos;
        hpatch_StreamPos_t length;
    } hpatch_TCover;

    typedef struct hpatch_TCovers{
        hpatch_StreamPos_t (*leave_cover_count)(const struct hpatch_TCovers* covers);
        //read out a cover,and to next cover pos; if error then return false
        hpatch_BOOL               (*read_cover)(struct hpatch_TCovers* covers,hpatch_TCover* out_cover);
        hpatch_BOOL                (*is_finish)(const struct hpatch_TCovers* covers);
        void                           (*close)(struct hpatch_TCovers* covers);
    } hpatch_TCovers;
    
    
    //byte rle type , ctrl code: high 2bit + packedLen(6bit+...)
    typedef enum TByteRleType{
        kByteRleType_rle0  = 0,    //00 rle 0  , data code:0 byte
        kByteRleType_rle255= 1,    //01 rle 255, data code:0 byte
        kByteRleType_rle   = 2,    //10 rle x(1--254), data code:1 byte (save x)
        kByteRleType_unrle = 3     //11 n byte data, data code:n byte(save no rle data)
    } TByteRleType;
    
    static const int kByteRleType_bit=2;
    
#ifdef __cplusplus
}
#endif
#endif
