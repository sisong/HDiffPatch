//  match_block.cpp
//  hdiff
/*
 The MIT License (MIT)
 Copyright (c) 2021 HouSisong
 
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
#include "match_block.h"
#include "diff.h"
#include "private_diff/limit_mem_diff/stream_serialize.h" //TAutoMem
#include "private_diff/limit_mem_diff/covers.h" // tm_collate_covers()
#include <algorithm>
#include <stdexcept>  //std::runtime_error
#define _check(value,info) { if (!(value)) { throw std::runtime_error(info); } }

namespace hdiff_private {
    typedef TMatchBlock::TPackedCover TPackedCover;

    template<class T> inline static
    void _clearV(std::vector<T>& v){
        std::vector<T> tmp;
        v.swap(tmp);
    }
    
    #define _cover_pos(isNew,pcover) (isNew?(pcover)->newPos:(pcover)->oldPos)

    template<bool isNew> static 
    void _getPackedCovers(hpatch_StreamPos_t dataSize,const std::vector<hpatch_TCover>& blockCovers,
                          std::vector<TPackedCover>& out_packedCovers){
        const hpatch_TCover* cover=blockCovers.data();
        const hpatch_TCover* cover_end=cover+blockCovers.size();
        hpatch_StreamPos_t dst=0;
        hpatch_StreamPos_t srcPos=0;
        for (;cover<cover_end;++cover){
            if (_cover_pos(isNew,cover)<=srcPos){
                srcPos=std::max(srcPos,_cover_pos(isNew,cover)+cover->length);
                continue;
            }
            hpatch_StreamPos_t moveLen=_cover_pos(isNew,cover)-srcPos;
            if (moveLen){
                TPackedCover pkcover={srcPos,dst,moveLen};
                out_packedCovers.push_back(pkcover);
                dst+=moveLen;
            }
            srcPos=_cover_pos(isNew,cover)+cover->length;
        }
        assert(dataSize>=srcPos);
        hpatch_StreamPos_t moveLen=dataSize-srcPos;
        if (moveLen){
            TPackedCover pkcover={srcPos,dst,moveLen};
            out_packedCovers.push_back(pkcover);
            dst+=moveLen;
        }
        //return dst;
    }


    struct TOutputCovers:public hpatch_TOutputCovers{
        TOutputCovers(std::vector<hpatch_TCover>& _blockCovers) :blockCovers(_blockCovers){ 
            blockCovers.clear();  push_cover=_push_cover;  collate_covers=_collate_covers; }
        static hpatch_BOOL _push_cover(struct hpatch_TOutputCovers* out_covers,const hpatch_TCover* cover){
            TOutputCovers* self=(TOutputCovers*)out_covers;
            self->blockCovers.push_back(*cover);
            return hpatch_TRUE;
        }
        static void _collate_covers(struct hpatch_TOutputCovers* out_covers){
            TOutputCovers* self=(TOutputCovers*)out_covers;
            tm_collate_covers(self->blockCovers);
        }
        std::vector<hpatch_TCover>& blockCovers;
    };
void TMatchBlock::getBlockCovers(){
    TOutputCovers covers(blockCovers);
    get_match_covers_by_block(newData,newData_end,oldData,oldData_end,
                              &covers,matchBlockSize,threadNum);
}

void TMatchBlock::getPackedCover(){
    std::sort(blockCovers.begin(),blockCovers.end(),cover_cmp_by_old_t<hpatch_TCover>());
    _getPackedCovers<false>(oldData_end-oldData,blockCovers,packedCoversForOld);
    std::sort(blockCovers.begin(),blockCovers.end(),cover_cmp_by_new_t<hpatch_TCover>());
    _getPackedCovers<true>(newData_end-newData,blockCovers,packedCoversForNew);
}

    
    static unsigned char* doPackData(unsigned char* data,unsigned char* data_end,
                                     const std::vector<TPackedCover>& packedCovers){
        unsigned char* dst=data;
        for (size_t i=0;i<packedCovers.size();++i){
            const TPackedCover& cv=packedCovers[i];
            assert(dst==data+cv.newPos);
            memmove(data+cv.newPos,data+cv.oldPos,(size_t)cv.length);
            dst=data+cv.newPos+cv.length;
        }
        return dst;
    }
void TMatchBlock::packData(){
    if (blockCovers.empty()) return;
    oldData_end_cur=doPackData(oldData,oldData_end,packedCoversForOld);
    newData_end_cur=doPackData(newData,newData_end,packedCoversForNew);
}


    template<class TCover,bool isNew> static 
    void _sortCover(TCover* pcovers,size_t coverCount){
        if (isNew)
            std::sort(pcovers,pcovers+coverCount,cover_cmp_by_new_t<TCover>());
        else
            std::sort(pcovers,pcovers+coverCount,cover_cmp_by_old_t<TCover>());
    }
    template<bool isNew> static 
    void sortCover(void* pcovers,size_t coverCount,bool isCover32){
        if (coverCount==0) return;
        if (isCover32)
            _sortCover<hpatch_TCover32,isNew>((hpatch_TCover32*)pcovers,coverCount);
        else
            _sortCover<hpatch_TCover,isNew>((hpatch_TCover*)pcovers,coverCount);
    }


    template<class TCover,class TPos,bool isNew> static 
    void _clipCover(TCover* pcovers,size_t coverCount,
                    const std::vector<TPackedCover>& packedCovers,
                    std::vector<hpatch_TCover>& out_clipCovers){
        if (packedCovers.empty()) return;
        const TPackedCover* clipCur=packedCovers.data();
        hpatch_StreamPos_t  clipPos=clipCur->newPos;
        hpatch_StreamPos_t  unpackLen=0;
        const TPackedCover* clipEnd=clipCur+packedCovers.size();
        for (size_t i=0;i<coverCount;++i){
            TCover& s=pcovers[i];
            hpatch_StreamPos_t sbegin=_cover_pos(isNew,&s);
            while (sbegin>=clipPos){
                _check(clipCur!=clipEnd,"error clip cover");
                unpackLen=clipCur->oldPos-clipCur->newPos;
                ++clipCur;
                if (clipCur!=clipEnd)
                    clipPos=clipCur->newPos;
                else
                    clipPos=hpatch_kNullStreamPos;
            }
            const TPackedCover* clipCuri=clipCur;
            hpatch_StreamPos_t  clipPosi=clipPos;
            hpatch_StreamPos_t  unpackLeni=unpackLen;
            do{
                if (sbegin+s.length<=clipPosi){
                    _cover_pos(isNew,&s)+=(TPos)unpackLeni;
                    break; //ok next cover
                }
                hpatch_TCover _c={s.oldPos,s.newPos,clipPosi-sbegin};
                _cover_pos(isNew,&_c)+=(TPos)unpackLeni;
                out_clipCovers.push_back(_c);
                s.oldPos+=(TPos)_c.length;
                s.newPos+=(TPos)_c.length;
                s.length-=(TPos)_c.length;
                sbegin=_cover_pos(isNew,&s);
                while (sbegin>=clipPosi){
                    _check(clipCuri!=clipEnd,"error clip cover");
                    unpackLeni=clipCuri->oldPos-clipCuri->newPos;
                    ++clipCuri;
                    if (clipCuri!=clipEnd)
                        clipPosi=clipCuri->newPos;
                    else
                        clipPosi=hpatch_kNullStreamPos;
                }
            } while (true);
        }
    }
    template<bool isNew> static 
    void doClipCover(void* pcovers,size_t coverCount,bool isCover32,
                    const std::vector<TPackedCover>& packedCovers,
                    std::vector<hpatch_TCover>& out_clipCovers){
        if (coverCount==0) return;
        if (isCover32)
            _clipCover<hpatch_TCover32,hpatch_uint32_t,isNew>(
                (hpatch_TCover32*)pcovers,coverCount,packedCovers,out_clipCovers);
        else
            _clipCover<hpatch_TCover,hpatch_StreamPos_t,isNew>(
                (hpatch_TCover*)pcovers,coverCount,packedCovers,out_clipCovers);
    }

    static void doUnpackData(unsigned char* data,unsigned char* data_end,
                             const std::vector<TPackedCover>& packedCovers){
        unsigned char* lastEnd=data_end;
        for (size_t i=packedCovers.size();i>0;--i){
            const TPackedCover& cv=packedCovers[i-1];
            unsigned char* dst=data+cv.oldPos;
            memmove(dst,data+cv.newPos,(size_t)cv.length);
            unsigned char* dstEnd=dst+cv.length;
            memset(dstEnd,0,lastEnd-dstEnd);
            lastEnd=dst;
        }
        memset(data+0,0,lastEnd-data);
    }

    #define _insertCovers(icovers){ \
        pcovers=diffi->insertCover(diffi,icovers.data(),icovers.size(),sizeof(*icovers.data())==sizeof(hpatch_TCover32)); \
        coverCount+=icovers.size(); \
    }

void TMatchBlock::unpackData(IDiffInsertCover* diffi,void* pcovers,size_t coverCount,bool isCover32){
    doUnpackData(oldData,oldData_end,packedCoversForOld);
    oldData_end_cur=oldData_end;
    doUnpackData(newData,newData_end,packedCoversForNew);
    newData_end_cur=newData_end;

    std::vector<hpatch_TCover> clipCovers;
    doClipCover<true>(pcovers,coverCount,isCover32,packedCoversForNew,clipCovers);
    _clearV(packedCoversForNew);
    _insertCovers(clipCovers);

    sortCover<false>(pcovers,coverCount,isCover32);
    clipCovers.clear();
    doClipCover<false>(pcovers,coverCount,isCover32,packedCoversForOld,clipCovers);
    _clearV(packedCoversForOld);
    _insertCovers(clipCovers);
    _clearV(clipCovers);
    _insertCovers(blockCovers);
    _clearV(blockCovers);

    sortCover<true>(pcovers,coverCount,isCover32);
}

void TCoversOptim::_insert_cover(ICoverLinesListener* listener,IDiffInsertCover* diffi,void* pcovers,size_t coverCount,bool isCover32,
                                    hpatch_StreamPos_t* newSize,hpatch_StreamPos_t* oldSize){
    TCoversOptim* self=(TCoversOptim*)listener;
    if (self->matchBlock!=0){
        _check((hpatch_StreamPos_t)(self->matchBlock->newData_end_cur-self->matchBlock->newData)==*newSize,"unpackData error!");
        _check((hpatch_StreamPos_t)(self->matchBlock->oldData_end_cur-self->matchBlock->oldData)==*oldSize,"unpackData error!");
        self->matchBlock->unpackData(diffi,pcovers,coverCount,isCover32);
        *newSize=self->matchBlock->newData_end_cur-self->matchBlock->newData;
        *oldSize=self->matchBlock->oldData_end_cur-self->matchBlock->oldData;
    }
}

void loadOldAndNewStream(TAutoMem& out_mem,const hpatch_TStreamInput* oldStream,const hpatch_TStreamInput* newStream){
    size_t old_size=oldStream?(size_t)oldStream->streamSize:0;
    size_t new_size=(size_t)newStream->streamSize;
    _check(newStream->streamSize==new_size,"loadOldAndNew() streamSize");
    if (old_size) _check(oldStream->streamSize+newStream->streamSize==old_size+new_size,"loadOldAndNew() streamSize");
    out_mem.realloc(old_size+new_size);
    if (old_size) _check(oldStream->read(oldStream,0,out_mem.data(),out_mem.data()+old_size),"loadOldAndNew() oldStream->read");
    _check(newStream->read(newStream,0,out_mem.data()+old_size,out_mem.data()+old_size+new_size),"loadOldAndNew() newStream->read");
}

} //namespace hdiff_private

using namespace hdiff_private;

void create_compressed_diff_block(unsigned char* newData,unsigned char* newData_end,
                                  unsigned char* oldData,unsigned char* oldData_end,
                                  std::vector<unsigned char>& out_diff,const hdiff_TCompress* compressPlugin,
                                  int kMinSingleMatchScore,bool isUseBigCacheMatch,
                                  size_t matchBlockSize,size_t threadNum){
    if (matchBlockSize==0){
        create_compressed_diff(newData,newData_end,oldData,oldData_end,
                               out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,0,threadNum);
        return;
    }
    TCoversOptimMB<TMatchBlock> coversOp(newData,newData_end,oldData,oldData_end,matchBlockSize,threadNum);
    create_compressed_diff(newData,coversOp.matchBlock->newData_end_cur,oldData,coversOp.matchBlock->oldData_end_cur,
                           out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,&coversOp,threadNum);
}
void create_compressed_diff_block(unsigned char* newData,unsigned char* newData_end,
                                  unsigned char* oldData,unsigned char* oldData_end,
                                  const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                                  int kMinSingleMatchScore,bool isUseBigCacheMatch,
                                  size_t matchBlockSize,size_t threadNum){
    if (matchBlockSize==0){
        create_compressed_diff(newData,newData_end,oldData,oldData_end,
                               out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,0,threadNum);
        return;
    }
    TCoversOptimMB<TMatchBlock> coversOp(newData,newData_end,oldData,oldData_end,matchBlockSize,threadNum);
    create_compressed_diff(newData,coversOp.matchBlock->newData_end_cur,oldData,coversOp.matchBlock->oldData_end_cur,
                           out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,&coversOp,threadNum);
}
void create_compressed_diff_block(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                                  const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                                  int kMinSingleMatchScore,bool isUseBigCacheMatch,
                                  size_t matchBlockSize,size_t threadNum){
    TAutoMem oldAndNewData;
    loadOldAndNewStream(oldAndNewData,oldData,newData);
    size_t old_size=oldData?(size_t)oldData->streamSize:0;
    unsigned char* pOldData=oldAndNewData.data();
    unsigned char* pNewData=pOldData+old_size;
    create_compressed_diff_block(pNewData,pNewData+(size_t)newData->streamSize,pOldData,pOldData+old_size,
                                 out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,matchBlockSize,threadNum);
}


void create_single_compressed_diff_block(unsigned char* newData,unsigned char* newData_end,
                                         unsigned char* oldData,unsigned char* oldData_end,
                                         const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                                         int kMinSingleMatchScore,size_t patchStepMemSize,
                                         bool isUseBigCacheMatch,size_t matchBlockSize,size_t threadNum){
    if (matchBlockSize==0){
        create_single_compressed_diff(newData,newData_end,oldData,oldData_end,
                                      out_diff,compressPlugin,kMinSingleMatchScore,
                                      patchStepMemSize,isUseBigCacheMatch,0,threadNum);
        return;
    }
    TCoversOptimMB<TMatchBlock> coversOp(newData,newData_end,oldData,oldData_end,matchBlockSize,threadNum);
    create_single_compressed_diff(newData,coversOp.matchBlock->newData_end_cur,oldData,coversOp.matchBlock->oldData_end_cur,
                                  out_diff,compressPlugin,kMinSingleMatchScore,
                                  patchStepMemSize,isUseBigCacheMatch,&coversOp,threadNum);                                      
}
void create_single_compressed_diff_block(unsigned char* newData,unsigned char* newData_end,
                                         unsigned char* oldData,unsigned char* oldData_end,
                                         std::vector<unsigned char>& out_diff,const hdiff_TCompress* compressPlugin,
                                         int kMinSingleMatchScore,size_t patchStepMemSize,
                                         bool isUseBigCacheMatch,size_t matchBlockSize,size_t threadNum){
    if (matchBlockSize==0){
        create_single_compressed_diff(newData,newData_end,oldData,oldData_end,
                                      out_diff,compressPlugin,kMinSingleMatchScore,
                                      patchStepMemSize,isUseBigCacheMatch,0,threadNum);
        return;
    }
    TCoversOptimMB<TMatchBlock> coversOp(newData,newData_end,oldData,oldData_end,matchBlockSize,threadNum);
    create_single_compressed_diff(newData,coversOp.matchBlock->newData_end_cur,oldData,coversOp.matchBlock->oldData_end_cur,
                                  out_diff,compressPlugin,kMinSingleMatchScore,
                                  patchStepMemSize,isUseBigCacheMatch,&coversOp,threadNum); 
}
void create_single_compressed_diff_block(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                                         const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                                         int kMinSingleMatchScore,size_t patchStepMemSize,
                                         bool isUseBigCacheMatch,size_t matchBlockSize,size_t threadNum){
    TAutoMem oldAndNewData;
    loadOldAndNewStream(oldAndNewData,oldData,newData);
    size_t old_size=oldData?(size_t)oldData->streamSize:0;
    unsigned char* pOldData=oldAndNewData.data();
    unsigned char* pNewData=pOldData+old_size;
    create_single_compressed_diff_block(pNewData,pNewData+(size_t)newData->streamSize,pOldData,pOldData+old_size,
                                        out_diff,compressPlugin,kMinSingleMatchScore,
                                        patchStepMemSize,isUseBigCacheMatch,matchBlockSize,threadNum);
}
