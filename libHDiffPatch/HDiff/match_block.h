// match_block.h
// hdiff
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
#ifndef hdiff_match_block_h
#define hdiff_match_block_h
#include "diff.h"
#include <vector>
static const size_t kDefaultFastMatchBlockSize = 1024*1;
namespace hdiff_private{
    struct TAutoMem;
    
    struct TMatchBlockBase{
        typedef hpatch_TCover TPackedCover;
        TMatchBlockBase(size_t _matchBlockSize,size_t _threadNum)
        :matchBlockSize(_matchBlockSize),threadNum(_threadNum){}
    protected:
        void _getPackedCover(hpatch_StreamPos_t newDataSize,hpatch_StreamPos_t oldDataSize);
        void _unpackData(IDiffInsertCover* diffi,void*& pcovers,size_t& coverCount,bool isCover32);
        const size_t   matchBlockSize;
        const size_t   threadNum;
        std::vector<hpatch_TCover> blockCovers;
        std::vector<TPackedCover> packedCoversForOld;
        std::vector<TPackedCover> packedCoversForNew;
    };

    //remove some big match block befor diff, in memory
    struct TMatchBlockMem:public TMatchBlockBase{
        unsigned char* newData;
        unsigned char* newData_end;
        unsigned char* newData_end_cur;
        unsigned char* oldData;
        unsigned char* oldData_end;
        unsigned char* oldData_end_cur;
        TMatchBlockMem(unsigned char* _newData,unsigned char* _newData_end,
                       unsigned char* _oldData,unsigned char* _oldData_end,
                       size_t _matchBlockSize,size_t _threadNumForMem)
        :TMatchBlockBase(_matchBlockSize,_threadNumForMem),
         newData(_newData),newData_end(_newData_end),newData_end_cur(_newData_end),
         oldData(_oldData),oldData_end(_oldData_end),oldData_end_cur(_oldData_end){ }
        inline hpatch_StreamPos_t curNewDataSize()const{ return (size_t)(newData_end_cur-newData); }
        inline hpatch_StreamPos_t curOldDataSize()const{ return (size_t)(oldData_end_cur-oldData); }
        void getBlockCovers();
        inline void getPackedCover() { _getPackedCover(newData_end-newData,oldData_end-oldData); }
        void packData();
        void unpackData(IDiffInsertCover* diffi,void* pcovers,size_t coverCount,bool isCover32);
    };

    //remove some big match block befor diff, used stream
    struct TMatchBlockStream:public TMatchBlockBase{
        unsigned char* newData;
        unsigned char* newData_end_cur;
        unsigned char* oldData;
        unsigned char* oldData_end_cur;
        const hpatch_TStreamInput* newStream;
        const hpatch_TStreamInput* oldStream;
        const size_t    threadNumForStream;
        TMatchBlockStream(const hpatch_TStreamInput* _newStream,const hpatch_TStreamInput* _oldStream,
                          size_t _matchBlockSize,size_t _threadNumForMem,size_t _threadNumForStream);
        ~TMatchBlockStream();
        inline hpatch_StreamPos_t curNewDataSize()const{ return _isUnpacked?newStream->streamSize:(size_t)(newData_end_cur-newData); }
        inline hpatch_StreamPos_t curOldDataSize()const{ return _isUnpacked?oldStream->streamSize:(size_t)(oldData_end_cur-oldData); }
        void getBlockCovers();
        inline void getPackedCover() { _getPackedCover(newStream->streamSize,oldStream->streamSize); }
        void packData();
        void unpackData(IDiffInsertCover* diffi,void* pcovers,size_t coverCount,bool isCover32);
        void map_streams(const hpatch_TStreamInput** pnewData,const hpatch_TStreamInput** poldData);

        struct TStreamInputMap:public hpatch_TStreamInput{
            const hpatch_TStreamInput*  baseStream;
            std::vector<TPackedCover>&  packedCovers;
            size_t                      curCoverIndex;
            const unsigned char*        data; //packed data
            const unsigned char*        data_end;
            inline TStreamInputMap(const hpatch_TStreamInput* _baseStream,std::vector<TPackedCover>& _packedCovers)
                :baseStream(_baseStream),packedCovers(_packedCovers),curCoverIndex(0){}
        };
    protected:
        TStreamInputMap _newStreamMap;
        TStreamInputMap _oldStreamMap;
        TAutoMem*       _packedNewOldMem;
        bool            _isUnpacked;
    };

    template<class _TMatchBlock>
    struct TCoversOptim:public ICoverLinesListener{
        explicit TCoversOptim(_TMatchBlock* _matchBlock):matchBlock(_matchBlock){
            ICoverLinesListener* listener=this;
            memset(listener,0,sizeof(*listener));
            insert_cover=_insert_cover;
        }
        _TMatchBlock* matchBlock;
    protected:
        static void _insert_cover(ICoverLinesListener* listener,IDiffInsertCover* diffi,void* pcovers,size_t coverCount,
                                  bool isCover32,hpatch_StreamPos_t* newSize,hpatch_StreamPos_t* oldSize){
            TCoversOptim<_TMatchBlock>* self=(TCoversOptim<_TMatchBlock>*)listener;
            if (self->matchBlock!=0){
                assert(self->matchBlock->curNewDataSize()==*newSize);
                assert(self->matchBlock->curOldDataSize()==*oldSize);
                self->matchBlock->unpackData(diffi,pcovers,coverCount,isCover32);
                *newSize=self->matchBlock->curNewDataSize();
                *oldSize=self->matchBlock->curOldDataSize();
            }
        }
        inline void _doPack(){
            matchBlock->getBlockCovers();
            matchBlock->getPackedCover();
            matchBlock->packData();
        }
    };

    struct TCoversOptimMem:public TCoversOptim<TMatchBlockMem>{
        TCoversOptimMem(unsigned char* newData,unsigned char* newData_end,
                       unsigned char* oldData,unsigned char* oldData_end,
                       size_t matchBlockSize,size_t threadNum)
        :TCoversOptim<TMatchBlockMem>(&_matchBlock),
         _matchBlock(newData,newData_end,oldData,oldData_end,matchBlockSize,threadNum){
            _doPack();
        }
    protected:
        TMatchBlockMem _matchBlock;
    };

    struct TCoversOptimStream:public TCoversOptim<TMatchBlockStream>{
        TCoversOptimStream(const hpatch_TStreamInput* newStream,const hpatch_TStreamInput* oldStream,
                       size_t matchBlockSize,size_t threadNumForMem,size_t threadNumForStream)
        :TCoversOptim<TMatchBlockStream>(&_matchBlock),
         _matchBlock(newStream,oldStream,matchBlockSize,threadNumForMem,threadNumForStream){
            map_streams_befor_serialize=_map_streams_befor_serialize;//for ICoverLinesListener
            _doPack();
        }
    protected:
        TMatchBlockStream _matchBlock;
        static void _map_streams_befor_serialize(ICoverLinesListener* listener,const hpatch_TStreamInput** pnewData,const hpatch_TStreamInput** poldData){
            TCoversOptimStream* self=(TCoversOptimStream*)listener;
            self->_matchBlock.map_streams(pnewData,poldData);
        }
    };
    
    void loadOldAndNewStream(TAutoMem& out_mem,const hpatch_TStreamInput* oldStream,const hpatch_TStreamInput* newStream);

} //namespace hdiff_private


//optimize diff speed by match block
//note: newData&oldData in memory will be changed
//see create_compressed_diff | create_single_compressed_diff

void create_compressed_diff_block(const hpatch_TStreamInput* newData,//will load needed in memory
                                  const hpatch_TStreamInput* oldData,//will load needed in memory
                                  const hpatch_TStreamOutput* out_diff,
                                  const hdiff_TCompress* compressPlugin=0,
                                  int kMinSingleMatchScore=kMinSingleMatchScore_default,
                                  bool isUseBigCacheMatch=false,
                                  size_t matchBlockSize=kDefaultFastMatchBlockSize,
                                  size_t threadNumForMem=1,size_t threadNumForStream=1);
void create_compressed_diff_block(unsigned char* newData,unsigned char* newData_end,
                                  unsigned char* oldData,unsigned char* oldData_end,
                                  const hpatch_TStreamOutput* out_diff,
                                  const hdiff_TCompress* compressPlugin=0,
                                  int kMinSingleMatchScore=kMinSingleMatchScore_default,
                                  bool isUseBigCacheMatch=false,
                                  size_t matchBlockSize=kDefaultFastMatchBlockSize,
                                  size_t threadNum=1);
void create_compressed_diff_block(unsigned char* newData,unsigned char* newData_end,
                                  unsigned char* oldData,unsigned char* oldData_end,
                                  std::vector<unsigned char>& out_diff,
                                  const hdiff_TCompress* compressPlugin=0,
                                  int kMinSingleMatchScore=kMinSingleMatchScore_default,
                                  bool isUseBigCacheMatch=false,
                                  size_t matchBlockSize=kDefaultFastMatchBlockSize,
                                  size_t threadNum=1);

void create_single_compressed_diff_block(const hpatch_TStreamInput* newData,//will load needed in memory
                                         const hpatch_TStreamInput* oldData,//will load needed in memory
                                         const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin=0,
                                         int kMinSingleMatchScore=kMinSingleMatchScore_default,
                                         size_t patchStepMemSize=kDefaultPatchStepMemSize,
                                         bool isUseBigCacheMatch=false,
                                         size_t matchBlockSize=kDefaultFastMatchBlockSize,
                                         size_t threadNumForMem=1,size_t threadNumForStream=1);
void create_single_compressed_diff_block(unsigned char* newData,unsigned char* newData_end,
                                         unsigned char* oldData,unsigned char* oldData_end,
                                         const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin=0,
                                         int kMinSingleMatchScore=kMinSingleMatchScore_default,
                                         size_t patchStepMemSize=kDefaultPatchStepMemSize,
                                         bool isUseBigCacheMatch=false,
                                         size_t matchBlockSize=kDefaultFastMatchBlockSize,
                                         size_t threadNum=1);
void create_single_compressed_diff_block(unsigned char* newData,unsigned char* newData_end,
                                         unsigned char* oldData,unsigned char* oldData_end,
                                         std::vector<unsigned char>& out_diff,const hdiff_TCompress* compressPlugin=0,
                                         int kMinSingleMatchScore=kMinSingleMatchScore_default,
                                         size_t patchStepMemSize=kDefaultPatchStepMemSize,
                                         bool isUseBigCacheMatch=false,
                                         size_t matchBlockSize=kDefaultFastMatchBlockSize,
                                         size_t threadNum=1);

#endif //hdiff_match_block_h
