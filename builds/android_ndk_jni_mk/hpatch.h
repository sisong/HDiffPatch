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
    int hpatchz(const char *oldFileName,const char *diffFileName,
                const char *outNewFileName,int64_t cacheMemory) H_PATCH_EXPORT;

#ifdef __cplusplus
}
#endif
#endif // hpatch_h
