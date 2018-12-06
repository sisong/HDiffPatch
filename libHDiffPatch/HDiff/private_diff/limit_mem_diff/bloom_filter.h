//bloom_filter.h
//bloom过滤的一个定制实现.
/*
 The MIT License (MIT)
 Copyright (c) 2017 HouSisong
 
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

#ifndef bloom_filter_h
#define bloom_filter_h
#include <string.h> //memset
#include <assert.h>
#include <stdexcept>//std::runtime_error
namespace hdiff_private{

class TBitSet{
public:
    inline TBitSet():m_bits(0),m_bitSize(0){}
    inline ~TBitSet(){ clear(0); }
    
    inline void set(size_t bitIndex){
        assert(bitIndex<m_bitSize);
        m_bits[bitIndex>>kBaseShr] |= ((base_t)1<<(bitIndex&kBaseMask));
    }
    inline bool is_hit(size_t bitIndex)const{
        //assert(bitIndex<m_bitSize);
        return 0!=(m_bits[bitIndex>>kBaseShr] & ((base_t)1<<(bitIndex&kBaseMask)));
    }
    
    inline size_t size()const{return m_bitSize; }
    
    void clear(size_t newBitSize){
        size_t count=bitSizeToCount(newBitSize);
        if (newBitSize==m_bitSize){
            if (count>0) memset(m_bits,0,count*sizeof(base_t));
        }else{
            m_bitSize=newBitSize;
            if (m_bits) { delete [] m_bits; m_bits=0; }
            if (count>0){
                m_bits=new base_t[count];
                memset(m_bits,0,count*sizeof(base_t));
            }
        }
    }
private:
    inline static size_t bitSizeToCount(size_t bitSize){ return (bitSize+(kBaseTBits-1))/kBaseTBits; }
    typedef size_t base_t;
    enum {
        kBaseShr=(sizeof(base_t)==8)?6:((sizeof(base_t)==4)?5:0),
        kBaseTBits=(1<<kBaseShr),
        kBaseMask=kBaseTBits-1 };
    //assert(kBaseTBits==sizeof(base_t)*8);
    struct __private_TBitSet_check_base_t_size { char _[(kBaseTBits==sizeof(base_t)*8)?1:-1]; };
    base_t* m_bits;
    size_t  m_bitSize;
};


template <class T>
class TBloomFilter{
public:
    inline TBloomFilter():m_bitSetMask(0){}
    void init(size_t dataCount){
        ++dataCount;
        m_bitSetMask=getMask(dataCount);//mask is 2^N-1
        m_bitSet.clear(m_bitSetMask+1);
    }
    inline void insert(T data){
        m_bitSet.set(hash0(data));
        m_bitSet.set(hash1(data));
        m_bitSet.set(hash2(data));
    }
    inline bool is_hit(T data)const{
        return m_bitSet.is_hit(hash0(data))
            && m_bitSet.is_hit(hash1(data))
            && m_bitSet.is_hit(hash2(data));
    }
private:
    TBitSet   m_bitSet;
    size_t    m_bitSetMask;
    enum { kZoom=32 };
    static size_t getMask(size_t dataCount){
        size_t bitSize=dataCount*kZoom;
        if ((bitSize/kZoom)!=dataCount)
            throw std::runtime_error("TBloomFilter::getMask() bitSize too large error!");
        unsigned int bit=10;
        while ( (((size_t)1<<bit)<bitSize) && (bit<sizeof(size_t)*8-1) )
            ++bit;
        return ((size_t)1<<bit)-1;
    }
    
    inline size_t hash0(T key)const { return (key^(key>>(sizeof(T)*4)))&m_bitSetMask; }
    inline size_t hash1(T key)const { return ((~key)+(key << (sizeof(T)*2+1))+1)%m_bitSetMask; }
    inline size_t hash2(T key)const {
        size_t h=(sizeof(T)>4)?_hash2_64(key):_hash2_32((size_t)key); return h%(m_bitSetMask-1); }
    static size_t _hash2_32(size_t key){//from: https://gist.github.com/badboy/6267743
        const size_t c2=0x27d4eb2d; // a prime or an odd constant
        key = (key ^ 61) ^ (key >> 16);
        key = key + (key << 3);
        key = key ^ (key >> 4);
        key = key * c2;
        key = key ^ (key >> 15);
        return key;
    }
    static size_t _hash2_64(T key){
        key = (~key) + (key << 18); // key = (key << 18) - key - 1;
        key = key ^ (key >> 31);
        key = key * 21; // key = (key + (key << 2)) + (key << 4);
        key = key ^ (key >> 11);
        key = key + (key << 6);
        key = key ^ (key >> 22);
        return (size_t)key;
    }
};

}//namespace hdiff_private
#endif /* bloom_filter_h */
