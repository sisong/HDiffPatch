# [HDiffPatch]
[![release](https://img.shields.io/badge/release-v4.8.0-blue.svg)](https://github.com/sisong/HDiffPatch/releases) 
[![license](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/sisong/HDiffPatch/blob/master/LICENSE) 
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-blue.svg)](https://github.com/sisong/HDiffPatch/pulls)
[![+issue Welcome](https://img.shields.io/github/issues-raw/sisong/HDiffPatch?color=green&label=%2Bissue%20welcome)](https://github.com/sisong/HDiffPatch/issues)   
[![release](https://img.shields.io/github/downloads/sisong/HDiffPatch/total?color=blue)](https://github.com/sisong/HDiffPatch/releases)
[![Build Status](https://github.com/sisong/HDiffPatch/workflows/ci/badge.svg?branch=master)](https://github.com/sisong/HDiffPatch/actions?query=workflow%3Aci+branch%3Amaster)
[![Build status](https://ci.appveyor.com/api/projects/status/t9ow8dft8lt898cv/branch/master?svg=true)](https://ci.appveyor.com/project/sisong/hdiffpatch/branch/master)   
 english | [中文版](README_cn.md)   

[HDiffPatch] is a C\C++ library and command-line tools for **diff** & **patch** between binary files or directories(folder); cross-platform; fast running; create small delta/differential; support large files and limit memory requires when diff & patch.   

[HDiffPatch] defines its own patch file format, this lib is also compatible with the [bsdiff4] & [endsley/bsdiff] patch file format and [partially compatible](https://github.com/sisong/HDiffPatch/issues/369#issuecomment-1869798843) with the [open-vcdiff] & [xdelta3] patch file format [VCDIFF(RFC 3284)].   

if need patch (OTA) on embedded systems,MCU,NB-IoT..., see demo [HPatchLite], +[tinyuz] decompressor can run on 1KB RAM devices!   

update your own Android Apk? Jar or Zip file diff & patch? try [ApkDiffPatch], to create smaller delta/differential! NOTE: *ApkDiffPath can't be used by Android app store, because it requires re-signing apks before diff.*   

[sfpatcher] not require re-signing apks (like [archive-patcher]), is designed for Android app store, patch speed up by a factor of xx than archive-patcher & run with O(1) memory.   

if you not have the old versions(too many or not obtain or have been modified), thus cannot create the delta in advance. you can see sync demo [hsynz] (like [zsync]), the new version is only need released once and the owners of the old version get the information about the new version and do the diff&patch themselves. hsynz support zstd compressor & run faster than zsync.
   
NOTE: *This library does not deal with file metadata, such as file last wirte time, permissions, link file, etc... To this library, a file is just as a stream of bytes; You can extend this library or use other tools.*   
   

[HDiffPatch]: https://github.com/sisong/HDiffPatch
[hsynz]: https://github.com/sisong/hsynz
[ApkDiffPatch]: https://github.com/sisong/ApkDiffPatch
[sfpatcher]: https://github.com/sisong/sfpatcher
[HPatchLite]: https://github.com/sisong/HPatchLite
[tinyuz]: https://github.com/sisong/tinyuz
[bsdiff4]: http://www.daemonology.net/bsdiff/
[endsley/bsdiff]: https://github.com/mendsley/bsdiff
[xdelta3]: https://github.com/jmacd/xdelta
[open-vcdiff]: https://github.com/google/open-vcdiff
[archive-patcher]: https://github.com/google/archive-patcher
[zsync]: http://zsync.moria.org.uk
[VCDIFF(RFC 3284)]: https://www.rfc-editor.org/rfc/rfc3284

---
## Releases/Binaries
[Download from latest release](https://github.com/sisong/HDiffPatch/releases) : Command line app for Windows, Linux, MacOS; and .so patch lib for Android.   
use cmdline to create a delta:   
`$hdiffz -m-6 -SD -c-zstd-21-24 -d oldPath newPath outDiffFile`   
if file is very large, try changing `-m-6` to `-s-64`   
apply the delta:   
`$hpatchz oldPath diffFile outNewPath`   

## Build it yourself
`$ cd <dir>/HDiffPatch`   
### Linux or MacOS X ###
Try:   
`$ make LDEF=0 LZMA=0 ZSTD=0 MD5=0`   
bzip2 : if the build fails with `fatal error: bzlib.h: No such file or directory`, use your system's package manager to install the libbz2 package and try again; or download & make with libbz2 source code:
```
$ git clone https://github.com/sisong/bzip2.git ../bzip2
$ make LDEF=0 LZMA=0 ZSTD=0 MD5=0 BZIP2=1
```
if need lzma zstd & md5 ... default support, Try:
```
$ git clone https://github.com/sisong/libmd5.git ../libmd5
$ git clone https://github.com/sisong/lzma.git ../lzma
$ git clone https://github.com/sisong/zstd.git ../zstd
$ git clone https://github.com/sisong/zlib.git ../zlib
$ git clone https://github.com/sisong/libdeflate.git ../libdeflate
$ make
```    
Tip: You can use `$ make -j` to compile in parallel.
   
### Windows ###
Before you build `builds/vc/HDiffPatch.sln` by [`Visual Studio`](https://visualstudio.microsoft.com), first get the libraries into sibling folders, like so: 
```
$ git clone https://github.com/sisong/libmd5.git ../libmd5
$ git clone https://github.com/sisong/lzma.git ../lzma
$ git clone https://github.com/sisong/zstd.git ../zstd
$ git clone https://github.com/sisong/zlib.git   ../zlib
$ git clone https://github.com/sisong/libdeflate.git ../libdeflate
$ git clone https://github.com/sisong/bzip2.git  ../bzip2
```
   
### libhpatchz.so for Android ###   
* install [Android NDK](https://developer.android.google.cn/ndk/downloads)
* `$ cd <dir>/HDiffPatch/builds/android_ndk_jni_mk`
* `$ build_libs.sh`  (or `$ build_libs.bat` on windows, then got \*.so files)
* import file `com/github/sisong/HPatch.java` (from `HDiffPatch/builds/android_ndk_jni_mk/java/`) & .so files, java code can call the patch function in libhpatchz.so
   

---
   
## **diff** command line usage:   
diff     usage: **hdiffz** [options] **oldPath newPath outDiffFile**   
compress usage: **hdiffz** [-c-...]  **"" newPath outDiffFile**   
test    usage: **hdiffz**    -t     **oldPath newPath testDiffFile**   
resave  usage: **hdiffz** [-c-...]  **diffFile outDiffFile**   
print    info: **hdiffz** -info **diffFile**   
get  manifest: **hdiffz** [-g#...] [-C-checksumType] **inputPath -M#outManifestTxtFile**   
manifest diff: **hdiffz** [options] **-M-old#oldManifestFile -M-new#newManifestFile oldPath newPath outDiffFile**   
```
  oldPath newPath inputPath can be file or directory(folder),
  oldPath can empty, and input parameter ""
options:
  -m[-matchScore]
      DEFAULT; all file load into Memory; best diffFileSize;
      requires (newFileSize+ oldFileSize*5(or *9 when oldFileSize>=2GB))+O(1)
        bytes of memory;
      matchScore>=0, DEFAULT -m-6, recommended bin: 0--4 text: 4--9 etc...
  -s[-matchBlockSize]
      all file load as Stream; fast;
      requires O(oldFileSize*16/matchBlockSize+matchBlockSize*5*parallelThreadNumber)bytes of memory;
      matchBlockSize>=4, DEFAULT -s-64, recommended 16,32,48,1k,64k,1m etc...
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
      also support run with -SD (not used stepSize), then create single compressed
      diffFile compatible with endsley/bsdiff (https://github.com/mendsley/bsdiff).
  -VCD[-compressLevel[-dictSize]]
      create diffFile compatible with VCDIFF, unsupport input directory(folder).
      DEFAULT no compress, out format same as $open-vcdiff ... or $xdelta3 -S -e -n ...
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
        -c-ldef[-{1..12}]               DEFAULT level 12
            compatible with -c-zlib, faster or better compress than zlib;
            used libdeflate compressor, & dictBits always 15.
            support run by multi-thread parallel, fast!
        -c-bzip2[-{1..9}]               (or -bz2) DEFAULT level 9
        -c-pbzip2[-{1..9}]              (or -pbz2) DEFAULT level 8
            support run by multi-thread parallel, fast!
            NOTE: code not compatible with it compressed by -c-bzip2!
        -c-lzma[-{0..9}[-dictSize]]     DEFAULT level 7
            dictSize can like 4096 or 4k or 4m or 128m etc..., DEFAULT 8m
            support run by 2-thread parallel.
        -c-lzma2[-{0..9}[-dictSize]]    DEFAULT level 7
            dictSize can like 4096 or 4k or 4m or 128m etc..., DEFAULT 8m
            support run by multi-thread parallel, fast!
            NOTE: code not compatible with it compressed by -c-lzma!
        -c-zstd[-{0..22}[-dictBits]]    DEFAULT level 20
            dictBits can 10--30, DEFAULT 23.
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
  -neq
      open check: if newPath & oldPath's all datas are equal, then return error;
      DEFAULT not check equal.
  -d  Diff only, do't run patch check, DEFAULT run patch check.
  -t  Test only, run patch check, patch(oldPath,testDiffFile)==newPath ?
  -f  Force overwrite, ignore write path already exists;
      DEFAULT (no -f) not overwrite and then return error;
      if used -f and write path is exist directory, will always return error.
  --patch
      swap to hpatchz mode.
  -info
      print infos of diffFile.
  -v  print Version info.
  -h (or -?)
      print usage info.
```
   
## **patch** command line usage:   
patch usage: **hpatchz** [options] **oldPath diffFile outNewPath**   
uncompress usage: **hpatchz** [options] **"" diffFile outNewPath**   
print  info: **hpatchz** -info **diffFile**   
create  SFX: **hpatchz** [-X-exe#selfExecuteFile] **diffFile -X#outSelfExtractArchive**   
run     SFX: **selfExtractArchive** [options] **oldPath -X outNewPath**   
extract SFX: **selfExtractArchive**  (same as: $selfExtractArchive -f {""|".\"} -X ".\")
```
  if oldPath is empty input parameter ""
options:
  -s[-cacheSize]
      DEFAULT -s-4m; oldPath loaded as Stream;
      cacheSize can like 262144 or 256k or 512m or 2g etc....
      requires (cacheSize + 4*decompress buffer size)+O(1) bytes of memory.
      if diffFile is single compressed diffData(created by hdiffz -SD-stepSize), then requires
        (cacheSize+ stepSize + 1*decompress buffer size)+O(1) bytes of memory;
      if diffFile is created by hdiffz -BSD,bsdiff4, hdiffz -VCD, then requires
        (cacheSize + 3*decompress buffer size)+O(1) bytes of memory;
      if diffFile is created by xdelta3,open-vcdiff, then requires
        (sourceWindowSize+targetWindowSize + 3*decompress buffer size)+O(1) bytes of memory.
  -m  oldPath all loaded into Memory;
      requires (oldFileSize + 4*decompress buffer size)+O(1) bytes of memory.
      if diffFile is single compressed diffData(created by hdiffz -SD-stepSize), then requires
        (oldFileSize+ stepSize + 1*decompress buffer size)+O(1) bytes of memory.
      if diffFile is created by hdiffz -BSD,bsdiff4, then requires
        (oldFileSize + 3*decompress buffer size)+O(1) bytes of memory.
      if diffFile is VCDIFF(created by hdiffz -VCD,xdelta3,open-vcdiff), then requires
        (sourceWindowSize+targetWindowSize + 3*decompress buffer size)+O(1) bytes of memory.
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
  -info
      print infos of diffFile.
  -v  print Version info.
  -h  (or -?)
      print usage info.
```
   
---
## library API usage:
**diff**&**patch** function in file: `libHDiffPatch/HDiff/diff.h` & `libHDiffPatch/HPatch/patch.h`   
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
#### hpatch lite API, optimized hpatch on MCU,NB-IoT... (demo [HPatchLite]): 
* **create_lite_diff()**
* **hpatch_lite_open()**
* **hpatch_lite_patch()**
#### bsdiff ([bsdiff4] & [endsley/bsdiff]) wrapper API:
* **create_bsdiff()**
* **create_bsdiff_stream()** 
* **bspatch_with_cache()**
#### vcdiff ([open-vcdiff] & [xdelta3]) wrapper API: 
* **create_vcdiff()**
* **create_vcdiff_stream()**
* **vcpatch_with_cache()**
#### hsynz API, diff&patch by sync (demo [hsynz]): 
* **create_sync_data()**
* **create_dir_sync_data()**
* **sync_patch()**
* **sync_patch_...()**
* **sync_local_diff()**
* **sync_local_diff_...()**
* **sync_local_patch()**
* **sync_local_patch_...()**

---
## [HDiffPatch] vs [bsdiff4] & [xdelta3]:
case list([download from OneDrive](https://1drv.ms/u/s!Aj8ygMPeifoQgULlawtabR9lhrQ8)):   
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
   

**test PC**: Windows11, CPU R9-7945HX, SSD PCIe4.0x4 4T, DDR5 5200MHz 32Gx2   
**Program version**: HDiffPatch4.6.3, hsynz 1.1.0, BsDiff4.3, xdelta3.1, zstd1.5.2  
**test Program**:   
**zstd --patch-from** diff with `--ultra -21 --long=24 -f --patch-from={old} {new} -o {pat}`   
 zstd patch with `-d -f --memory=2047MB --patch-from={old} {pat} -o {new}`  
**xdelta3** diff with `-S lzma -e -9 -n -f -s {old} {new} {pat}`   
**xdelta3** patch with `-d -f -s {old} {pat} {new}`   
& adding **hpatchz** test: `hpatchz -m -f {old} {xdelta3-pat} {new}`   
**xdelta3 -B** diff with `-S lzma -B {oldSize} -e -9 -n -f -s {old} {new} {pat}`   
**xdelta3 -B** patch with `-B {oldSize} -d -f -s {old} {pat} {new}`   
& adding **hpatchz** test: `hpatchz -m -f {old} {xdelta3-B-pat} {new}`   
**bsdiff** diff with `{old} {new} {pat}`   
**bspatch** patch with `{old} {new} {pat}`   
& adding **hpatchz** test: `hpatchz -m -f {old} {bsdiff-pat} {new}`   
**hdiffz -BSD** diff with `-m-6 -BSD -d -f -p-1 {old} {new} {pat}`   
**hdiffz zlib** diff with `-m-6 -SD -d -f -p-1 -c-zlib-9 {old} {new} {pat}`   
**hdiffz lzma2** diff with `-m-6 -SD -d -f -p-1 -c-lzma2-9-16m {old} {new} {pat}`   
**hdiffz zstd** diff with `-m-6 -SD -d -f -p-1 -c-zstd-21-24 {old} {new} {pat}`   
**hdiffz -s zlib** diff with `-s-64 -SD -d -f -p-1 -c-zlib-9 {old} {new} {pat}`   
**hdiffz -s lzma2** diff with `-s-64 -SD -d -f -p-1 -c-lzma2-9-16m {old} {new} {pat}`   
**hdiffz -s zstd** diff with `-s-64 -SD -d -f -p-1 -c-zstd-21-24 {old} {new} {pat}`   
& adding all **hdiffz**  test with -p-8   
**hpatchz** patch with `-s-3m -f {old} {pat} {new}`    
**hsynz** test, make sync info by `hsync_make -s-2k {new} {out_newi} {out_newz}`,    
client sync diff&patch by `hsync_demo {old} {newi} {newz} {out_new} -p-1`   
**hsynz p1 zlib** run hsync_make with `-p-1 -c-zlib-9`   
**hsynz p8 zlib** run hsync_make with `-p-8 -c-zlib-9` (run `hsync_demo` with `-p-8`)   
**hsynz p1 zstd** run hsync_make with `-p-1 -c-zstd-21-24`   
**hsynz p8 zstd** run hsync_make with `-p-8 -c-zstd-21-24` (run `hsync_demo` with `-p-8`)   
   
**test result average**:
|Program|compress|diff mem|speed|patch mem|max mem|speed|
|:----|----:|----:|----:|----:|----:|----:|
|bzip2-9 |33.67%||22.9MB/s|||66MB/s|
|zlib-9 |36.53%||19.8MB/s|||539MB/s|
|lzma2-9-16m |25.85%||5.3MB/s|||215MB/s|
|zstd-21-24 |27.21%||4.2MB/s|||976MB/s|
||
|zstd --patch-from|7.96%|2798M|3.3MB/s|629M|2303M|828MB/s|
|xdelta3|13.60%|409M|6.9MB/s|86M|102M|159MB/s|
|xdelta3 +hpatchz -m|13.60%|409M|6.9MB/s|70M|82M|377MB/s|
|xdelta3 -B|9.63%|2282M|10.9MB/s|460M|2070M|267MB/s|
|xdelta3 -B +hpatchz -m|9.63%|2282M|10.9MB/s|315M|1100M|477MB/s|
|bsdiff|8.17%|2773M|2.5MB/s|637M|2312M|167MB/s|
|bsdiff +hpatchz -m|8.17%|2773M|2.5MB/s|321M|1101M|197MB/s|
|hdiffz p1 -BSD|7.72%|1210M|13.4MB/s|14M|14M|172MB/s|
|hdiffz p8 -BSD|7.72%|1191M|31.2MB/s|14M|14M|172MB/s|
|hdiffz p1 zlib|7.79%|1214M|14.4MB/s|4M|4M|564MB/s|
|hdiffz p8 zlib|7.79%|1190M|44.8MB/s|4M|4M|559MB/s|
|hdiffz p1 lzma2|6.44%|1209M|11.4MB/s|16M|20M|431MB/s|
|hdiffz p8 lzma2|6.44%|1191M|33.4MB/s|16M|20M|428MB/s|
|hdiffz p1 zstd|6.74%|1211M|11.5MB/s|16M|21M|592MB/s|
|hdiffz p8 zstd|6.74%|1531M|24.3MB/s|16M|21M|586MB/s|
|hdiffz -s p1 -BSD|11.96%|91M|46.0MB/s|14M|14M|148MB/s|
|hdiffz -s p8 -BSD|11.96%|95M|59.8MB/s|14M|14M|148MB/s|
|hdiffz -s p1 zlib|12.52%|91M|46.4MB/s|3M|4M|611MB/s|
|hdiffz -s p8 zlib|12.53%|95M|178.9MB/s|3M|4M|609MB/s|
|hdiffz -s p1 lzma2|9.11%|170M|18.1MB/s|17M|20M|402MB/s|
|hdiffz -s p8 lzma2|9.13%|370M|50.6MB/s|17M|20M|400MB/s|
|hdiffz -s p1 zstd|9.60%|195M|18.0MB/s|17M|21M|677MB/s|
|hdiffz -s p8 zstd|9.60%|976M|28.5MB/s|17M|21M|678MB/s|
|hsynz p1 zlib|20.05%|6M|17.3MB/s|6M|22M|273MB/s|
|hsynz p8 zlib|20.05%|30M|115.1MB/s|13M|29M|435MB/s|
|hsynz p1 zstd|14.96%|532M|1.9MB/s|24M|34M|264MB/s|
|hsynz p8 zstd|14.95%|3349M|10.1MB/s|24M|34M|410MB/s|
    

## input Apk Files for test: 
case list:
| |app|newFile <-- oldFile|newSize|oldSize|
|----:|:---:|:----|----:|----:|
|1|<img src="https://github.com/sisong/sfpatcher/raw/master/img/cn.wps.moffice_eng.png" width="36">|cn.wps.moffice_eng_13.30.0.apk <-- 13.29.0|95904918|94914262|
|2|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.achievo.vipshop.png" width="36">|com.achievo.vipshop_7.80.2.apk <-- 7.79.9|127395632|120237937|
|3|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.adobe.reader.png" width="36">|com.adobe.reader_22.9.0.24118.apk <-- 22.8.1.23587|27351437|27087718|
|4|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.alibaba.android.rimet.png" width="36">|com.alibaba.android.rimet_6.5.50.apk <-- 6.5.45|195314449|193489159|
|5|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.amazon.mShop.android.shopping.png" width="36">|com.amazon.mShop.android.shopping_24.18.2.apk <-- 24.18.0|76328858|76287423|
|6|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.baidu.BaiduMap.png" width="36">|com.baidu.BaiduMap_16.5.0.apk <-- 16.4.5|131382821|132308374|
|7|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.dragon.read.png" width="36">|com.dragon.read_5.5.3.33.apk <-- 5.5.1.32|45112658|43518658|
|8|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.ebay.mobile.png" width="36">|com.ebay.mobile_6.80.0.1.apk <-- 6.79.0.1|61202587|61123285|
|9|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.eg.android.AlipayGphone.png" width="36">|com.eg.android.AlipayGphone_10.3.0.apk <-- 10.2.96|122073135|119046208|
|10|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.google.android.apps.translate.png" width="36">|com.google.android.apps.translate_6.46.0.apk <-- 6.45.0|48892967|48843378|
|11|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.google.android.googlequicksearchbox.png" width="36">|com.google.android.googlequicksearchbox_13.38.11.apk <-- 13.37.10|190539272|189493966|
|12|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.jingdong.app.mall.png" width="36">|com.jingdong.app.mall_11.3.2.apk <-- 11.3.0|101098430|100750191|
|13|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.netease.cloudmusic.png" width="36">|com.netease.cloudmusic_8.8.45.apk <-- 8.8.40|181914846|181909451|
|14|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.reddit.frontpage.png" width="36">|com.reddit.frontpage_2022.36.0.apk <-- 2022.34.0|50205119|47854461|
|15|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.sankuai.meituan.takeoutnew.png" width="36">|com.sankuai.meituan.takeoutnew_7.94.3.apk <-- 7.92.2|74965893|74833926|
|16|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.sankuai.meituan.png" width="36">|com.sankuai.meituan_12.4.207.apk <-- 12.4.205|93613732|93605911|
|17|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.sina.weibo.png" width="36">|com.sina.weibo_12.10.0.apk <-- 12.9.5|156881776|156617913|
|18|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.smile.gifmaker.png" width="36">|com.smile.gifmaker_10.8.40.27845.apk <-- 10.8.30.27728|102403847|101520138|
|19|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.ss.android.article.news.png" width="36">|com.ss.android.article.news_9.0.7.apk <-- 9.0.6|54444003|53947221|
|20|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.ss.android.ugc.aweme.png" width="36">|com.ss.android.ugc.aweme_22.6.0.apk <-- 22.5.0|171683897|171353597|
|21|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.taobao.taobao.png" width="36">|com.taobao.taobao_10.18.10.apk <-- 10.17.0|117218670|117111874|
|22|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.tencent.mm.png" width="36">|com.tencent.mm_8.0.28.apk <-- 8.0.27|266691829|276603782|
|23|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.tencent.mobileqq.png" width="36">|com.tencent.mobileqq_8.9.15.apk <-- 8.9.13|311322716|310529631|
|24|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.tencent.mtt.png" width="36">|com.tencent.mtt_13.2.0.0103.apk <-- 13.2.0.0045|97342747|97296757|
|25|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.tripadvisor.tripadvisor.png" width="36">|com.tripadvisor.tripadvisor_49.5.apk <-- 49.3|28744498|28695346|
|26|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.twitter.android.png" width="36">|com.twitter.android_9.61.0.apk <-- 9.58.2|36141840|35575484|
|27|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.ubercab.png" width="36">|com.ubercab_4.442.10002.apk <-- 4.439.10002|69923232|64284150|
|28|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.ximalaya.ting.android.png" width="36">|com.ximalaya.ting.android_9.0.66.3.apk <-- 9.0.62.3|115804845|113564876|
|29|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.xunmeng.pinduoduo.png" width="36">|com.xunmeng.pinduoduo_6.30.0.apk <-- 6.29.1|30896833|30951567|
|30|<img src="https://github.com/sisong/sfpatcher/raw/master/img/com.youdao.dict.png" width="36">|com.youdao.dict_9.2.29.apk <-- 9.2.28|110624682|110628778|
|31|<img src="https://github.com/sisong/sfpatcher/raw/master/img/org.mozilla.firefox.png" width="36">|org.mozilla.firefox_105.2.0.apk <-- 105.1.0|83078464|83086656|
|32|<img src="https://github.com/sisong/sfpatcher/raw/master/img/tv.danmaku.bili.png" width="36">|tv.danmaku.bili_7.1.0.apk <-- 7.0.0|104774723|104727005|
   

**changed test Program**:   
**hdiffz ...** `-m-6 -SD` changed to `-m-1 -SD-2m -cache`, `-s-64 -SD` changed to `-s-16 -SD-2m`   
**hdiffz ...** lzma2 dict size `16m` changed to `8m`, zstd dict bit `24` changed to `23`   
**hsynz ...** make `-s-2k` changed to `-s-1k`   
& adding **hsynz p1**, **hsynz p8** make without compressor   
**archive-patcher** v1.0, diff with `--generate --old {old} --new {new} --patch {pat}`,   
  patch with `--apply --old {old} --patch {pat} --new {new}`   
  NOTE: archive-patcher's delta file compressed by lzma2-9-16m, diff&patch time not include compress&decompress delta file's memory&time.   
**sfpatcher -1 zstd** v1.1.1 diff with `-o-1 -c-zstd-21-23 -m-1 -step-3m -lp-512k -p-8 -cache -d {old} {new} {pat}`   
& **sfpatcher -2 lzma2** diff with `-o-2 -c-lzma2-9-4m -m-1 -step-2m -lp-8m -p-8 -cache -d {old} {new} {pat}`   
 sfpatcher patch with `-lp -p-8 {old} {pat} {new}`   
**sfpatcher -1 clA zstd** v1.3.0 used `$sf_normalize -cl-A` normalized apks before diff   
adding test hpatchz&sfpatcher on Android, arm CPU Kirin980(2×A76 2.6G + 2×A76 1.92G + 4×A55 1.8G)   
( [archive-patcher], [sfpatcher] optimized diff&patch between apk files )  

**test result average**:
|Program|compress|diff mem|speed|patch mem|max mem|speed|arm Kirin980|
|:----|----:|----:|----:|----:|----:|----:|----:|
|zstd --patch-from|53.18%|2199M|3.6MB/s|209M|596M|609MB/s|
|xdelta3|54.51%|422M|3.8MB/s|98M|99M|170MB/s|
|xdelta3 +hpatchz -m|54.51%|422M|3.8MB/s|70M|81M|438MB/s|
|bsdiff|53.84%|931M|1.2MB/s|218M|605M|54MB/s|
|bsdiff+hpatchz -m|53.84%|931M|1.2MB/s|116M|310M|57MB/s|
|bsdiff+hpatchz -s|53.84%|931M|1.2MB/s|14M|14M|54MB/s|
|hdiffz p1 -BSD|53.69%|509M|6.8MB/s|14M|14M|55MB/s|
|hdiffz p8 -BSD|53.70%|514M|15.3MB/s|14M|14M|55MB/s|
|hdiffz p1|54.40%|509M|8.8MB/s|5M|6M|682MB/s|
|hdiffz p8|54.41%|514M|32.4MB/s|5M|6M|686MB/s|443MB/s|
|hdiffz p1 zlib|53.21%|509M|8.2MB/s|5M|6M|514MB/s|
|hdiffz p8 zlib|53.22%|514M|31.1MB/s|5M|6M|512MB/s|343MB/s|
|hdiffz p1 lzma2|52.93%|525M|4.1MB/s|21M|22M|260MB/s|
|hdiffz p8 lzma2|52.94%|557M|18.9MB/s|21M|22M|261MB/s|131MB/s|
|hdiffz p1 zstd|53.04%|537M|5.4MB/s|21M|22M|598MB/s|
|hdiffz p8 zstd|53.05%|1251M|11.1MB/s|21M|22M|604MB/s|371MB/s|
|hdiffz -s p1 zlib|53.73%|118M|26.8MB/s|4M|6M|513MB/s|
|hdiffz -s p8 zlib|53.73%|122M|97.3MB/s|4M|6M|513MB/s|
|hdiffz -s p1 lzma2|53.30%|197M|6.4MB/s|20M|22M|258MB/s|
|hdiffz -s p8 lzma2|53.30%|309M|32.4MB/s|20M|22M|258MB/s|
|hdiffz -s p1 zstd|53.44%|221M|10.1MB/s|20M|22M|620MB/s|
|hdiffz -s p8 zstd|53.44%|1048M|14.4MB/s|20M|22M|613MB/s|
|hsynz p1|62.43%|4M|1533.5MB/s|4M|10M|236MB/s|
|hsynz p8|62.43%|18M|2336.4MB/s|12M|18M|394MB/s|
|hsynz p1 zlib|58.67%|5M|22.7MB/s|4M|11M|243MB/s|
|hsynz p8 zlib|58.67%|29M|138.6MB/s|12M|19M|410MB/s|
|hsynz p1 zstd|57.74%|534M|2.7MB/s|24M|28M|234MB/s|
|hsynz p8 zstd|57.74%|3434M|13.4MB/s|24M|28M|390MB/s|
|archive-patcher|31.65%|1448M|0.9MB/s|558M|587M|14MB/s|
|sfpatcher-1 p1 zstd|31.08%|818M|2.3MB/s|15M|19M|201MB/s|92MB/s|
|sfpatcher-1 p8 zstd|31.07%|1025M|4.6MB/s|18M|25M|424MB/s|189MB/s|
|sfpatcher-2 p1 lzma2|24.11%|976M|2.1MB/s|15M|20M|37MB/s|19MB/s|
|sfpatcher-2 p8 lzma2|24.15%|968M|5.0MB/s|20M|26M|108MB/s|45MB/s|
|sfpatcher-1 p1 clA zstd|21.72%|1141M|2.5MB/s|19M|22M|85MB/s|63MB/s|
|sfpatcher-1 p8 clA zstd|21.74%|1156M|5.4MB/s|26M|29M|240MB/s|129MB/s|
    

---
## Contact
housisong@hotmail.com  

