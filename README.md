# [HDiffPatch](https://github.com/sisong/HDiffPatch)
[![release](https://img.shields.io/badge/release-v4.0.0-blue.svg)](https://github.com/sisong/HDiffPatch/releases) 
[![license](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/sisong/HDiffPatch/blob/master/LICENSE) 
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-blue.svg)](https://github.com/sisong/HDiffPatch/pulls)
[![+issue Welcome](https://img.shields.io/github/issues-raw/sisong/HDiffPatch?color=green&label=%2Bissue%20welcome)](https://github.com/sisong/HDiffPatch/issues)   

[![Build Status](https://travis-ci.org/sisong/HDiffPatch.svg?branch=master)](https://travis-ci.org/sisong/HDiffPatch) 
[![Build status](https://ci.appveyor.com/api/projects/status/t9ow8dft8lt898cv/branch/master?svg=true)](https://ci.appveyor.com/project/sisong/hdiffpatch/branch/master)   

a C\C++ library and command-line tools for Diff & Patch between binary files or directories(folder); cross-platform; run fast; create small delta/differential; support large files and limit memory requires when diff & patch.   
   
( NOTE: This library does not deal with file metadata, such as file last wirte time, permissions, link file, etc... To this library, a file is just as a stream of bytes; You can extend this library or use other tools. )   
( update Android Apk? Jar or Zip file diff & patch? try [ApkDiffPatch](https://github.com/sisong/ApkDiffPatch)!    
 but ApkDiffPath can't be used in the Android app store, because it requires re-signing apk;   
[sfpatcher](https://github.com/sisong/sfpatcher) like [archive-patcher](https://github.com/google/archive-patcher), is designed for Android app store, but patch is much faster than archive-patcher. )   
   
---
## Releases/Binaries
[Download from latest release](https://github.com/sisong/HDiffPatch/releases) : Command line app for Windows, Linux, MacOS; and .so patch lib for Android.     
( release files build by projects in path `HDiffPatch/builds` )   

## Builds
`$ cd <dir>/HDiffPatch`   
if on linux or macos, try :   
`$ make LZMA=0 ZSTD=0 MD5=0`   
or    
```
$ git clone https://github.com/sisong/lzma.git   ../lzma
$ git clone https://github.com/facebook/zstd.git ../zstd
$ git clone https://github.com/sisong/libmd5.git ../libmd5
$ make
```    
   
if on windows, befor compile `builds/vc/HDiffPatch.sln` by `Visual Studio` 
```
$ git clone https://github.com/sisong/lzma.git   ../lzma
$ git clone https://github.com/facebook/zstd.git ../zstd
$ git clone https://github.com/sisong/libmd5.git ../libmd5
$ git clone https://github.com/sisong/zlib.git   ../zlib
$ git clone https://github.com/sisong/bzip2.git  ../bzip2
```
   
build libhpatchz.so for android:   
* install Android NDK
* `$ cd <dir>/HDiffPatch/builds/android_ndk_jni_mk`
* `$ build_libs.sh`   (or `$ build_libs.bat`, then got \*.so files)
* import file `com/github/sisong/HPatch.java` (from `HDiffPatch/builds/android_ndk_jni_mk/java/`) & .so files, java code can call the patch function in libhpatchz.so
   
## diff command line usage:   
diff     usage: **hdiffz** [options] **oldPath newPath outDiffFile**   
compress usage: **hdiffz** [-c-...]  **"" newPath outDiffFile**   
test    usage: **hdiffz**    -t     **oldPath newPath testDiffFile**   
resave  usage: **hdiffz** [-c-...]  **diffFile outDiffFile**   
get  manifest: **hdiffz** [-g#...] [-C-checksumType] **inputPath -M#outManifestTxtFile**   
manifest diff: **hdiffz** [options] **-M-old#oldManifestFile -M-new#newManifestFile oldPath newPath outDiffFile**   
```
  oldPath newPath inputPath can be file or directory(folder),
  oldPath can empty, and input parameter ""
memory options:
  -m[-matchScore]
      DEFAULT; all file load into Memory; best diffFileSize;
      requires (newFileSize+ oldFileSize*5(or *9 when oldFileSize>=2GB))+O(1)
        bytes of memory;
      matchScore>=0, DEFAULT -m-6, recommended bin: 0--4 text: 4--9 etc...
  -s[-matchBlockSize]
      all file load as Stream; fast;
      requires O(oldFileSize*16/matchBlockSize+matchBlockSize*5)bytes of memory;
      matchBlockSize>=4, DEFAULT -s-64, recommended 16,32,48,1k,64k,1m etc...
special options:
  -SD[-stepSize]
      create single compressed diffData, only need one decompress buffer
      when patch, and support step by step patching when step by step downloading!
      stepSize>=(1024*4), DEFAULT -SD-256k, recommended 64k,2m etc...
  -p-parallelThreadNumber
      if parallelThreadNumber>1 then open multi-thread Parallel mode;
      DEFAULT -p-4; requires more memory!
  -c-compressType[-compressLevel]
      set outDiffFile Compress type & level, DEFAULT uncompress;
      for resave diffFile,recompress diffFile to outDiffFile by new set;
      support compress type & level:
       (re. https://github.com/sisong/lzbench/blob/master/lzbench171_sorted.md )
        -c-zlib[-{1..9}]                DEFAULT level 9
        -c-pzlib[-{1..9}]               DEFAULT level 6
            support run by multi-thread parallel, fast!
            WARNING: code not compatible with it compressed by -c-zlib!
              and code size may be larger than if it compressed by -c-zlib.
        -c-bzip2[-{1..9}]               (or -bz2) DEFAULT level 9
        -c-pbzip2[-{1..9}]              (or -pbz2) DEFAULT level 8
            support run by multi-thread parallel, fast!
            WARNING: code not compatible with it compressed by -c-bzip2!
               and code size may be larger than if it compressed by -c-bzip2.
        -c-lzma[-{0..9}[-dictSize]]     DEFAULT level 7
            dictSize can like 4096 or 4k or 4m or 128m etc..., DEFAULT 8m
            support run by 2-thread parallel.
        -c-lzma2[-{0..9}[-dictSize]]    DEFAULT level 7
            dictSize can like 4096 or 4k or 4m or 128m etc..., DEFAULT 8m
            support run by multi-thread parallel, fast!
            WARNING: code not compatible with it compressed by -c-lzma!
        -c-zstd[-{0..22}[-dictBits]]    DEFAULT level 20
            dictBits can 10--31, DEFAULT 24.
            support run by multi-thread parallel, fast!
  -C-checksumType
      set outDiffFile Checksum type for directory diff, DEFAULT -C-fadler64;
      support checksum type:
        -C-no                   no checksum
        -C-crc32
        -C-fadler64             DEFAULT
        -C-md5
  -n-maxOpenFileNumber
      limit Number of open files at same time when stream directory diff;
      maxOpenFileNumber>=8, DEFAULT -n-48, the best limit value by different
        operating system.
  -g#ignorePath[#ignorePath#...]
      set iGnore path list when Directory Diff; ignore path list such as:
        #.DS_Store#desktop.ini#*thumbs*.db#.git*#.svn/#cache_*/00*11/*.tmp
      # means separator between names; (if char # in name, need write #: )
      * means can match any chars in name; (if char * in name, need write *: );
      / at the end of name means must match directory;
  -g-old#ignorePath[#ignorePath#...]
      set iGnore path list in oldPath when Directory Diff;
      if oldFile can be changed, need add it in old ignore list;
  -g-new#ignorePath[#ignorePath#...]
      set iGnore path list in newPath when Directory Diff;
      in general, new ignore list should is empty;
  -M#outManifestTxtFile
      create a Manifest file for inputPath; it is a text file, saved infos of
      all files and directoriy list in inputPath; this file while be used in
      manifest diff, support re-checksum data by manifest diff;
      can be used to protect historical versions be modified!
  -M-old#oldManifestFile
      oldManifestFile is created from oldPath; if no oldPath not need -M-old;
  -M-new#newManifestFile
      newManifestFile is created from newPath;
  -D  force run Directory diff between two files; DEFAULT (no -D) run
      directory diff need oldPath or newPath is directory.
  -d  Diff only, do't run patch check, DEFAULT run patch check.
  -t  Test only, run patch check, patch(oldPath,testDiffFile)==newPath ?
  -f  Force overwrite, ignore write path already exists;
      DEFAULT (no -f) not overwrite and then return error;
      if used -f and write path is exist directory, will always return error.
  --patch
      swap to hpatchz mode.
  -h or -?
      output Help info (this usage).
  -v  output Version info.
```
   
## patch command line usage:   
patch usage: **hpatchz** [options] **oldPath diffFile outNewPath**   
uncompress usage: **hpatchz** [options] **"" diffFile outNewPath**   
create  SFX: **hpatchz** [-X-exe#selfExecuteFile] **diffFile -X#outSelfExtractArchive**   
run     SFX: **selfExtractArchive** [options] **oldPath -X outNewPath**   
extract SFX: **selfExtractArchive**   (same as: selfExtractArchive -f "" -X "./")
```
memory options:
  -s[-cacheSize]
      DEFAULT -s-64m; oldPath loaded as Stream;
      cacheSize can like 262144 or 256k or 512m or 2g etc....
      requires (cacheSize + 4*decompress buffer size)+O(1) bytes of memory;
      if diffFile is single compressed diffData, then requires
        (oldFileSize+ stepSize + 1*decompress buffer size)+O(1) bytes of memory;
        see: hdiffz -SD-stepSize option.
  -m  oldPath all loaded into Memory;
      requires (oldFileSize + 4*decompress buffer size)+O(1) bytes of memory;
      if diffFile is single compressed diffData, then requires
        (oldFileSize+ stepSize + 1*decompress buffer size)+O(1) bytes of memory.
special options:
  -C-checksumSets
      set Checksum data for directory patch, DEFAULT -C-new-copy;
      checksumSets support (can choose multiple):
        -C-diff         checksum diffFile;
        -C-old          checksum old reference files;
        -C-new          checksum new files edited from old reference files;
        -C-copy         checksum new files copy from old same files;
        -C-no           no checksum;
        -C-all          same as: -C-diff-old-new-copy;
  -n-maxOpenFileNumber
      limit Number of open files at same time when stream directory patch;
      maxOpenFileNumber>=8, DEFAULT -n-24, the best limit value by different
        operating system.
  -f  Force overwrite, ignore write path already exists;
      DEFAULT (no -f) not overwrite and then return error;
      support oldPath outNewPath same path!(patch to tempPath and overwrite old)
      if used -f and outNewPath is exist file:
        if patch output file, will overwrite;
        if patch output directory, will always return error;
      if used -f and outNewPath is exist directory:
        if patch output file, will always return error;
        if patch output directory, will overwrite, but not delete
          needless existing files in directory.
  -h or -?
      output Help info (this usage).
  -v  output Version info.
```
   
---
## library API usage:
all **diff**&**patch** function in file: `libHDiffPatch/HDiff/diff.h` & `libHDiffPatch/HPatch/patch.h`   
**dir_diff()** & **dir patch** in: `dirDiffPatch/dir_diff/dir_diff.h` & `dirDiffPatch/dir_patch/dir_patch.h`   
### manual:
* **create diff**(in newData,in oldData,out diffData);
   release the diffData for update oldData.  
* **patch**(out newData,in oldData,in diffData);
   ok , get the newData. 
### v1 API, uncompressed diffData:
* **create_diff()**
* **patch()**
* **patch_stream()**
* **patch_stream_with_cache()**
### v2 API, compressed diffData:
* **create_compressed_diff()**
* **create_compressed_diff_stream()**
* **resave_compressed_diff()**
* **patch_decompress()**
* **patch_decompress_with_cache()**
* **patch_decompress_mem()**
### v3 API, **diff**&**patch** between directories(folder):
* **dir_diff()**
* **TDirPatcher_\*** functions with **struct TDirPatcher()**
### v4 API, single compressed diffData:
* **create_single_compressed_diff()**
* **create_single_compressed_diff_stream()**
* **resave_single_compressed_diff()**
* **patch_single_stream()**
* **patch_single_stream_mem()**
* **patch_single_compressed_diff()**
* **patch_single_stream_diff()**

---
## HDiffPatch vs BsDiff:
system: macOS10.12.6, compiler: xcode8.3.3 x64, CPU: i7 2.5G(turbo3.7G,6MB L3 cache),SSD Disk,Memroy:8G*2 DDR3 1600MHz   
   (purge file cache before every test)
```
HDiffPatch2.4 hdiffz run by: -m -c-bzip2-9|-c-lzma-7-4m|-c-zlib-9 oldFile newFile outDiffFile
              hpatchz run by: -m oldFile diffFile outNewFile
BsDiff4.3 with bzip2 and all data in memory;
          (NOTE: when compiling BsDiff4.3-x64, suffix string index type int64 changed to int32, 
            faster and memory requires to be halved!)   
=======================================================================================================
         Program               Uncompressed Compressed Compressed  BsDiff             hdiffz
(newVersion<--oldVersion)           (tar)     (bzip2)    (lzma)    (bzip2)    (bzip2   lzma     zlib)
-------------------------------------------------------------------------------------------------------
apache-maven-2.2.1-src <--2.0.11    5150720   1213258    1175464    115723     83935    80997    91921
httpd_2.4.4-netware-bin <--2.2.24  22612480   4035904    3459747   2192308   1809555  1616435  1938953
httpd-2.4.4-src <-- 2.2.24         31809536   4775534    4141266   2492534   1882555  1717468  2084843
Firefox-21.0-mac-en-US.app<--20.0  98740736  39731352   33027837  16454403  15749937 14018095 15417854
emacs-24.3 <-- 23.4               185528320  42044895   33707445  12892536   9574423  8403235 10964939
eclipse-java-juno-SR2-macosx
  -cocoa-x86_64 <--x86_32         178595840 156054144  151542885   1595465   1587747  1561773  1567700
gcc-src-4.8.0 <--4.7.0            552775680  86438193   64532384  11759496   8433260  7288783  9445004
-------------------------------------------------------------------------------------------------------
Average Compression                 100.00%    31.76%     28.47%     6.63%     5.58%    5.01%    5.86%
=======================================================================================================

=======================================================================================================
   Program   run time(Second)   memory(MB)        run time(Second)              memory(MB)
               BsDiff hdiffz  BsDiff  hdiffz   BsPatch       hpatchz        BsPatch     hpatchz 
              (bzip2)(bzip2)  (bzip2)(bzip2)   (bzip2) (bzip2  lzma  zlib)  (bzip2) (bzip2 lzma zlib)
-------------------------------------------------------------------------------------------------------
apache-maven...  1.3   0.4       42     28       0.09    0.04  0.03  0.02       14      8     7     6
httpd bin...     8.6   3.0      148    124       0.72    0.36  0.18  0.13       50     24    23    18
httpd src...    20     5.1      322    233       0.99    0.46  0.24  0.17       78     44    42    37
Firefox...      94    28        829    582       3.0     2.2   1.2   0.57      198    106   106    94
emacs...       109    32       1400   1010       4.9     2.3   1.1   0.78      348    174   168   161
eclipse        100    33       1500   1000       1.5     0.56  0.57  0.50      350    176   174   172
gcc-src...     366    69       4420   3030       7.9     3.5   2.1   1.85     1020    518   517   504
-------------------------------------------------------------------------------------------------------
Average        100%   28.9%    100%   71.5%      100%   52.3% 29.9% 21.3%      100%  52.3% 50.3% 45.5%
=======================================================================================================
```
   
## HDiffPatch vs xdelta:
system: macOS10.12.6, compiler: xcode8.3.3 x64, CPU: i7 2.5G(turbo3.7G,6MB L3 cache),SSD Disk,Memroy:8G*2 DDR3 1600MHz   
   (purge file cache before every test)
```
HDiffPatch2.4 hdiffz run by: -s-128 -c-bzip2-9 oldFile newFile outDiffFile
              hpatchz run by: -s-4m oldFile diffFile outNewFile
xdelta3.1 diff run by: -e -s old_file new_file delta_file   
          patch run by: -d -s old_file delta_file decoded_new_file
         (NOTE fix: xdelta3.1 diff "gcc-src..." fail, add -B 530000000 diff ok,
           out 14173073B and used 1070MB memory!)
=======================================================================================================
   Program              diff       run time(Second)  memory(MB)    patch run time(Second) memory(MB)
                  xdelta3   hdiffz   xdelta3 hdiffz xdelta3 hdiffz  xdelta3  hpatchz   xdelta3 hpatchz
-------------------------------------------------------------------------------------------------------
apache-maven...   116265     83408     0.16   0.13     65    11       0.07    0.06        12      6
httpd bin...     2174098   2077625     1.1    1.2     157    15       0.25    0.65        30      8
httpd src...     2312990   2034666     1.3    1.7     185    15       0.30    0.91        50      8
Firefox...      28451567  27504156    16     11       225    16       2.0     4.1        100      8
emacs...        31655323  12033450    19      9.4     220    33       3.2     4.0         97     10
eclipse          1590860   1636221     1.5    1.2     207    34       0.46    0.49        77      8 
gcc-src...     107003829  12305741    56     19       224    79       9.7     9.5        102     11 
           (fix 14173073)
-------------------------------------------------------------------------------------------------------
Average           12.18%    7.81%     100%  79.0%     100%  15.5%      100%  169.1%      100%  18.9%
              (fix 9.78%)
=======================================================================================================

HDiffPatch2.4 hdiffz run by: -s-64 -c-lzma-7-4m  oldFile newFile outDiffFile
              hpatchz run by: -s-4m oldFile diffFile outNewFile
xdelta3.1 diff run by: -S lzma -9 -s old_file new_file delta_file   
          patch run by: -d -s old_file delta_file decoded_new_file
          (NOTE fix: xdelta3.1 diff "gcc-src..." fail, add -B 530000000 diff ok,
            out 11787978B and used 2639MB memory.)
=======================================================================================================
   Program              diff       run time(Second)  memory(MB)    patch run time(Second) memory(MB)
                  xdelta3   hdiffz   xdelta3 hdiffz xdelta3 hdiffz  xdelta3  hpatchz   xdelta3 hpatchz
-------------------------------------------------------------------------------------------------------
apache-maven...    98434     83668     0.37   0.29    220    24       0.04    0.06         12     5
httpd bin...     1986880   1776553     2.5    2.9     356    59       0.24    0.52         30     8
httpd src...     2057118   1794029     3.3    4.2     375    62       0.28    0.78         50     8
Firefox...      27046727  21882343    27     32       416    76       1.8     2.2         100     9
emacs...        29392254   9698236    38     32       413    97       3.1     2.9          97     9
eclipse          1580342   1589045     3.0    1.9     399    76       0.48    0.48         77     6 
gcc-src...      95991977   9118368   128     44       417   148       8.9     8.6         102    11 
           (fix 11787978)
-------------------------------------------------------------------------------------------------------
Average           11.24%    6.44%     100%  88.9%     100%  20.0%      100%  151.1%       100%  17.3%
              (fix 9.06%)
=======================================================================================================
```
  
---
## Contact
housisong@hotmail.com  

