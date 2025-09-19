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
    typedef TMatchBlockBase::TPackedCover TPackedCover;

    template<class T> inline static
    void _clearV(std::vector<T>& v){
        std::vector<T> tmp;
        v.swap(tmp);
    }
    
    #define _cover_pos(isNew,pcover) (isNew?(pcover)->newPos:(pcover)->oldPos)

    #define _kMinMoveLen_new    1  // must 1
    #define _kMinMoveLen_old    16
    #define kMinMoveLen   (isNew?_kMinMoveLen_new:_kMinMoveLen_old)

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
            if (moveLen>=kMinMoveLen){
                TPackedCover pkcover={srcPos,dst,moveLen};
                out_packedCovers.push_back(pkcover);
                dst+=moveLen;
            }
            srcPos=_cover_pos(isNew,cover)+cover->length;
        }
        assert(dataSize>=srcPos);
        hpatch_StreamPos_t moveLen=dataSize-srcPos;
        if (moveLen>=kMinMoveLen){
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
void TMatchBlockMem::getBlockCovers(){
    TOutputCovers covers(blockCovers);
    get_match_covers_by_block(newData,newData_end,oldData,oldData_end,
                              &covers,matchBlockSize,threadNum);
}

void TMatchBlockStream::getBlockCovers(){
    TOutputCovers covers(blockCovers);
    const hdiff_TMTSets_s mtsets={threadNum,threadNumForStream,false,false};
    get_match_covers_by_block(newStream,oldStream,&covers,matchBlockSize,&mtsets);
}

void TMatchBlockBase::_getPackedCover(hpatch_StreamPos_t newDataSize,hpatch_StreamPos_t oldDataSize){
    std::sort(blockCovers.begin(),blockCovers.end(),cover_cmp_by_old_t<hpatch_TCover>());
    _getPackedCovers<false>(oldDataSize,blockCovers,packedCoversForOld);
    std::sort(blockCovers.begin(),blockCovers.end(),cover_cmp_by_new_t<hpatch_TCover>());
    _getPackedCovers<true>(newDataSize,blockCovers,packedCoversForNew);
}

    
    static unsigned char* doPackData(unsigned char* data,unsigned char* data_end,
                                     const std::vector<TPackedCover>& packedCovers){
        unsigned char* dst=data;
        for (size_t i=0;i<packedCovers.size();++i){
            const TPackedCover& cv=packedCovers[i];
            assert(dst==data+cv.newPos);
            memmove(dst,data+cv.oldPos,(size_t)cv.length);
            dst+=cv.length;
        }
        return dst;
    }
void TMatchBlockMem::packData(){
    if (blockCovers.empty()) return;
    oldData_end_cur=doPackData(oldData,oldData_end,packedCoversForOld);
    newData_end_cur=doPackData(newData,newData_end,packedCoversForNew);
}

    static inline hpatch_StreamPos_t _getPackedSize(const std::vector<TPackedCover>& packedCovers){
        const TPackedCover* cv=packedCovers.empty()?0:&packedCovers[packedCovers.size()-1];
        return cv?cv->newPos+cv->length:0;
    }
    static bool loadPackData(unsigned char* dst_begin,unsigned char* dst_end,
                             const hpatch_TStreamInput* srcStream,const std::vector<TPackedCover>& packedCovers){
        unsigned char* dst=dst_begin;
        for (size_t i=0;i<packedCovers.size();++i){
            const TPackedCover& cv=packedCovers[i];
            assert(dst==dst_begin+cv.newPos);
            if (!srcStream->read(srcStream,cv.oldPos,dst,dst+cv.length)) return false;
            dst+=cv.length;
        }
        assert(dst==dst_end);
        return true;
    }
void TMatchBlockStream::packData(){
    //load packed new & old data
    const hpatch_StreamPos_t packedOldSize=_getPackedSize(packedCoversForOld);
    const hpatch_StreamPos_t packedNewSize=_getPackedSize(packedCoversForNew);
    _check(packedNewSize==(size_t)packedNewSize,"TMatchBlockStream::packData() packedNewSize");
    if (packedOldSize) _check(packedNewSize+packedOldSize==(size_t)(packedNewSize+packedOldSize),"TMatchBlockStream::packData() packedOldSize");
    _packedNewOldMem->realloc((size_t)(packedNewSize+packedOldSize));
    oldData=_packedNewOldMem->data();
    oldData_end_cur=oldData+packedOldSize;
    newData=oldData_end_cur;
    newData_end_cur=newData+packedNewSize;
    _out_diff_info("  load datas into memory from old & new ...\n");
    _check(loadPackData(oldData,oldData_end_cur,oldStream,packedCoversForOld),"loadPackData(oldStream)");
    _check(loadPackData(newData,newData_end_cur,newStream,packedCoversForNew),"loadPackData(newStream)");
}

TMatchBlockStream::TMatchBlockStream(const hpatch_TStreamInput* _newStream,const hpatch_TStreamInput* _oldStream,
                                     size_t _matchBlockSize,size_t _threadNumForMem,size_t _threadNumForStream)
:TMatchBlockBase(_matchBlockSize,_threadNumForMem),
 newData(0),newData_end_cur(0),oldData(0),oldData_end_cur(0),newStream(_newStream),oldStream(_oldStream),
 threadNumForStream(_threadNumForStream),_newStreamMap(_newStream,packedCoversForNew),
 _oldStreamMap(_oldStream,packedCoversForOld),_packedNewOldMem(0),_isUnpacked(false){
    assert(_oldStream);
    _packedNewOldMem=new TAutoMem();
}
TMatchBlockStream::~TMatchBlockStream(){
    if (_packedNewOldMem) delete _packedNewOldMem;
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

void TMatchBlockBase::_unpackData(IDiffInsertCover* diffi,void*& pcovers,size_t& coverCount,bool isCover32){
    std::vector<hpatch_TCover> clipCovers;
    doClipCover<true>(pcovers,coverCount,isCover32,packedCoversForNew,clipCovers);
    _insertCovers(clipCovers);

    sortCover<false>(pcovers,coverCount,isCover32);
    clipCovers.clear();
    doClipCover<false>(pcovers,coverCount,isCover32,packedCoversForOld,clipCovers);
    _insertCovers(clipCovers);
    _clearV(clipCovers);
    _insertCovers(blockCovers);
    _clearV(blockCovers);

    sortCover<true>(pcovers,coverCount,isCover32);
}

void TMatchBlockMem::unpackData(IDiffInsertCover* diffi,void* pcovers,size_t coverCount,bool isCover32){
    doUnpackData(oldData,oldData_end,packedCoversForOld);
    oldData_end_cur=oldData_end;
    doUnpackData(newData,newData_end,packedCoversForNew);
    newData_end_cur=newData_end;
    _unpackData(diffi,pcovers,coverCount,isCover32);
    _clearV(packedCoversForOld);
    _clearV(packedCoversForNew);
}

void TMatchBlockStream::unpackData(IDiffInsertCover* diffi,void* pcovers,size_t coverCount,bool isCover32){
    _unpackData(diffi,pcovers,coverCount,isCover32);
    _isUnpacked=true;
}
    static inline bool _isHitPackedCover(const TPackedCover* pc,hpatch_StreamPos_t readFromPos){
        return (pc->oldPos<=readFromPos)&(readFromPos<pc->oldPos+pc->length);
    }
    typedef TMatchBlockStream::TStreamInputMap TStreamInputMap;
    static hpatch_BOOL _TStreamInputMap_read(const hpatch_TStreamInput* stream,hpatch_StreamPos_t readFromPos,
                                       unsigned char* out_data,unsigned char* out_data_end){
        TStreamInputMap* self=(TStreamInputMap*)stream->streamImport;
        //return self->baseStream->read(self->baseStream,readFromPos,out_data,out_data_end); //only for debug test
        while (out_data<out_data_end) {
            const TPackedCover* pc=&self->packedCovers[self->curCoverIndex];
            if (!_isHitPackedCover(pc,readFromPos)){//not hit, research
                const TPackedCover vc={readFromPos,0,~(hpatch_StreamPos_t)0};
                const TPackedCover* const pc0=self->packedCovers.data();
                const TPackedCover* uppc=std::upper_bound(pc0,pc0+self->packedCovers.size(),
                                                          vc,cover_cmp_by_old_t<TPackedCover>());
                assert(uppc>pc0);
                pc=uppc-1;
                self->curCoverIndex=pc-pc0;
                if (!_isHitPackedCover(pc,readFromPos)){//can't hit
                    self->curCoverIndex++;
                    size_t readLen=out_data_end-out_data;
                    readLen=(readFromPos+readLen<=uppc->oldPos)?readLen:(size_t)(uppc->oldPos-readFromPos);
                    memset(out_data,0,readLen);
                    readFromPos+=readLen;
                    out_data+=readLen;
                    continue;
                }
            }

            //hit
            size_t readLen=out_data_end-out_data;
            readLen=(readFromPos+readLen<=pc->oldPos+pc->length)?readLen:(size_t)(pc->oldPos+pc->length-readFromPos);
            memcpy(out_data,self->data+pc->newPos+(readFromPos-pc->oldPos),readLen);
            readFromPos+=readLen;
            self->curCoverIndex+=(readFromPos==pc->oldPos+pc->length)?1:0;
            out_data+=readLen;
        }
        return hpatch_TRUE;
    }
    
    inline static void unpackDataAsStream(TStreamInputMap* self,unsigned char* data,unsigned char* data_end){
        self->streamImport=self;
        self->streamSize=self->baseStream->streamSize;
        self->read=_TStreamInputMap_read;
        self->data=data;
        self->data_end=data_end;
        //sort packedCovers for serach
        std::sort(self->packedCovers.begin(),self->packedCovers.end(),cover_cmp_by_old_t<TPackedCover>());
        TPackedCover pc={0,0,0};
        if (self->packedCovers.empty()||self->packedCovers[0].oldPos>0)
            self->packedCovers.insert(self->packedCovers.begin(),pc);
        pc.oldPos=self->streamSize+1;
        self->packedCovers.push_back(pc);
    }
void TMatchBlockStream::map_streams(const hpatch_TStreamInput** pnewData,const hpatch_TStreamInput** poldData){
    //*pnewData=newStream; *poldData=oldStream;    return; //only for debug test
    unpackDataAsStream(&_newStreamMap,newData,newData_end_cur);
    unpackDataAsStream(&_oldStreamMap,oldData,oldData_end_cur);
    *pnewData=&_newStreamMap;
    *poldData=&_oldStreamMap;
}

void loadOldAndNewStream(TAutoMem& out_mem,const hpatch_TStreamInput* oldStream,const hpatch_TStreamInput* newStream){
    _out_diff_info("  load all datas into memory from old & new ...\n");
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
    TVectorAsStreamOutput outDiffStream(out_diff);
    create_compressed_diff_block(newData,newData_end,oldData,oldData_end,
                                 &outDiffStream,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,matchBlockSize,threadNum);
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
    TCoversOptimMem coversOp(newData,newData_end,oldData,oldData_end,matchBlockSize,threadNum);
    create_compressed_diff(coversOp.matchBlock->newData,coversOp.matchBlock->newData_end_cur,
                           coversOp.matchBlock->oldData,coversOp.matchBlock->oldData_end_cur,
                           out_diff,compressPlugin,kMinSingleMatchScore,isUseBigCacheMatch,&coversOp,threadNum);
}
void create_compressed_diff_block(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                                  const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                                  int kMinSingleMatchScore,bool isUseBigCacheMatch,size_t matchBlockSize,
                                  size_t threadNumForMem,size_t threadNumForStream){
    if (matchBlockSize==0){
        TAutoMem oldAndNewData;
        loadOldAndNewStream(oldAndNewData,oldData,newData);
        size_t old_size=oldData?(size_t)oldData->streamSize:0;
        unsigned char* pOldData=oldAndNewData.data();
        unsigned char* pNewData=pOldData+old_size;
        create_compressed_diff(pNewData,pNewData+(size_t)newData->streamSize,pOldData,pOldData+old_size,
                               out_diff,compressPlugin,kMinSingleMatchScore,
                               isUseBigCacheMatch,0,threadNumForMem);
        return;
    }
    TCoversOptimStream coversOp(newData,oldData,matchBlockSize,threadNumForMem,threadNumForStream);
    create_compressed_diff(coversOp.matchBlock->newData,coversOp.matchBlock->newData_end_cur,
                           coversOp.matchBlock->oldData,coversOp.matchBlock->oldData_end_cur,
                           out_diff,compressPlugin,kMinSingleMatchScore,
                           isUseBigCacheMatch,&coversOp,threadNumForMem);
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
    TCoversOptimMem coversOp(newData,newData_end,oldData,oldData_end,matchBlockSize,threadNum);
    create_single_compressed_diff(coversOp.matchBlock->newData,coversOp.matchBlock->newData_end_cur,
                                  coversOp.matchBlock->oldData,coversOp.matchBlock->oldData_end_cur,
                                  out_diff,compressPlugin,kMinSingleMatchScore,
                                  patchStepMemSize,isUseBigCacheMatch,&coversOp,threadNum);                                      
}
void create_single_compressed_diff_block(unsigned char* newData,unsigned char* newData_end,
                                         unsigned char* oldData,unsigned char* oldData_end,
                                         std::vector<unsigned char>& out_diff,const hdiff_TCompress* compressPlugin,
                                         int kMinSingleMatchScore,size_t patchStepMemSize,
                                         bool isUseBigCacheMatch,size_t matchBlockSize,size_t threadNum){
    TVectorAsStreamOutput outDiffStream(out_diff);
    create_single_compressed_diff_block(newData,newData_end,oldData,oldData_end,
                                        &outDiffStream,compressPlugin,kMinSingleMatchScore,
                                        patchStepMemSize,isUseBigCacheMatch,matchBlockSize,threadNum);
}
void create_single_compressed_diff_block(const hpatch_TStreamInput* newData,const hpatch_TStreamInput* oldData,
                                         const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin,
                                         int kMinSingleMatchScore,size_t patchStepMemSize,
                                         bool isUseBigCacheMatch,size_t matchBlockSize,
                                         size_t threadNumForMem,size_t threadNumForStream){
    if (matchBlockSize==0){
        TAutoMem oldAndNewData;
        loadOldAndNewStream(oldAndNewData,oldData,newData);
        size_t old_size=oldData?(size_t)oldData->streamSize:0;
        unsigned char* pOldData=oldAndNewData.data();
        unsigned char* pNewData=pOldData+old_size;
        create_single_compressed_diff(pNewData,pNewData+(size_t)newData->streamSize,pOldData,pOldData+old_size,
                                      out_diff,compressPlugin,kMinSingleMatchScore,
                                      patchStepMemSize,isUseBigCacheMatch,0,threadNumForMem);
        return;
    }
    TCoversOptimStream coversOp(newData,oldData,matchBlockSize,threadNumForMem,threadNumForStream);
    create_single_compressed_diff(coversOp.matchBlock->newData,coversOp.matchBlock->newData_end_cur,
                                  coversOp.matchBlock->oldData,coversOp.matchBlock->oldData_end_cur,
                                  out_diff,compressPlugin,kMinSingleMatchScore,
                                  patchStepMemSize,isUseBigCacheMatch,&coversOp,threadNumForMem);
}

