**HDiffPatch**
================
[![release](https://img.shields.io/badge/release-v2.4-blue.svg)](https://github.com/sisong/HDiffPatch/releases)  [![license](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/sisong/HDiffPatch/blob/master/LICENSE)  [![PRs Welcome](https://img.shields.io/badge/PRs-welcome-blue.svg)](https://github.com/sisong/HDiffPatch/pulls)   
binary data Diff & Patch C\C++ library, and file Diff & Patch tools.   
   
( Apk\Jar\Zip file diff&patch? can used [ApkDiffPatch](https://github.com/sisong/ApkDiffPatch) )

---
## command line usage:
**hdiffz**  [-m[-matchScore]|-s[-matchBlockSize]] [-c-compressType[-compressLevel]] [-o]  **oldFile newFile outDiffFile**
```
memory options:
  -m-matchScore
      all file load into Memory, DEFAULT, with matchScore;
      best diffFileSize, max memory requires O(oldFileSize*9+newFileSize),
      matchScore>=0, DEFAULT 6, recommended bin: 0--4 text: 4--9 etc... 
  -s-matchBlockSize
      all file load as Stream, with matchBlockSize;
      fast, memory requires O(oldSize*16/matchBlockSize+matchBlockSize*5),
      matchBlockSize>=2, DEFAULT 128, recommended 2^3--2^14 32k 1m etc...
special options:
  -c-compressType-compressLevel 
      set diffFile Compress type & level, otherwise DEFAULT uncompress;
      support compress type & level:
        (reference: https://github.com/sisong/lzbench/blob/master/lzbench171_sorted.md )
        -zlib[-{1..9}]              DEFAULT level 9
        -bzip2[-{1..9}]             DEFAULT level 9
        -lzma[-{0..9}[-dictSize]]   DEFAULT level 7
            dictSize can like 4096 or 4k or 4m or 128m etc..., DEFAULT 4m
        -lz4
        -lz4hc[-{3..12}]            DEFAULT level 11
        -zstd[-{0..22}]             DEFAULT level 20
  -o  Original diff, unsupport run with -s or -c, DEPRECATED;
      compatible with "diff_demo.c",
      diffFile must patch by "patch_demo.cpp" or "hpatchz -o ..."
```
**hpatchz**  [-m|-s[-nbytes]] [-o]  **oldFile diffFile outNewFile**
```
memory options:
  -m  oldFile all load into Memory;
      fast, memory requires O(oldFileSize + 4 * decompress stream)
  -s-nbytes 
      oldFile load as Stream, DEFAULT, with nbytes(cache memory size);
      memory requires O(nbytes + 4 * decompress stream),
      nbytes can like 524288 or 512k or 128m(DEFAULT) or 1g etc...
special options:
  -o  Original patch, DEPRECATED; compatible with "patch_demo.c",
      diffFile must created by "diff_demo.cpp" or "hdiffz -o ..."
```

---
## library API usage:

*  **create_diff**(newData,oldData,out diffData);
   
   release the diffData for update oldData.  
   `note:` create_diff() out **uncompressed** diffData;     
    you can compressed it by yourself or use **create_compressed_diff()**/patch_decompress() create **compressed** diffData;   
    if your file size very large or request faster and less memory requires, you can use **create_compressed_diff_stream()**. 
   
*  bool **patch**(out newData,oldData,diffData);
   
   ok , get the newData. 

---
*  **patch()** runs in O(oldSize+newSize) time , and requires oldSize+newSize+O(1) bytes of memory. (oldSize and newSize \<2^63 Byte);     
   **patch_stream()** min requires O(1) bytes of memory;   
   **patch_decompress()** min requires 4\*(decompress stream memory)+O(1) bytes.   
   
   **create_diff()**/**create_compressed_diff()** runs in O(oldSize+newSize) time , and if oldSize \< 2G Byte then requires oldSize\*5+newSize+O(1) bytes of memory; if oldSize \>= 2G Byte then requires oldSize\*9+newSize+O(1) bytes of memory;  
   **create_compressed_diff_stream()** min requires (oldSize\*16/kMatchBlockSize+kMatchBlockSize\*5)+O(1) bytes of memory.

---
## HDiffPatch vs  BsDiff4.3 & xdelta3.1:
system: macOS10.12.6, compiler: xcode8.3.3 x64, CPU: i7 2.5G(turbo3.7G,6MB L3 cache),SSD Disk,Memroy:8G*2 DDR3 1600MHz   
   (purge file cache before every test)
```
HDiffPatch2.4 hdiffz run by: -m -c-bzip2-9|-c-lzma-7-4m|-c-zlib-9 oldFile newFile outDiffFile
              hpatchz run by: -m oldFile diffFile outNewFile
BsDiff4.3 with bzip2 and all data in memory;
          (NOTE: when compiling BsDiff4.3-x64, suffix string index type int64 changed to int32, 
            faster and memroy requires to be halved!)   
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
Average Compression                 100.00%    31.76%     28.47%     6.64%     5.58%    5.01%    5.86%
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

HDiffPatch hdiffz run by: -s-64 -c-lzma-7-4m  oldFile newFile outDiffFile
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
by housisong@gmail.com  

