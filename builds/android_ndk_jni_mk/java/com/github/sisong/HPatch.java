package com.github.sisong;

public class HPatch{

    // auto load libhpatchz.so ?
    // static { System.loadLibrary("hpatchz"); }

    //patch:
    //  return THPatchResult, 0 is ok
    //  cacheMemory:
    //    cacheMemory is used for file IO, different cacheMemory only affects patch speed;
    //    recommended 256*1024,1024*1024,... if cacheMemory<0 then default 256*1024; 
    //    NOTE: if diffFile created by $xdelta3(-S,-S lzma),$open-vcdiff, alloc memory+=sourceWindowSize+targetWindowSize;
    //        ( sourceWindowSize & targetWindowSize is set when diff, C API getVcDiffInfo() can got values from diffFile. )
    //    if diffFile created by $bsdiff4, and patch very slow,
    //      then cacheMemory recommended oldFileSize+256*1024;
    public static native int patch(String oldFileName,String diffFileName,String outNewFileName,long cacheMemory);
    public static final int patch(String oldFileName,String diffFileName,String outNewFileName){
        return patch(oldFileName,diffFileName,outNewFileName,-1);
    }
}
