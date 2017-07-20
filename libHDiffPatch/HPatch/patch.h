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

//hpatch_StreamPos_t for support large file
#ifdef _MSC_VER
    typedef unsigned __int64        hpatch_StreamPos_t;
#else
    typedef unsigned long long      hpatch_StreamPos_t;
#endif

    typedef void* hpatch_TStreamInputHandle;

    typedef struct hpatch_TStreamInput{
        hpatch_TStreamInputHandle streamHandle;
        hpatch_StreamPos_t        streamSize;
        //read() must return (out_data_end-out_data), otherwise error
        long                      (*read) (hpatch_TStreamInputHandle streamHandle,
                                           const hpatch_StreamPos_t readFromPos,
                                           unsigned char* out_data,unsigned char* out_data_end);
    } hpatch_TStreamInput;
    
    typedef struct hpatch_TStreamOutput{
        hpatch_TStreamInputHandle streamHandle;
        hpatch_StreamPos_t        streamSize;
        //write() must return (out_data_end-out_data), otherwise error
        //   first writeToPos==0; the next writeToPos+=(data_end-data)
        long                      (*write)(hpatch_TStreamInputHandle streamHandle,
                                           const hpatch_StreamPos_t writeToPos,
                                           const unsigned char* data,const unsigned char* data_end);
    } hpatch_TStreamOutput;
    
//once I/O (read/write) max byte size
#define kStreamCacheSize  (1024)

//patch by stream , only used 7*(kStreamCacheSize stack memory) for I/O cache
//  serializedDiff create by create_diff()
hpatch_BOOL patch_stream(const struct hpatch_TStreamOutput* out_newData,
                         const struct hpatch_TStreamInput*  oldData,
                         const struct hpatch_TStreamInput*  serializedDiff);
    
    
    
//patch_decompress() patch with decompress plugin
//  compressedDiff create by create_compress_diff()
hpatch_BOOL compressedDiffInfo(hpatch_StreamPos_t* out_newDataSize,
                               hpatch_StreamPos_t* out_oldDataSize,
                               char* out_compressType,char* out_compressType_end,
                               const struct hpatch_TStreamInput* compressedDiff);
    
    typedef void*  hpatch_decompressHandle;
    typedef struct hpatch_TDecompress{
        //error return 0.
        hpatch_decompressHandle  (*open)(struct hpatch_TDecompress* decompressPlugin,
                                         const char* compressType,
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
    
//patch with decompress, used 5*(kStreamCacheSize stack memory) + 4*(decompress used memory)
hpatch_BOOL patch_decompress(const struct hpatch_TStreamOutput* out_newData,
                             const struct hpatch_TStreamInput*  oldData,
                             const struct hpatch_TStreamInput*  compressedDiff,
                             hpatch_TDecompress* decompressPlugin);
    
#ifdef __cplusplus
}
#endif

#endif
