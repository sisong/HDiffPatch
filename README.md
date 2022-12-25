# [HDiffPatch](https://github.com/sisong/HDiffPatch)
[![release](https://img.shields.io/badge/release-v4.5.2-blue.svg)](https://github.com/sisong/HDiffPatch/releases) 
[![license](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/sisong/HDiffPatch/blob/master/LICENSE) 
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-blue.svg)](https://github.com/sisong/HDiffPatch/pulls)
[![+issue Welcome](https://img.shields.io/github/issues-raw/sisong/HDiffPatch?color=green&label=%2Bissue%20welcome)](https://github.com/sisong/HDiffPatch/issues)   

[![Build Status](https://github.com/sisong/HDiffPatch/workflows/ci/badge.svg?branch=master)](https://github.com/sisong/HDiffPatch/actions?query=workflow%3Aci+branch%3Amaster)
[![Build status](https://ci.appveyor.com/api/projects/status/t9ow8dft8lt898cv/branch/master?svg=true)](https://ci.appveyor.com/project/sisong/hdiffpatch/branch/master)   

a C\C++ library and command-line tools for Diff & Patch between binary files or directories(folder); cross-platform; runs fast; create small delta/differential; support large files and limit memory requires when diff & patch.   
   
if need patch (OTA) on embedded systems,MCU,NB-IoT..., see demo [HPatchLite](https://github.com/sisong/HPatchLite), + [tinyuz](https://github.com/sisong/tinyuz) can run on 1KB RAM devices!   

update your own Android Apk? Jar or Zip file diff & patch? try [ApkDiffPatch](https://github.com/sisong/ApkDiffPatch), to create smaller delta/differential! NOTE: *ApkDiffPath can't be used by Android app store, because it requires re-signing apks before diff.*   

[sfpatcher](https://github.com/sisong/sfpatcher) not require re-signing apks (like [archive-patcher](https://github.com/google/archive-patcher)), is designed for Android app store, patch speed up by a factor of xx than archive-patcher & run with O(1) memory.   

   
NOTE: *This library does not deal with file metadata, such as file last wirte time, permissions, link file, etc... To this library, a file is just as a stream of bytes; You can extend this library or use other tools.*   
   
---
## Releases/Binaries
[Download from latest release](https://github.com/sisong/HDiffPatch/releases) : Command line app for Windows, Linux, MacOS; and .so patch lib for Android.     
( release files build by projects in path `HDiffPatch/builds` )   

## Build it yourself
`$ cd <dir>/HDiffPatch`   
### Linux or MacOS X ###
Try:   
`$ make LZMA=0 ZSTD=0 MD5=0`   
bzip2 : if the build fails with `fatal error: bzlib.h: No such file or directory`, use your system's package manager to install the libbz2 package and try again.   install bzip2: `$ apt-get install libbz2` or `$ sudo apt-get install libbz2-dev` or `$ yum -y install bzip2` or `$ brew install bzip2` ...   
Alternatively, get the optional library headers (+bzip2 library) and build completely: `$ git clone https://github.com/sisong/bzip2.git ../bzip2 && pushd ../bzip2 && make && sudo make install && popd`   
   
if need lzma zstd md5 support, Try:    
```
$ git clone https://github.com/sisong/libmd5.git ../libmd5
$ git clone https://github.com/sisong/lzma.git ../lzma
$ git clone -b v1.5.2 https://github.com/facebook/zstd.git ../zstd
$ make
```    
Tip: You can use `$ make -j` to compile in parallel.
   
### Windows ###
Before you build `builds/vc/HDiffPatch.sln` by [`Visual Studio`](https://visualstudio.microsoft.com), first get the libraries into sibling folders, like so: 
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
### command line usage Chinese version: [命令行使用说明中文版](README_cmdline_cn.md)
   
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
  -p-search-searchThreadNumber
      must run with -s[-matchBlockSize];
      DEFAULT searchThreadNumber same as parallelThreadNumber;
      but multi-thread search need frequent random disk reads when matchBlockSize
      is small, so some times multi-thread maybe much slower than single-thread!
      if (searchThreadNumber<=1) then to close multi-thread search mode.
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
####  hpatch lite API, optimized hpatch on MCU,NB-IoT... (demo [HPatchLite](https://github.com/sisong/HPatchLite)): 
* **create_lite_diff()**
* **hpatch_lite_open()**
* **hpatch_lite_patch()**
#### bsdiff wrapper API:
* **create_bsdiff()**
* **create_bsdiff_stream()** 
* **bspatch_with_cache()**
####  vcdiff wrapper API: 
* **create_vcdiff()**
* **create_vcdiff_stream()**
* **vcpatch_with_cache()**

---
## HDiffPatch vs BsDiff & xdelta:
case list([download from OneDrive](https://1drv.ms/u/s!Aj8ygMPeifoQgUIZxYac5_uflNoN)):   
| |newFile <-- oldFile|newSize|oldSize|
|----:|:----|----:|----:|
|1|7-Zip_22.01.win.tar <-- 7-Zip_21.07.win.tar|5908992|5748224|
|2|Chrome_107.0.5304.122-x64-Stable.win.tar <-- 106.0.5249.119|278658560|273026560|
|3|cpu-z_2.03-en.win.tar <-- cpu-z_2.02-en.win.tar|8718336|8643072|
|4|curl_7.86.0.src.tar <-- curl_7.85.0.src.tar|26275840|26030080|
|5|douyin_1.5.1.mac.tar <-- douyin_1.4.2.mac.tar|407940608|407642624|
|6|Emacs_28.2-universal.mac.tar <-- Emacs_27.2-3-universal.mac.tar|196380160|257496064|
|7|FFmpeg-n_5.1.2.src.tar <-- FFmpeg-n_4.4.3.src.tar|80527360|76154880|
|8|gcc_12.2.0.src.tar <-- gcc_11.3.0.src.tar|865884160|824309760|
|9|git_2.33.0-intel-universal-mavericks.mac.tar <-- 2.31.0|73302528|70990848|
|10|go_1.19.3.linux-amd64.tar <-- go_1.19.2.linux-amd64.tar|468835840|468796416|
|11|jdk_x64_mac_openj9_16.0.1_9_openj9-0.26.0.tar <-- 9_15.0.2_7-0.24.0|363765760|327188480|
|12|jre_1.8.0_351-linux-x64.tar <-- jre_1.8.0_311-linux-x64.tar|267796480|257996800|
|13|linux_5.19.9.src.tar <-- linux_5.15.80.src.tar|1269637120|1138933760|
|14|Minecraft_175.win.tar <-- Minecraft_172.win.tar|166643200|180084736|
|15|OpenOffice_4.1.13.mac.tar <-- OpenOffice_4.1.10.mac.tar|408364032|408336896|
|16|postgresql_15.1.src.tar <-- postgresql_14.6.src.tar|151787520|147660800|
|17|QQ_9.6.9.win.tar <-- QQ_9.6.8.win.tar|465045504|464837120|
|18|tensorflow_2.10.1.src.tar <-- tensorflow_2.8.4.src.tar|275548160|259246080|
|19|VSCode-win32-x64_1.73.1.tar <-- VSCode-win32-x64_1.69.2.tar|364025856|340256768|
|20|WeChat_3.8.0.41.win.tar <-- WeChat_3.8.0.33.win.tar|505876992|505018368|
   

**test PC**: Windows11, CPU Ryzen 5800H, SSD Disk, Memroy 8G*2 DDR4 3200MHz   
**Program version**: HDiffPatch4.5.1, BsDiff4.3, xdelta3.1   
**test Program**:   
**xdelta** diff with `-S lzma -e -9 -n -f -s {old} {new} {pat}`   
**xdelta** patch with `-d -f -s {old} {pat} {new}`   
add **hpatchz** test: `hpatchz -m -f {old} {xdelta3-pat} {new}`   
**xdelta -B** diff with `-S lzma -B {oldSize} -e -9 -n -f -s {old} {new} {pat}`   
**xdelta -B** patch with `-B {oldSize} -d -f -s {old} {pat} {new}`   
add **hpatchz** test: `hpatchz -m -f {old} {xdelta3-B-pat} {new}`   
**bsdiff** diff with `{old} {new} {pat}`   
**bspatch** patch with `{old} {new} {pat}`   
add **hpatchz** test: `hpatchz -m -f {old} {bsdiff-pat} {new}`   
**hdiffz -BSD** diff with `-m-6 -BSD -d -f -p-1 {old} {new} {pat}`   
**hdiffz -bzip2** diff with `-m-6 -SD -d -f -p-1 -c-bzip2-9 {old} {new} {pat}`   
**hdiffz -zlib** diff with `-m-6 -SD -d -f -p-1 -c-zlib-9 {old} {new} {pat}`   
**hdiffz -lzma2** diff with `-m-6 -SD -d -f -p-1 -c-lzma2-9-16m {old} {new} {pat}`   
**hdiffz -zstd** diff with `-m-6 -SD -d -f -p-1 -c-zstd-21-24 {old} {new} {pat}`   
**hdiffz -s -zlib** diff with `-s-64 -SD -d -f -p-1 -c-zlib-9 {old} {new} {pat}`   
**hdiffz -s -lzma2** diff with `-s-64 -SD -d -f -p-1 -c-lzma2-9-16m {old} {new} {pat}`   
**hdiffz -s -zstd-17** diff with `-s-64 -SD -d -f -p-1 -c-zstd-17-24 {old} {new} {pat}`   
all **hdiffz** add test with -p-8   
 **hpatchz** patch with `-s-3m -f {old} {pat} {new}`   
add **zstd --patch-from** diff with `--ultra -21 --long=24 -f --patch-from={old} {new} -o {pat}`   
 zstd patch with `-d -f --memory=2000MB --patch-from={old} {pat} -o {new}`   
   
**test result average**:
|Program|compress|diff mem|speed|patch mem|max mem|speed|
|:----|----:|----:|----:|----:|----:|----:|
|bzip2-9 |33.67%||16.8MB/s|||44MB/s|
|zlib-9 |36.53%||15.9MB/s|||421MB/s|
|lzma2-9-16m |25.85%||3.9MB/s|||162MB/s|
|zstd-21-24 |27.21%||2.7MB/s|||619MB/s|
||
|zstd --patch-from|7.96%|2798M|2.4MB/s|631M|2303M|647MB/s|
|xdelta3|13.60%|409M|4.7MB/s|86M|102M|95MB/s|
|xdelta3 +hpatchz -m|13.60%|409M|4.7MB/s|72M|82M|280MB/s|
|xdelta3 -B |9.63%|2282M|7.3MB/s|460M|2070M|159MB/s|
|xdelta3 -B +hpatchz -m|9.63%|2282M|7.3MB/s|317M|1100M|345MB/s|
|bsdiff |8.17%|2773M|1.9MB/s|637M|2312M|121MB/s|
|bsdiff +hpatchz -m|8.17%|2773M|1.9MB/s|321M|1101M|141MB/s|
|hdiffz p1 -BSD |7.72%|1215M|10.9MB/s|14M|14M|124MB/s|
|hdiffz p8 -BSD |7.72%|1191M|22.0MB/s|14M|14M|123MB/s|
|hdiffz p1 -bzip2 |7.96%|1215M|11.6MB/s|7M|7M|182MB/s|
|hdiffz p8 -pbzip2 |7.95%|1191M|30.5MB/s|7M|7M|177MB/s|
|hdiffz p1 -zlib |7.79%|1214M|11.6MB/s|4M|4M|415MB/s|
|hdiffz p8 -zlib |7.79%|1191M|30.5MB/s|4M|4M|409MB/s|
|hdiffz p1 -lzma2 |6.44%|1212M|9.2MB/s|17M|20M|312MB/s|
|hdiffz p8 -lzma2 |6.44%|1192M|23.2MB/s|17M|20M|309MB/s|
|hdiffz p1 -zstd |6.74%|1217M|9.0MB/s|16M|21M|422MB/s|
|hdiffz p8 -zstd |6.74%|1531M|16.7MB/s|16M|21M|418MB/s|
|hdiffz -s p1 -BSD |11.96%|91M|33.3MB/s|14M|14M|105MB/s|
|hdiffz -s p8 -BSD |11.96%|95M|40.6MB/s|14M|14M|105MB/s|
|hdiffz -s p1 -bzip2 |11.96%|91M|39.9MB/s|7M|7M|137MB/s|
|hdiffz -s p8 -pbzip2 |11.97%|107M|108.7MB/s|7M|7M|133MB/s|
|hdiffz -s p1 -zlib |12.52%|90M|35.2MB/s|4M|4M|439MB/s|
|hdiffz -s p8 -zlib |12.53%|95M|104.4MB/s|4M|4M|434MB/s|
|hdiffz -s p1 -lzma2 |9.11%|170M|13.7MB/s|17M|20M|289MB/s|
|hdiffz -s p8 -lzma2 |9.13%|370M|34.7MB/s|17M|20M|286MB/s|
|hdiffz -s p1 -zstd-17 |9.99%|103M|19.8MB/s|18M|21M|482MB/s|
|hdiffz -s p8 -zstd-17 |9.99%|775M|30.4MB/s|18M|21M|481MB/s|
   

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
**sfpatcher -1 -zstd** diff with `-o-1 -c-zstd-21-23 -m-1 -step-3m -lp-512k -p-8 -cache -d {old} {new} {pat}`   
**sfpatcher -2 -lzma2** diff with `-o-2 -c-lzma2-9-4m -m-1 -step-2m -lp-8m -p-8 -cache -d {old} {new} {pat}`   
**sfpatcher -3 -lzma2** diff with `-o-3 -c-lzma2-9-4m -m-1 -step-2m -lp-8m -p-8 -cache -d {old} {new} {pat}`   
 sfpatcher patch with `-lp -p-8 {old} {pat} {new}`   
( [sfpatcher](https://github.com/sisong/sfpatcher) optimized diff&patch between apk files )  

**test result average**:
|Program|compress|diff mem|speed|patch mem|max mem|speed|arm Kirin980 speed|
|:----|----:|----:|----:|----:|----:|----:|----:|
|zstd --patch-from|58.81%|2248M|3.0MB/s|234M|741M|670MB/s|
|xdelta3|59.68%|422M|2.7MB/s|98M|99M|162MB/s|
|xdelta3 +hpatchz -m|59.68%|422M|2.7MB/s|69M|81M|472MB/s|
|xdelta3 -B |59.26%|953M|3.0MB/s|204M|547M|161MB/s|
|xdelta3 -B +hpatchz -m|59.26%|953M|3.0MB/s|126M|381M|407MB/s|
|bsdiff |59.76%|1035M|1.0MB/s|243M|751M|43MB/s|
|bsdiff +hpatchz -m|59.76%|1035M|1.0MB/s|128M|383M|45MB/s|
|bsdiff +hpatchz -s|59.76%|1035M|1.0MB/s|14M|14M|44MB/s|
|hdiffz p1 -BSD|59.50%|522M|5.9MB/s|14M|14M|44MB/s|
|hdiffz p8 -BSD|59.52%|527M|11.5MB/s|14M|14M|44MB/s|
|hdiffz p1 -bzip2|59.51%|522M|6.0MB/s|8M|9M|57MB/s|
|hdiffz p8 -pbzip2|59.53%|527M|19.6MB/s|7M|9M|54MB/s|
|hdiffz p1 -zlib|59.10%|522M|7.2MB/s|5M|6M|588MB/s|226MB/s|
|hdiffz p8 -zlib|59.12%|527M|22.0MB/s|5M|6M|595MB/s|
|hdiffz p1 -lzma2|58.66%|537M|3.7MB/s|21M|22M|285MB/s|
|hdiffz p8 -lzma2|58.68%|610M|13.7MB/s|21M|22M|288MB/s|
|hdiffz p1 -zstd|58.73%|546M|4.9MB/s|21M|22M|684MB/s|265MB/s|
|hdiffz p8 -zstd|58.75%|1315M|9.3MB/s|21M|22M|676MB/s|
|hdiffz -s p1 -BSD|60.00%|131M|13.8MB/s|14M|14M|44MB/s|
|hdiffz -s p8 -BSD|60.00%|135M|18.9MB/s|14M|14M|44MB/s|
|hdiffz -s p1 -bzip2|59.98%|131M|14.4MB/s|6M|7M|59MB/s|
|hdiffz -s p8 -pbzip2|59.98%|143M|59.7MB/s|6M|7M|55MB/s|
|hdiffz -s p1 -zlib|59.52%|131M|24.3MB/s|4M|4M|596MB/s|
|hdiffz -s p8 -zlib|59.52%|135M|85.4MB/s|4M|4M|594MB/s|
|hdiffz -s p1 -lzma2|59.02%|208M|5.7MB/s|20M|20M|279MB/s|
|hdiffz -s p8 -lzma2|59.02%|373M|25.5MB/s|20M|20M|281MB/s|
|hdiffz -s p1 -zstd-17|59.26%|137M|9.2MB/s|20M|21M|730MB/s|
|hdiffz -s p8 -zstd-17|59.26%|871M|13.3MB/s|20M|21M|734MB/s|
|sf_diff -o-1 p8 -zstd|31.61%|982M|5.2MB/s|16M|20M|382MB/s|218MB/s|
|sf_diff -o-2 p8 -lzma2|27.30%|859M|6.5MB/s|19M|27M|105MB/s|59MB/s|
|sf_diff -o-3 p8 -lzma2|23.54%|973M|5.9MB/s|22M|27M|64MB/s|36MB/s|

---
## Contact
housisong@hotmail.com  

