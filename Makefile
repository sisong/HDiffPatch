# args
DIR_DIFF := 1
MT       := 1
LZMA     := 1
ZSTD     := 1
MD5      := 1
BSD      := 1  # support bsdiff&bspatch?
CL  	 := 0  # clang?

HDIFF_OBJ  := 
HPATCH_OBJ := \
    libHDiffPatch/HPatch/patch.o \
    file_for_patch.o

ifeq ($(DIR_DIFF),0)
else
  HPATCH_OBJ += \
    dirDiffPatch/dir_patch/dir_patch.o \
    dirDiffPatch/dir_patch/res_handle_limit.o \
    dirDiffPatch/dir_patch/ref_stream.o \
    dirDiffPatch/dir_patch/new_stream.o \
    dirDiffPatch/dir_patch/dir_patch_tools.o \
    dirDiffPatch/dir_patch/new_dir_output.o \
    libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.o
endif
ifeq ($(BSD),0)
else
	HPATCH_OBJ += bsdiff_wrapper/bspatch_wrapper.o
endif
MD5_PATH := ../libmd5
ifeq ($(DIR_DIFF),0)
else
  ifeq ($(MD5),0)
  else # https://sourceforge.net/projects/libmd5-rfc  https://github.com/sisong/libmd5
	HPATCH_OBJ += $(MD5_PATH)/md5.o
  endif
endif
LZMA_PATH := ../lzma/C
ifeq ($(LZMA),0)
else # https://www.7-zip.org  https://github.com/sisong/lzma
  HPATCH_OBJ += $(LZMA_PATH)/LzmaDec.o \
  				$(LZMA_PATH)/Lzma2Dec.o 
  HDIFF_OBJ  += $(LZMA_PATH)/LzFind.o \
  				$(LZMA_PATH)/LzFindOpt.o \
  				$(LZMA_PATH)/CpuArch.o \
  				$(LZMA_PATH)/LzmaEnc.o \
				$(LZMA_PATH)/Lzma2Enc.o  
  ifeq ($(MT),0)  
  else  
    HDIFF_OBJ+= $(LZMA_PATH)/LzFindMt.o \
  				$(LZMA_PATH)/MtCoder.o \
  				$(LZMA_PATH)/MtDec.o \
				$(LZMA_PATH)/Threads.o
  endif
endif
ZSTD_PATH := ../zstd/lib
ifeq ($(ZSTD),0)
else # # https://github.com/facebook/zstd
  HPATCH_OBJ += $(ZSTD_PATH)/common/debug.o \
  				$(ZSTD_PATH)/common/entropy_common.o \
  				$(ZSTD_PATH)/common/error_private.o \
  				$(ZSTD_PATH)/common/fse_decompress.o \
  				$(ZSTD_PATH)/common/xxhash.o \
  				$(ZSTD_PATH)/common/zstd_common.o \
  				$(ZSTD_PATH)/decompress/huf_decompress.o \
  				$(ZSTD_PATH)/decompress/zstd_ddict.o \
  				$(ZSTD_PATH)/decompress/zstd_decompress.o \
  				$(ZSTD_PATH)/decompress/zstd_decompress_block.o
  HDIFF_OBJ  += $(ZSTD_PATH)/compress/fse_compress.o \
  				$(ZSTD_PATH)/compress/hist.o \
  				$(ZSTD_PATH)/compress/huf_compress.o \
  				$(ZSTD_PATH)/compress/zstd_compress.o \
  				$(ZSTD_PATH)/compress/zstd_compress_literals.o \
  				$(ZSTD_PATH)/compress/zstd_compress_sequences.o \
  				$(ZSTD_PATH)/compress/zstd_compress_superblock.o \
  				$(ZSTD_PATH)/compress/zstd_double_fast.o \
  				$(ZSTD_PATH)/compress/zstd_fast.o \
  				$(ZSTD_PATH)/compress/zstd_lazy.o \
  				$(ZSTD_PATH)/compress/zstd_ldm.o \
  				$(ZSTD_PATH)/compress/zstd_opt.o
  ifeq ($(MT),0)
  else  
    HDIFF_OBJ+= $(ZSTD_PATH)/common/pool.o \
				$(ZSTD_PATH)/common/threading.o \
				$(ZSTD_PATH)/compress/zstdmt_compress.o
  endif
endif

HDIFF_OBJ += \
    hdiffz_import_patch.o \
    libHDiffPatch/HDiff/diff.o \
    libHDiffPatch/HDiff/match_block.o \
    libHDiffPatch/HDiff/private_diff/bytes_rle.o \
    libHDiffPatch/HDiff/private_diff/suffix_string.o \
    libHDiffPatch/HDiff/private_diff/compress_detect.o \
    libHDiffPatch/HDiff/private_diff/limit_mem_diff/digest_matcher.o \
    libHDiffPatch/HDiff/private_diff/limit_mem_diff/stream_serialize.o \
    libHDiffPatch/HDiff/private_diff/libdivsufsort/divsufsort64.o \
    libHDiffPatch/HDiff/private_diff/libdivsufsort/divsufsort.o \
    libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.o \
    $(HPATCH_OBJ)

ifeq ($(DIR_DIFF),0)
else
  HDIFF_OBJ += \
    dirDiffPatch/dir_diff/dir_diff.o \
    dirDiffPatch/dir_diff/dir_diff_tools.o \
    dirDiffPatch/dir_diff/dir_manifest.o
endif
ifeq ($(BSD),0)
else
	HDIFF_OBJ += bsdiff_wrapper/bsdiff_wrapper.o
endif
ifeq ($(MT),0)
else
  HDIFF_OBJ += \
    libParallel/parallel_import.o \
    libParallel/parallel_channel.o \
    compress_parallel.o
endif


DEF_FLAGS := \
    -O3 -DNDEBUG -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 \
    -D_IS_NEED_ALL_CompressPlugin=0 \
    -D_IS_NEED_DEFAULT_CompressPlugin=0 \
    -D_CompressPlugin_zlib  \
    -D_CompressPlugin_bz2  \
    -D_IS_NEED_ALL_ChecksumPlugin=0 \
    -D_IS_NEED_DEFAULT_ChecksumPlugin=0 
ifeq ($(DIR_DIFF),0)
  DEF_FLAGS += -D_IS_NEED_DIR_DIFF_PATCH=0
else
  DEF_FLAGS += \
    -D_IS_NEED_DIR_DIFF_PATCH=1 \
    -D_ChecksumPlugin_crc32 \
    -D_ChecksumPlugin_fadler64
  ifeq ($(MD5),0)
  else
    DEF_FLAGS += -D_ChecksumPlugin_md5 -I$(MD5_PATH)
  endif
endif
ifeq ($(BSD),0)
	DEF_FLAGS += -D_IS_NEED_BSDIFF=0
else
	DEF_FLAGS += -D_IS_NEED_BSDIFF=1
endif
ifeq ($(LZMA),0)
else
  DEF_FLAGS += -D_CompressPlugin_lzma -D_CompressPlugin_lzma2 -I$(LZMA_PATH)
endif
ifeq ($(ZSTD),0)
else
  DEF_FLAGS += \
    -D_CompressPlugin_zstd  -I$(ZSTD_PATH) -I$(ZSTD_PATH)/common \
	-I$(ZSTD_PATH)/compress -I$(ZSTD_PATH)/decompress
endif

ifeq ($(MT),0)
  DEF_FLAGS += \
    -D_7ZIP_ST \
    -D_IS_USED_MULTITHREAD=0
else
  DEF_FLAGS += \
    -DZSTD_MULTITHREAD=1 \
    -D_IS_USED_MULTITHREAD=1 \
    -D_IS_USED_PTHREAD=1
endif

PATCH_LINK := -lz -lbz2
DIFF_LINK  := $(PATCH_LINK)
ifeq ($(MT),0)
else
  DIFF_LINK += -lpthread
endif
ifeq ($(CL),1)
  CXX := clang++
  CC  := clang
  DIFF_LINK += -lstdc++
endif

CFLAGS   += $(DEF_FLAGS) 
CXXFLAGS += $(DEF_FLAGS)

.PHONY: all install clean

all: libhdiffpatch.a hpatchz hdiffz mostlyclean

libhdiffpatch.a: $(HDIFF_OBJ)
	$(AR) rcs $@ $^

hpatchz: $(HPATCH_OBJ)
	$(CC) hpatchz.c $(HPATCH_OBJ) $(CFLAGS) $(PATCH_LINK) -o hpatchz
hdiffz: libhdiffpatch.a
	$(CXX) hdiffz.cpp libhdiffpatch.a $(CXXFLAGS) $(DIFF_LINK) -o hdiffz

RM := rm -f
INSTALL_X := install -m 0755
INSTALL_BIN := $(DESTDIR)/usr/local/bin

mostlyclean: hpatchz hdiffz
	$(RM) $(HDIFF_OBJ)
clean:
	$(RM) libhdiffpatch.a hpatchz hdiffz $(HDIFF_OBJ)

install: all
	$(INSTALL_X) hdiffz $(INSTALL_BIN)/hdiffz
	$(INSTALL_X) hpatchz $(INSTALL_BIN)/hpatchz

uninstall:
	$(RM)  $(INSTALL_BIN)/hdiffz  $(INSTALL_BIN)/hpatchz
