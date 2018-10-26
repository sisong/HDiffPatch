HPATCH_OBJ := \
    libHDiffPatch/HPatch/patch.o

HDIFF_OBJ := \
    libHDiffPatch/HDiff/diff.o \
    libHDiffPatch/HDiff/private_diff/bytes_rle.o \
    libHDiffPatch/HDiff/private_diff/suffix_string.o \
    libHDiffPatch/HDiff/private_diff/compress_detect.o \
    libHDiffPatch/HDiff/private_diff/limit_mem_diff/digest_matcher.o \
    libHDiffPatch/HDiff/private_diff/limit_mem_diff/stream_serialize.o \
    libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.o \
    libHDiffPatch/HDiff/private_diff/libdivsufsort/divsufsort64.o \
    libHDiffPatch/HDiff/private_diff/libdivsufsort/divsufsort.o \
    $(HPATCH_OBJ)

CFLAGS     += -O3 -DNDEBUG
CXXFLAGS   += -O3 -DNDEBUG

APP_FLAGS := \
    -D_IS_NEED_ALL_CompressPlugin=0 \
    -D_CompressPlugin_zlib -lz \
    -D_CompressPlugin_bz2 -lbz2 \
    -D_CompressPlugin_lzma  -I'../lzma/C' \
    -D_CompressPlugin_lz4   -I'../lz4/lib' \
    -D_CompressPlugin_zstd  -I'../zstd/lib'

.PHONY: all clean

all: lzmaLib lz4Lib zstdLib libhdiffpatch.a hdiffz hpatchz

LZ4_OBJ := lz4.o lz4hc.o
lz4Lib: # https://github.com/lz4/lz4  https://github.com/sisong/lz4
	cc -c -D_7ZIP_ST '../lz4/lib/lz4.c' '../lz4/lib/lz4hc.c' 

LZMA_OBJ := 'LzFind.o' 'LzmaDec.o' 'LzmaEnc.o'
lzmaLib: # https://www.7-zip.org/sdk.html  https://github.com/sisong/lzma
	cc -c -D_7ZIP_ST '../lzma/C/LzFind.c' '../lzma/C/LzmaDec.c' '../lzma/C/LzmaEnc.c'

ZSTD_OBJ := fse_decompress.o zstd_common.o threading.o entropy_common.o error_private.o pool.o xxhash.o \
	zstdmt_compress.o fse_compress.o zstd_compress.o huf_compress.o huf_decompress.o zstd_decompress.o
zstdLib: # https://github.com/facebook/zstd  https://github.com/sisong/zstd
	cc -c -I'../zstd/lib/common' -I'../zstd/lib' \
             '../zstd/lib/common/fse_decompress.c' \
             '../zstd/lib/common/zstd_common.c' \
             '../zstd/lib/common/threading.c' \
             '../zstd/lib/common/entropy_common.c' \
             '../zstd/lib/common/error_private.c' \
             '../zstd/lib/common/pool.c' \
             '../zstd/lib/common/xxhash.c' \
             '../zstd/lib/compress/zstdmt_compress.c' \
             '../zstd/lib/compress/fse_compress.c' \
             '../zstd/lib/compress/zstd_compress.c' \
             '../zstd/lib/compress/huf_compress.c' \
             '../zstd/lib/decompress/huf_decompress.c' \
             '../zstd/lib/decompress/zstd_decompress.c'

libhdiffpatch.a: $(HDIFF_OBJ)
	$(AR) rcs $@ $^

hdiffz: 
	c++ hdiffz.cpp libhdiffpatch.a $(LZMA_OBJ) $(LZ4_OBJ) $(ZSTD_OBJ) $(APP_FLAGS) -o hdiffz
hpatchz: 
	cc  hpatchz.c $(HPATCH_OBJ) $(LZMA_OBJ) $(LZ4_OBJ) $(ZSTD_OBJ) $(APP_FLAGS) -o hpatchz

clean:
	-rm -f libhdiffpatch.a hdiffz hpatchz $(HDIFF_OBJ) $(LZMA_OBJ) $(LZ4_OBJ) $(ZSTD_OBJ)
