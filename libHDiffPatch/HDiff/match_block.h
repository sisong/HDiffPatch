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
static const size_t kDefaultFastMatchBlockSize = 1024*4;
namespace hdiff_private{

    //remove some big match block befor diff
    struct TMatchBlock{
        unsigned char* newData;
        unsigned char* newData_end;
        unsigned char* newData_end_cur;
        unsigned char* oldData;
        unsigned char* oldData_end;
        unsigned char* oldData_end_cur;
        const size_t   matchBlockSize;
        const size_t   threadNum;
        typedef hpatch_TCover TPackedCover;
        TMatchBlock(unsigned char* _newData,unsigned char* _newData_end,
                    unsigned char* _oldData,unsigned char* _oldData_end,
                    size_t _matchBlockSize,size_t _threadNum)
        :newData(_newData),newData_end(_newData_end),newData_end_cur(_newData_end),
         oldData(_oldData),oldData_end(_oldData_end),oldData_end_cur(_oldData_end),
         matchBlockSize(_matchBlockSize),threadNum(_threadNum){}
        void getBlockCovers();
        void getPackedCover();
        void packData();
        void unpackData(IDiffInsertCover* diffi,void* pcovers,size_t coverCount,bool isCover32);
    protected:
        std::vector<hpatch_TCover> blockCovers;
        std::vector<TPackedCover> packedCoversForOld;
        std::vector<TPackedCover> packedCoversForNew;
    };

    struct TCoversOptim:public ICoverLinesListener{
        explicit TCoversOptim(TMatchBlock* _matchBlock):matchBlock(_matchBlock){
            ICoverLinesListener* listener=this;
            memset(listener,0,sizeof(*listener));
            insert_cover=_insert_cover;
        }
        TMatchBlock* matchBlock;
    protected:
        static void _insert_cover(ICoverLinesListener* listener,IDiffInsertCover* diffi,void* pcovers,size_t coverCount,bool isCover32,
                                  hpatch_StreamPos_t* newSize,hpatch_StreamPos_t* oldSize);
    };

    template<class _TMatchBlock>
    struct TCoversOptimMB:public TCoversOptim{
        TCoversOptimMB(unsigned char* newData,unsigned char* newData_end,
                       unsigned char* oldData,unsigned char* oldData_end,
                       size_t matchBlockSize,size_t threadNum)
        :TCoversOptim(&_matchBlock),_matchBlock(newData,newData_end,oldData,oldData_end,
                                                matchBlockSize,threadNum){
            _matchBlock.getBlockCovers();
            _matchBlock.getPackedCover();
            _matchBlock.packData();
        }
    protected:
        _TMatchBlock _matchBlock;
    };
    
    struct TAutoMem;
    void loadOldAndNewStream(TAutoMem& out_mem,const hpatch_TStreamInput* oldStream,const hpatch_TStreamInput* newStream);

} //namespace hdiff_private


//optimize diff speed by match block
//note: newData&oldData in memory will be changed
//see create_compressed_diff | create_single_compressed_diff

void create_compressed_diff_block(const hpatch_TStreamInput* newData,//will load in memory
                                  const hpatch_TStreamInput* oldData,//will load in memory
                                  const hpatch_TStreamOutput* out_diff,
                                  const hdiff_TCompress* compressPlugin=0,
                                  int kMinSingleMatchScore=kMinSingleMatchScore_default,
                                  bool isUseBigCacheMatch=false,
                                  size_t matchBlockSize=kDefaultFastMatchBlockSize,
                                  size_t threadNum=1);
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

void create_single_compressed_diff_block(const hpatch_TStreamInput* newData,//will load in memory
                                         const hpatch_TStreamInput* oldData,//will load in memory
                                         const hpatch_TStreamOutput* out_diff,const hdiff_TCompress* compressPlugin=0,
                                         int kMinSingleMatchScore=kMinSingleMatchScore_default,
                                         size_t patchStepMemSize=kDefaultPatchStepMemSize,
                                         bool isUseBigCacheMatch=false,
                                         size_t matchBlockSize=kDefaultFastMatchBlockSize,
                                         size_t threadNum=1);
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
