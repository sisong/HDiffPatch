HPATCH_OBJ := \
    libHDiffPatch/HPatch/patch.o \
    dirDiffPatch/dir_patch/dir_patch.o \
    dirDiffPatch/dir_patch/res_handle_limit.o \
    dirDiffPatch/dir_patch/ref_stream.o \
    dirDiffPatch/dir_patch/new_stream.o \
    file_for_patch.o

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
    dirDiffPatch/dir_diff/dir_diff.o \
    $(HPATCH_OBJ)

CFLAGS     += -O3 -DNDEBUG
CXXFLAGS   += -O3 -DNDEBUG

APP_FLAGS := \
    -D_IS_NEED_DEFAULT_CompressPlugin=0 \
    -D_IS_NEED_ALL_CompressPlugin=0 \
    -D_CompressPlugin_zlib -lz \
    -D_CompressPlugin_bz2 -lbz2 \
    -D_CompressPlugin_lzma  -I'../lzma/C'

.PHONY: all install clean

all: lzmaLib libhdiffpatch.a hdiffz hpatchz

LZMA_DEC_OBJ := 'LzmaDec.o'
LZMA_OBJ := 'LzFind.o' 'LzmaEnc.o' $(LZMA_DEC_OBJ)
lzmaLib: # https://www.7-zip.org/sdk.html  https://github.com/sisong/lzma
	$(CC) -c $(CFLAGS) -D_7ZIP_ST '../lzma/C/LzFind.c' '../lzma/C/LzmaDec.c' '../lzma/C/LzmaEnc.c'

libhdiffpatch.a: $(HDIFF_OBJ)
	$(AR) rcs $@ $^

hdiffz: 
	$(CXX) hdiffz.cpp libhdiffpatch.a $(LZMA_OBJ) $(APP_FLAGS) -o hdiffz
hpatchz: 
	$(CC) hpatchz.c $(HPATCH_OBJ) $(LZMA_DEC_OBJ) $(APP_FLAGS) -o hpatchz

RM = rm -f
INSTALL_X = install -m 0755
INSTALL_BIN = $(DESTDIR)/usr/local/bin

clean:
	$(RM) libhdiffpatch.a hdiffz hpatchz $(HDIFF_OBJ) $(LZMA_OBJ)

install: all
	$(INSTALL_X) hdiffz $(INSTALL_BIN)/hdiffz
	$(INSTALL_X) hpatchz $(INSTALL_BIN)/hpatchz

uninstall:
	$(RM)  $(INSTALL_BIN)/hdiffz  $(INSTALL_BIN)/hpatchz
