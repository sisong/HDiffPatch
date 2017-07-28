**HDiffPatch**
================
Version 2.0.0  
byte data Diff & Patch  C\C++ library.  

---
uses:

*  **create_diff(newData,oldData,out diffData);**
  
   release the diffData for update oldData.  
   (note: create_diff() out **uncompressed diffData**,   
    v2.0.0 you can use **create_compressed_diff()**/patch_decompress() create **compressed diffData**.)   
  
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
    **HDiff2.0.0 vs  BSDiff4.3 :**    
    system: macOS10.12.5, compiler: xcode8.3.3, CPU: i7 2.5G(turbo3.7G, 6MB shared L3 cache), Memroy: 8G*2 DDR3 1600MHz  
    (when compiling BSDiff4.3-x64, suffix string index type int64 changed to int32, memroy needs to be halved and faster)   
```
========================================================================================================
         Program               Uncompressed Compressed Compressed BSDiff4.3-x64    HDiff2.0-x64  HDiff2.0
(newVersion<--oldVersion)           (tar)     (bzip2)    (LZMA)       (bzip2)        (bzip2)    (LZMA)
--------------------------------------------------------------------------------------------------------
apache-maven-2.2.1-src <--2.0.11    5150720   1213258    1175464       115723         83935       80997
httpd_2.4.4-netware-bin <--2.2.24  22612480   4035904    3459747      2192308       1809555     1616435
httpd-2.4.4-src <-- 2.2.24         31809536   4775534    4141266      2492534       1882555     1717468
Firefox-21.0-mac-en-US.app<--20.0  98740736  39731352   33027837     16454403      15749937    14018095
emacs-24.3 <-- 23.4               185528320  42044895   33707445     12892536       9574423     8403235
eclipse-java-juno-SR2-macosx
  -cocoa-x86_64 <--x86_32         178595840 156054144  151542885      1595465       1587747     1561773
gcc-src-4.8.0 <--4.7.0            552775680  86438193   64532384     11759496       8433260     7288783
--------------------------------------------------------------------------------------------------------
Average Compression                 100.00%    31.76%     28.47%        6.64%         5.58%       5.01%
========================================================================================================

======================================================
   Program     I/O+run time(second)   run memory(MB)
                 BSDiff  HDiff         BSDiff  HDiff
------------------------------------------------------
apache-maven...     1.3    0.4           42      28
httpd bin...        8.6    3.0          148     124
httpd src...       20      5.1          322     233
Firefox...         94     28            829     582
emacs...          109     32           1400    1010
eclipse           100     33           1500    1000
gcc-src...        366     69           4420    3030
------------------------------------------------------
                359.09%  100%        140.64%   100%
======================================================
```
  
---
by housisong@gmail.com  

