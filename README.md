HDiffPatch
================
Version 1.0.1  
byte data Diff & Patch  C\C++ library.  

---
uses like this:

*  **create_diff(newData,oldData,out diffData);**
  
  release the diffData for update oldData.  
  (using LZMA or ZIP compress the diffData before release is a better option. )  
  
*  **bool patch(out newData,oldData,diffData);**
  
  ok  
  
---
*  
    HDiff runs in O(oldSize+newSize) time,and requires oldSize*5+newSize+O(1) bytes of memory.  
    HPatch runs in O(oldSize+newSize) time,and requires oldSize+newSize+O(1) bytes of memory.  
    oldSize or newSize < 2G Byte.  
  
===
**HDiff1.0 vs  BSDiff4.3 :**    
    system: Windows7-x64 ,  CPU: i7 2.3G , Memroy: 6G DDR3 1600MHz, 7Zip use LZMA2  

```
===================================================================================================================
         Program               Uncompressed Compressed Compressed BSDiff-4.3-x64 HDiff-1.0-x64 HDiffb+zip2 HDiff+7z
(newVersion<--oldVersion)          (tar)      (bzip2)     (7z)     (with bzip2)  (Uncompressed)
-------------------------------------------------------------------------------------------------------------------
apache-maven-2.2.1-src <--2.0.11    5150720   1213258    1175464       115723        312852      105941       93996
httpd_2.4.4-netware-bin <--2.2.24  22612480   4035904    3459747      2192308       4154954     1961079     1765089
httpd-2.4.4-src <-- 2.2.24         31809536   4775534    4141266      2492534       4893630     2146854     1931397
Firefox-21.0-mac-en-US.app<--20.0  98740736  39731352   33027837     16454403      26000512    16017001    14637920
emacs-24.3 <-- 23.4               185528320  42044895   33707445     12892536      25236398    11139071     9790479
eclipse-java-juno-SR2-macosx
  -cocoa-x86_64 <--x86_32         178595840 156054144  151542885      1595465       1650702     1591549     1569777
gcc-src-4.8.0 <--4.7.0            552775680  86438193   64532384     11759496      26538554     9961692     8668268
-------------------------------------------------------------------------------------------------------------------
Average Compression                 100.00%    31.76%     28.47%        6.63%        12.21%       6.06%       5.46%
===================================================================================================================

================================================
   Program      I/O+run time(s)   run memory(MB)
                BSDiff   HDiff    BSDiff  HDiff
------------------------------------------------
apache-maven...    2.1    0.5        42      29
httpd bin...      13.3    3.9       151     111
httpd src...      32.3    6.6       330     221
Firefox...         137     63       849     579
emacs...           203     47      1466    1026
eclipse            253    108      1575    1051
gcc-src...         678    111      4639    3119
------------------------------------------------
                 394.62%          145.47%
================================================
```
  
---
by housisong@gmail.com  

