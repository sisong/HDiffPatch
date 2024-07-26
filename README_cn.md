# [HDiffPatch]
[![release](https://img.shields.io/badge/release-v4.8.0-blue.svg)](https://github.com/sisong/HDiffPatch/releases) 
[![license](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/sisong/HDiffPatch/blob/master/LICENSE) 
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-blue.svg)](https://github.com/sisong/HDiffPatch/pulls)
[![+issue Welcome](https://img.shields.io/github/issues-raw/sisong/HDiffPatch?color=green&label=%2Bissue%20welcome)](https://github.com/sisong/HDiffPatch/issues)   
[![release](https://img.shields.io/github/downloads/sisong/HDiffPatch/total?color=blue)](https://github.com/sisong/HDiffPatch/releases)
[![Build Status](https://github.com/sisong/HDiffPatch/workflows/ci/badge.svg?branch=master)](https://github.com/sisong/HDiffPatch/actions?query=workflow%3Aci+branch%3Amaster)
[![Build status](https://ci.appveyor.com/api/projects/status/t9ow8dft8lt898cv/branch/master?svg=true)](https://ci.appveyor.com/project/sisong/hdiffpatch/branch/master)   
 中文版 | [english](README.md)   

[HDiffPatch] 是一个C\C++库和命令行工具，用于在二进制文件或文件夹之间执行 **diff**(创建补丁) 和 **patch**(打补丁)；跨平台、运行速度快、创建的补丁小、支持巨大的文件并且diff和patch时都可以控制内存占用量。   

[HDiffPatch] 定义了自己的补丁包格式，同时这个库也完全兼容了 [bsdiff4] 和 [endsley/bsdiff] 的补丁包格式，并[部分兼容](https://github.com/sisong/HDiffPatch/issues/369#issuecomment-1869798843)了 [open-vcdiff] 和 [xdelta3] 的补丁包格式 [VCDIFF(RFC 3284)]。   

如果你需要在嵌入式系统(MCU、NB-IoT)等设备上进行增量更新(OTA), 可以看看例子 [HPatchLite], +[tinyuz] 解压缩器可以在1KB内存的设备上运行!   

需要更新你自己的安卓apk? 需要对Jar或Zip文件执行 diff 和 patch ? 可以试试 [ApkDiffPatch], 可以创建更小的补丁!  注意: *ApkDiffPath 不能被安卓应用商店作为增量更新所用，因为该算法要求在diff前对apk文件进行重新签名。*   

[sfpatcher] 不要求对apk文件进行重新签名 (类似 [archive-patcher])，是为安卓应用商店专门设计优化的算法，patch速度是 archive-patcher 的xx倍，并且只需要O(1)内存。   

如果你没有旧版本的数据(或者旧版本非常多或者被修改)，因此不能提前创建好补丁包。那你可以看看使用同步算法来进行增量更新的例子 [hsynz] (类似 [zsync])，新版本只需要发布处理一次，然后旧版本数据的拥有者可以根据获得的新版本的信息自己执行diff和patch。hsynz 支持 zstd 压缩算法并且比 zsync 速度更快。
   
注意: *本库不处理文件元数据，如文件最后写入时间、权限、链接文件等。对于这个库，文件就像一个字节流；如果需要您可以扩展此库或使用其他工具。*   
   

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
## 二进制发布包
[从 release 下载](https://github.com/sisong/HDiffPatch/releases) : 命令行程序分别运行在 Windows、Linux、MacOS操作系统。 .so库文件用于安卓。   
用命令行创建一个补丁:   
`$hdiffz -m-6 -SD -c-zstd-21-24 -d oldPath newPath outDiffFile`   
如果文件非常大，可以试试将 `-m-6` 改为 `-s-64`   
打补丁:   
`$hpatchz oldPath diffFile outNewPath`   

## 自己编译
`$ cd <dir>/HDiffPatch`   
### Linux or MacOS X ###
试试:   
`$ make LDEF=0 LZMA=0 ZSTD=0 MD5=0`   
bzip2 : 如果编译失败，显示 `fatal error: bzlib.h: No such file or directory`，请使用系统的包管理器安装libbz2，然后再试一次；或者下载并使用libbz2源代码来编译:
```
$ git clone https://github.com/sisong/bzip2.git ../bzip2
$ make LDEF=0 LZMA=0 ZSTD=0 MD5=0 BZIP2=1
```
如果需要支持 lzma、zstd 和 md5 等 默认编译设置，试试:    
```
$ git clone https://github.com/sisong/libmd5.git ../libmd5
$ git clone https://github.com/sisong/lzma.git ../lzma
$ git clone https://github.com/sisong/zstd.git ../zstd
$ git clone https://github.com/sisong/zlib.git ../zlib
$ git clone https://github.com/sisong/libdeflate.git ../libdeflate
$ make
```    
提示:你可以使用 `$ make -j` 来并行编译。
   
### Windows ###
使用 [`Visual Studio`](https://visualstudio.microsoft.com) 打开 `builds/vc/HDiffPatch.sln` 来编译之前，先将第三方库下载到同级文件夹中，如下所示: 
```
$ git clone https://github.com/sisong/libmd5.git ../libmd5
$ git clone https://github.com/sisong/lzma.git ../lzma
$ git clone https://github.com/sisong/zstd.git ../zstd
$ git clone https://github.com/sisong/zlib.git   ../zlib
$ git clone https://github.com/sisong/libdeflate.git ../libdeflate
$ git clone https://github.com/sisong/bzip2.git  ../bzip2
```
   
### libhpatchz.so for Android ###   
* 安装 [Android NDK](https://developer.android.google.cn/ndk/downloads)
* `$ cd <dir>/HDiffPatch/builds/android_ndk_jni_mk`
* `$ build_libs.sh`  (或者 Windows下执行 `$ build_libs.bat`, 就可以得到 \*.so 安卓库了)
* 在你的安卓项目中添加 `com/github/sisong/HPatch.java` (所在路径 `HDiffPatch/builds/android_ndk_jni_mk/java/`) 和 .so 文件， java 代码就可以调用 libhpatchz.so 中的 patch 函数了。
   

---
   
## diff 命令行用法和参数说明：
创建新旧版本间的补丁： **hdiffz** [options] **oldPath newPath outDiffFile**   
压缩一个文件或文件夹： **hdiffz** [-c-...]  **"" newPath outDiffFile**   
测试补丁是否正确： **hdiffz**    -t     **oldPath newPath testDiffFile**   
补丁使用新的压缩插件另存： **hdiffz** [-c-...]  **diffFile outDiffFile**   
显示补丁的信息: **hdiffz** -info **diffFile**   
创建该版本的校验清单： **hdiffz** [-g#...] [-C-checksumType] **inputPath -M#outManifestTxtFile**   
校验输入数据后创建补丁： **hdiffz** [options] **-M-old#oldManifestFile -M-new#newManifestFile oldPath newPath outDiffFile**   
```
  oldPath、newPath、inputPath 可以是文件或文件夹, 
  oldPath可以为空, 输入参数为 ""
选项:
  -m[-matchScore]
      默认选项; 所有文件都会被加载到内存; 一般生成的补丁文件比较小;
      需要的内存大小:(新版本文件大小+ 旧版本文件大小*5(或*9 当旧版本文件大小>=2GB时))+O(1);
      匹配分数matchScore>=0,默认为6,二进制数据时推荐设置为0到4,文件数据时推荐4--9等,跟输入
      数据的可压缩性相关,一般输入数据的可压缩性越大,这个值就可以越大。
  -s[-matchBlockSize]
      所有文件当作文件流加载;一般速度比较快;
      需要的内存大小: O(旧版本文件大小*16/matchBlockSize+matchBlockSize*5*parallelThreadNumber);
      匹配块大小matchBlockSize>=4, 默认为64, 推荐16,32,48,1k,64k,1m等;
      一般匹配块越大,内存占用越小,速度越快,但补丁包可能变大。
  -block-fastMatchBlockSize
      必须和-m配合使用;
      在使用较慢的逐字节匹配之前使用基于块的快速匹配, 默认-block-4k;
      如果设置为-block-0，意思是关闭基于块的提前匹配；
      快速块匹配大小fastMatchBlockSize>=4, 推荐256,1k,64k,1m等;
      如果新版本和旧版本相同数据比较多,那diff速度就会比较快,并且减少内存占用,
      但有很小的可能补丁包会变大。
  -cache
      必须和-m配合使用;
      给较慢的匹配开启一个大型缓冲区,来加快匹配速度(不影响补丁大小), 默认不开启;
      如果新版本和旧版本不相同数据比较多,那diff速度就会比较快;
      该大型缓冲区最大占用O(旧版本文件大小)的内存, 并且需要较多的时间来创建(从而可能降低diff速度)。
  -SD[-stepSize]
      创建单压缩流的补丁文件, 这样patch时就只需要一个解压缩缓冲区, 并且可以支持边下载边patch;
      压缩步长stepSize>=(1024*4), 默认为256k, 推荐64k,2m等。
  -BSD
      创建一个和bsdiff4兼容的补丁, 不支持参数为文件夹。
      也支持和-SD选项一起运行(不使用其stepSize), 从而创建单压缩流的补丁文件，
      兼容endsley/bsdiff格式 (https://github.com/mendsley/bsdiff)。
  -VCD[-compressLevel[-dictSize]]
      创建一个标准规范VCDIFF格式的补丁, 不支持参数为文件夹。
      默认输出补丁不带压缩, 格式和 $open-vcdiff ... 或 $xdelta3 -S -e -n ... 命令输出的补丁格式兼容；
      如果设置了压缩级别compressLevel, 那输出格式和 $xdelta3 -S lzma -e -n ...命令输出的补丁格式兼容；
      压缩输出时补丁文件使用7zXZ(xz)算法压缩, compressLevel可以选择0到9, 默认级别7；
      压缩字典大小dictSize可以设置为 4096, 4k, 4m, 16m等, 默认为8m
      支持多线程并行压缩。
      注意: 输出的补丁的格式中可能使用了巨大的源窗口大小!
  -p-parallelThreadNumber
      设置线程数parallelThreadNumber>1时,开启多线程并行模式;
      默认为4;需要占用较多的内存。
  -p-search-searchThreadNumber
      必须和-s[-matchBlockSize]配合使用;
      默认情况下搜索线程数searchThreadNumber的值和parallelThreadNumber相同;
      但当matchBlockSize较小时，多线程搜索需要频繁的随机磁盘读取，
      所以有些时候多线程搜索反而比单线程搜索还慢很多!
      如果设置searchThreadNumber<=1，可以关闭多线程搜索模式。
  -c-compressType[-compressLevel]
      设置补丁数据使用的压缩算法和压缩级别等, 默认不压缩;
      补丁另存时,使用新的压缩参数设置来输出新补丁;
      支持的压缩算法、压缩级别和字典大小等:
        -c-zlib[-{1..9}[-dictBits]]     默认级别 9
            压缩字典比特数dictBits可以为9到15, 默认为15。
            支持多线程并行压缩,很快！
        -c-ldef[-{1..12}]               默认级别 12
            输出压缩数据格式兼容于-c-zlib, 但比zlib压缩得更快或压缩得更小;
            使用了libdeflate压缩算法，且压缩字典比特数dictBits始终为15。
            支持多线程并行压缩,很快！
        -c-bzip2[-{1..9}]               (或 -bz2) 默认级别 9
        -c-pbzip2[-{1..9}]              (或 -pbz2) 默认级别 8
            支持并行压缩,生成的补丁和-c-bzip2的输出格式稍有不同。
        -c-lzma[-{0..9}[-dictSize]]     默认级别 7
            压缩字典大小dictSize可以设置为 4096, 4k, 4m, 128m等, 默认为8m
            支持2个线程并行压缩。
        -c-lzma2[-{0..9}[-dictSize]]    默认级别 7
            压缩字典大小dictSize可以设置为 4096, 4k, 4m, 128m等, 默认为8m
            支持多线程并行压缩,很快。
            警告: lzma和lzma2是不同的压缩编码格式。
        -c-zstd[-{0..22}[-dictBits]]    默认级别 20
            压缩字典比特数dictBits 可以为10到30, 默认为23。
            支持多线程并行压缩,较快。
  -C-checksumType
      为文件夹间diff设置数据校验算法, 默认为fadler64;
      支持的校验选项:
        -C-no                   不校验
        -C-crc32
        -C-fadler64             默认
        -C-md5
  -n-maxOpenFileNumber
      为文件夹间的-s模式diff设置最大允许同时打开的文件数;
      maxOpenFileNumber>=8, 默认为48; 合适的限制值可能不同系统下不同。
  -g#ignorePath[#ignorePath#...]
      为文件夹间的diff设置忽略路径(路径可能是文件或文件夹); 忽略路径列表的格式如下:
        #.DS_Store#desktop.ini#*thumbs*.db#.git*#.svn/#cache_*/00*11/*.tmp
      # 意味着路径名称之间的间隔; (如果名称中有“#”号, 需要改写为“#:” )
      * 意味着匹配名称中的任意一段字符; (如果名称中有“*”号, 需要改写为“*:” )
      / 如果该符号放在名称末尾,意味着必须匹配文件夹;
  -g-old#ignorePath[#ignorePath#...]
      为文件夹间的diff设置忽略旧版本的路径;
      如果旧版本中的某个文件数据可以被运行中的程序改动,那可以将该文件放到该忽略列表中;
  -g-new#ignorePath[#ignorePath#...]
      为文件夹间的diff设置忽略新版本的路径;
      一般情况下,该列表应该是空的吧?
  -M#outManifestTxtFile
      创建inputPath的校验清单文件; 该输出文件是一个文本文件, 保存着inputPath中所有
      文件和文件夹的信息; 该文件被用于manifest diff, 支持diff之前重新校验数据的正确性;
      可用于确定历史版本的数据没有被篡改!
  -M-old#oldManifestFile
      设置oldPath的清单文件oldManifestFile; 如果没有oldPath,那就不需要设置-M-old;
  -M-new#newManifestFile
      设置newPath的清单文件newManifestFile;
  -D  强制执行文件夹间的diff, 即使输入的是2个文件; 从而为文件间的补丁添加校验功能。
      默认情况下oldPath或newPath有一个是文件夹时才会执行文件夹间的diff。
  -neq
      打开检查: 如果newPath和oldPath的数据都相同，则返回错误；
      默认不执行该相等检查。
  -d  只执行diff, 不要执行patch检查, 默认会执行patch检查.
  -t  只执行patch检查, 检查是否 patch(oldPath,testDiffFile)==newPath ?
  -f  强制文件写覆盖, 忽略输出的路径是否已经存在;
      默认不执行覆盖, 如果输出路径已经存在, 直接返回错误;
      如果设置了-f,但路径已经存在并且是一个文件夹, 那么会始终返回错误。
  --patch
      切换到 hpatchz 模式; 可以支持hpatchz命令行的相关参数和功能。
  -info
      显示补丁的信息。
  -v  输出程序版本信息。
  -h 或 -?
      输出命令行帮助信息 (该说明)。
```
   
## patch 命令行用法和参数说明：  
打补丁： **hpatchz** [options] **oldPath diffFile outNewPath**   
解压缩一个文件或文件夹： **hpatchz** [options] **"" diffFile outNewPath**   
显示补丁的信息: **hpatchz** -info **diffFile**   
创建一个自释放包： **hpatchz** [-X-exe#selfExecuteFile] **diffFile -X#outSelfExtractArchive**   
  (将目标平台的hpatchz可执行文件和补丁包文件合并成一个可执行文件, 称作自释放包SFX)   
执行一个自释放包： **selfExtractArchive** [options] **oldPath -X outNewPath**   
  (利用自释放包来打补丁,将包中自带的补丁数据应用到oldPath上, 合成outNewPath)   
执行一个自解压包： **selfExtractArchive**  (等价于：$selfExtractArchive -f {""|".\"} -X "./")
```
  oldPath可以为空, 输入参数为 ""
选项:
  -s[-cacheSize]
      默认选项,并且默认设置为-s-4m; oldPath所有文件被当作文件流来加载;
      cacheSize可以设置为262144 或 256k, 512m, 2g等
      需要的内存大小: (cacheSize + 4*解压缩缓冲区)+O(1)
      而如果diffFile是单压缩流的补丁文件(用hdiffz -SD-stepSize所创建)
        那需要的内存大小: (cacheSize+ stepSize + 1*解压缩缓冲区)+O(1);
      如果diffFile是用hdiffz -BSD、bsdiff4、hdiffz -VCD、xdelta3、open-vcdiff所创建
        那需要的内存大小: (cacheSize + 3*解压缩缓冲区);
      如果diffFile是VCDIFF格式补丁文件： 如果是用hdiffz -VCD创建，那推荐用-s模式；
        如果是xdelta、open-vcdiff所创建，那推荐用-m模式。
  -m  oldPath所有文件数据被加载到内存中;
      需要的内存大小: (oldFileSize + 4*解压缩缓冲区)+O(1)
      而如果diffFile是单压缩流的补丁文件(用hdiffz -SD-stepSize所创建)
        那需要的内存大小: (oldFileSize+ stepSize + 1*解压缩缓冲区)+O(1);
      如果diffFile是用hdiffz -BSD、bsdiff4所创建
        那需要的内存大小: (oldFileSize + 3*解压缩缓冲区);
      如果diffFile是VCDIFF格式补丁文件(用hdiffz -VCD、xdelta3、open-vcdiff所创建)
        那需要的内存大小: (源窗口大小+目标窗口大小 + 3*解压缩缓冲区);
  -C-checksumSets
      为文件夹patch设置校验方式, 默认设置为 -C-new-copy;
      校验设置支持(可以多选):
        -C-diff         校验补丁数据;
        -C-old          校验引用到的旧版本的文件;
        -C-new          校验有修改过的新版本的文件;
        -C-copy         校验从旧版本直接copy到新版本的文件;
        -C-no           不执行校验;
        -C-all          等价于: -C-diff-old-new-copy;
  -C-no 或 -C-new
      如果diffFile是VCDIFF格式补丁文件, 使用该选项可以关闭或打开校验，默认打开.
  -n-maxOpenFileNumber
      为文件夹间的-s模式patch设置最大允许同时打开的文件数;
      maxOpenFileNumber>=8, 默认为24; 合适的限制值可能不同系统下不同。
  -f  强制文件写覆盖, 忽略输出的路径是否已经存在;
      默认不执行覆盖, 如果输出路径已经存在, 直接返回错误;
      该模式支持oldPath和outNewPath为相同路径!(patch到一个临时路径,完成后再覆盖回old)
      如果设置了-f,但outNewPath已经存在并且是一个文件:
        如果patch输出一个文件, 那么会执行写覆盖;
        如果patch输出一个文件夹, 那么会始终返回错误。
      如果设置了-f,但outNewPath已经存在并且是一个文件夹:
        如果patch输出一个文件, 那么会始终返回错误;
        如果patch输出一个文件夹, 那么会执行写覆盖, 但不会删除文件夹中已经存在的无关文件。
  -info
      显示补丁的信息。
  -v  输出程序版本信息。
  -h 或 -?
      输出命令行帮助信息 (该说明)。
```
   
---
## 库 API 使用说明:
**diff**和**patch** 函数在文件: `libHDiffPatch/HDiff/diff.h` & `libHDiffPatch/HPatch/patch.h`   
**dir_diff()** 和 **dir patch** 在: `dirDiffPatch/dir_diff/dir_diff.h` & `dirDiffPatch/dir_patch/dir_patch.h`   
### 使用方式:
* **create diff**(in newData,in oldData,out diffData);
   发布 diffData 来升级 oldData。  
* **patch**(out newData,in oldData,in diffData);
   ok，得到了 newData。 
### v1 API, 未压缩补丁:
* **create_diff()**
* **patch()**
* **patch_stream()**
* **patch_stream_with_cache()**
### v2 API, 压缩的补丁包:
* **create_compressed_diff()**
* **create_compressed_diff_stream()**
* **resave_compressed_diff()**
* **patch_decompress()**
* **patch_decompress_with_cache()**
* **patch_decompress_mem()**
### v3 API, 在文件夹间 **diff**&**patch**:
* **dir_diff()**
* **TDirPatcher_\*()** functions with **struct TDirPatcher**
### v4 API, 单压缩流补丁包:
* **create_single_compressed_diff()**
* **create_single_compressed_diff_stream()**
* **resave_single_compressed_diff()**
* **patch_single_stream()**
* **patch_single_stream_mem()**
* **patch_single_compressed_diff()**
* **patch_single_stream_diff()**
#### hpatch lite API,  为 MCU,NB-IoT... 优化了的 hpatch (例子 [HPatchLite]): 
* **create_lite_diff()**
* **hpatch_lite_open()**
* **hpatch_lite_patch()**
#### bsdiff ([bsdiff4] & [endsley/bsdiff]) 兼容包装 API:
* **create_bsdiff()**
* **create_bsdiff_stream()** 
* **bspatch_with_cache()**
#### vcdiff ([open-vcdiff] & [xdelta3]) 兼容包装 API: 
* **create_vcdiff()**
* **create_vcdiff_stream()**
* **vcpatch_with_cache()**
#### hsynz API, 同步 diff&patch (例子 [hsynz]): 
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
测试用例([从 OneDrive 下载](https://1drv.ms/u/s!Aj8ygMPeifoQgULlawtabR9lhrQ8)):   
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
   

**测试 PC**: Windows11, CPU R9-7945HX, SSD PCIe4.0x4 4T, DDR5 5200MHz 32Gx2   
**参与测试的程序和版本**: HDiffPatch4.6.3, hsynz 1.1.0, BsDiff4.3, xdelta3.1, zstd1.5.2  
**参与测试的程序的参数**:   
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
   
**测试结果取平均**:
|程序|包大小|diff内存|速度|patch内存|最大内存|速度|
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
    

## 使用 Apk 文件来测试: 
测试用例:
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
   

**对测试程序的参数进行一些调整**:   
**hdiffz ...** `-m-6 -SD` changed to `-m-1 -SD-2m -cache`, `-s-64 -SD` changed to `-s-16 -SD-2m`   
**hdiffz ...** lzma2 dict size `16m` changed to `8m`, zstd dict bit `24` changed to `23`   
**hsynz ...** make `-s-2k` changed to `-s-1k`   
& adding **hsynz p1**, **hsynz p8** make without compressor   
**archive-patcher** v1.0, diff with `--generate --old {old} --new {new} --patch {pat}`,   
  patch with `--apply --old {old} --patch {pat} --new {new}`   
  注意: archive-patcher 统计的补丁包大小是经过了 lzma2-9-16m 压缩后的, 而 diff&patch 统计时并不包含压缩和解压缩补丁所需的内存和时间。   
**sfpatcher -1 zstd** v1.1.1 diff with `-o-1 -c-zstd-21-23 -m-1 -step-3m -lp-512k -p-8 -cache -d {old} {new} {pat}`   
& **sfpatcher -2 lzma2** diff with `-o-2 -c-lzma2-9-4m -m-1 -step-2m -lp-8m -p-8 -cache -d {old} {new} {pat}`   
**sfpatcher -1 clA zstd** v1.3.0 used `$sf_normalize -cl-A` normalized apks before diff   
 sfpatcher patch with `-lp -p-8 {old} {pat} {new}`   
adding test hpatchz&sfpatcher on Android, arm CPU 麒麟980(2×A76 2.6G + 2×A76 1.92G + 4×A55 1.8G)   
( [archive-patcher]、[sfpatcher] diff&patch 时针对apk文件格式进行了优化 )  

**测试结果取平均**:
|程序|包大小|diff内存|速度|patch内存|最大内存|速度|arm 麒麟980|
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
## 联系
housisong@hotmail.com  

