//bloom_filter.h
//多层bloom过滤的一个定制实现.
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
#include <stdlib.h>
#include <string.h> //memset
#include <assert.h>
#include <stdexcept>
#ifdef _MSC_VER
#   if (_MSC_VER < 1300)
typedef unsigned int      uint32_t;
#   else
typedef unsigned __int32  uint32_t;
#   endif
#else
#   include <stdint.h> //for uint32_t
#endif


class TBitSet{
public:
    inline TBitSet():m_bits(0),m_bitSize(0){}
    inline ~TBitSet(){ clear(0); }
    
    inline void set(size_t bitIndex){
        assert(bitIndex<m_bitSize);
        m_bits[bitIndex/kBaseTBit] |= ((base_t)1<<(bitIndex%kBaseTBit));
    }
    inline bool is_hit(size_t bitIndex)const{
        //assert(bitIndex<m_bitSize);
        return 0!=(m_bits[bitIndex/kBaseTBit] & ((base_t)1<<(bitIndex%kBaseTBit)));
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
    inline static size_t bitSizeToCount(size_t bitSize){ return (bitSize+(kBaseTBit-1))/kBaseTBit; }
    typedef unsigned int base_t;
    enum { kBaseTBit=sizeof(base_t)*8 };
    base_t* m_bits;
    size_t  m_bitSize;
};


class TBloomFilter{
public:
    inline TBloomFilter():m_bitSetMask0(0){}
    void init(size_t dataCount){
        ++dataCount;
        m_bitSetMask0=getMask(dataCount*kZoom0);//mask is 2^N-1
        m_bitSet0.clear(m_bitSetMask0+1);
        m_bitSet1.clear(dataCount*kZoom1-1);
        m_bitSet2.clear(dataCount*kZoom2-1);
    }
    inline void insert(uint32_t data){
        m_bitSet0.set(hash0(data));
        m_bitSet1.set(hash1(data));
        m_bitSet2.set(hash2(data));
    }
    inline bool is_hit(uint32_t data)const{
        return m_bitSet0.is_hit(hash0(data))
            && m_bitSet1.is_hit(hash1(data))
            && m_bitSet2.is_hit(hash2(data));
    }
private:
    enum { kZoom0=7, kZoom1=8, kZoom2=9 };
    TBitSet   m_bitSet0;//todo: 使用同一块内存地址的不同区域？使用同一个逻辑区域？
    TBitSet   m_bitSet1;
    TBitSet   m_bitSet2;
    size_t    m_bitSetMask0;
    
    inline size_t hash0(uint32_t key)const { return (key^(key>>16))&m_bitSetMask0; }
    inline size_t hash1(uint32_t key)const { return key%m_bitSet1.size(); }
    inline size_t hash2(uint32_t key)const { return _hash2(key)%m_bitSet2.size(); }
    static uint32_t _hash2(uint32_t key){//from: https://gist.github.com/badboy/6267743
        int c2=0x27d4eb2d; // a prime or an odd constant
        key = (key ^ 61) ^ (key >> 16);
        key = key + (key << 3);
        key = key ^ (key >> 4);
        key = key * c2;
        key = key ^ (key >> 15);
        return key;
    }
    static size_t getMask(size_t count){
        unsigned int bit=8;
        for (;(((size_t)1<<bit)<count) && (bit<sizeof(size_t)*8); ++bit){}
        if (bit==sizeof(size_t)*8)
            throw std::runtime_error("TBloomFilter::getMask() error!");
        return ((size_t)1<<bit)-1;
    }
};

#endif /* bloom_filter_h */
