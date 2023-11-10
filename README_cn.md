# [HDiffPatch](https://github.com/sisong/HDiffPatch) 命令行使用说明中文版

## diff 命令行用法和参数说明：
创建新旧版本间的补丁： **hdiffz** [options] **oldPath newPath outDiffFile**   
压缩一个文件或文件夹： **hdiffz** [-c-...]  **"" newPath outDiffFile**   
测试补丁是否正确： **hdiffz**    -t     **oldPath newPath testDiffFile**   
补丁使用新的压缩插件另存： **hdiffz** [-c-...]  **diffFile outDiffFile**   
创建该版本的校验清单： **hdiffz** [-g#...] [-C-checksumType] **inputPath -M#outManifestTxtFile**   
校验输入数据后创建补丁： **hdiffz** [options] **-M-old#oldManifestFile -M-new#newManifestFile oldPath newPath outDiffFile**   
```
  oldPath、newPath、inputPath 可以是文件或文件夹, 
  oldPath可以为空, 输入参数为 ""
内存选项:
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
其他选项:
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
        -c-bzip2[-{1..9}]               (或 -bz2) 默认级别 9
        -c-pbzip2[-{1..9}]              (或 -pbz2) 默认级别 8
            支持并行压缩,生成的补丁和-c-bzip2的输出格式不同,一般也可能稍大一点。
        -c-lzma[-{0..9}[-dictSize]]     默认级别 7
            压缩字典大小dictSize可以设置为 4096, 4k, 4m, 128m等, 默认为8m
            支持2个线程并行压缩。
        -c-lzma2[-{0..9}[-dictSize]]    默认级别 7
            压缩字典大小dictSize可以设置为 4096, 4k, 4m, 128m等, 默认为8m
            支持多线程并行压缩,很快。
            警告: lzma和lzma2是不同的压缩编码格式。
        -c-zstd[-{0..22}[-dictBits]]    默认级别 20
            压缩字典比特数dictBits 可以为10到31, 默认为24。
            支持多线程并行压缩,很快。
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
  -d  只执行diff, 不要执行patch检查, 默认会执行patch检查.
  -t  只执行patch检查, 检查是否 patch(oldPath,testDiffFile)==newPath ?
  -f  强制文件写覆盖, 忽略输出的路径是否已经存在;
      默认不执行覆盖, 如果输出路径已经存在, 直接返回错误;
      如果设置了-f,但路径已经存在并且是一个文件夹, 那么会始终返回错误。
  --patch
      切换到 hpatchz 模式; 可以支持hpatchz命令行的相关参数和功能。
  -v  输出程序版本信息。
  -h 或 -?
      输出命令行帮助信息 (该说明)。
```
   
## patch 命令行用法和参数说明：  
打补丁： **hpatchz** [options] **oldPath diffFile outNewPath**   
解压缩一个文件或文件夹： **hpatchz** [options] **"" diffFile outNewPath**   
创建一个自释放包： **hpatchz** [-X-exe#selfExecuteFile] **diffFile -X#outSelfExtractArchive**   
  (将目标平台的hpatchz可执行文件和补丁包文件合并成一个可执行文件, 称作自释放包SFX)   
执行一个自释放包： **selfExtractArchive** [options] **oldPath -X outNewPath**   
  (利用自释放包来打补丁,将包中自带的补丁数据应用到oldPath上, 合成outNewPath)   
执行一个自解压包： **selfExtractArchive**   (等价于： selfExtractArchive -f "" -X "./")
```
内存选项:
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
其他选项:
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
  -v  输出程序版本信息。
  -h 或 -?
      输出命令行帮助信息 (该说明)。
```
   
