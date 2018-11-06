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
    -D_CompressPlugin_lz4hc \
    -D_CompressPlugin_zstd  -I'../zstd/lib'

.PHONY: all install clean

all: lzmaLib lz4Lib zstdLib libhdiffpatch.a hdiffz hpatchz

LZ4_OBJ := lz4.o lz4hc.o
lz4Lib: # https://github.com/lz4/lz4  https://github.com/sisong/lz4
	$(CC) -c $(CFLAGS) '../lz4/lib/lz4.c' '../lz4/lib/lz4hc.c' 

LZMA_OBJ := 'LzFind.o' 'LzmaDec.o' 'LzmaEnc.o'
lzmaLib: # https://www.7-zip.org/sdk.html  https://github.com/sisong/lzma
	$(CC) -c $(CFLAGS) -D_7ZIP_ST '../lzma/C/LzFind.c' '../lzma/C/LzmaDec.c' '../lzma/C/LzmaEnc.c'

ZSTD_OBJ := debug.o entropy_common.o error_private.o fse_decompress.o pool.o threading.o \
	xxhash.o zstd_common.o fse_compress.o hist.o huf_compress.o zstd_compress.o \
	zstd_double_fast.o zstd_fast.o zstd_lazy.o zstd_ldm.o zstd_opt.o zstdmt_compress.o \
	huf_decompress.o zstd_ddict.o zstd_decompress_block.o zstd_decompress.o
zstdLib: # https://github.com/facebook/zstd  https://github.com/sisong/zstd
	$(CC) -c $(CFLAGS) -I'../zstd/lib/common' -I'../zstd/lib' \
		../zstd/lib/common/debug.c \
		../zstd/lib/common/entropy_common.c \
		../zstd/lib/common/error_private.c \
		../zstd/lib/common/fse_decompress.c \
		../zstd/lib/common/pool.c \
		../zstd/lib/common/threading.c \
		../zstd/lib/common/xxhash.c \
		../zstd/lib/common/zstd_common.c \
		../zstd/lib/compress/fse_compress.c \
		../zstd/lib/compress/hist.c \
		../zstd/lib/compress/huf_compress.c \
		../zstd/lib/compress/zstd_compress.c \
		../zstd/lib/compress/zstd_double_fast.c \
		../zstd/lib/compress/zstd_fast.c \
		../zstd/lib/compress/zstd_lazy.c \
		../zstd/lib/compress/zstd_ldm.c \
		../zstd/lib/compress/zstd_opt.c \
		../zstd/lib/compress/zstdmt_compress.c \
		../zstd/lib/decompress/huf_decompress.c \
		../zstd/lib/decompress/zstd_ddict.c \
		../zstd/lib/decompress/zstd_decompress_block.c \
		../zstd/lib/decompress/zstd_decompress.c

libhdiffpatch.a: $(HDIFF_OBJ)
	$(AR) rcs $@ $^

hdiffz: 
	$(CXX) hdiffz.cpp libhdiffpatch.a $(LZMA_OBJ) $(LZ4_OBJ) $(ZSTD_OBJ) $(APP_FLAGS) -o hdiffz
hpatchz: 
	$(CC) hpatchz.c $(HPATCH_OBJ) $(LZMA_OBJ) $(LZ4_OBJ) $(ZSTD_OBJ) $(APP_FLAGS) -o hpatchz

RM = rm -f
INSTALL_X = install -m 0755
INSTALL_BIN = $(DESTDIR)/usr/local/bin

clean:
	$(RM) libhdiffpatch.a hdiffz hpatchz $(HDIFF_OBJ) $(LZMA_OBJ) $(LZ4_OBJ) $(ZSTD_OBJ)

install: all
	$(INSTALL_X) hdiffz $(INSTALL_BIN)/hdiffz
	$(INSTALL_X) hpatchz $(INSTALL_BIN)/hpatchz

uninstall:
	$(RM)  $(INSTALL_BIN)/hdiffz  $(INSTALL_BIN)/hpatchz
