//  compress_parallel.h
//  parallel compress for HDiffz
/*
 The MIT License (MIT)
 Copyright (c) 2019 HouSisong
 
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

#ifndef HDiff_compress_parallel_h
#define HDiff_compress_parallel_h
#include "libHDiffPatch/HDiff/diff_types.h"
#include "libParallel/parallel_import.h"
#if (_IS_USED_MULTITHREAD)
#ifdef __cplusplus
extern "C" {
#endif
    
    typedef void* hdiff_compressBlockHandle;
    typedef struct hdiff_TParallelCompress{
        void*                                    import;
        hpatch_StreamPos_t          (*maxCompressedSize)(hpatch_StreamPos_t dataSize);
        hdiff_compressBlockHandle (*openBlockCompressor)(struct hdiff_TParallelCompress* pc);
        void                     (*closeBlockCompressor)(struct hdiff_TParallelCompress* pc,
                                                         hdiff_compressBlockHandle blockCompressor);
        //compressBlock() called multiple times by thread
        size_t  (*compressBlock)(struct hdiff_TParallelCompress* pc,hdiff_compressBlockHandle blockCompressor,
                                 hpatch_StreamPos_t blockIndex,unsigned char* out_code,unsigned char* out_codeEnd,
                                 const unsigned char* block_data,const unsigned char* block_dataEnd);
    } hdiff_TParallelCompress;
    
    hpatch_StreamPos_t parallel_compress_blocks(hdiff_TParallelCompress* pc,
                                                int threadNum,size_t blockSize,
                                                const hdiff_TStreamOutput* out_code,
                                                const hdiff_TStreamInput*  in_data);
#ifdef __cplusplus
}
#endif
#endif
#endif
