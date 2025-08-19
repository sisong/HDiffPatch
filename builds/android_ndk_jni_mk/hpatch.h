// hpatch.h
// Created by sisong on 2019-12-30.
#ifndef hpatch_h
#define hpatch_h
#include <assert.h>
#include <string.h>
#include <stdint.h> //int64_t
#ifdef __cplusplus
extern "C" {
#endif
    #ifndef H_PATCH_EXPORT
    #   if ( __GNUC__ >= 4 )
        #define H_PATCH_EXPORT __attribute__((visibility("default")))
    #   else
        #define H_PATCH_EXPORT
    #   endif
    #endif

    // return THPatchResult, 0 is ok
    //  threadNum: 1..5
    //    multi-thread (threadNum>1) patching now only support single compressed diffData(created by hdiffz -SD-stepSize);
    //  cacheMemory:
    //    cacheMemory is used for file IO, different cacheMemory only affects patch speed;
    //    recommended 256*1024,1024*1024,... if cacheMemory<0 then default 256*1024;
    int hpatchz(const char *oldFileName,const char *diffFileName,
                const char *outNewFileName,int64_t cacheMemory,size_t threadNum) H_PATCH_EXPORT;

#ifdef __cplusplus
}
#endif
#endif // hpatch_h
