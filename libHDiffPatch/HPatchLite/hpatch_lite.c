//hpatch_lite.c
//
/*
 The MIT License (MIT)
 Copyright (c) 2020-2022 HouSisong All Rights Reserved.
*/
#include "hpatch_lite.h"
#include "hpatch_lite_input_cache.h"

#define _hpi_FALSE  hpi_FALSE
//hpi_size_t __debug_check_false_x=0; //for debug
//#define _hpi_FALSE (1/__debug_check_false_x)

#if (_IS_RUN_MEM_SAFE_CHECK)
#   define __RUN_MEM_SAFE_CHECK
#endif

#define _CHECK(code)                { if (!(code)) return _hpi_FALSE; }

#ifdef __RUN_MEM_SAFE_CHECK
#   define  _SAFE_CHECK(checkv)     _CHECK(checkv)
#else
#   define  _SAFE_CHECK(checkv)
#endif

#define _TInputCache            _hpi_TInputCache
#define _cache_read_1byte       _hpi_cache_read_1byte
#define _cache_update           _hpi_cache_update
#define _cache_success_finish   _hpi_cache_success_finish

hpi_BOOL _cache_update(struct _TInputCache* self){
    //    [                    cache  buf                        ]
    hpi_size_t len=self->cache_end;
    assert(len==self->cache_begin); //empty
    if (!self->read_code(self->inputStream,self->cache_buf,&len))
        len=0;
    //    |                                   len|               |
    self->cache_begin=0;
    self->cache_end=len;
    return len!=0;
}

hpi_fast_uint8 _cache_read_1byte(struct _TInputCache* self){
    if (self->cache_begin!=self->cache_end){
__cache_read_1byte:
        return self->cache_buf[self->cache_begin++];
    }
    if(_cache_update(self))
        goto __cache_read_1byte;
    else
        return 0;
}

static hpi_pos_t _cache_unpackUInt(_TInputCache* self,hpi_pos_t v,hpi_fast_uint8 isNext){
    while (isNext){
        hpi_fast_uint8 b=_cache_read_1byte(self);
        v=(v<<7)|(b&127);
        isNext=b>>7;
    }
    return v;
}

static hpi_BOOL _patch_copy_diff(hpatchi_listener_t* out_newData,
                                 _TInputCache* diff,hpi_pos_t copyLength){
    while (copyLength>0){
        hpi_size_t decodeStep=diff->cache_end-diff->cache_begin;
        if (decodeStep==0){
            _CHECK(_cache_update(diff));
            decodeStep=diff->cache_end-diff->cache_begin;
        }
        if (decodeStep>copyLength)
            decodeStep=(hpi_size_t)copyLength;
        _CHECK(out_newData->write_new(out_newData,diff->cache_buf+diff->cache_begin,decodeStep));
        diff->cache_begin+=decodeStep;
        copyLength-=(hpi_pos_t)decodeStep;
    }
    return hpi_TRUE;
}

    static hpi_force_inline void addData(hpi_byte* dst,const hpi_byte* src,hpi_size_t length){
        while (length--) { *dst++ += *src++; } }
static hpi_BOOL _patch_add_old_withClip(hpatchi_listener_t* old_and_new,_TInputCache* diff,hpi_BOOL isNotNeedSubDiff,
                                        hpi_size_t oldPos,hpi_size_t addLength,hpi_byte* temp_cache){
    while (addLength>0){
        hpi_size_t decodeStep=diff->cache_end;
        if (!isNotNeedSubDiff){
            decodeStep-=diff->cache_begin;
            if (decodeStep==0){
                _cache_update(diff);
                decodeStep=diff->cache_end-diff->cache_begin;
            }
        }
        _CHECK(decodeStep>0);
        if (decodeStep>addLength)
            decodeStep=(hpi_size_t)addLength;
        _CHECK(old_and_new->read_old(old_and_new,oldPos,temp_cache,(hpi_pos_t)decodeStep));
        if (!isNotNeedSubDiff){
            addData(temp_cache,diff->cache_buf+diff->cache_begin,decodeStep);
            diff->cache_begin+=decodeStep;
        }
        _CHECK(old_and_new->write_new(old_and_new,temp_cache,decodeStep));
        oldPos+=decodeStep;
        addLength-=decodeStep;
    }
    return hpi_TRUE;
}

static hpi_pos_t _hpi_readSize(const hpi_byte* buf,hpi_size_t len){
    hpi_pos_t v=0;
    while(len--){
        v=(v<<8)|(hpi_pos_t)buf[len];
    }
    return v;
}

hpi_BOOL hpatch_lite_open(hpi_TInputStreamHandle diff_data,hpi_TInputStream_read read_diff,
                          hpi_compressType* out_compress_type,hpi_pos_t* out_newSize,hpi_pos_t* out_uncompressSize){
    #define kHPatchLite_versionCode     1
    hpi_size_t lenn=hpi_kHeadSize;
    hpi_size_t lenu;
    hpi_byte   buf[(hpi_kHeadSize>sizeof(hpi_pos_t))?hpi_kHeadSize:sizeof(hpi_pos_t)];
    
    _CHECK(read_diff(diff_data,buf,&lenn));
    //HPatchLite type tag 2byte, version code(high 2bit)
    lenu=buf[3];
    _SAFE_CHECK((lenn==hpi_kHeadSize)&(buf[0]=='h')&(buf[1]=='I')&((lenu>>6)==kHPatchLite_versionCode));
    *out_compress_type=buf[2]; //compress type
    lenn=lenu&7; //newSize bytes(low 3bit)
    lenu=(lenu>>3)&7; //uncompressSize bytes(mid 3bit)

    _SAFE_CHECK((lenn<=sizeof(hpi_pos_t))&(lenu<=sizeof(hpi_pos_t)));
    _CHECK(read_diff(diff_data,buf,&lenn));
    *out_newSize=_hpi_readSize(buf,lenn);
    _CHECK(read_diff(diff_data,buf,&lenu));
    *out_uncompressSize=_hpi_readSize(buf,lenu);
    return hpi_TRUE;
}

hpi_BOOL hpatch_lite_patch(hpatchi_listener_t* listener,hpi_pos_t newSize,
                           hpi_byte* temp_cache,hpi_size_t temp_cache_size){
    _TInputCache  diff;
    hpi_pos_t  coverCount;
    hpi_pos_t  newPosBack=0;
    hpi_pos_t  oldPosBack=0;
    {
        assert(temp_cache);
        _SAFE_CHECK(temp_cache_size>=hpi_kMinCacheSize);
        temp_cache_size>>=1;
        diff.cache_begin=temp_cache_size;
        diff.cache_end=temp_cache_size;
        diff.cache_buf=temp_cache+temp_cache_size;
        diff.inputStream=listener->diff_data;
        diff.read_code=listener->read_diff;
    }
    coverCount=_cache_unpackUInt(&diff,0,hpi_TRUE);

    while (coverCount--){
        hpi_pos_t cover_oldPos;
        hpi_pos_t cover_newPos;
        hpi_pos_t cover_length;
        hpi_BOOL  isNotNeedSubDiff;
        {//read cover
            cover_length=_cache_unpackUInt(&diff,0,hpi_TRUE);
            {
                hpi_fast_uint8 tag=_cache_read_1byte(&diff);
                cover_oldPos=_cache_unpackUInt(&diff,tag&31,tag&(1<<5));
                isNotNeedSubDiff=(tag>>7);
                if (tag&(1<<6))
                    cover_oldPos=oldPosBack-cover_oldPos;
                else
                    cover_oldPos+=oldPosBack;
            }
            cover_newPos=_cache_unpackUInt(&diff,0,hpi_TRUE);
            cover_newPos+=newPosBack;
        }

        _SAFE_CHECK(cover_newPos>=newPosBack);
        if (newPosBack<cover_newPos)
            _CHECK(_patch_copy_diff(listener,&diff,cover_newPos-newPosBack));
        _CHECK(_patch_add_old_withClip(listener,&diff,isNotNeedSubDiff,cover_oldPos,cover_length,temp_cache));
        newPosBack=cover_newPos+cover_length;
        oldPosBack=cover_oldPos+cover_length;
        _SAFE_CHECK((cover_length>0)|(coverCount==0));
    }
    return (newSize==newPosBack)&_cache_success_finish(&diff);
}



// inplace hpatchi by extra

hpi_BOOL hpatchi_inplace_open(hpi_TInputStreamHandle diff_data,hpi_TInputStream_read read_diff,
                              hpi_compressType* out_compress_type,hpi_pos_t* out_newSize,
                              hpi_pos_t* out_uncompressSize,hpi_size_t* out_extraSafeSize){
    #define kHPatchLite_inplaceCode     2
    hpi_size_t lenn=hpi_kInplaceHeadSize;
    hpi_size_t lenu,lene;
    hpi_byte   buf[(hpi_kInplaceHeadSize>sizeof(hpi_pos_t))?hpi_kInplaceHeadSize:sizeof(hpi_pos_t)];
    
    _CHECK(read_diff(diff_data,buf,&lenn));
    //HPatchLite type tag 2byte, version code(high 2bit)
    lenu=buf[3];
    _SAFE_CHECK((lenn==hpi_kInplaceHeadSize)&(buf[0]=='h')&(buf[1]=='I')&((lenu>>6)==kHPatchLite_inplaceCode));
    *out_compress_type=buf[2]; //compress type
    lenn=lenu&7; //newSize bytes(low 3bit)
    lenu=(lenu>>3)&7; //uncompressSize bytes(mid 3bit)
    lene=buf[4];

    _SAFE_CHECK((lenn<=sizeof(hpi_pos_t))&(lenu<=sizeof(hpi_pos_t))&(lene<=sizeof(hpi_size_t)));
    _CHECK(read_diff(diff_data,buf,&lenn));
    *out_newSize=_hpi_readSize(buf,lenn);
    _CHECK(read_diff(diff_data,buf,&lenu));
    *out_uncompressSize=_hpi_readSize(buf,lenu);
    _CHECK(read_diff(diff_data,buf,&lene));
    *out_extraSafeSize=(hpi_size_t)_hpi_readSize(buf,lene);
    return hpi_TRUE;
}



#if (_IS_WTITE_NEW_BY_PAGE!=0)
#   define _hpi_Do4Page(code)   do{ code; }while(0)
#else
#   define _hpi_Do4Page(code)
#endif //_IS_WTITE_NEW_BY_PAGE

//hpatchi_listener_extra_t: a first-in-first-out ring buffer
//  always cache kWriteExtraSize bytes latest data for _wrap_listener->write_new;
typedef struct hpatchi_listener_extra_t{
    hpatchi_listener_t      base;
    hpatchi_listener_t*     _wrap_listener;
    hpi_byte*  write_extra;
    hpi_size_t kWriteExtraSize;
    hpi_size_t cached_data_pos;
    hpi_size_t cached_data_len;
    #if (_IS_WTITE_NEW_BY_PAGE!=0)
    hpi_size_t kPageSize;
    hpi_size_t page_data_len; //cur page data length
    #endif
} hpatchi_listener_extra_t;

static hpi_BOOL _hpatchi_listener_extra_read_old(hpatchi_listener_t* listener,hpi_pos_t read_from_pos,hpi_byte* out_data,hpi_size_t data_size){
    hpatchi_listener_extra_t* self=(hpatchi_listener_extra_t*)listener;
    return self->_wrap_listener->read_old(self->_wrap_listener,read_from_pos,out_data,data_size);
}

#if (_IS_WTITE_NEW_BY_PAGE!=0)
    static hpi_BOOL _hpatchi_listener_page_out(hpatchi_listener_extra_t* self){
        hpi_BOOL result=self->_wrap_listener->write_new(self->_wrap_listener,
                            self->write_extra+self->kWriteExtraSize,self->page_data_len);
        self->page_data_len=0;
        return result;
    }

    static hpi_force_inline
    hpi_BOOL _hpatchi_listener_write_page(hpatchi_listener_extra_t* self,const hpi_byte* data,hpi_size_t data_size){
        hpi_byte* page_buf=self->write_extra+self->kWriteExtraSize;
        while (data_size){
            hpi_size_t cur_len=self->kPageSize-self->page_data_len;
            if (cur_len==0){ //need out cur page
                if (!_hpatchi_listener_page_out(self)) return _hpi_FALSE;
                cur_len=self->kPageSize;
            }
            cur_len=(cur_len<=data_size)?cur_len:data_size;
            data_size-=cur_len;
            while (cur_len--)//save data to page
                page_buf[self->page_data_len++]=*data++;
        }
        return hpi_TRUE;
    }
#endif //_IS_WTITE_NEW_BY_PAGE

static hpi_BOOL _hpatchi_listener_extra_out(hpatchi_listener_extra_t* self,hpi_size_t out_size){
    assert(out_size<=self->cached_data_len);
    while (out_size){
        hpi_size_t cur_len=self->kWriteExtraSize-self->cached_data_pos;
        cur_len=(cur_len<=out_size)?cur_len:out_size;
    #if (_IS_WTITE_NEW_BY_PAGE!=0)
        if (!_hpatchi_listener_write_page(self,self->write_extra+self->cached_data_pos,cur_len))
            return _hpi_FALSE;
    #else
        if (!self->_wrap_listener->write_new(self->_wrap_listener,self->write_extra+self->cached_data_pos,cur_len))
            return _hpi_FALSE;
    #endif //_IS_WTITE_NEW_BY_PAGE
        self->cached_data_pos+=cur_len;
        self->cached_data_pos-=(self->cached_data_pos<self->kWriteExtraSize)?0:self->kWriteExtraSize;
        self->cached_data_len-=cur_len;
        out_size-=cur_len;
    }
    return hpi_TRUE;
}

static hpi_force_inline
hpi_BOOL _hpatchi_listener_extra_flush(hpatchi_listener_extra_t* self){
    hpi_BOOL result=_hpatchi_listener_extra_out(self,self->cached_data_len);
#if (_IS_WTITE_NEW_BY_PAGE!=0)
    if (result&&(self->page_data_len))
        result=_hpatchi_listener_page_out(self);
#endif //_IS_WTITE_NEW_BY_PAGE
    return result;
}

static hpi_BOOL _hpatchi_listener_extra_write_new(hpatchi_listener_t* listener,const hpi_byte* data,hpi_size_t data_size){
    hpatchi_listener_extra_t* self=(hpatchi_listener_extra_t*)listener;
    while (data_size){
        hpi_size_t posi;
        hpi_size_t cur_len=data_size+self->cached_data_len;
        if (cur_len>self->kWriteExtraSize){//need out cached data by _wrap_listener->write_new()
            cur_len-=self->kWriteExtraSize;
            cur_len=(cur_len<=self->cached_data_len)?cur_len:self->cached_data_len;
            if (!_hpatchi_listener_extra_out(self,cur_len)) return _hpi_FALSE;
        }
        assert(self->cached_data_len<self->kWriteExtraSize);

        cur_len=self->kWriteExtraSize-self->cached_data_len;
        cur_len=(cur_len<=data_size)?cur_len:data_size;
        data_size-=cur_len;
        posi=self->cached_data_pos+self->cached_data_len;
        self->cached_data_len+=cur_len;
        while (cur_len--){//save data to cache
            posi-=(posi<self->kWriteExtraSize)?0:self->kWriteExtraSize;
            self->write_extra[posi++]=*data++;
        }
    }
    return hpi_TRUE;
}

static hpi_inline
void _hpatchi_listener_extra_init(hpatchi_listener_extra_t* self,hpatchi_listener_t* wrap_listener,
                                  hpi_byte* write_extra,hpi_size_t kWriteExtraSize){
    self->base.diff_data=wrap_listener->diff_data;
    self->base.read_diff=wrap_listener->read_diff; 
    self->base.read_old=_hpatchi_listener_extra_read_old;
    self->base.write_new=_hpatchi_listener_extra_write_new; 
    self->_wrap_listener=wrap_listener;
    self->write_extra=write_extra;
    self->kWriteExtraSize=kWriteExtraSize;
    self->cached_data_pos=0;
    self->cached_data_len=0;
}

#if (_IS_WTITE_NEW_BY_PAGE!=0)
hpi_BOOL hpatchi_inplaceB_by_page(hpatchi_listener_t* listener,hpi_pos_t newSize,hpi_byte* temp_cache,
                                  hpi_size_t extraSafeSize,hpi_size_t pageSize,hpi_size_t temp_cache_size){
#else //_IS_WTITE_NEW_BY_PAGE==0
hpi_BOOL hpatchi_inplaceB(hpatchi_listener_t* listener,hpi_pos_t newSize,
                          hpi_byte* temp_cache,hpi_size_t extraSafeSize,hpi_size_t temp_cache_size){
    const hpi_size_t pageSize=0;
#endif //_IS_WTITE_NEW_BY_PAGE
    hpi_BOOL result;
    hpatchi_listener_extra_t extra_listener;
    const hpi_size_t needExtra=extraSafeSize+pageSize;
    assert(temp_cache_size>=hpi_kMinInplaceCacheSize+extraSafeSize+pageSize);
    _hpi_Do4Page(assert(pageSize>0));
    if (needExtra){
        hpi_size_t cache_size=(temp_cache_size-needExtra)>>1;
        extraSafeSize=(extraSafeSize>=cache_size)?extraSafeSize:cache_size;//for small extraSafeSize
    }
    _hpatchi_listener_extra_init(&extra_listener,listener,temp_cache,extraSafeSize);
    _hpi_Do4Page(extra_listener.kPageSize=pageSize);
    _hpi_Do4Page(extra_listener.page_data_len=0);
    _hpi_Do4Page(extraSafeSize+=pageSize);//page buf for page data
    result=hpatch_lite_patch(needExtra?&extra_listener.base:listener,newSize,
                             temp_cache+extraSafeSize,temp_cache_size-extraSafeSize);
    return result&&_hpatchi_listener_extra_flush(&extra_listener);
}
