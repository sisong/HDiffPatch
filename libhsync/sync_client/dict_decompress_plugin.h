//  dict_decompress_plugin.h
//  sync_client
//  Created by housisong on 2022-12-08.
/*
 The MIT License (MIT)
 Copyright (c) 2020-2022 HouSisong
 
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
        hsync_dictDecompressHandle (*dictDecompressOpen)(struct hsync_TDictDecompress* decompressPlugin);
        void                      (*dictDecompressClose)(struct hsync_TDictDecompress* decompressPlugin,
                                                         hsync_dictDecompressHandle dictHandle);
        //dictDecompress() must out (out_dataEnd-out_dataBegin), otherwise error return hpatch_FALSE
        hpatch_BOOL                    (*dictDecompress)(hpatch_decompressHandle dictHandle,
                                                         const unsigned char* in_code,const unsigned char* in_codeEnd,
                                                         const unsigned char* in_dict,unsigned char* in_dictEnd_and_out_dataBegin,
                                                         unsigned char* out_dataEnd,hpatch_BOOL dict_isReset,hpatch_BOOL out_isEnd);
    } hsync_TDictDecompress;

#ifdef __cplusplus
}
#endif
#endif // dict_decompress_plugin_h
