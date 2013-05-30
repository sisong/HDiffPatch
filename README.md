HDiffPatch
================

API uses like this:

1. create_diff(newData,oldData,out diffData);

release the diffData for update oldData.
(using LZMA or ZIP compress the diffData before release is a better option. )

2. bool patch(out newData,oldData,diffData);


by housisong@gmail.com
