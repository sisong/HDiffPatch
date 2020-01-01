// hpatch_jni.c
// Created by sisong on 2019-09-30.
#include <jni.h>
#include "hpatch.h"
#ifdef __cplusplus
extern "C" {
#endif
    
    JNIEXPORT int
    Java_com_github_sisong_HPatch_patch(JNIEnv* jenv,jobject jobj,
                                        jstring oldFileName,jstring diffFileName,
                                        jstring outNewFileName,jlong cacheMemory){
        const char* cOldFileName    = (*jenv)->GetStringUTFChars(jenv,oldFileName, NULL);
        const char* cDiffFileName  = (*jenv)->GetStringUTFChars(jenv,diffFileName, NULL);
        const char* cOutNewFileName = (*jenv)->GetStringUTFChars(jenv,outNewFileName, NULL);
        size_t cCacheMemory=(size_t)cacheMemory;
        assert((jlong)cCacheMemory==cacheMemory);
        int result=hpatchz(cOldFileName,cDiffFileName,cOutNewFileName,cCacheMemory);
        (*jenv)->ReleaseStringUTFChars(jenv,outNewFileName,cOutNewFileName);
        (*jenv)->ReleaseStringUTFChars(jenv,diffFileName,cDiffFileName);
        (*jenv)->ReleaseStringUTFChars(jenv,oldFileName,cOldFileName);
        return result;
    }
    
#ifdef __cplusplus
}
#endif

