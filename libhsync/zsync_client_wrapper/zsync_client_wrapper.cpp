// zsync_client_wrapper.h
// zsync_client for hsynz
/*
 The MIT License (MIT)
 Copyright (c) 2025 HouSisong
 
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
#include "zsync_client_wrapper.h"
#include "zsync_client_type_private.h"
#include "zsync_info_client.h"
#include "zsync_match_in_old.h"
#include "../sync_client/sync_client_private.h"
#include "../sync_client/sync_diff_data.h"
#include "../../libHDiffPatch/HPatch/patch_private.h"
#include "../../file_for_patch.h"
#include <stdexcept>
namespace sync_private{

#define check(v,errorCode) \
            do{ if (!(v)) { if (result==kSyncClient_ok) result=errorCode; \
                            if (!_inClear) goto clear; } }while(0)

void z_checkChecksumInit(unsigned char* checkChecksumBuf,size_t kStrongChecksumByteSize){
    zsync_checkChecksum_t* self=(zsync_checkChecksum_t*)(checkChecksumBuf+2*kStrongChecksumByteSize);
    assert((kStrongChecksumByteSize%sizeof(self->checkBlockIndex))==0);
    self->checkBlockIndex=0;
    self->state=0; //not run
}

void z_checkChecksumAppendData(unsigned char* checkChecksumBuf,uint32_t checksumIndex,
                               hpatch_TChecksum* checkChecksumPlugin,hpatch_checksumHandle checkChecksum,
                               const unsigned char* blockChecksum,const unsigned char* blockData,size_t blockDataSize){
    const size_t kStrongChecksumByteSize=checkChecksumPlugin->checksumByteSize();
    zsync_checkChecksum_t* self=(zsync_checkChecksum_t*)(checkChecksumBuf+2*kStrongChecksumByteSize);
    assert(self->checkBlockIndex<=checksumIndex); //must check by order
    self->checkBlockIndex++;
    if (self->state==0){
        assert(checksumIndex==0);
        self->state=1; //checking
        checkChecksumPlugin->begin(checkChecksum);
    }
    checkChecksumPlugin->append(checkChecksum,blockData,blockData+blockDataSize);
}

void z_checkChecksumEndTo(unsigned char* dst,unsigned char* checkChecksumBuf,
                          hpatch_TChecksum* checkChecksumPlugin,hpatch_checksumHandle checkChecksum){
    const size_t kStrongChecksumByteSize=checkChecksumPlugin->checksumByteSize();
    zsync_checkChecksum_t* self=(zsync_checkChecksum_t*)(checkChecksumBuf+2*kStrongChecksumByteSize);
    unsigned char* checksum=checkChecksumBuf+kStrongChecksumByteSize;
    if (self->state==0){
        self->state=1;
        checkChecksumPlugin->begin(checkChecksum);
    }
    if (self->state==1){
        checkChecksumPlugin->end(checkChecksum,checksum,checksum+kStrongChecksumByteSize);
        self->state=2; //end
    }
    assert(self->state==2);
    memcpy(dst,checksum,kStrongChecksumByteSize);
}


static TSyncClient_resultType z_writeToNewOrDiff(_TWriteDatas& wd) {
    _IWriteToNewOrDiff_by wr_by={0};
    wr_by.checkChecksumAppendData=
        ((const TNewDataZsyncInfo*)wd.newSyncInfo)->fileChecksumPlugin?z_checkChecksumAppendData:0;
    return _writeToNewOrDiff_by(&wr_by,wd);
}

TSyncClient_resultType
    _zsync_patch(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,TSyncDiffData* diffData,
                 const hpatch_TStreamInput* oldStream,const TNewDataZsyncInfo* newSyncInfo,
                 const hpatch_TStreamOutput* out_newStream,const hpatch_TStreamInput* newDataContinue,
                 const hpatch_TStreamOutput* out_diffStream,TSyncDiffType diffType,
                 const hpatch_TStreamInput* diffContinue,int threadNum){
    _ISyncPatch_by sp_by={0};
    sp_by.matchNewDataInOld=z_matchNewDataInOld;
    sp_by.writeToNewOrDiff=z_writeToNewOrDiff;
    sp_by.checkChecksumInit=newSyncInfo->fileChecksumPlugin?z_checkChecksumInit:0;
    sp_by.checkChecksumEndTo=newSyncInfo->fileChecksumPlugin?z_checkChecksumEndTo:0;
    return _sync_patch_by(&sp_by, listener,syncDataListener,diffData,oldStream,newSyncInfo,
                          out_newStream,newDataContinue,out_diffStream,diffType,diffContinue,threadNum);
}

}//end namespace sync_private
using namespace sync_private;

TSyncClient_resultType zsync_patch(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                                   const hpatch_TStreamInput* oldStream,const TNewDataZsyncInfo* newSyncInfo,
                                   const hpatch_TStreamOutput* out_newStream,const hpatch_TStreamInput* newDataContinue,
                                   const hpatch_TStreamOutput* out_diffInfoStream,const hpatch_TStreamInput* diffInfoContinue,int threadNum){
    return _zsync_patch(listener,syncDataListener,0,oldStream,newSyncInfo,out_newStream,newDataContinue,
                        out_diffInfoStream,kSyncDiff_info,diffInfoContinue,threadNum);
}

TSyncClient_resultType zsync_local_diff(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                                        const hpatch_TStreamInput* oldStream,const TNewDataZsyncInfo* newSyncInfo,
                                        const hpatch_TStreamOutput* out_diffStream,TSyncDiffType diffType,
                                        const hpatch_TStreamInput* diffContinue,int threadNum){
    return _zsync_patch(listener,syncDataListener,0,oldStream,newSyncInfo,0,0,
                        out_diffStream,diffType,diffContinue,threadNum);
}


TSyncClient_resultType zsync_local_patch(ISyncInfoListener* listener,const hpatch_TStreamInput* in_diffStream,
                                         const hpatch_TStreamInput* oldStream,const TNewDataZsyncInfo* newSyncInfo,
                                         const hpatch_TStreamOutput* out_newStream,const hpatch_TStreamInput* newDataContinue,int threadNum){
    TSyncDiffData diffData;
    if (!_TSyncDiffData_load(&diffData,in_diffStream)) return kSyncClient_loadDiffError;
    return _zsync_patch(listener,0,&diffData,oldStream,newSyncInfo,out_newStream,newDataContinue,
                      0,kSyncDiff_default,0,threadNum);
}

namespace sync_private{

static TSyncClient_resultType
    _zsync_patch_file2file(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                           TSyncDiffData* diffData,const char* oldFile,const char* newSyncInfoFile,hpatch_BOOL isIgnoreCompressInfo,
                           const char* outNewFile,hpatch_BOOL isOutNewContinue,
                           const hpatch_TStreamOutput* out_diffStream,TSyncDiffType diffType,
                           const hpatch_TStreamInput* diffContinue,int threadNum){
    TSyncClient_resultType result=kSyncClient_ok;
    int _inClear=0;
    TNewDataZsyncInfo           newSyncInfo;
    hpatch_TFileStreamInput     oldData;
    hpatch_TFileStreamOutput    out_newData;
    const hpatch_TStreamInput*  newDataContinue=0;
    hpatch_TStreamInput         _newDataContinue;
    const hpatch_TStreamInput*  oldStream=0;
    bool isOldPathInputEmpty=(oldFile==0)||(strlen(oldFile)==0);
    
    TNewDataZsyncInfo_init(&newSyncInfo);
    hpatch_TFileStreamInput_init(&oldData);
    hpatch_TFileStreamOutput_init(&out_newData);
    result=TNewDataZsyncInfo_open_by_file(&newSyncInfo,newSyncInfoFile,isIgnoreCompressInfo,listener);
    check(result==kSyncClient_ok,result);
    
    if (!isOldPathInputEmpty)
        check(hpatch_TFileStreamInput_open(&oldData,oldFile),kSyncClient_oldFileOpenError);
    oldStream=&oldData.base;
    if (outNewFile){
        check(_open_continue_out(isOutNewContinue,outNewFile,&out_newData,&_newDataContinue,newSyncInfo.newDataSize),
              isOutNewContinue?kSyncClient_newFileReopenWriteError:kSyncClient_newFileCreateError);
        if (isOutNewContinue) newDataContinue=&_newDataContinue;
    }
    
    result=_zsync_patch(listener,syncDataListener,diffData,oldStream,&newSyncInfo,
                        outNewFile?&out_newData.base:0,newDataContinue,
                        out_diffStream,diffType,diffContinue,threadNum);
clear:
    _inClear=1;
    check(hpatch_TFileStreamOutput_close(&out_newData),kSyncClient_newFileCloseError);
    check(hpatch_TFileStreamInput_close(&oldData),kSyncClient_oldFileCloseError);
    TNewDataZsyncInfo_close(&newSyncInfo);
    return result;
}

}//end namespace sync_private

TSyncClient_resultType zsync_patch_file2file(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                                             const char* oldFile,const char* newSyncInfoFile,hpatch_BOOL isIgnoreCompressInfo,
                                             const char* outNewFile,hpatch_BOOL isOutNewContinue,
                                             const char* cacheDiffInfoFile,int threadNum){
    TSyncClient_resultType result=kSyncClient_ok;
    int _inClear=0;
    hpatch_TFileStreamOutput out_diffInfo;
    hpatch_TFileStreamOutput_init(&out_diffInfo);
    const hpatch_TStreamInput* diffContinue=0;
    hpatch_TStreamInput _diffContinue;
    if (cacheDiffInfoFile){
        hpatch_BOOL isOutDiffContinue=hpatch_TRUE;
        check(_open_continue_out(isOutDiffContinue,cacheDiffInfoFile,&out_diffInfo,&_diffContinue),
            isOutDiffContinue?kSyncClient_diffFileReopenWriteError:kSyncClient_diffFileCreateError);
        if (isOutDiffContinue) diffContinue=&_diffContinue;
    }

    result=_zsync_patch_file2file(listener,syncDataListener,0,oldFile,newSyncInfoFile,isIgnoreCompressInfo,
                                  outNewFile,isOutNewContinue,
                                  cacheDiffInfoFile?&out_diffInfo.base:0,kSyncDiff_info,diffContinue,threadNum);
clear:
    _inClear=1;
    check(hpatch_TFileStreamOutput_close(&out_diffInfo),kSyncClient_diffFileCloseError);
    return result;
}

TSyncClient_resultType zsync_local_diff_file2file(ISyncInfoListener* listener,IReadSyncDataListener* syncDataListener,
                                                  const char* oldFile,const char* newSyncInfoFile,hpatch_BOOL isIgnoreCompressInfo,
                                                  const char* outDiffFile,TSyncDiffType diffType,
                                                  hpatch_BOOL isOutDiffContinue,int threadNum){
    TSyncClient_resultType result=kSyncClient_ok;
    int _inClear=0;
    hpatch_TFileStreamOutput out_diff;
    hpatch_TFileStreamOutput_init(&out_diff);
    const hpatch_TStreamInput* diffContinue=0;
    hpatch_TStreamInput _diffContinue;
    
    check(_open_continue_out(isOutDiffContinue,outDiffFile,&out_diff,&_diffContinue),
          isOutDiffContinue?kSyncClient_diffFileReopenWriteError:kSyncClient_diffFileCreateError);
    if (isOutDiffContinue) diffContinue=&_diffContinue;
    
    result=_zsync_patch_file2file(listener,syncDataListener,0,oldFile,newSyncInfoFile,isIgnoreCompressInfo,0,hpatch_FALSE,
                                  &out_diff.base,diffType,diffContinue,threadNum);

clear:
    _inClear=1;
    check(hpatch_TFileStreamOutput_close(&out_diff),kSyncClient_diffFileCloseError);
    return result;
}

TSyncClient_resultType zsync_local_patch_file2file(ISyncInfoListener* listener,const char* inDiffFile,
                                                   const char* oldFile,const char* newSyncInfoFile,hpatch_BOOL isIgnoreCompressInfo,
                                                   const char* outNewFile,hpatch_BOOL isOutNewContinue,int threadNum){
    TSyncClient_resultType result=kSyncClient_ok;
    int _inClear=0;
    TSyncDiffData diffData;
    hpatch_TFileStreamInput in_diffData;
    hpatch_TFileStreamInput_init(&in_diffData);
    check(hpatch_TFileStreamInput_open(&in_diffData,inDiffFile),
          kSyncClient_diffFileOpenError);
    check(_TSyncDiffData_load(&diffData,&in_diffData.base),kSyncClient_loadDiffError);
    result=_zsync_patch_file2file(listener,0,&diffData,oldFile,newSyncInfoFile,isIgnoreCompressInfo,outNewFile,isOutNewContinue,
                                 0,kSyncDiff_default,0,threadNum);
clear:
    _inClear=1;
    check(hpatch_TFileStreamInput_close(&in_diffData),kSyncClient_diffFileCloseError);
    return result;
}
