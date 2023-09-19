// hpatch.c
// Created by sisong on 2019-12-30.
#include "hpatch.h"

#define _IS_NEED_PRINT_LOG              0
#ifndef _IS_USED_MULTITHREAD
#define _IS_USED_MULTITHREAD            0
#endif
#define _IS_NEED_DIR_DIFF_PATCH         0
#define _IS_NEED_MAIN                   0
#define _IS_NEED_CMDLINE                0
#define _IS_NEED_SFX                    0
#define _IS_NEED_ALL_CompressPlugin     0
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

int hpatchz(const char *oldFileName,const char *diffFileName,
            const char *outNewFileName, size_t cacheMemory){
    return hpatch(oldFileName,diffFileName,outNewFileName,
                  hpatch_FALSE,cacheMemory,0,0,1,1);
}
