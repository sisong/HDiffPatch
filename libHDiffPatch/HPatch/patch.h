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
extern "C"
{
#endif
    
#define hpatch_BOOL   int
#define hpatch_FALSE  0

//if patch false return hpatch_FALSE
hpatch_BOOL patch(unsigned char* out_newData,unsigned char* out_newData_end,
                  const unsigned char* oldData,const unsigned char* oldData_end,
                  const unsigned char* serializedDiff,const unsigned char* serializedDiff_end);

#ifdef __cplusplus
}
#endif

    
//patch_stream()  patch by stream , recommended use in limited memory systems
#ifdef __cplusplus
extern "C"
{
#endif

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
        long                      (*read) (hpatch_TStreamInputHandle streamHandle,
                                           const hpatch_StreamPos_t readFromPos,
                                           unsigned char* out_data,unsigned char* out_data_end);
                                           //read() must return (out_data_end-out_data), otherwise read error
    } hpatch_TStreamInput;
    
    typedef struct hpatch_TStreamOutput{
        hpatch_TStreamInputHandle streamHandle;
        hpatch_StreamPos_t        streamSize;
        long                      (*write)(hpatch_TStreamInputHandle streamHandle,
                                           const hpatch_StreamPos_t writeToPos,
                                           const unsigned char* data,const unsigned char* data_end);
                                           //write() must return (out_data_end-out_data), otherwise write error
                                           //first writeToPos==0; the next writeToPos+=(data_end-data)
    } hpatch_TStreamOutput;
    
//once I/O (read/write) max byte size
#define kStreamCacheSize  (1024)

//patch by stream , only used 7*kStreamCacheSize stack memory for I/O cache
hpatch_BOOL patch_stream(const struct hpatch_TStreamOutput* out_newData,
                         const struct hpatch_TStreamInput*  oldData,
                         const struct hpatch_TStreamInput*  serializedDiff);

#ifdef __cplusplus
}
#endif

#endif
