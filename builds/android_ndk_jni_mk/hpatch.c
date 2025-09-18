// hpatch.c
// Created by sisong on 2019-12-30.
#include "hpatch.h"

#define _IS_NEED_PRINT_LOG              0
#define _IS_NEED_MAIN                   0
#define _IS_NEED_CMDLINE                0
#define _IS_NEED_SFX                    0
#define _IS_NEED_ALL_CompressPlugin     0
#define _IS_NEED_ALL_ChecksumPlugin     0
#include "../../hpatchz.c"

#ifdef _CompressPlugin_bz2
# ifdef BZ_NO_STDIO
#ifdef __cplusplus
extern "C" {
#endif
void bz_internal_error(int errcode){
    LOG_ERR("\n\nbzip2 v%s: internal error number %d.\n",
            BZ2_bzlibVersion(),errcode);
    exit(HPATCH_DECOMPRESSER_DECOMPRESS_ERROR);
}
#ifdef __cplusplus
}
#endif
# endif
#endif

    static inline size_t limitCacheMemory(int64_t cacheMemory){
        #define _kPatchCacheSize_min  (1024*256)
        #define _kPatchCacheSize_max  (((size_t)1)<<((sizeof(size_t)>4)?32:30))
        if (cacheMemory<=_kPatchCacheSize_min) return _kPatchCacheSize_min;
        return (size_t)(((uint64_t)cacheMemory<_kPatchCacheSize_max)?cacheMemory:_kPatchCacheSize_max);
    }

int hpatchz(const char *oldFileName,const char *diffFileName,
            const char *outNewFileName,int64_t cacheMemory,size_t threadNum){
#if (_IS_NEED_DIR_DIFF_PATCH)
    const hpatch_BOOL isDirDiff=getIsDirDiffFile(diffFileName);
    if (isDirDiff){
        TDirPatchChecksumSet checksumSet={0,hpatch_FALSE,hpatch_TRUE,hpatch_TRUE,hpatch_FALSE};
        return hpatch_dir(oldFileName,diffFileName,outNewFileName,
                          hpatch_FALSE,limitCacheMemory(cacheMemory),kMaxOpenFileNumber_default_patch,
                          &checksumSet,&defaultPatchDirlistener,0,0,threadNum);
    } else
#endif
        return hpatch(oldFileName,diffFileName,outNewFileName,
                      hpatch_FALSE,limitCacheMemory(cacheMemory),0,0,1,1,threadNum);
}
