package com.github.sisong;

public class HPatch{
    // return THPatchResult, 0 is ok
    //  'diffFileName' file is create by hdiffz app,or by create_compressed_diff(),create_compressed_diff_stream()
    public static native int patch(String oldFileName,String diffFileName,
                                   String outNewFileName,long cacheMemory);
}
