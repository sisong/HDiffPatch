HDiffPatch
================
Version 1.0  
byte data Diff & Patch  C\C++ library.  

---
uses like this:

*  **create_diff(newData,oldData,out diffData);**
  
  release the diffData for update oldData.  
  *--发布diffData就可以用来升级oldData到最新版。*  
  (using LZMA or ZIP compress the diffData before release is a better option. )  
  *--(发布前建议选择用LZMA或ZIP等压缩diffData。)*  
  
*  **bool patch(out newData,oldData,diffData);**
  
  ok  
  
---
*  
    HDiff runs in O(oldSize+newSize) time,and requires oldSize*5+newSize+O(1) bytes of memory.  
    *--HDiff算法时间复杂度O(oldSize+newSize),内存使用oldSize\*5+newSize+O(1)字节。*  
    HPatch runs in O(oldSize+newSize) time,and requires oldSize+newSize+O(1) bytes of memory.  
    *--HPatch算法时间复杂度O(oldSize+newSize),内存使用oldSize+newSize+O(1)字节。*  
    oldSize or newSize < 2G Byte.  
    *--oldSize或newSize的大小都不要超过2G字节。*  
  
---
by housisong@gmail.com  

