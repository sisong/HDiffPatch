**HDiffPatch**
================
Version 2.1.2   
byte data Diff & Patch  C\C++ library.  

---
uses:

*  **create_diff(newData,oldData,out diffData);**
  
   release the diffData for update oldData.  
   (note: create_diff() out **uncompressed** diffData,   
    you can compressed it by yourself or use **create_compressed_diff()**/patch_decompress() create **compressed** diffData;   
    if your file size very large or request faster and less memory requires, you can use **create_compressed_diff_stream()**.) 
  
*  **bool patch(out newData,oldData,diffData);**
  
   ok , get the newData. 
  
---
*  **HPatch:**  **patch()** runs in O(oldSize+newSize) time , and requires oldSize+newSize+O(1) bytes of memory. (oldSize and newSize \<2^63 Byte);     
     **patch_stream()**, min requires O(1) bytes of memory;   
     **patch_decompress()** min requires 4\*(decompress stream memory)+O(1) bytes.   
            
   **HDiff:**  **create_diff()**/**create_compressed_diff()** runs in O(oldSize+newSize) time , and if oldSize \< 2G Byte then requires oldSize\*5+newSize+O(1) bytes of memory; if oldSize \>= 2G Byte then requires oldSize\*9+newSize+O(1) bytes of memory;  
   **create_compressed_diff_stream()** min requires (oldSize\*16/kMatchBlockSize+kMatchBlockSize\*5)+O(1) bytes of memory.
  
---
*  **HDiffPatch vs  BsDiff4.3 & xdelta3.1:**  
system: macOS10.12.6, compiler: xcode8.3.3 x64, CPU: i7 2.5G(turbo3.7G,6MB L3 cache),SSD Disk,Memroy:8G*2 DDR3 1600MHz   
   (purge file cache before every test)
```
HDiff2.0 diff used create_compressed_diff() + bzip2 | lzma | zlib , all data in memory;
         patch used patch_decompress() + load oldFile data into memory.
BsDiff4.3 with bzip2 and all data in memory;
(when compiling BsDiff4.3-x64, suffix string index type int64 changed to int32, 
   faster and memroy requires to be halved.)   
=======================================================================================================
         Program               Uncompressed Compressed Compressed BsDiff4.3          HDiff2.0
(newVersion<--oldVersion)           (tar)     (bzip2)    (lzma)    (bzip2)   (bzip2    lzma      zlib)
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
               BsDiff HDiff   BsDiff  HDiff    BsPatch      HPatch2.0       BsPatch    HPatch2.0
              (bzip2)(bzip2)  (bzip2)(bzip2)   (bzip2) (bzip2  lzma  zlib)  (bzip2)(bzip2 lzma zlib)
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


HDiff2.1 diff used create_compressed_diff_stream() + bzip2 , kMatchBlockSize=128, all use file stream;
         patch used patch_decompress(), all use file stream.
xdelta3.1 diff run by: -e -s old_file new_file delta_file   
         patch run by: -d -s old_file delta_file decoded_new_file
(note fix: xdelta3.1 diff "gcc-src..." fail, add -B 530000000 diff ok,
    out 14173073B and used 1070MB memory.)
=======================================================================================================
   Program              diff       run time(Second)  memory(MB)    patch run time(Second) memory(MB)
                  xdelta3   HDiff    xdelta3 HDiff  xdelta3 HDiff   xdelta3 HPatch2.0   xdelta3 HPatch
-------------------------------------------------------------------------------------------------------
apache-maven...   116265     84585     0.16   0.14     65    10       0.07    0.06         12     6
httpd bin...     2174098   2091177     1.1    1.2     157    13       0.25    0.65         30     8
httpd src...     2312990   2044177     1.3    1.6     185    16       0.30    0.91         50     8
Firefox...      28451567  27510882    16     11       225    17       2.0     4.1         100     8
emacs...        31655323  12067133    19      8.8     220    18       3.2     4.0          97    10
eclipse          1590860   1637038     1.5    1.2     207    17       0.46    0.49         77     8 
gcc-src...     107003829  12439052    56     19       224    48       9.7     9.5         102    11 
           (fix 14173073)
-------------------------------------------------------------------------------------------------------
Average           12.18%    7.84%     100%  78.4%     100%  11.1%      100%  169.1%       100%  18.9%
              (fix 9.78%)
=======================================================================================================
```
  
---
by housisong@gmail.com  

