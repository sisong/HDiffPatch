HDiffPatch
================

API uses like this:

1: create_diff(newData,oldData,out diffData);

release the diffData for update oldData.
(using LZMA or ZIP compress the diffData before release is a better option. )

发布diffData就可以用来升级oldData。
(发布前建议选择用LZMA或ZIP压缩diffData。)

2: bool patch(out newData,oldData,diffData);


by housisong@gmail.com
