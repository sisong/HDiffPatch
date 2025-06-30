//  dict_compress_plugin.h
//  sync_make
//  Created by housisong on 2020-03-08.
/*
 The MIT License (MIT)
 Copyright (c) 2020-2023 HouSisong
 
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
#ifndef dict_compress_plugin_h
#define dict_compress_plugin_h
#include "../../libHDiffPatch/HPatch/patch_types.h"
#ifdef __cplusplus
extern "C" {
#endif
    
    typedef void*  hsync_dictCompressHandle;
    
    //dict compress plugin
    typedef struct hsync_TDictCompress{
        //return type tag; strlen(result)<=hpatch_kMaxPluginTypeLength; (Note:result lifetime)
        const char*                  (*compressType)(void);//ascii cstring,cannot contain '&'
        //return the max compressed size, if input dataSize data;
        hpatch_StreamPos_t      (*maxCompressedSize)(hpatch_StreamPos_t in_dataSize);
        size_t                (*limitDictSizeByData)(struct hsync_TDictCompress* compressPlugin,size_t blockCount,size_t blockSize);
        size_t              (*getBestWorkBlockCount)(struct hsync_TDictCompress* compressPlugin,size_t blockCount,
                                                     size_t blockSize,size_t defaultWorkBlockCount);
        size_t                        (*getDictSize)(struct hsync_TDictCompress* compressPlugin);
        hsync_dictCompressHandle (*dictCompressOpen)(struct hsync_TDictCompress* compressPlugin,size_t blockCount,size_t blockSize);
        void                    (*dictCompressClose)(struct hsync_TDictCompress* compressPlugin,
                                                     hsync_dictCompressHandle dictHandle);
        //if not need send info to decompressor, dictCompressInfo can NULL
        size_t                   (*dictCompressInfo)(const struct hsync_TDictCompress* compressPlugin,hsync_dictCompressHandle dictHandle,
                                                     hpatch_byte* out_info,hpatch_byte* out_infoEnd);
        hpatch_byte*           (*getResetDictBuffer)(hsync_dictCompressHandle dictHandle,size_t blockIndex,
                                                     size_t* out_dictSize);
        size_t                       (*dictCompress)(hsync_dictCompressHandle dictHandle,size_t blockIndex,
                                                     hpatch_byte* out_code,hpatch_byte* out_codeEnd,
                                                     const hpatch_byte* in_dataBegin,size_t in_dataSize,size_t in_borderSize);
        const char*        (*compressTypeForDisplay)(void);//like compressType but just for display,can NULL
        size_t              (*getDictCompressBorder)(void);//if 0,can NULL
    } hsync_TDictCompress;

    #define  kDictCompressCancel   (~(size_t)0)
    #define  kDictCompressError     ((size_t)0)

    
    static hpatch_byte* _CacheBlockDict_getResetDictBuffer(_CacheBlockDict_t* self,size_t blockIndex,
                                                           size_t* out_dictSize){
        size_t dictSize=self->dictSize;
        hpatch_StreamPos_t prefixSize=((hpatch_StreamPos_t)blockIndex)*self->blockSize;
        assert(self->uncompress);
        assert(prefixSize>=0);
        if (dictSize>prefixSize) dictSize=(size_t)prefixSize;
        self->block_cache_i=blockIndex;
        *out_dictSize=dictSize;
        self->uncompressCur=self->uncompress+dictSize;
        return self->uncompress;
    }

#ifdef __cplusplus
}
#endif
#endif // dict_compress_plugin_h
