// hpatch.c
// Created by sisong on 2019-12-30.
#include "hpatch.h"

#define _IS_USED_MULTITHREAD            0
#define _IS_NEED_DIR_DIFF_PATCH         0
#define _IS_NEED_MAIN                   0
#define _IS_NEED_ORIGINAL               0
#define _IS_NEED_SFX                    0
#define _IS_NEED_ALL_CompressPlugin     0
#define _IS_NEED_DEFAULT_CompressPlugin 0
#include "../../hpatchz.c"

int hpatchz(const char *oldFileName,const char *diffFileName,
            const char *outNewFileName, size_t cacheMemory){
    return hpatch(oldFileName,diffFileName,outNewFileName,
                  hpatch_FALSE,hpatch_FALSE,cacheMemory,0,0);
}
