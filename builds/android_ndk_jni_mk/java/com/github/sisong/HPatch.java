package com.github.sisong;

public class HPatch{

    // load libhpatchz.so
    static { System.loadLibrary("hpatchz"); }

    //patch:
    //  return THPatchResult, 0 is ok, other is error;
    //  cacheMemory:
    //    cacheMemory is used for file IO, different cacheMemory only affects patch speed;
    //    recommended 64*1024,256*1024,1024*1024,...
    //    NOTE: if diffFile created by $xdelta3(-S,-S lzma),$open-vcdiff, alloc memory=cacheMemory+sourceWindowSize+targetWindowSize;
    //        ( sourceWindowSize & targetWindowSize is set when diff, C API getVcDiffInfo() can got values from diffFile. )
    //  if need VCDIFF (xdelta3 or open-vcdiff) format support, build .so with VCD=1 can opened;
    //  if need bsdiff4 & endsley/bsdiff format support, build .so with BSD=1 BZIP2=1 can opened;
    //  if need supported oldFileName or outNewFileName is directory, build .so with DIR=1 can opened;
    //  threadNum: default not used; build .so with MT=1 can opened multi-thread patching; now only support single compressed diffData(created by hdiffz -SD)
    public static native int patch(String oldFileName,String diffFileName,String outNewFileName,long cacheMemory,int _threadNum);
    public static final int patch(String oldFileName,String diffFileName,String outNewFileName,long cacheMemory){
        return patch(oldFileName,diffFileName,outNewFileName,cacheMemory,1);
    }
    public static final int patch(String oldFileName,String diffFileName,String outNewFileName){
        return patch(oldFileName,diffFileName,outNewFileName,256*1024,1);
    }
}
