//patch.h
//
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

#ifndef HPatch_patch_h
#define HPatch_patch_h
#include "patch_types.h"

#ifdef __cplusplus
extern "C" {
#endif
    
#define hpatch_BOOL   int
#define hpatch_FALSE  0

//if patch() false return hpatch_FALSE
//  serializedDiff create by create_diff()
hpatch_BOOL patch(unsigned char* out_newData,unsigned char* out_newData_end,
                  const unsigned char* oldData,const unsigned char* oldData_end,
                  const unsigned char* serializedDiff,const unsigned char* serializedDiff_end);


//patch_stream()  patch by stream , recommended use in limited memory systems

//once I/O (read/write) max byte size
#define hpatch_kStreamCacheSize  (1024)

//patch by stream , only used 7*(hpatch_kStreamCacheSize stack memory) for I/O cache
//  serializedDiff create by create_diff()
hpatch_BOOL patch_stream(const struct hpatch_TStreamOutput* out_newData,
                         const struct hpatch_TStreamInput*  oldData,
                         const struct hpatch_TStreamInput*  serializedDiff);
    

    
//patch_decompress() patch with decompress plugin
    
    //hpatch_kNodecompressPlugin is pair of hdiff_kNocompressPlugin
    #define hpatch_kNodecompressPlugin ((hpatch_TDecompress*)0)

//compressedDiff created by create_compressed_diff()
hpatch_BOOL getCompressedDiffInfo(hpatch_compressedDiffInfo* out_diffInfo,
                                  const struct hpatch_TStreamInput* compressedDiff);
inline static hpatch_BOOL
    getCompressedDiffInfo_mem(hpatch_compressedDiffInfo* out_diffInfo,
                              const unsigned char* compressedDiff,
                              const unsigned char* compressedDiff_end){
        hpatch_TStreamInput  diffStream;
        memory_as_inputStream(&diffStream,compressedDiff,compressedDiff_end);
        return getCompressedDiffInfo(out_diffInfo,&diffStream);
    }

    
//patch with decompress, used 5*(hpatch_kStreamCacheSize stack memory) + 4*(decompress used memory)
hpatch_BOOL patch_decompress(const struct hpatch_TStreamOutput* out_newData,
                             const struct hpatch_TStreamInput*  oldData,
                             const struct hpatch_TStreamInput*  compressedDiff,
                             hpatch_TDecompress* decompressPlugin);
inline static hpatch_BOOL
    patch_decompress_mem(unsigned char* out_newData,unsigned char* out_newData_end,
                         const unsigned char* oldData,const unsigned char* oldData_end,
                         const unsigned char* compressedDiff,const unsigned char* compressedDiff_end,
                         hpatch_TDecompress* decompressPlugin){
        hpatch_TStreamOutput newStream;
        hpatch_TStreamInput  oldStream;
        hpatch_TStreamInput  diffStream;
        memory_as_outputStream(&newStream,out_newData,out_newData_end);
        memory_as_inputStream(&oldStream,oldData,oldData_end);
        memory_as_inputStream(&diffStream,compressedDiff,compressedDiff_end);
        return patch_decompress(&newStream,&oldStream,&diffStream,decompressPlugin);
}

#ifdef __cplusplus
}
#endif

#endif
