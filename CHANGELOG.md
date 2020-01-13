# HDiffPatch Change Log

full changelog at: https://github.com/sisong/HDiffPatch/commits   

## [v3.0.8](https://github.com/sisong/HDiffPatch/tree/v3.0.8) - 2020-01-01
### Added
* patch demo for Android, out lib file libhpatchz.so; 

## [v3.0.7](https://github.com/sisong/HDiffPatch/tree/v3.0.7) - 2019-12-19
### Fixed
* Fix a bug when create dir's manifest file without checksum (hdiffz -C-no -M...);
* Fix a bug when create dir's manifest file on windows operating system;

## [v3.0.6](https://github.com/sisong/HDiffPatch/tree/v3.0.6) - 2019-09-18
### Fixed
* Fix a patch bug when old file size >=4GB and patch with large cache memory and run as 32bit app;

## [v3.0.5](https://github.com/sisong/HDiffPatch/tree/v3.0.5) - 2019-09-18
### Fixed
* Fix a directory patch bug when changed file's sumSize >=4GB and run as 32bit app;

## [v3.0.0](https://github.com/sisong/HDiffPatch/tree/v3.0.0) - 2019-03-01
### Added
* hdiffz,hpatchz command line support diff&patch between directories(folder);
* support checksum plugin: crc32ChecksumPlugin,adler32ChecksumPlugin,adler64ChecksumPlugin,fadler32ChecksumPlugin,fadler64ChecksumPlugin,fadler128ChecksumPlugin,md5ChecksumPlugin ;
* lzmaCompressPlugin support parallel compress;
* add parallel compress plugin: pzlibCompressPlugin,pbz2CompressPlugin,lzma2CompressPlugin
* command line support SFX(self extract archive);

## [v2.5.3](https://github.com/sisong/HDiffPatch/tree/v2.5.3) - 2018-12-25
### Fixed
* Fix a bug when cancel LzmaEnc_Encode in lzmaCompressPlugin when diff;

## [v2.5.0](https://github.com/sisong/HDiffPatch/tree/v2.5.0) - 2018-12-01
### Added
* resave_compressed_diff(...) support resave diffFile with new compress type;

## [v2.4.2](https://github.com/sisong/HDiffPatch/tree/v2.4.2) - 2018-11-06
### Added
* Add CI(ci.appveyor.com) for this repository;

## [v2.4.1](https://github.com/sisong/HDiffPatch/tree/v2.4.1) - 2018-08-14
### Fixed
* Fix a memroy bug in zlibCompressPlugin when diff;
### Added
* Add CI(travis-ci.org) for this repository;

## [v2.4](https://github.com/sisong/HDiffPatch/tree/v2.4) - 2018-04-26
### Changed
* Improve hdiffz,hpatchz command line;

## [v2.3](https://github.com/sisong/HDiffPatch/tree/v2.3) - 2018-03-21
### Added
* hpatch_coverList_open_serializedDiff(...),hpatch_coverList_open_compressedDiff(...) support open diffFile read coverList;

## [v2.2.2](https://github.com/sisong/HDiffPatch/tree/v2.2.2) - 2017-09-10
### Changed
* Optimize patch_stream_with_cache(...) speed when patch with large cache memory;

## [v2.2.1](https://github.com/sisong/HDiffPatch/tree/v2.2.1) - 2017-09-06
### Added
* add compress&decompress plugin lz4hcCompressPlugin,zstdCompressPlugin,zstdDecompressPlugin;

## [v2.2.0](https://github.com/sisong/HDiffPatch/tree/v2.2.0) - 2017-09-02
### Added
* Optimize patch_decompress_with_cache(...) speed when patch with large cache memory;

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

## [v1.2.2](https://github.com/sisong/HDiffPatch/tree/v1.2.2) - 2017-07-17
### Fixed
* Fix a memroy bug when diff (never encountered, maybe happen);

## [v1.2.0](https://github.com/sisong/HDiffPatch/tree/v1.2.0) - 2017-07-02
### Changed
* Optimize diff speed;

## [v1.1.4](https://github.com/sisong/HDiffPatch/tree/v1.1.4) - 2017-06-10
### Added
* Add MakeFile for support make; by author [JayXon](https://github.com/JayXon);
### Changed
* Slightly optimize diff required memory size;

## [v1.1.3](https://github.com/sisong/HDiffPatch/tree/v1.1.3) - 2017-01-01
### Changed
* used divsufsort (still can select sais or std::sort) for suffix string sort;

## [v1.1.2](https://github.com/sisong/HDiffPatch/tree/v1.1.2) - 2016-09-01
### Fixed
* Fix a bug when write out diffFile on Windows operating system; by author [Wenhai Lin](https://github.com/WenhaiLin);

## [v1.1.1](https://github.com/sisong/HDiffPatch/tree/v1.1.1) - 2015-04-20
### Fixed
* Fix a code bug when diff (never encountered, maybe happen);

## [v1.1.0](https://github.com/sisong/HDiffPatch/tree/v1.1.0) - 2014-09-13
### Added
* patch_stream(...), same as patch(...) but used O(1) bytes of memory;

## [v1.0.6](https://github.com/sisong/HDiffPatch/tree/v1.0.6) - 2014-09-07
### Added
* Diff&Patch support big datas(>2GB, int32 to int64) on 64bit operating system;

## [v1.0.2](https://github.com/sisong/HDiffPatch/tree/v1.0.2) - 2013-08-03
### Changed
* Optimize diff speed with two unrelated data;

## [v1.0.1](https://github.com/sisong/HDiffPatch/tree/v1.0.1) - 2013-08-02
### Fixed
* Fix diff fail between two equal datas;

## [v1.0.0](https://github.com/sisong/HDiffPatch/tree/v1.0.0) - 2013-06-06
### Added
* Add Readme file; 
* Performance test, compare with BSDiff4.3;

## [Init Release](https://github.com/sisong/HDiffPatch/commit/6d71005c65e1713a8f0c02f9fd2eced32940b4c2) - 2013-05-30
### Init Release, MIT copyright;
* create_diff(...) support create differential between two memory datas(src->dst bytes);
* patch(...) support apply differential for update src bytes to dst bytes;
* demo app for diff&patch between two files;
