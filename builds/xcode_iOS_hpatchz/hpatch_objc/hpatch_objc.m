//
//  hpatch_objc.m
//  hpatchz
//
//  Created by hss on 2023-08-25.
//

#import "hpatch_objc.h"
#include "../../android_ndk_jni_mk/hpatch.h"


static size_t getCacheMemory(int64_t cacheMemory){
    #define kPatchCacheSize_default  (1024*256)
    #define kPatchCacheSize_max ((int64_t)((size_t)(~(size_t)0)))
    if (cacheMemory<0) return kPatchCacheSize_default;
    if (sizeof(int64_t)<=sizeof(size_t)) return (size_t)cacheMemory;
    return (size_t)((cacheMemory<kPatchCacheSize_max)?cacheMemory:kPatchCacheSize_max);
}


@implementation hpatcher

+ (int)patchWithOld:(NSString *)oldFileName
           withDiff:(NSString *)diffFileName
              toNew:(NSString *)outNewFileName
{
    return [hpatcher patchWithOld:oldFileName withDiff:diffFileName toNew:outNewFileName byMemory:-1];
}

+ (int)patchWithOld:(NSString *)oldFileName
           withDiff:(NSString *)diffFileName
              toNew:(NSString *)outNewFileName
           byMemory:(int64_t)cacheMemory
{
    size_t cCacheMemory=getCacheMemory(cacheMemory);
    const char* old_cstr=(oldFileName==NULL)?0:[oldFileName UTF8String];
    return hpatchz(old_cstr,[diffFileName UTF8String],[outNewFileName UTF8String],cCacheMemory);
}

@end
