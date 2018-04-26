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

LZMA_OBJ := \
    LzFind.o \
    LzmaDec.o \
    LzmaEnc.o

CFLAGS     += -O3 -DNDEBUG
CXXFLAGS   += -O3 -DNDEBUG

APP_FLAGS := \
    -D_IS_NEED_ALL_CompressPlugin=0 \
    -D_CompressPlugin_zlib -lz \
    -D_CompressPlugin_bz2 -lbz2 \
    -D_CompressPlugin_lzma \
    #-D_CompressPlugin_lz4 -llz4 \
    #-D_CompressPlugin_zstd -lzstd

.PHONY: all clean

all: lzmaLib libhdiffpatch.a hdiffz hpatchz 

lzmaLib: # https://www.7-zip.org/sdk.html  https://github.com/sisong/lzma
	cc -c  -D_7ZIP_ST  '../lzma/C/LzFind.c'  '../lzma/C/LzmaDec.c'  '../lzma/C/LzmaEnc.c'

libhdiffpatch.a: $(HDIFF_OBJ)
	$(AR) rcs $@ $^

hdiffz: 
	c++ hdiffz.cpp libhdiffpatch.a $(LZMA_OBJ) $(APP_FLAGS) -o hdiffz
hpatchz: 
	cc  hpatchz.c $(HPATCH_OBJ) $(LZMA_OBJ) $(APP_FLAGS) -o hpatchz

clean:
	-rm -f hdiffz hpatchz libhdiffpatch.a $(HDIFF_OBJ) $(LZMA_OBJ)
