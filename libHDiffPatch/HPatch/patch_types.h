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

#include <string.h> //for size_t memset memcpy memmove
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HDIFFPATCH_VERSION_MAJOR    4
#define HDIFFPATCH_VERSION_MINOR    0
#define HDIFFPATCH_VERSION_RELEASE  4

#define _HDIFFPATCH_VERSION          HDIFFPATCH_VERSION_MAJOR.HDIFFPATCH_VERSION_MINOR.HDIFFPATCH_VERSION_RELEASE
#define _HDIFFPATCH_QUOTE(str) #str
#define _HDIFFPATCH_EXPAND_AND_QUOTE(str) _HDIFFPATCH_QUOTE(str)
#define HDIFFPATCH_VERSION_STRING   _HDIFFPATCH_EXPAND_AND_QUOTE(_HDIFFPATCH_VERSION)

#ifndef hpatch_int
    typedef int                 hpatch_int;
#endif
#ifndef hpatch_uint
    typedef unsigned int        hpatch_uint;
#endif
#ifndef hpatch_size_t
    typedef size_t              hpatch_size_t;
#endif
#ifndef hpatch_uint32_t
#ifdef _MSC_VER
#   if (_MSC_VER >= 1300)
    typedef unsigned __int32    hpatch_uint32_t;
#   else
    typedef unsigned int        hpatch_uint32_t;
#   endif
#else
    typedef unsigned int        hpatch_uint32_t;
#endif
#endif
#ifndef hpatch_uint64_t
#ifdef _MSC_VER
    typedef unsigned __int64    hpatch_uint64_t;
#else
    typedef unsigned long long  hpatch_uint64_t;
#endif
#endif
#ifndef hpatch_StreamPos_t
    typedef hpatch_uint64_t     hpatch_StreamPos_t; // file size type
#endif

#ifndef hpatch_BOOL
    typedef int                 hpatch_BOOL;
#endif
#define     hpatch_FALSE    0
#define     hpatch_TRUE     ((hpatch_BOOL)(!hpatch_FALSE))
    
#ifdef _MSC_VER
#   define hpatch_inline _inline
#else
#   define hpatch_inline inline
#endif

//PRIu64 for printf type hpatch_StreamPos_t
#ifndef PRIu64
#   ifdef _MSC_VER
#       define PRIu64 "I64u"
#   else
#       define PRIu64 "llu"
#   endif
#endif
    
#define _hpatch_align_type_lower(uint_type,p,align2pow) (((uint_type)(p)) & (~(uint_type)((align2pow)-1)))
#define _hpatch_align_lower(p,align2pow) _hpatch_align_type_lower(hpatch_size_t,p,align2pow)
#define _hpatch_align_upper(p,align2pow) _hpatch_align_lower(((hpatch_size_t)(p))+((align2pow)-1),align2pow)
    
    typedef void* hpatch_TStreamInputHandle;
    typedef void* hpatch_TStreamOutputHandle;
    
    typedef struct hpatch_TStreamInput{
        void*            streamImport;
        hpatch_StreamPos_t streamSize; //stream size,max readable range;
        //read() must read (out_data_end-out_data), otherwise error return hpatch_FALSE
        hpatch_BOOL            (*read)(const struct hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                       unsigned char* out_data,unsigned char* out_data_end);
        void*        _private_reserved;
    } hpatch_TStreamInput;
    
    typedef struct hpatch_TStreamOutput{
        void*            streamImport;
        hpatch_StreamPos_t streamSize; //stream size,max writable range; not is write pos!
        //read_writed for ReadWriteIO, can null!
        hpatch_BOOL     (*read_writed)(const struct hpatch_TStreamOutput* stream,hpatch_StreamPos_t readFromPos,
                                       unsigned char* out_data,unsigned char* out_data_end);
        //write() must wrote (out_data_end-out_data), otherwise error return hpatch_FALSE
        hpatch_BOOL           (*write)(const struct hpatch_TStreamOutput* stream,hpatch_StreamPos_t writeToPos,
                                       const unsigned char* data,const unsigned char* data_end);
    } hpatch_TStreamOutput;
    
    //default once I/O (read/write) byte size
    #ifndef hpatch_kStreamCacheSize
    #   define hpatch_kStreamCacheSize      (1024*4)
    #endif
    #ifndef hpatch_kFileIOBufBetterSize
    #   define hpatch_kFileIOBufBetterSize  (1024*64)
    #endif
    
    #ifndef hpatch_kMaxPluginTypeLength
    #   define hpatch_kMaxPluginTypeLength   259
    #endif

    typedef struct hpatch_compressedDiffInfo{
        hpatch_StreamPos_t  newDataSize;
        hpatch_StreamPos_t  oldDataSize;
        hpatch_uint         compressedCount;//need open hpatch_decompressHandle number
        char                compressType[hpatch_kMaxPluginTypeLength+1]; //ascii cstring 
    } hpatch_compressedDiffInfo;
    
    typedef void*  hpatch_decompressHandle;
    typedef struct hpatch_TDecompress{
        hpatch_BOOL        (*is_can_open)(const char* compresseType);
        //error return 0.
        hpatch_decompressHandle   (*open)(struct hpatch_TDecompress* decompressPlugin,
                                          hpatch_StreamPos_t dataSize,
                                          const struct hpatch_TStreamInput* codeStream,
                                          hpatch_StreamPos_t code_begin,
                                          hpatch_StreamPos_t code_end);//codeSize==code_end-code_begin
        hpatch_BOOL              (*close)(struct hpatch_TDecompress* decompressPlugin,
                                          hpatch_decompressHandle decompressHandle);
        //decompress_part() must out (out_part_data_end-out_part_data), otherwise error return hpatch_FALSE
        hpatch_BOOL    (*decompress_part)(hpatch_decompressHandle decompressHandle,
                                          unsigned char* out_part_data,unsigned char* out_part_data_end);
    } hpatch_TDecompress;

    
    
    void mem_as_hStreamInput(hpatch_TStreamInput* out_stream,
                             const unsigned char* mem,const unsigned char* mem_end);
    void mem_as_hStreamOutput(hpatch_TStreamOutput* out_stream,
                              unsigned char* mem,unsigned char* mem_end);
    
    hpatch_BOOL hpatch_deccompress_mem(hpatch_TDecompress* decompressPlugin,
                                       const unsigned char* code,const unsigned char* code_end,
                                       unsigned char* out_data,unsigned char* out_data_end);
    
    typedef struct{
        hpatch_TStreamInput         base;
        const hpatch_TStreamInput*  srcStream;
        hpatch_StreamPos_t          clipBeginPos;
    } TStreamInputClip;
    //clip srcStream from clipBeginPos to clipEndPos as a new StreamInput;
    void TStreamInputClip_init(TStreamInputClip* self,const hpatch_TStreamInput*  srcStream,
                               hpatch_StreamPos_t clipBeginPos,hpatch_StreamPos_t clipEndPos);
    typedef struct{
        hpatch_TStreamOutput        base;
        const hpatch_TStreamOutput* srcStream;
        hpatch_StreamPos_t          clipBeginPos;
    } TStreamOutputClip;
    //clip srcStream from clipBeginPos to clipEndPos as a new StreamInput;
    void TStreamOutputClip_init(TStreamOutputClip* self,const hpatch_TStreamOutput*  srcStream,
                                hpatch_StreamPos_t clipBeginPos,hpatch_StreamPos_t clipEndPos);

    
    #define  hpatch_kMaxPackedUIntBytes ((sizeof(hpatch_StreamPos_t)*8+6)/7+1)
    hpatch_BOOL hpatch_packUIntWithTag(unsigned char** out_code,unsigned char* out_code_end,
                                       hpatch_StreamPos_t uValue,hpatch_uint highTag,const hpatch_uint kTagBit);
    hpatch_uint hpatch_packUIntWithTag_size(hpatch_StreamPos_t uValue,const hpatch_uint kTagBit);
    #define hpatch_packUInt(out_code,out_code_end,uValue) \
                hpatch_packUIntWithTag(out_code,out_code_end,uValue,0,0)
    #define hpatch_packUInt_size(uValue) hpatch_packUIntWithTag_size(uValue,0)
    
    hpatch_BOOL hpatch_unpackUIntWithTag(const unsigned char** src_code,const unsigned char* src_code_end,
                                         hpatch_StreamPos_t* result,const hpatch_uint kTagBit);
    #define hpatch_unpackUInt(src_code,src_code_end,result) \
                hpatch_unpackUIntWithTag(src_code,src_code_end,result,0)

    
    typedef struct hpatch_TCover{
        hpatch_StreamPos_t oldPos;
        hpatch_StreamPos_t newPos;
        hpatch_StreamPos_t length;
    } hpatch_TCover;
    typedef struct hpatch_TCover32{
        hpatch_uint32_t oldPos;
        hpatch_uint32_t newPos;
        hpatch_uint32_t length;
    } hpatch_TCover32;

    typedef struct hpatch_TCovers{
        hpatch_StreamPos_t (*leave_cover_count)(const struct hpatch_TCovers* covers);
        //read out a cover,and to next cover pos; if error then return false
        hpatch_BOOL               (*read_cover)(struct hpatch_TCovers* covers,hpatch_TCover* out_cover);
        hpatch_BOOL                (*is_finish)(const struct hpatch_TCovers* covers);
        hpatch_BOOL                    (*close)(struct hpatch_TCovers* covers);
    } hpatch_TCovers;
    
    
    typedef struct{
        hpatch_StreamPos_t  newDataSize;
        hpatch_StreamPos_t  oldDataSize;
        hpatch_StreamPos_t  uncompressedSize;
        hpatch_StreamPos_t  compressedSize;
        hpatch_StreamPos_t  diffDataPos;
        hpatch_StreamPos_t  coverCount;
        hpatch_StreamPos_t  stepMemSize;
        char                compressType[hpatch_kMaxPluginTypeLength+1]; //ascii cstring
    } hpatch_singleCompressedDiffInfo;
    
    typedef struct sspatch_listener_t{ 
        void*         import;   
        hpatch_BOOL (*onDiffInfo)(struct sspatch_listener_t* listener,
                                  const hpatch_singleCompressedDiffInfo* info,
                                  hpatch_TDecompress** out_decompressPlugin,//find decompressPlugin by info->compressType
                                  unsigned char** out_temp_cache,    //*out_temp_cacheEnd-*out_temp_cache == info->stepMemSize + (I/O cache memory)
                                  unsigned char** out_temp_cacheEnd);//  note: (I/O cache memory) >= hpatch_kStreamCacheSize*3
        void        (*onPatchFinish)(struct sspatch_listener_t* listener, //onPatchFinish can null
                                     unsigned char* temp_cache, unsigned char* temp_cacheEnd);
    } sspatch_listener_t;

    typedef struct{
        hpatch_TStreamInput     base;
        hpatch_TDecompress*     _decompressPlugin;
        hpatch_decompressHandle _decompressHandle;
    } hpatch_TUncompresser_t;

    typedef struct sspatch_coversListener_t{
        void*         import;
        void        (*onStepCoversReset)(struct sspatch_coversListener_t* listener,hpatch_StreamPos_t leaveCoverCount);//can NULL, data(in covers_cache) will invalid
        void        (*onStepCovers)(struct sspatch_coversListener_t* listener,
                                    const unsigned char* covers_cache,const unsigned char* covers_cacheEnd);
    } sspatch_coversListener_t;
    
    typedef struct{
        const unsigned char* covers_cache;
        const unsigned char* covers_cacheEnd;
        hpatch_StreamPos_t   lastOldEnd;
        hpatch_StreamPos_t   lastNewEnd;
        hpatch_TCover        cover;
    } sspatch_covers_t;

    hpatch_inline static void sspatch_covers_init(sspatch_covers_t* self) { memset(self,0,sizeof(*self)); }
    hpatch_inline static void sspatch_covers_setCoversCache(sspatch_covers_t* self,const unsigned char* covers_cache,const unsigned char* covers_cacheEnd){
                                    self->covers_cache=covers_cache; self->covers_cacheEnd=covers_cacheEnd; }
    hpatch_inline static hpatch_BOOL sspatch_covers_isHaveNextCover(const sspatch_covers_t* self) { return (self->covers_cache!=(self)->covers_cacheEnd); }

    hpatch_BOOL sspatch_covers_nextCover(sspatch_covers_t* self);
    
#ifdef __cplusplus
}
#endif
#endif
