//decompress_plugin_demo.h
//  decompress plugin demo for HDiffz\HPatchz
/*
 The MIT License (MIT)
 Copyright (c) 2012-2017 HouSisong
 
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
#ifndef HPatch_decompress_plugin_demo_h
#define HPatch_decompress_plugin_demo_h
//decompress plugin demo:
//  hpatch_TDeompress zlibDecompressPlugin;
//  hpatch_TDeompress bz2DecompressPlugin;
//  hpatch_TDeompress lzmaDecompressPlugin;

#include "libHDiffPatch/HPatch/patch.h"

#ifdef  _CompressPlugin_zlib
#include "zlib.h"
#endif//_CompressPlugin_zlib
    
#ifdef  _CompressPlugin_bz2
#include "bzlib.h"
#endif//_CompressPlugin_bz2
    
#ifdef  _CompressPlugin_lzma
#include "../lzma/C/LzmaDec.h"
#endif//_CompressPlugin_lzma

#endif
