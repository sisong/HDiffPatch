// hpatch.h
// Created by sisong on 2019-12-30.
#ifndef hpatch_h
#define hpatch_h
#include <assert.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
    #define H_PATCH_EXPORT __attribute__((visibility("default")))
    
    // return THPatchResult, 0 is ok
    //  'diffFileName' file is create by hdiffz app,or by create_compressed_diff(),create_compressed_diff_stream()
    int hpatchz(const char *oldFileName,const char *diffFileName,
                const char *outNewFileName, size_t cacheMemory) H_PATCH_EXPORT;

#ifdef __cplusplus
}
#endif
#endif // hpatch_h
