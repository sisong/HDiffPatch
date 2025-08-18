//  sync_make_private.h
//  sync_make
/*
 The MIT License (MIT)
 Copyright (c) 2019-2025 HouSisong
 
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
#ifndef hsync_make_private_h
#define hsync_make_private_h
#include "../../libHDiffPatch/HPatch/checksum_plugin.h"
#include "../../libHDiffPatch/HDiff/diff_types.h"
#include "../../dirDiffPatch/dir_diff/dir_diff_tools.h"
#include "../sync_client/sync_client_type.h"
#include "../sync_client/match_in_types.h"
#include "dict_compress_plugin.h"
#include "hsynz_plugin.h"
namespace sync_private{

struct _TCompress;
struct _TCreateDatas;
struct TWorkBuf;

struct _ICreateSync_by{
    tm_roll_uint     (*roll_hash_start)(const adler_data_t* pdata,size_t n);
    tm_roll_uint (*toSavedPartRollHash)(tm_roll_uint rollHash,size_t savedRollHashBits);
    void           (*checkChecksumInit)(unsigned char* checkChecksumBuf,size_t checksumByteSize);
    void     (*checkChecksumAppendData)(unsigned char* checkChecksumBuf,uint32_t checksumIndex,
                                        hpatch_TChecksum* checkChecksumPlugin,hpatch_checksumHandle checkChecksum,
                                        const unsigned char* blockChecksum,const unsigned char* blockData,size_t blockDataSize);
    void          (*checkChecksumEndTo)(unsigned char* dst,unsigned char* checkChecksumBuf,
                                        hpatch_TChecksum* checkChecksumPlugin,hpatch_checksumHandle checkChecksum);
    void       (*create_sync_data_part)(_TCreateDatas& cd,TWorkBuf* workData,
                                        hdiff_private::CChecksum& checksumBlockData,_TCompress& compress,void* _mt);
    
};

void _create_sync_data_by(_ICreateSync_by* cs_by,TNewDataSyncInfo* newSyncInfo,
                          const hpatch_TStreamInput* newData,const hpatch_TStreamOutput* out_hsynz,
                          hsync_TDictCompress* compressPlugin,hsync_THsynz* hsynzPlugin,size_t threadNum);

void _private_create_sync_data(TNewDataSyncInfo* newSyncInfo, const hpatch_TStreamInput*  newData,
                               const hpatch_TStreamOutput* out_hsyni, const hpatch_TStreamOutput* out_hsynz,
                               hsync_TDictCompress* compressPlugin, hsync_THsynz* hsynzPlugin,size_t threadNum);
               
}
#endif // hsync_make_private_h
