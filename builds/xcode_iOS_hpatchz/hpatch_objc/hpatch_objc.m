//
//  hpatch_objc.m
//  hpatchz
//
//  Created by hss on 2023-08-25.
//

#import "hpatch_objc.h"
#include "../../android_ndk_jni_mk/hpatch.h"


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
    #define HPATCH_OPTIONS_ERROR 1
    const char* old_cstr=(oldFileName==NULL)?0:[oldFileName UTF8String];
    if ((diffFileName==NULL)||(outNewFileName==NULL)) return HPATCH_OPTIONS_ERROR;
    return hpatchz(old_cstr,[diffFileName UTF8String],[outNewFileName UTF8String],cacheMemory);
}

@end
