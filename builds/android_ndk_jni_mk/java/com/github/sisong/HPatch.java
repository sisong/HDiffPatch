package com.github.sisong;

public class HPatch{
    // return THPatchResult, 0 is ok
    //  'diffFileName' file is create by hdiffz app,or by create_compressed_diff(),create_compressed_diff_stream()
    //      or by create_single_compressed_diff(),create_single_compressed_diff_stream()
    //  cacheMemory recommended 256*1024 1024*1024...
    public static native int patch(String oldFileName,String diffFileName,
                                   String outNewFileName,long cacheMemory);
}
