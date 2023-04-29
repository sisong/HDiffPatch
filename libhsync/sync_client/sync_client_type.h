//  sync_client_type.h
//  sync_client
//  Created by housisong on 2019-09-17.
/*
 The MIT License (MIT)
 Copyright (c) 2019-2020 HouSisong
 
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
#ifndef sync_client_type_h
#define sync_client_type_h
#include "../../libHDiffPatch/HPatch/patch_types.h"
#include "../../libHDiffPatch/HPatch/checksum_plugin.h"
#include "../../dirDiffPatch/dir_patch/dir_patch_types.h"
#include "dict_decompress_plugin.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

static const size_t kDecompressInfoMaxSize = 32;
static const size_t _kNeedMinRollHashBits   = 8;
static const size_t _kNeedMinStrongHashBits = 16;
static const size_t kSafeHashClashBit_min   = 14; //for safe
static const uint32_t _kSyncBlockSize_min_limit = 64;
static const uint32_t _kStrongChecksumByteSize_max_limit=1024*64/8;

hpatch_inline static
hpatch_StreamPos_t getSyncBlockCount(hpatch_StreamPos_t newDataSize,uint32_t kSyncBlockSize){
    return (newDataSize+(kSyncBlockSize-1))/kSyncBlockSize; }


typedef struct TSameNewBlockPair{
    uint32_t  curIndex;
    uint32_t  sameIndex; // sameIndex < curIndex;
} TSameNewBlockPair;


#if (_IS_NEED_DIR_DIFF_PATCH)
typedef struct{
    size_t                  dir_newPathCount;
    uint8_t                 dir_newNameList_isCString;
    const void*             dir_utf8NewNameList;//is const char** or const std::string* type
    const char*             dir_utf8NewRootPath;
    hpatch_StreamPos_t*     dir_newSizeList;
    size_t                  dir_newPathSumCharSize;
    size_t                  dir_newExecuteCount;
    size_t*                 dir_newExecuteIndexList;
} TNewDataSyncInfo_dir;
#endif

typedef struct{
    const char*             compressType;
    const char*             strongChecksumType;
    size_t                  kStrongChecksumByteSize;
    size_t                  savedStrongChecksumByteSize;
    size_t                  savedRollHashByteSize;
    size_t                  savedStrongChecksumBits;
    size_t                  savedRollHashBits;
    size_t                  dictSize;
    uint32_t                kSyncBlockSize;
    uint32_t                samePairCount;
    uint8_t                 isDirSyncInfo;
    uint8_t                 decompressInfoSize;
    uint8_t                 decompressInfo[kDecompressInfoMaxSize];
    hpatch_StreamPos_t      newDataSize;      // newData version size;
    hpatch_StreamPos_t      newSyncDataSize;  // .hsynz size ,saved newData or complessed newData
    hpatch_StreamPos_t      newSyncDataOffsert;
    hpatch_StreamPos_t      newSyncInfoSize;  // .hsyni size ,saved newData's info
    hpatch_StreamPos_t      infoChecksumEndPos; //saved infoChecksum data end pos in stream
    unsigned char*          savedNewDataCheckChecksum; // out new data's strongChecksum's checksum
    unsigned char*          infoChecksum; // this info data's strongChecksum
    unsigned char*          infoPartHashChecksum; // this info data's strongChecksum
    TSameNewBlockPair*      samePairList;
    uint32_t*               savedSizes;
    uint8_t*                rollHashs;
    uint8_t*                partChecksums;
#if (_IS_NEED_DIR_DIFF_PATCH)
    TNewDataSyncInfo_dir    dirInfo;
    size_t                  dirInfoSavedSize;
#endif
    hpatch_TChecksum*       _strongChecksumPlugin;
    hsync_TDictDecompress*  _decompressPlugin;
    void*                   _import;
} TNewDataSyncInfo;

#define _bitsToBytes(bits) (((bits)+7)>>3)

struct TNeedSyncInfos;
typedef void (*TSync_getBlockInfoByIndex)(const struct TNeedSyncInfos* needSyncInfos,uint32_t blockIndex,
                                          hpatch_BOOL* out_isNeedSync,uint32_t* out_syncSize);
typedef struct TNeedSyncInfos{
    hpatch_StreamPos_t          newDataSize;     // new data size
    hpatch_StreamPos_t          newSyncDataSize; // new data size or .hsynz file size
    hpatch_StreamPos_t          newSyncInfoSize; // .hsyni file size
    hpatch_StreamPos_t          localDiffDataSize; // local diff data
    hpatch_StreamPos_t          needSyncSumSize; // all need download from new data or .hsynz file
    uint32_t                    kSyncBlockSize;
    uint32_t                    blockCount;
    uint32_t                    needSyncBlockCount;
    TSync_getBlockInfoByIndex   getBlockInfoByIndex;
    void*                       import; //private
} TNeedSyncInfos;

size_t TNeedSyncInfos_getNextRanges(const TNeedSyncInfos* nsi,hpatch_StreamPos_t* dstRanges,size_t maxGetRangeLen,
                                    uint32_t* curBlockIndex,hpatch_StreamPos_t* curPosInNewSyncData);
static hpatch_inline
size_t TNeedSyncInfos_getRangeCount(const TNeedSyncInfos* nsi,
                                    uint32_t curBlockIndex,hpatch_StreamPos_t curPosInNewSyncData){
    return TNeedSyncInfos_getNextRanges(nsi,0,~(size_t)0,&curBlockIndex,&curPosInNewSyncData); }

typedef struct IReadSyncDataListener{
    void*       readSyncDataImport;
    //onNeedSyncInfo can null
    void        (*onNeedSyncInfo)   (struct  IReadSyncDataListener* listener,const TNeedSyncInfos* needSyncInfo);
    //readSyncDataBegin can null
    hpatch_BOOL (*readSyncDataBegin)(struct  IReadSyncDataListener* listener,const TNeedSyncInfos* needSyncInfo,
                                     uint32_t blockIndex,hpatch_StreamPos_t posInNewSyncData,hpatch_StreamPos_t posInNeedSyncData);
    //download range data
    hpatch_BOOL (*readSyncData)     (struct IReadSyncDataListener* listener,uint32_t blockIndex,
                                     hpatch_StreamPos_t posInNewSyncData,hpatch_StreamPos_t posInNeedSyncData,
                                     unsigned char* out_syncDataBuf,uint32_t syncDataSize);
    //readSyncDataEnd can null
    void        (*readSyncDataEnd)  (struct IReadSyncDataListener* listener);
} IReadSyncDataListener;

typedef enum TSyncDiffType{
    kSyncDiff_default=0, // out diff (info+data)
    kSyncDiff_info,      // out diff info, for optimize continue speed
    kSyncDiff_data,      // out diff data, for test download file size; NOTE: now only support run with single thread
} TSyncDiffType;
#define _kSyncDiff_TYPE_MAX_ kSyncDiff_data

#ifdef __cplusplus
}
#endif
#endif //sync_client_type_h
