//  dict_compress_plugin.h
//  sync_make
//  Created by housisong on 2020-03-08.
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
        size_t                  (*maxCompressedSize)(size_t in_dataSize);
        hsync_dictCompressHandle (*dictCompressOpen)(const struct hsync_TDictCompress* dictCompressPlugin);
        void                    (*dictCompressClose)(const struct hsync_TDictCompress* dictCompressPlugin,
                                                     hsync_dictCompressHandle dictHandle);
        size_t                       (*dictCompress)(hsync_dictCompressHandle dictHandle,hpatch_byte* out_code,hpatch_byte* out_codeEnd,
                                                     const hpatch_byte* in_data,const hpatch_byte* in_dictEnd,const hpatch_byte* in_dataEnd);
    } hsync_TDictCompress;

#ifdef __cplusplus
}
#endif
#endif // dict_compress_plugin_h
