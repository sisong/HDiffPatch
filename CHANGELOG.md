# HDiffPatch Change Log

full changelog at: https://github.com/sisong/HDiffPatch/commits   

## [v4.8.0](https://github.com/sisong/HDiffPatch/tree/v4.8.0) - 2024-07-26
### Added
* cmdline hdiffz support option "-c-ldef-{1..12}"; used libdeflate compressor, compatible with -c-zlib, faster or better than zlib;
 (hpatchz now default closed libdeflate decompressor)
* add plugin ldefCompressPlugin, pldefCompressPlugin, ldefDecompressPlugin;
### Changed
* released Android libhpatchz.so support Android 15 with 16KB page size;

## [v4.7.0](https://github.com/sisong/HDiffPatch/tree/v4.7.0) - 2024-07-12
### Added
* cmdline hdiffz support option "-BSD -SD", to create diffFile compatible with another BSDIFF format "ENDSLEY/BSDIFF43", https://github.com/mendsley/bsdiff ; patch support this format from v4.6.7
* cmdline hdiffz support option "-neq"; if opened, hdiffz will refuse to created diffFile when oldData==newData.
### Fixed
* fixed SFX auto extract logic (SFX executable file is hpatchz file + diffFile)   
SFX extract with default option `$selfExtractArchive -f ".\" -X ".\"` when diffFile created by directories;   
if diffFile created by empty oldPath, then extract with default option `$selfExtractArchive -f "" -X ".\"`.   

## [v4.6.9](https://github.com/sisong/HDiffPatch/tree/v4.6.9) - 2023-12-01
### Added
* cmdline hdiffz & hpatchz support option "-info diffFile", print infos of diffFile;

## [v4.6.8](https://github.com/sisong/HDiffPatch/tree/v4.6.8) - 2023-11-02
### Changed
* hdiffz.exe&hpatchz.exe support long path on Windows OS;

## [v4.6.7](https://github.com/sisong/HDiffPatch/tree/v4.6.7) - 2023-08-31
### Added
* patch compatible with another BSDIFF format "ENDSLEY/BSDIFF43", https://github.com/mendsley/bsdiff

## [v4.6.6](https://github.com/sisong/HDiffPatch/tree/v4.6.6) - 2023-08-27
### Added
* patch SDK support iOS & MacOS, out lib file libhpatchz.a;

## [v4.6.3](https://github.com/sisong/HDiffPatch/tree/v4.6.3) - 2023-05-19
### Added
* support block devices on linux OS; by contributor [Alexander Zakharov](https://github.com/uglym8);

## [v4.6.0](https://github.com/sisong/HDiffPatch/tree/v4.6.0) - 2023-04-20
### Added
* add libhsync for diff&patch by sync; see demo [hsynz](https://github.com/sisong/hsynz) (like [zsync](http://zsync.moria.org.uk))
* add function create_sync_data(),create_dir_sync_data(),sync_patch(),sync_patch_...(),sync_local_diff(),sync_local_diff_...(),sync_local_patch(),sync_local_patch_...()

## [v4.5.2](https://github.com/sisong/HDiffPatch/tree/v4.5.2) - 2022-12-25
### Fixed
* fix a bug when run dir_diff by multi-thread parallel;

## [v4.5.0](https://github.com/sisong/HDiffPatch/tree/v4.5.0) - 2022-11-23
### Added
* cmdline added option "-VCD[-compressLevel[-dictSize]]", create diffFile compatible with VCDIFF format;
* cmdline hpatchz support apply VCDIFF format diffFile, created by `$hdiffz -VCD`,`$xdelta3`,`$open-vcdiff`;
* add function create_vcdiff(),create_vcdiff_stream(), vcpatch_with_cache();
* cmdline option "-BSD" support run with -s (before only supported -m), to fast create diffFile compatible with bsdiff4;
* add function create_bsdiff_stream(), same as create_bsdiff(), but can control memory requires and run speed by different kMatchBlockSize value;

## [v4.4.0](https://github.com/sisong/HDiffPatch/tree/v4.4.0) - 2022-10-09
### Changed
* optimize diff -m & -s speed by multi-thread parallel, requires C++11.

## [v4.3.0](https://github.com/sisong/HDiffPatch/tree/v4.3.0) - 2022-09-23
### Changed
* recode some patch error code: decompressor errors, file error, disk space full error, jni error

## [v4.2.0](https://github.com/sisong/HDiffPatch/tree/v4.2.0) - 2022-05-15
### Added
* add function create_lite_diff() & hpatch_lite_open(),hpatch_lite_patch(); optimized hpatch on MCU,NB-IoT... (demo [HPatchLite](https://github.com/sisong/HPatchLite))
* add compress&decompress plugin tuzCompressPlugin,tuzDecompressPlugin;

## [v4.1.2](https://github.com/sisong/HDiffPatch/tree/v4.1.2) - 2021-12-02
### Added
* add Github Actions CI.
### Removed
* remove travis-ci.org CI.

## [v4.1.0](https://github.com/sisong/HDiffPatch/tree/v4.1.0) - 2021-11-27
### Added
* cmdline add option "-BSD", to create diffFile compatible with bsdiff4;
* cmdline hpatchz support apply bsdiff4 format diffFile, created by `$hdiffz -BSD`,`$bsdiff`;
* cmdline add option "-cache", optimize `$hdiffz -m` speed; note:the big cache max used O(oldFileSize) memory, and build slow.
* cmdline add option "-block", optimize `$hdiffz -m` speed;
* add function create_bsdiff() & bspatch_with_cache().
* add function create_single_compressed_diff_block() & create_compressed_diff_block() & create_bsdiff_block().

## [v4.0.0](https://github.com/sisong/HDiffPatch/tree/v4.0.0) - 2021-06-14
### Added
* cmdline add option "-SD", to create single compressed diffData, for optimize decompress buffer when patch, and support step by step patching when step by step downloading; it's better for IoT!  NOTE: old patcher can't work with this new format diffData. 
* the added create_single_compressed_diff()&patch_single_stream() can be used;
* add create_single_compressed_diff_stream(), same as create_single_compressed_diff(), but can control memory requires and run speed by different kMatchBlockSize value;
* now, zstd plugin default added in cmdline;
### Changed
* check_compressed_diff_stream() rename to check_compressed_diff();
* patch_single_stream_by() (preview func) rename to patch_single_stream(); 
* patch_single_stream_by_mem() (preview func) rename to patch_single_stream_mem(); 

## [v3.1.1](https://github.com/sisong/HDiffPatch/tree/v3.1.1) - 2021-04-02
### Added
* add compress&decompress plugin brotliCompressPlugin,brotliDecompressPlugin,lzhamCompressPlugin,lzhamDecompressPlugin;
### Removed
* remove sais-lite("sais.hxx") from suffix string sort (still can select divsufsort or std::sort)

## [v3.1.0](https://github.com/sisong/HDiffPatch/tree/v3.1.0) - 2020-12-16
### Added
* add a memory cache for patch to newStream, can reduce write I/O times;
* add create_single_compressed_diff()&patch_single_compressed_diff(), for v4.0, preview;
### Removed
* cmdline remove option "-o", no Original diff, you can continue to call patch()|patch_stream() by yourself;  
* remove patch_decompress_repeat_out(), you need use patch_decompress*() to replace it;

## [v3.0.8](https://github.com/sisong/HDiffPatch/tree/v3.0.8) - 2020-01-01
### Added
* patch demo for Android, out lib file libhpatchz.so;

## [v3.0.7](https://github.com/sisong/HDiffPatch/tree/v3.0.7) - 2019-12-19
### Fixed
* fix a bug when create dir's manifest file without checksum (`$hdiffz -C-no -M...`);
* fix a bug when create dir's manifest file on Windows OS;

## [v3.0.6](https://github.com/sisong/HDiffPatch/tree/v3.0.6) - 2019-09-18
### Fixed
* fix a patch bug when old file size >=4GB and patch with large cache memory and run as 32bit app;

## [v3.0.5](https://github.com/sisong/HDiffPatch/tree/v3.0.5) - 2019-09-18
### Fixed
* fix a directory patch bug when changed file's sumSize >=4GB and run as 32bit app;

## [v3.0.4](https://github.com/sisong/HDiffPatch/tree/v3.0.4) - 2019-09-06
### Fixed
* fix dir_patch can't remove some files bug when patch to same dir on Windows OS;

## [v3.0.0](https://github.com/sisong/HDiffPatch/tree/v3.0.0) - 2019-03-01
### Added
* hdiffz,hpatchz command line support diff&patch between directories(folder);
* support checksum plugin: crc32ChecksumPlugin,adler32ChecksumPlugin,adler64ChecksumPlugin,fadler32ChecksumPlugin,fadler64ChecksumPlugin,fadler128ChecksumPlugin,md5ChecksumPlugin ;
* lzmaCompressPlugin support parallel compress;
* add parallel compress plugin: pzlibCompressPlugin,pbz2CompressPlugin,lzma2CompressPlugin
* command line support SFX(self extract archive);

## [v2.5.3](https://github.com/sisong/HDiffPatch/tree/v2.5.3) - 2018-12-25
### Fixed
* fix a bug when cancel LzmaEnc_Encode in lzmaCompressPlugin when diff;

## [v2.5.0](https://github.com/sisong/HDiffPatch/tree/v2.5.0) - 2018-12-01
### Added
* resave_compressed_diff(...) support resave diffFile with new compress type;

## [v2.4.2](https://github.com/sisong/HDiffPatch/tree/v2.4.2) - 2018-11-06
### Added
* add CI(ci.appveyor.com) for this repository;

## [v2.4.1](https://github.com/sisong/HDiffPatch/tree/v2.4.1) - 2018-08-14
### Fixed
* fix a memory bug in zlibCompressPlugin when diff;
### Added
* add CI(travis-ci.org) for this repository;

## [v2.4](https://github.com/sisong/HDiffPatch/tree/v2.4) - 2018-04-26
### Changed
* improve hdiffz,hpatchz command line;

## [v2.3](https://github.com/sisong/HDiffPatch/tree/v2.3) - 2018-03-21
### Added
* hpatch_coverList_open_serializedDiff(...),hpatch_coverList_open_compressedDiff(...) support open diffFile read coverList;

## [v2.2.2](https://github.com/sisong/HDiffPatch/tree/v2.2.2) - 2017-09-10
### Changed
* optimize patch_stream_with_cache(...) speed when patch with large cache memory;

## [v2.2.1](https://github.com/sisong/HDiffPatch/tree/v2.2.1) - 2017-09-06
### Added
* add compress&decompress plugin lz4hcCompressPlugin,zstdCompressPlugin,zstdDecompressPlugin;

## [v2.2.0](https://github.com/sisong/HDiffPatch/tree/v2.2.0) - 2017-09-02
### Changed
* optimize patch_decompress_with_cache(...) speed when patch with large cache memory;

## [v2.1.1](https://github.com/sisong/HDiffPatch/tree/v2.1.1) - 2017-08-23
### Added
* add compress&decompress plugin lz4CompressPlugin,lz4DecompressPlugin;

## [v2.1.0](https://github.com/sisong/HDiffPatch/tree/v2.1.0) - 2017-08-22
### Added
* create_compressed_diff_stream(...), same as create_compressed_diff(...), but can control memory requires and run speed by different kMatchBlockSize value;
* add performance test, compare with xdelta3.1;

## [v2.0.3](https://github.com/sisong/HDiffPatch/tree/v2.0.3) - 2017-08-05
### Added
* patch_decompress_with_cache(...), same as patch_decompress(...), but support set cache memory size for optimize patch speed;
* patch_stream_with_cache(...), same as patch_stream(...), but support set cache memory size for optimize patch speed;


## [v2.0.0](https://github.com/sisong/HDiffPatch/tree/v2.0.0) - 2017-07-29
### Added
* create_compressed_diff(...) support create compressed differential;
* add compress plugin zlibCompressPlugin, bz2CompressPlugin, lzmaCompressPlugin;
* patch_decompress(...) support patch apply compressed differential;
* add decompress plugin zlibDecompressPlugin, bz2DecompressPlugin, lzmaDecompressPlugin;

## [v1.2.0](https://github.com/sisong/HDiffPatch/tree/v1.2.0) - 2017-07-02
### Changed
* optimize diff speed;

## [v1.1.4](https://github.com/sisong/HDiffPatch/tree/v1.1.4) - 2017-06-10
### Added
* add MakeFile for support make; by contributor [JayXon](https://github.com/JayXon);
### Changed
* slightly optimize diff required memory size;

## [v1.1.3](https://github.com/sisong/HDiffPatch/tree/v1.1.3) - 2017-01-01
### Changed
* used divsufsort (still can select sais or std::sort) for suffix string sort;

## [v1.1.2](https://github.com/sisong/HDiffPatch/tree/v1.1.2) - 2016-09-01
### Fixed
* fix a bug when write out diffFile on Windows OS; by contributor [Wenhai Lin](https://github.com/WenhaiLin);

## [v1.1.0](https://github.com/sisong/HDiffPatch/tree/v1.1.0) - 2014-09-13
### Added
* patch_stream(...), same as patch(...) but used O(1) bytes of memory;

## [v1.0.6](https://github.com/sisong/HDiffPatch/tree/v1.0.6) - 2014-09-07
### Changed
* Diff&Patch support big datas(>2GB, int32 to int64) on 64bit operating system;

## [v1.0.2](https://github.com/sisong/HDiffPatch/tree/v1.0.2) - 2013-08-03
### Changed
* optimize diff speed with two unrelated data;

## [v1.0.1](https://github.com/sisong/HDiffPatch/tree/v1.0.1) - 2013-08-02
### Fixed
* fix diff fail between two equal datas;

## [v1.0.0](https://github.com/sisong/HDiffPatch/tree/v1.0.0) - 2013-06-06
### Added
* add Readme file; 
* performance test, compare with BSDiff4.3;

## [Init Release](https://github.com/sisong/HDiffPatch/commit/6d71005c65e1713a8f0c02f9fd2eced32940b4c2) - 2013-05-30
### Init Release, MIT copyright;
* create_diff(...) support create differential between two memory datas(src->dst bytes);
* patch(...) support apply differential for update src bytes to dst bytes;
* demo app for diff&patch between two files;
