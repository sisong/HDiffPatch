HDiffPatch
================
Version 1.0

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
    HDiff runs in O(newSize+oldSize) time,and requires newSize*5+oldSize+O(1) bytes of memory.  
    *--HDiff算法时间复杂度O(newSize+oldSize),内存使用newSize\*5+oldSize+O(1)字节。*  
    HPatch runs in O(newSize+oldSize) time,and requires newSize+oldSize+O(1) bytes of memory.  
    *--HPatch算法时间复杂度O(newSize+oldSize),内存使用newSize+oldSize+O(1)字节。*  
    newSize or oldSize < 2G Byte.  
    *--新数据和老数据的大小都不要超过2G字节。*  
  
---
by housisong@gmail.com  

