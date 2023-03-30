//  dict_decompress_plugin.h
//  sync_client
//  Created by housisong on 2022-12-08.
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
#ifndef dict_decompress_plugin_h
#define dict_decompress_plugin_h
#include "../../libHDiffPatch/HPatch/patch_types.h"
#ifdef __cplusplus
extern "C" {
#endif
    
    typedef void*  hsync_dictDecompressHandle;
    
    //dict decompress plugin
    typedef struct hsync_TDictDecompress{
        hpatch_BOOL                       (*is_can_open)(const char* compresseType);
        //error return 0.
        hsync_dictDecompressHandle (*dictDecompressOpen)(struct hsync_TDictDecompress* decompressPlugin,size_t blockCount,size_t blockSize,
                                                         const hpatch_byte* in_info,const hpatch_byte* in_infoEnd);
        void                      (*dictDecompressClose)(struct hsync_TDictDecompress* decompressPlugin,
                                                         hsync_dictDecompressHandle dictHandle);
        //dictDecompress() must out (out_dataEnd-out_dataBegin), otherwise error return hpatch_FALSE
        hpatch_BOOL                    (*dictDecompress)(hpatch_decompressHandle dictHandle,size_t blockIndex,
                                                         const hpatch_byte* in_code,const hpatch_byte* in_codeEnd,
                                                         hpatch_byte* out_dataBegin,hpatch_byte* out_dataEnd);
        void                           (*dictUncompress)(hpatch_decompressHandle dictHandle,size_t blockIndex,size_t lastCompressedBlockIndex,
                                                         const hpatch_byte* dataBegin,const hpatch_byte* dataEnd);
    } hsync_TDictDecompress;


    typedef struct{
        size_t          blockCount;
        size_t          block_cache_i;
        size_t          block_dec_i;
        size_t          blockSize;
        size_t          dictSize;
        hpatch_byte*    uncompress;
        hpatch_byte*    uncompressEnd;
        hpatch_byte*    uncompressCur;
    } _CacheBlockDict_t;

    #define _kMinCacheBlockDictMemSize ((1<<10)*256)
    #define _kMinCacheBlockDictSize_r  4    // dictSize/_kMinCacheBlockDictSize_r
    //return 0 or >=dictSize
    static size_t _getCacheBlockDictSizeUp(size_t dictSize,size_t kBlockCount,size_t kBlockSize,
                                           const size_t bestMinDictMemSize){
        size_t bestMem;
        size_t blockCount;
        if (kBlockCount<=1)
            return 0; //not need cache
        bestMem=dictSize+dictSize/_kMinCacheBlockDictSize_r;
        if (bestMem<_kMinCacheBlockDictMemSize)
            bestMem=_kMinCacheBlockDictMemSize;
        blockCount=(bestMem+kBlockSize-1)/kBlockSize;
        if (blockCount>=kBlockCount)
            blockCount=kBlockCount-1;
        return blockCount*kBlockSize;
    }
    
    static size_t _getCacheBlockDictSize(size_t dictSize,size_t kBlockCount,size_t kBlockSize){
        return _getCacheBlockDictSizeUp(dictSize,kBlockCount,kBlockSize,_kMinCacheBlockDictMemSize);
    }

    static 
    void _CacheBlockDict_init(_CacheBlockDict_t* self,hpatch_byte* cacheDict,size_t cacheDictSize,
                              size_t dictSize,size_t blockCount,size_t blockSize){
        memset(self,0,sizeof(*self));
        self->dictSize=dictSize;
        self->blockCount=blockCount;
        self->blockSize=blockSize;
        if (cacheDictSize){
            self->uncompress=cacheDict;
            self->uncompressCur=self->uncompress;
            self->uncompressEnd=self->uncompress+cacheDictSize;
        }
    }
    static hpatch_inline 
    hpatch_BOOL _CacheBlockDict_isHaveDict(const _CacheBlockDict_t* self){
        return (self->uncompressCur>self->uncompress);
    }
    static hpatch_inline 
    hpatch_BOOL _CacheBlockDict_isMustResetDict(const _CacheBlockDict_t* self,size_t blockIndex){
        hpatch_BOOL result=_CacheBlockDict_isHaveDict(self)&&(blockIndex>self->block_dec_i);
        assert(blockIndex>=self->block_dec_i);
        if (result) assert(blockIndex==self->block_cache_i);
        return result;
    }
    static hpatch_inline 
    void _CacheBlockDict_usedDict(_CacheBlockDict_t* self,size_t blockIndex,
                                  hpatch_byte** out_dict,size_t* out_dictSize){
        size_t dsize=self->uncompressCur-self->uncompress;
        if (dsize>self->dictSize) dsize=self->dictSize;
        *out_dict=self->uncompressCur-dsize;
        *out_dictSize=dsize;
        self->block_dec_i=blockIndex+1;
    }
    static 
    void _CacheBlockDict_dictUncompress(_CacheBlockDict_t* self,size_t blockIndex,size_t lastCompressedBlockIndex,
                                        const hpatch_byte* dataBegin,const hpatch_byte* dataEnd){
        size_t dataSize=dataEnd-dataBegin;
        if (blockIndex!=self->block_cache_i)
            self->uncompressCur=self->uncompress;
        self->block_cache_i=blockIndex+1;
        if (self->uncompress==0) return;
        hpatch_StreamPos_t distance=(hpatch_StreamPos_t)self->blockSize * (size_t)(lastCompressedBlockIndex-blockIndex-1);
        if (distance>=self->dictSize){
            self->uncompressCur=self->uncompress;
            return;
        }

        if (dataSize>(size_t)(self->uncompressEnd-self->uncompressCur)){
            if (dataSize<self->dictSize){
                size_t msize=self->dictSize-dataSize;
                memmove(self->uncompress,self->uncompressCur-msize,msize);
                self->uncompressCur=self->uncompress+msize;
            }else{
                dataBegin+=(dataSize-self->dictSize);
                dataSize=dataEnd-dataBegin;
                self->uncompressCur=self->uncompress;
            }
        }
        memcpy(self->uncompressCur,dataBegin,dataSize);
        self->uncompressCur+=dataSize;
    }

#ifdef __cplusplus
}
#endif
#endif // dict_decompress_plugin_h
