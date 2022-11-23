# [HDiffPatch](https://github.com/sisong/HDiffPatch)
[![release](https://img.shields.io/badge/release-v4.5.0-blue.svg)](https://github.com/sisong/HDiffPatch/releases) 
[![license](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/sisong/HDiffPatch/blob/master/LICENSE) 
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-blue.svg)](https://github.com/sisong/HDiffPatch/pulls)
[![+issue Welcome](https://img.shields.io/github/issues-raw/sisong/HDiffPatch?color=green&label=%2Bissue%20welcome)](https://github.com/sisong/HDiffPatch/issues)   

[![Build Status](https://github.com/sisong/HDiffPatch/workflows/ci/badge.svg?branch=master)](https://github.com/sisong/HDiffPatch/actions?query=workflow%3Aci+branch%3Amaster)
[![Build status](https://ci.appveyor.com/api/projects/status/t9ow8dft8lt898cv/branch/master?svg=true)](https://ci.appveyor.com/project/sisong/hdiffpatch/branch/master)   

a C\C++ library and command-line tools for Diff & Patch between binary files or directories(folder); cross-platform; runs fast; create small delta/differential; support large files and limit memory requires when diff & patch.   
   
( NOTE: This library does not deal with file metadata, such as file last wirte time, permissions, link file, etc... To this library, a file is just as a stream of bytes; You can extend this library or use other tools. )   
( if need patch (OTA) on embedded systems,MCU,NB-IoT..., see demo [HPatchLite](https://github.com/sisong/HPatchLite) )   
( update your own Android Apk? Jar or Zip file diff & patch? try [ApkDiffPatch](https://github.com/sisong/ApkDiffPatch)! )   
( ApkDiffPath can't be used by Android app store, because it requires re-signing apks;   
[sfpatcher](https://github.com/sisong/sfpatcher) (like [archive-patcher](https://github.com/google/archive-patcher)) not require re-signing apks, is designed for Android app store, patch speed up by a factor of xx than archive-patcher & run with O(1) memory. )   
   
---
## Releases/Binaries
[Download from latest release](https://github.com/sisong/HDiffPatch/releases) : Command line app for Windows, Linux, MacOS; and .so patch lib for Android.     
( release files build by projects in path `HDiffPatch/builds` )   

## Build it yourself
`$ cd <dir>/HDiffPatch`   
### Linux or MacOS X ###
Try:   
`$ make LZMA=0 ZSTD=0 MD5=0`   
if the build fails with `fatal error: bzlib.h: No such file or directory`, use your system's package manager to install the libbz2 package and try again.   install bzip2: `$ apt-get install libbz2` or `$ sudo apt-get install libbz2-dev` or `$ yum -y install bzip2` or `$ brew install bzip2` ...   
Alternatively, get the optional library headers (+bzip2 library) and build completely: `$ git clone https://github.com/sisong/bzip2.git ../bzip2 && pushd ../bzip2 && make && sudo make install && popd`   
   
if need lzma zstd md5 support, Try:    
```
$ git clone https://github.com/sisong/libmd5.git ../libmd5
$ git clone https://github.com/sisong/lzma.git ../lzma
$ git clone -b v1.5.2 https://github.com/facebook/zstd.git ../zstd
$ make
```    
Tip: You can use `$ make -j` to compile in parallel
   
### Windows ###
Before you build `builds/vc/HDiffPatch.sln` with [`Visual Studio`](https://visualstudio.microsoft.com), first get the libraries into sibling folders, like so: 
```
$ git clone https://github.com/sisong/libmd5.git ../libmd5
$ git clone https://github.com/sisong/lzma.git ../lzma
$ git clone -b v1.5.2 https://github.com/facebook/zstd.git ../zstd
$ git clone https://github.com/sisong/zlib.git   ../zlib
$ git clone https://github.com/sisong/bzip2.git  ../bzip2
```
   
### libhpatchz.so for Android ###   
* install [Android NDK](https://developer.android.google.cn/ndk/downloads)
* `$ cd <dir>/HDiffPatch/builds/android_ndk_jni_mk`
* `$ build_libs.sh`  (or `$ build_libs.bat` on windows, then got \*.so files)
* import file `com/github/sisong/HPatch.java` (from `HDiffPatch/builds/android_ndk_jni_mk/java/`) & .so files, java code can call the patch function in libhpatchz.so
   

---
## command line usage Chinese version: [命令行使用说明中文版](README_cmdline_cn.md)
   
## **diff** command line usage:   
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
      requires O(oldFileSize*16/matchBlockSize+matchBlockSize*5*parallelThreadNumber)bytes of memory;
      matchBlockSize>=4, DEFAULT -s-64, recommended 16,32,48,1k,64k,1m etc...
special options:
  -block-fastMatchBlockSize
      must run with -m;
      set block match befor slow byte-by-byte match, DEFAULT -block-4k;
      if set -block-0, means don't use block match;
      fastMatchBlockSize>=4, recommended 256,1k,64k,1m etc...
      if newData similar to oldData then diff speed++ & diff memory--,
      but small possibility outDiffFile's size+
  -cache
      must run with -m;
      set is use a big cache for slow match, DEFAULT false;
      if newData not similar to oldData then diff speed++,
      big cache max used O(oldFileSize) memory, and build slow(diff speed--)
  -SD[-stepSize]
      create single compressed diffData, only need one decompress buffer
      when patch, and support step by step patching when step by step downloading!
      stepSize>=(1024*4), DEFAULT -SD-256k, recommended 64k,2m etc...
  -BSD
      create diffFile compatible with bsdiff4, unsupport input directory(folder).
  -VCD[-compressLevel[-dictSize]]
      create diffFile compatible with VCDIFF, unsupport input directory(folder).
      DEFAULT no compress, out format same as $open-vcdiff delta ... or $xdelta3 -S -e -n ...
      if set compressLevel, out format same as $xdelta3 -S lzma -e -n ...
      compress by 7zXZ(xz), compressLevel in {0..9}, DEFAULT level 7;
      dictSize can like 4096 or 4k or 4m or 16m etc..., DEFAULT 8m
      support compress by multi-thread parallel.
      NOTE: out diffFile used large source window size!
  -p-parallelThreadNumber
      if parallelThreadNumber>1 then open multi-thread Parallel mode;
      DEFAULT -p-4; requires more memory!
  -c-compressType[-compressLevel]
      set outDiffFile Compress type, DEFAULT uncompress;
      for resave diffFile,recompress diffFile to outDiffFile by new set;
      support compress type & level & dict:
        -c-zlib[-{1..9}[-dictBits]]     DEFAULT level 9
            dictBits can 9--15, DEFAULT 15.
            support run by multi-thread parallel, fast!
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
            dictBits can 10--31, DEFAULT 23.
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
  -v  output Version info.
  -h (or -?)
      output usage info.
```
   
## **patch** command line usage:   
patch usage: **hpatchz** [options] **oldPath diffFile outNewPath**   
uncompress usage: **hpatchz** [options] **"" diffFile outNewPath**   
create  SFX: **hpatchz** [-X-exe#selfExecuteFile] **diffFile -X#outSelfExtractArchive**   
run     SFX: **selfExtractArchive** [options] **oldPath -X outNewPath**   
extract SFX: **selfExtractArchive**   (same as: selfExtractArchive -f "" -X "./")
```
memory options:
  -s[-cacheSize]
      DEFAULT -s-4m; oldPath loaded as Stream;
      cacheSize can like 262144 or 256k or 512m or 2g etc....
      requires (cacheSize + 4*decompress buffer size)+O(1) bytes of memory.
      if diffFile is single compressed diffData(created by hdiffz -SD-stepSize), then requires
        (cacheSize+ stepSize + 1*decompress buffer size)+O(1) bytes of memory;
      if diffFile is created by hdiffz -BSD,bsdiff4, hdiffz -VCD,xdelta3,open-vcdiff, then requires
        (cacheSize + 3*decompress buffer size)+O(1) bytes of memory;
      if diffFile is VCDIFF: if created by hdiffz -VCD, then recommended patch by -s;
          if created by xdelta3,open-vcdiff, then recommended patch by -m.
  -m  oldPath all loaded into Memory;
      requires (oldFileSize + 4*decompress buffer size)+O(1) bytes of memory.
      if diffFile is single compressed diffData(created by hdiffz -SD-stepSize), then requires
        (oldFileSize+ stepSize + 1*decompress buffer size)+O(1) bytes of memory.
      if diffFile is created by hdiffz -BSD,bsdiff4, then requires
        (oldFileSize + 3*decompress buffer size)+O(1) bytes of memory.
      if diffFile is VCDIFF(created by hdiffz -VCD,xdelta3,open-vcdiff), then requires
        (sourceWindowSize+targetWindowSize + 3*decompress buffer size)+O(1) bytes of memory.
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
  -C-no or -C-new
      if diffFile is VCDIFF, then to close or open checksum, DEFAULT -C-new.
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
  -v  output Version info.
  -h  (or -?)
      output usage info.
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
* **TDirPatcher_\*()** functions with **struct TDirPatcher**
### v4 API, single compressed diffData:
* **create_single_compressed_diff()**
* **create_single_compressed_diff_stream()**
* **resave_single_compressed_diff()**
* **patch_single_stream()**
* **patch_single_stream_mem()**
* **patch_single_compressed_diff()**
* **patch_single_stream_diff()**
#### v4.1 API, bsdiff wrapper:
* **create_bsdiff()**
* **bspatch_with_cache()**
#### v4.2 API, optimized hpatch on MCU,NB-IoT... (demo [HPatchLite](https://github.com/sisong/HPatchLite)): 
* **create_lite_diff()**
* **hpatch_lite_open()**
* **hpatch_lite_patch()**
#### v4.5 API, vcdiff wrapper: 
* **create_vcdiff()**
* **create_vcdiff_stream()**
* **vcpatch_with_cache()**
* **create_bsdiff_stream()** for bsdiff

---
## HDiffPatch vs BsDiff & xdelta:
case list:
| |newFile <-- oldFile|newSize|oldSize|
|----:|:----|----:|----:|
|1|apache-maven-2.2.1-src.tar <-- 2.0.11-src.tar|5150720|4689920|
|2|httpd_2.4.4-netware-bin.tar <-- 2.2.24-netware-bin.tar|22612480|17059328|
|3|httpd-2.4.4-src.tar <-- 2.2.24-src.tar|31809536|37365760|
|4|Firefox-21.0-mac-en-US.app.tar <-- 20.0-mac-en-US.app.tar|98740736|96340480|
|5|emacs-24.3.tar <-- 23.4.tar|185528320|166420480|
|6|eclipse-java-juno-SR2-macosx-cocoa-x86_64.tar <-- SR2-macosx-cocoa.tar|178595840|178800640|
|7|gcc-4.8.0.tar <-- 4.7.0.tar|552775680|526745600|
   

**test PC**: Windows11, CPU Ryzen 5800H, SSD Disk, Memroy 8G*2 DDR4 3200MHz   
**Program version**: HDiffPatch4.1, BsDiff4.3, xdelta3.1   
**test Program**:   
**xdelta** diff with `-e -n -f -s {old} {new} {pat}`   
**xdelta** patch with `-d -f -s {old} {pat} {new}`   
add **hpatchz**4.5 test: `hpatchz -m -f {old} {xdelta3-pat} {new}`   
**xdelta -B** diff with `-B {oldSize} -e -n -f -s {old} {new} {pat}`   
**xdelta -B** patch with `-B {oldSize} -d -f -s {old} {pat} {new}`   
**bsdiff** diff with `{old} {new} {pat}`   
**bspatch** patch with `{old} {new} {pat}`   
**hdiffz -BSD** diff with `-m-6 -BSD -block -d -f -p-1 {old} {new} {pat}`   
**hdiffz -bzip2** diff with `-m-6 -SD -block -d -f -p-1 -c-bzip2-9 {old} {new} {pat}`   
**hdiffz -zlib** diff with `-m-6 -SD -block -d -f -p-1 -c-zlib-9 {old} {new} {pat}`   
**hdiffz -lzma2** diff with `-m-6 -SD -block -d -f -p-1 -c-lzma2-9-16m {old} {new} {pat}`   
**hdiffz -zstd** diff with `-m-6 -SD -block -d -f -p-1 -c-zstd-20-24 {old} {new} {pat}`   
**hdiffz -s -zlib** diff with `-s-64 -SD -d -f -p-1 -c-zlib-9 {old} {new} {pat}`   
**hdiffz -s -lzma2** diff with `-s-64 -SD -d -f -p-1 -c-lzma2-9-16m {old} {new} {pat}`   
**hdiffz -s -zstd-17** diff with `-s-64 -SD -d -f -p-1 -c-zstd-17-24 {old} {new} {pat}`   
**hpatchz** patch with `-s-256k -f {old} {pat} {new}`   
   
**test result average**:
|Program|compress|diff mem|speed|patch mem|max mem|speed|
|:----|----:|----:|----:|----:|----:|----:|
|bzip2|31.76%|
|lzma2|28.47%|
|xdelta3|12.18%|212M|6.9MB/s|84M|98M|54MB/s|
|xdelta3+hpatchz|12.18%|212M|6.9MB/s|58M|81M|167MB/s|
|xdelta3 -B|7.35%|442M|19.5MB/s|197M|534M|174MB/s|
|xdelta3 -B+hpatchz|7.35%|442M|19.5MB/s|154M|518M|423MB/s|
|bsdiff|6.63%|1263M|2.3MB/s|298M|1043M|120MB/s|
|hdiffz -BSD|5.67%|596M|15.8MB/s|12M|14M|131MB/s|
|hdiffz bzip2|5.77%|596M|17.4MB/s|7M|7M|208MB/s|
|hdiffz zlib|5.93%|596M|17.4MB/s|4M|4M|374MB/s|
|hdiffz lzma2|5.02%|597M|13.2MB/s|12M|20M|321MB/s|
|hdiffz -zstd|5.22%|660M|12.3MB/s|13M|20M|383MB/s|
|hdiffz -s zlib|8.13%|44M|40.7MB/s|4M|4M|436MB/s|
|hdiffz -s lzma2|6.39%|119M|17.4MB/s|13M|20M|337MB/s|
|hdiffz -s zstd-17|6.82%|73M|24.1MB/s|13M|20M|464MB/s|
   

## input Apk Files for test: 
case list:
| |app|newFile <-- oldFile|newSize|oldSize|
|----:|:---:|:----|----:|----:|
|1|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.MobileTicket.png" width="36">|12306_5.2.11.apk <-- 5.1.2.apk| 61120025|66209244|
|2|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.eg.android.AlipayGphone.png" width="36">|alipay10.1.99.apk <-- 10.1.95.apk|94178674|90951351|
|3|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.eg.android.AlipayGphone.png" width="36">|alipay10.2.0.apk <-- 10.1.99.apk|95803005|94178674|
|4|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.baidu.BaiduMap.png" width="36">|baidumaps10.25.0.apk <-- 10.24.12.apk|95539893|104527191|
|5|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.baidu.BaiduMap.png" width="36">|baidumaps10.25.5.apk <-- 10.25.0.apk|95526276|95539893|
|6|<img src="https://github.com/sisong/sfpatcher/raw/master/img/tv.danmaku.bili.png" width="36">|bilibili6.15.0.apk <-- 6.14.0.apk|74783182|72067209|
|7|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.android.chrome.png" width="36">|chrome-64-0-3282-137.apk <-- 64-0-3282-123.apk|43879588|43879588|
|8|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.android.chrome.png" width="36">|chrome-65-0-3325-109.apk <-- 64-0-3282-137.apk|43592997|43879588|
|9|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.sdu.didi.psnger.png" width="36">|didi6.0.2.apk <-- 6.0.0.apk|100866981|91462767|
|10|<img src="https://github.com/sisong/sfpatcher/raw/master/img/org.mozilla.firefox.png" width="36">|firefox68.10.0.apk <-- 68.9.0.apk|43543846|43531470|
|11|<img src="https://github.com/sisong/sfpatcher/raw/master/img/org.mozilla.firefox.png" width="36">|firefox68.10.1.apk <-- 68.10.0.apk|43542786|43543846|
|12|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.google.android.apps.maps.png" width="36">|google-maps-9-71-0.apk <-- 9-70-0.apk|50568872|51304768|
|13|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.google.android.apps.maps.png" width="36">|google-maps-9-72-0.apk <-- 9-71-0.apk|54342938|50568872|
|14|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.jingdong.app.mall.png" width="36">|jd9.0.0.apk <-- 8.5.12.apk|96891703|94233891|
|15|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.jingdong.app.mall.png" width="36">|jd9.0.8.apk <-- 9.0.0.apk|97329322|96891703|
|16|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.tencent.tmgp.jnbg2.png" width="36">|jinianbeigu2_1.12.4.apk <-- 1.12.3.apk|171611658|159691189|
|17|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.blizzard.wtcg.hearthstone.cn.png" width="36">|lushichuanshuo19.4.71003.apk <-- 19.2.69054.apk|93799693|93442621|
|18|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.sankuai.meituan.png" width="36">|meituan10.9.401.apk <-- 10.9.203.apk|88956726|89384406|
|19|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.netease.mc.png" width="36">|minecraft1.17.30.apk <-- 1.17.20.apk|373025314|370324338|
|20|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.netease.mc.png" width="36">|minecraft1.18.10.apk <-- 1.17.30.apk|401075178|373025314|
|21|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.popcap.pvz2cthd.png" width="36">|popcap.pvz2_2.4.84.1010.apk <-- 2.4.84.1009.apk|387572492|386842079|
|22|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.supercell.clashofclans.png" width="36">|supercell.clashofclans13.369.3.apk <-- 13.180.18.apk|152896934|149011539|
|23|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.outfit7.talkingtomgoldrun.png" width="36">|tangmumaopaoku4.8.0.971.apk <-- 4.6.0.913.apk|105486308|104732413|
|24|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.taobao.taobao.png" width="36">|taobao9.8.0.apk <-- 9.7.2.apk|178734456|176964070|
|25|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.taobao.taobao.png" width="36">|taobao9.9.1.apk <-- 9.8.0.apk|184437315|178734456|
|26|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.ss.android.ugc.aweme.png" width="36">|tiktok11.5.0.apk <-- 11.3.0.apk|88544106|87075000|
|27|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.google.android.apps.translate.png" width="36">|translate6.9.0.apk <-- 6.8.0.apk|28171978|28795243|
|28|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.google.android.apps.translate.png" width="36">|translate6.9.1.apk <-- 6.9.0.apk|31290990|28171978|
|29|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.tencent.mm.png" width="36">|weixin7.0.15.apk <-- 7.0.14.apk|148405483|147695111|
|30|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.tencent.mm.png" width="36">|weixin7.0.16.apk <-- 7.0.15.apk|158906413|148405483|
|31|<img src="https://github.com/sisong/sfpatcher/raw/master/img/cn.wps.moffice_eng.png" width="36">|wps12.5.2.apk <-- 12.5.1.apk|51293286|51136905|
|32|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.tanwan.yscqlyzf.png" width="36">|yuanshichuanqi1.3.608.apk <-- 1.3.607.apk|192578139|192577253|
   
**changed test Program**:   
**hdiffz ...** `-m-6 -SD` changed to `-m-1 -SD-2m -cache`, `-s-64 -SD` changed to `-s-16 -SD-2m`   
**hdiffz ...** lzma2 dict size `16m` changed to `8m`, zstd dict bit `24` changed to `23`   
**hdiffz** added diff with `-m-1 -SD-2m -cache -block -d -f -p-1 {old} {new} {pat}`   
**hdiffz -s** added diff with `-s-16 -SD-2m -d -f -p-1 {old} {new} {pat}`   
**sfpatcher -1 -zstd** diff with `-o-1 -c-zstd-21-23 -m-1 -step-3m -lp-512k -p-1 -block -cache -d {old} {new} {pat}`, patch with `-lp -p-8 {old} {pat} {new}`   
**sfpatcher -2 -lzma2** diff with `-o-2 -c-lzma2-9-4m -m-2 -step-2m -lp-8m -p-1 -block -cache -d {old} {new} {pat}`, patch with `-lp -p-8 {old} {pat} {new}`   
**sfpatcher -3 -lzma2** diff with `-o-3 -c-lzma2-9-4m -m-4 -step-2m -lp-8m -p-1 -block -cache -d {old} {new} {pat}`, patch with `-lp -p-8 {old} {pat} {new}`   
( [sfpatcher](https://github.com/sisong/sfpatcher) optimized diff&patch between apk files )  

**test result average**:
|Program|compress|diff mem|speed|patch mem|max mem|speed|arm Kirin980 speed|
|:----|----:|----:|----:|----:|----:|----:|----:|
|xdelta3|59.92%|228M|2.9MB/s|100M|100M|159MB/s|
|xdelta3+hpatchz|59.92%|228M|2.9MB/s|72M|84M|456MB/s|
|xdelta3 -B|59.51%|440M|3.1MB/s|206M|548M|157MB/s|
|xdelta3 -B+hpatchz|59.51%|440M|3.1MB/s|129M|384M|400MB/s|
|bsdiff|59.76%|1035M|1.0MB/s|243M|751M|42MB/s|
|hdiffz -BSD|59.50%|523M|5.4MB/s|13M|14M|44MB/s|
|hdiffz bzip2|59.51%|523M|5.5MB/s|7M|9M|57MB/s|
|hdiffz|59.87%|523M|7.5MB/s|4M|5M|780MB/s|268MB/s|
|hdiffz zlib|59.10%|523M|6.9MB/s|4M|5M|587MB/s|226MB/s|
|hdiffz zstd|58.74%|612M|5.0MB/s|13M|14M|680MB/s|265MB/s|
|hdiffz lzma2|58.67%|523M|3.7MB/s|12M|13M|285MB/s|
|hdiffz -s|60.46%|133M|31.8MB/s|3M|4M|806MB/s|
|hdiffz -s zlib|59.52%|133M|23.5MB/s|3M|4M|608MB/s|
|hdiffz -s zstd-17|59.27%|136M|9.7MB/s|12M|12M|763MB/s|
|hdiffz -s lzma2|59.03%|147M|5.9MB/s|11M|12M|288MB/s|
|sf_diff -1 zstd|31.70%|774M|2.8MB/s|19M|22M|394MB/s|218MB/s|
|sf_diff -2 lzma2|27.53%|859M|2.5MB/s|21M|29M|107MB/s|59MB/s|
|sf_diff -3 lzma2|23.73%|976M|2.3MB/s|24M|29M|66MB/s|36MB/s|

---
## Contact
housisong@hotmail.com  

