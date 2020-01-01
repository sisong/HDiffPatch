// hpatch_jni.cpp
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
        const char* cOldFileName    = jenv->GetStringUTFChars(oldFileName, NULL);
        const char* cDiffFileName  = jenv->GetStringUTFChars(diffFileName, NULL);
        const char* cOutNewFileName = jenv->GetStringUTFChars(outNewFileName, NULL);
        size_t cCacheMemory=(size_t)cacheMemory;
        assert((jlong)cCacheMemory==cacheMemory);
        int result=hpatch(cOldFileName,cDiffFileName,cOutNewFileName,cCacheMemory);
        jenv->ReleaseStringUTFChars(outNewFileName,cOutNewFileName);
        jenv->ReleaseStringUTFChars(diffFileName,cDiffFileName);
        jenv->ReleaseStringUTFChars(oldFileName,cOldFileName);
        return result;
    }
    
#ifdef __cplusplus
}
#endif
