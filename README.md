**HDiffPatch**
================
Version 2.0.0  
byte data Diff & Patch  C\C++ library.  

---
uses:

*  **create_diff(newData,oldData,out diffData);**
  
   release the diffData for update oldData.  
   (note: create_diff() out **uncompressed** diffData,   
    v2.0.0 you can use **create_compressed_diff()**/patch_decompress() create **compressed** diffData.)   
  
*  **bool patch(out newData,oldData,diffData);**
  
   ok , get the newData. 
  

---
*  
    **HPatch** runs in O(oldSize+newSize) time , and requires oldSize+newSize+O(1) bytes of memory. (oldSize and newSize \<2^63 Byte)     
    notice: if use **patch_stream()**, min requires O(1) bytes of memory,   
     **patch_decompress()** min requires O(4*decompress stream memory)+O(1) bytes.   
            
    **HDiff** runs in O(oldSize+newSize) time , and if oldSize \< 2G Byte then requires oldSize\*5+newSize+O(1) bytes of memory; if oldSize \>= 2G Byte then requires oldSize\*9+newSize+O(1) bytes of memory.    
  
---
*  
    **HDiff2.0.0 vs  BsDiff4.3 :**    
    system: macOS10.12.5, compiler: xcode8.3.3 x64, CPU: i7 2.5G(turbo3.7G, 6MB shared L3 cache), Memroy: 8G*2 DDR3 1600MHz  
    (when compiling BsDiff4.3-x64, suffix string index type int64 changed to int32, memroy needs to be halved and faster)   
```
=========================================================================================================
         Program               Uncompressed Compressed Compressed  BsDiff4.3  HDiff2.0 HDiff2.0 HDiff2.0
(newVersion<--oldVersion)           (tar)     (bzip2)    (lzma)    (bzip2)     (bzip2)  (lzma)   (zlib)    
---------------------------------------------------------------------------------------------------------
apache-maven-2.2.1-src <--2.0.11    5150720   1213258    1175464     115723     83935    80997    91921
httpd_2.4.4-netware-bin <--2.2.24  22612480   4035904    3459747    2192308   1809555  1616435  1938953
httpd-2.4.4-src <-- 2.2.24         31809536   4775534    4141266    2492534   1882555  1717468  2084843
Firefox-21.0-mac-en-US.app<--20.0  98740736  39731352   33027837   16454403  15749937 14018095 15417854
emacs-24.3 <-- 23.4               185528320  42044895   33707445   12892536   9574423  8403235 10964939
eclipse-java-juno-SR2-macosx
  -cocoa-x86_64 <--x86_32         178595840 156054144  151542885    1595465   1587747  1561773  1567700
gcc-src-4.8.0 <--4.7.0            552775680  86438193   64532384   11759496   8433260  7288783  9445004
---------------------------------------------------------------------------------------------------------
Average Compression                 100.00%    31.76%     28.47%      6.64%     5.58%    5.01%    5.86%
=========================================================================================================

============================================================================================================
   Program   run time(Second)   memory(MB)          run time(Second)                  memory(MB)
               BsDiff HDiff   BsDiff  HDiff    BsPatch  HPatch HPatch HPatch   BsPatch HPatch HPatch HPatch 
              (bzip2)(bzip2)  (bzip2)(bzip2)   (bzip2)  (bzip2)(lzma) (zlib)   (bzip2) (bzip2)(lzma) (zlib)
------------------------------------------------------------------------------------------------------------
apache-maven...  1.3   0.4       42     28       0.11    0.06   0.02   0.02       14      3.4   3.0   2.5
httpd bin...     8.6   3.0      148    124       0.79    0.45   0.21   0.15       50      7.8  10.5   2.3
httpd src...    20     5.1      322    233       1.1     0.58   0.26   0.20       78      8.1  11.0   2.6
Firefox...      94    28        829    582       3.0     2.3    1.2    0.60      198     14.5  18.2   2.4
emacs...       109    32       1400   1010       5.0     2.5    1.2    0.89      348     15.3  11.9   2.6
eclipse        100    33       1500   1000       1.5     0.56   0.49   0.40      350      5.4   5.2   2.5
gcc-src...     366    69       4420   3030       7.6     3.8    2.2    2.01     1020     15.6  18.2   2.4
------------------------------------------------------------------------------------------------------------
Average        100%  28.9%     100%   71.5%      100%    54.0%  27.7%  20.9%     100%     9.3% 10.3%  4.1%
============================================================================================================
```
  
---
by housisong@gmail.com  

