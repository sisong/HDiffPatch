package com.github.sisong;

public class HPatch{

    // auto load libhpatchz.so ?
    // static { System.loadLibrary("hpatchz"); }

    // return THPatchResult, 0 is ok
    //  different cacheMemory only affects patch speed, if cacheMemory<0 then default 256*1024;
    //  if diffFile created by hdiffz,hdiffz -SD,hdiffz -VCD,bsdiff4,hdiffz -BSD,
    //     then cacheMemory recommended 256*1024,1024*1024,...
    //  if diffFile created by xdelta3(-S,-S lzma),open-vcdiff, and patch very slow,
    //     then cacheMemory recommended sourceWindowSize+targetWindowSize+256*1024;
    //       ( window sizes is set when diff, C API getVcDiffInfo() can got values. )
    //  if diffFile created by bsdiff4, and patch very slow,
    //     then cacheMemory recommended oldFileSize+256*1024;
    public static native int patch(String oldFileName,String diffFileName,
                                   String outNewFileName,long cacheMemory);

    public static int patch(String oldFileName,String diffFileName,String outNewFileName){
        return patch(oldFileName,diffFileName,outNewFileName,-1);
    }
}
