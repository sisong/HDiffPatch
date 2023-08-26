//
//  hpatch_objc.h
//  hpatchz
//
//  Created by hss on 2023-08-25.
//

#import <Foundation/Foundation.h>

@interface hpatcher : NSObject

//  return THPatchResult, 0 is ok
//  cacheMemory:
//    cacheMemory is used for file IO, different cacheMemory only affects patch speed;
//    recommended 256*1024,1024*1024,... if cacheMemory<0 then default 256*1024;
//    NOTE: if diffFile created by $xdelta3(-S,-S lzma),$open-vcdiff, alloc memory+=sourceWindowSize+targetWindowSize;
//        ( sourceWindowSize & targetWindowSize is set when diff, C API getVcDiffInfo() can got values from diffFile. )
//    if diffFile created by $bsdiff4, and patch very slow,
//      then cacheMemory recommended oldFileSize+256*1024;

+ (int)patchWithOld:(NSString *)oldFileName
           withDiff:(NSString *)diffFileName
              toNew:(NSString *)outNewFileName;

+ (int)patchWithOld:(NSString *)oldFileName
           withDiff:(NSString *)diffFileName
              toNew:(NSString *)outNewFileName
           byMemory:(int64_t)cacheMemory;

@end
