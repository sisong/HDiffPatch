# args
DIR_DIFF := 1
MT       := 1
# used libdeflate?
LDEF     := 1
# 0: not need zlib;  1: compile zlib source code;  2: used -lz to link zlib lib;
ifeq ($(LDEF),0)
  ZLIB     := 2
else
  ZLIB     := 1
endif
# 0: not need lzma;  1: compile lzma source code;  2: used -llzma to link lzma lib;
LZMA     := 1
# lzma decompressor used arm64 asm optimize? 
ARM64ASM := 0
# lzma only can used software CRC? (no hardware CRC)
USE_CRC_EMU := 0
# supported atomic uint64?
ATOMIC_U64 := 1
# 0: not need zstd;  1: compile zstd source code;  2: used -lzstd to link zstd lib;
ZSTD     := 1
MD5      := 1
STATIC_CPP := 0
# used clang?
CL  	 := 0
# build with -m32?
M32      := 0
# build for out min size
MINS     := 0
# need support VCDIFF? 
VCD      := 1
# need support bsdiff&bspatch?
BSD      := 1
ifeq ($(OS),Windows_NT) # mingw?
  CC    := gcc
  BZIP2 := 1
else
  # 0: not need bzip2 (must BSD=0);  1: compile bzip2 source code;  2: used -lbz2 to link bzip2 lib;
  BZIP2 := 2
endif
ifeq ($(BSD),0)
else
  ifeq ($(BZIP2),0)
  $(error error: support bsdiff need BZIP2! set BSD=0 or BZIP2>0 continue)
  endif
endif

ifeq ($(LDEF),0)
else
  ifeq ($(ZLIB),2)
  $(error error: now libdeflate decompressor not support -lz! need zlib source code, set ZLIB=1 continue)
  else
    ifeq ($(ZLIB),0)
    $(warning warning: libdeflate can't support all of the deflate code, when no zlib source code)
    endif
  endif
endif

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
ifeq ($(LZMA),1)
  # https://www.7-zip.org  https://github.com/sisong/lzma
  HPATCH_OBJ += $(LZMA_PATH)/LzmaDec.o \
  				$(LZMA_PATH)/Lzma2Dec.o \
  				$(LZMA_PATH)/CpuArch.o \
  				$(LZMA_PATH)/Alloc.o
  ifeq ($(ARM64ASM),0)
  else
  	HPATCH_OBJ += $(LZMA_PATH)/../Asm/arm64/LzmaDecOpt.o
  endif
  ifeq ($(MT),0)  
  else  
    HPATCH_OBJ+=$(LZMA_PATH)/MtDec.o \
				$(LZMA_PATH)/Threads.o
  endif
  HDIFF_OBJ  += $(LZMA_PATH)/LzFind.o \
  				$(LZMA_PATH)/LzFindOpt.o \
  				$(LZMA_PATH)/LzmaEnc.o \
				  $(LZMA_PATH)/Lzma2Enc.o  
  ifeq ($(MT),0)  
  else  
    HDIFF_OBJ+= $(LZMA_PATH)/LzFindMt.o \
  				$(LZMA_PATH)/MtCoder.o
  endif
endif
ifeq ($(VCD),0)
else
  HPATCH_OBJ += vcdiff_wrapper/vcpatch_wrapper.o
  HDIFF_OBJ  += vcdiff_wrapper/vcdiff_wrapper.o
  ifeq ($(LZMA),0)
  else
	HPATCH_OBJ+=$(LZMA_PATH)/7zCrc.o \
				$(LZMA_PATH)/7zCrcOpt.o \
				$(LZMA_PATH)/7zStream.o \
				$(LZMA_PATH)/Bra.o \
				$(LZMA_PATH)/Bra86.o \
				$(LZMA_PATH)/BraIA64.o \
				$(LZMA_PATH)/Delta.o \
				$(LZMA_PATH)/Sha256.o \
				$(LZMA_PATH)/Sha256Opt.o \
				$(LZMA_PATH)/Xz.o \
				$(LZMA_PATH)/XzCrc64.o \
				$(LZMA_PATH)/XzCrc64Opt.o \
				$(LZMA_PATH)/XzDec.o
	HDIFF_OBJ +=$(LZMA_PATH)/XzEnc.o
  endif
endif

ZSTD_PATH := ../zstd/lib
ifeq ($(ZSTD),1) # https://github.com/sisong/zstd
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

BZ2_PATH := ../bzip2
ifeq ($(BZIP2),1) # http://www.bzip.org  https://github.com/sisong/bzip2
  HPATCH_OBJ += $(BZ2_PATH)/blocksort.o \
  				$(BZ2_PATH)/bzlib.o \
  				$(BZ2_PATH)/compress.o \
  				$(BZ2_PATH)/crctable.o \
  				$(BZ2_PATH)/decompress.o \
  				$(BZ2_PATH)/huffman.o \
  				$(BZ2_PATH)/randtable.o
endif

ZLIB_PATH := ../zlib
ifeq ($(ZLIB),1) # https://github.com/sisong/zlib/tree/bit_pos_padding
  HPATCH_OBJ += $(ZLIB_PATH)/adler32.o \
  				$(ZLIB_PATH)/crc32.o \
  				$(ZLIB_PATH)/inffast.o \
  				$(ZLIB_PATH)/inflate.o \
  				$(ZLIB_PATH)/inftrees.o \
  				$(ZLIB_PATH)/trees.o \
  				$(ZLIB_PATH)/zutil.o
  HDIFF_OBJ +=  $(ZLIB_PATH)/deflate.o
endif

LDEF_PATH := ../libdeflate
ifeq ($(LDEF),1) # https://github.com/sisong/libdeflate/tree/stream-mt
  HPATCH_OBJ += $(LDEF_PATH)/lib/deflate_decompress.o\
                $(LDEF_PATH)/lib/utils.o \
                $(LDEF_PATH)/lib/x86/cpu_features.o
  HDIFF_OBJ +=  $(LDEF_PATH)/lib/deflate_compress.o
endif

HDIFF_OBJ += \
    hdiffz_import_patch.o \
    libHDiffPatch/HPatchLite/hpatch_lite.o \
    libHDiffPatch/HDiff/diff.o \
    libHDiffPatch/HDiff/match_block.o \
    libHDiffPatch/HDiff/private_diff/bytes_rle.o \
    libHDiffPatch/HDiff/private_diff/suffix_string.o \
    libHDiffPatch/HDiff/private_diff/compress_detect.o \
    libHDiffPatch/HDiff/private_diff/limit_mem_diff/digest_matcher.o \
    libHDiffPatch/HDiff/private_diff/limit_mem_diff/stream_serialize.o \
    libHDiffPatch/HDiff/private_diff/libdivsufsort/divsufsort64.o \
    libHDiffPatch/HDiff/private_diff/libdivsufsort/divsufsort.o \
    $(HPATCH_OBJ)
ifeq ($(DIR_DIFF),0)
  HDIFF_OBJ += libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.o
endif

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
    -D_IS_NEED_ALL_ChecksumPlugin=0 \
    -D_IS_NEED_DEFAULT_ChecksumPlugin=0 
ifeq ($(ATOMIC_U64),0)
  DEF_FLAGS += -D_IS_NO_ATOMIC_U64=1
endif
ifeq ($(M32),0)
else
  DEF_FLAGS += -m32
endif
ifeq ($(MINS),0)
else
  DEF_FLAGS += \
    -s \
    -Wno-error=format-security \
    -fvisibility=hidden  \
    -ffunction-sections -fdata-sections \
    -ffat-lto-objects -flto
  CXXFLAGS += -fvisibility-inlines-hidden
endif

ifeq ($(BZIP2),0)
else
    DEF_FLAGS += -D_CompressPlugin_bz2
	ifeq ($(BZIP2),1)
        DEF_FLAGS += -I$(BZ2_PATH)
	endif
endif
ifeq ($(ZLIB),0)
else
    DEF_FLAGS += -D_CompressPlugin_zlib
	ifeq ($(ZLIB),1)
    DEF_FLAGS += -I$(ZLIB_PATH)
	endif
endif
ifeq ($(LDEF),0)
else
    DEF_FLAGS += -D_CompressPlugin_ldef
	ifeq ($(LDEF),1)
    DEF_FLAGS += -I$(LDEF_PATH)
	endif
  ifeq ($(ZLIB),1)
    DEF_FLAGS += -D_CompressPlugin_ldef_is_use_zlib=1
  else
    DEF_FLAGS += -D_CompressPlugin_ldef_is_use_zlib=0
	endif
endif
ifeq ($(DIR_DIFF),0)
  DEF_FLAGS += -D_IS_NEED_DIR_DIFF_PATCH=0
else
  DEF_FLAGS += \
    -D_IS_NEED_DIR_DIFF_PATCH=1 \
    -D_ChecksumPlugin_fadler64
  ifeq ($(ZLIB),0)
  else
    DEF_FLAGS += -D_ChecksumPlugin_crc32
  endif
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
ifeq ($(VCD),0)
	DEF_FLAGS += -D_IS_NEED_VCDIFF=0
else
	DEF_FLAGS += -D_IS_NEED_VCDIFF=1
endif
ifeq ($(LZMA),0)
else
  DEF_FLAGS += -D_CompressPlugin_lzma -D_CompressPlugin_lzma2
  ifeq ($(VCD),0)
  else
    DEF_FLAGS += -D_CompressPlugin_7zXZ
  endif
  ifeq ($(LZMA),1)
    DEF_FLAGS += -I$(LZMA_PATH)
    ifeq ($(ARM64ASM),0)
    else
      DEF_FLAGS += -DZ7_LZMA_DEC_OPT
    endif
    ifneq ($(VCD),0)
      ifneq ($(USE_CRC_EMU),0)
        DEF_FLAGS += -DUSE_CRC_EMU
      endif
    endif
  endif
endif
ifeq ($(ZSTD),0)
else
  DEF_FLAGS += -D_CompressPlugin_zstd
  ifeq ($(ZSTD),1)
    DEF_FLAGS += -DZSTD_HAVE_WEAK_SYMBOLS=0 -DZSTD_TRACE=0 -DZSTD_DISABLE_ASM=1 -DZSTDLIB_VISIBLE= -DZSTDLIB_HIDDEN= \
	               -I$(ZSTD_PATH) -I$(ZSTD_PATH)/common -I$(ZSTD_PATH)/compress -I$(ZSTD_PATH)/decompress
	endif
endif

ifeq ($(MT),0)
  DEF_FLAGS += \
    -DZ7_ST \
    -D_IS_USED_MULTITHREAD=0
else
  DEF_FLAGS += \
    -DZSTD_MULTITHREAD=1 \
    -D_IS_USED_MULTITHREAD=1 \
    -D_IS_USED_CPP11THREAD=1
endif

PATCH_LINK := 
ifeq ($(ZLIB),2)
  PATCH_LINK += -lz			# link zlib
endif
ifeq ($(BZIP2),2)
  PATCH_LINK += -lbz2		# link bzip2
endif
ifeq ($(ZSTD),2)
  PATCH_LINK += -lzstd		# link zstd
endif
ifeq ($(LZMA),2)
  PATCH_LINK += -llzma		# link lzma
endif
ifeq ($(MT),0)
else
  PATCH_LINK += -lpthread	# link pthread
endif
DIFF_LINK  := $(PATCH_LINK)
ifeq ($(M32),0)
else
  DIFF_LINK += -m32
endif
ifeq ($(MINS),0)
else
  DIFF_LINK += -s -Wl,--gc-sections,--as-needed
endif
ifeq ($(CL),1)
  CXX := clang++
  CC  := clang
endif
ifeq ($(STATIC_CPP),0)
  DIFF_LINK += -lstdc++
else
  DIFF_LINK += -static-libstdc++
endif

CFLAGS   += $(DEF_FLAGS) 
CXXFLAGS += $(DEF_FLAGS) -std=c++11

.PHONY: all install clean

all: libhdiffpatch.a hpatchz hdiffz mostlyclean

libhdiffpatch.a: $(HDIFF_OBJ)
	$(AR) rcs $@ $^

hpatchz: $(HPATCH_OBJ)
	$(CC) hpatchz.c $(HPATCH_OBJ) $(CFLAGS) $(PATCH_LINK) -o hpatchz
hdiffz: libhdiffpatch.a
	$(CXX) hdiffz.cpp libhdiffpatch.a $(CXXFLAGS) $(DIFF_LINK) -o hdiffz

ifeq ($(OS),Windows_NT) # mingw?
  RM := del /Q /F
  DEL_HDIFF_OBJ := $(subst /,\,$(HDIFF_OBJ))
else
  RM := rm -f
  DEL_HDIFF_OBJ := $(HDIFF_OBJ)
endif
INSTALL_X := install -m 0755
INSTALL_BIN := $(DESTDIR)/usr/local/bin

mostlyclean: hpatchz hdiffz
	$(RM) $(DEL_HDIFF_OBJ)
clean:
	$(RM) libhdiffpatch.a hpatchz hdiffz $(DEL_HDIFF_OBJ)

install: all
	$(INSTALL_X) hdiffz $(INSTALL_BIN)/hdiffz
	$(INSTALL_X) hpatchz $(INSTALL_BIN)/hpatchz

uninstall:
	$(RM)  $(INSTALL_BIN)/hdiffz  $(INSTALL_BIN)/hpatchz
