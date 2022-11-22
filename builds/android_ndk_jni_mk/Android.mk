LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := hpatchz

# args
ZSTD  := 0
# if open BSD,must open BZIP2
BSD   := 0
VCD   := 0
BZIP2 := 0
LZMA  := 1

ifeq ($(BZIP2),0)
  Bz2_Files :=
else
  # http://www.bzip.org  https://github.com/sisong/bzip2
  BZ2_PATH  :=  $(LOCAL_PATH)/../../../bzip2/
  Bz2_Files :=  $(BZ2_PATH)/blocksort.c \
  				$(BZ2_PATH)/bzlib.c \
  				$(BZ2_PATH)/compress.c \
  				$(BZ2_PATH)/crctable.c \
  				$(BZ2_PATH)/decompress.c \
  				$(BZ2_PATH)/huffman.c \
  				$(BZ2_PATH)/randtable.c
endif

ifeq ($(LZMA),0)
  Lzma_Files :=
else
  # https://github.com/sisong/lzma
  LZMA_PATH  := $(LOCAL_PATH)/../../../lzma/C/
  Lzma_Files := $(LZMA_PATH)/LzmaDec.c \
                $(LZMA_PATH)/Lzma2Dec.c
 ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
  Lzma_Files += $(LZMA_PATH)/../Asm/arm64/LzmaDecOpt.S
 endif
  ifeq ($(VCD),0)
  else
    Lzma_Files+=$(LZMA_PATH)/7zCrc.c \
				$(LZMA_PATH)/7zCrcOpt.c \
				$(LZMA_PATH)/Bra.c \
				$(LZMA_PATH)/Bra86.c \
				$(LZMA_PATH)/BraIA64.c \
				$(LZMA_PATH)/Delta.c \
				$(LZMA_PATH)/Sha256.c \
				$(LZMA_PATH)/Sha256Opt.c \
				$(LZMA_PATH)/Xz.c \
				$(LZMA_PATH)/XzCrc64.c \
				$(LZMA_PATH)/XzCrc64Opt.c \
				$(LZMA_PATH)/XzDec.c \
				$(LZMA_PATH)/CpuArch.c
  endif
endif

ifeq ($(ZSTD),0)
  Zstd_Files :=
else
  # https://github.com/facebook/zstd
  ZSTD_PATH  := $(LOCAL_PATH)/../../../zstd/lib/
  Zstd_Files := $(ZSTD_PATH)/common/debug.c \
  				$(ZSTD_PATH)/common/entropy_common.c \
  				$(ZSTD_PATH)/common/error_private.c \
  				$(ZSTD_PATH)/common/fse_decompress.c \
  				$(ZSTD_PATH)/common/xxhash.c \
  				$(ZSTD_PATH)/common/zstd_common.c \
  				$(ZSTD_PATH)/decompress/huf_decompress.c \
  				$(ZSTD_PATH)/decompress/zstd_ddict.c \
  				$(ZSTD_PATH)/decompress/zstd_decompress.c \
  				$(ZSTD_PATH)/decompress/zstd_decompress_block.c
endif

HDP_PATH  := $(LOCAL_PATH)/../../
Hdp_Files := $(HDP_PATH)/file_for_patch.c \
             $(HDP_PATH)/libHDiffPatch/HPatch/patch.c
ifeq ($(BSD),0)
else
  Hdp_Files += $(HDP_PATH)/bsdiff_wrapper/bspatch_wrapper.c
endif
ifeq ($(VCD),0)
else
  Hdp_Files += $(HDP_PATH)/vcdiff_wrapper/vcpatch_wrapper.c
  Hdp_Files += $(HDP_PATH)/libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.c
endif

Src_Files := $(LOCAL_PATH)/hpatch_jni.c \
             $(LOCAL_PATH)/hpatch.c

DEF_FLAGS := -Os -D_IS_NEED_CACHE_OLD_BY_COVERS=0 -D_IS_NEED_DEFAULT_CompressPlugin=0
DEF_FLAGS += -D_CompressPlugin_zlib
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
ifeq ($(BZIP2),0)
else
  DEF_FLAGS += -D_CompressPlugin_bz2 -I$(BZ2_PATH)
endif
ifeq ($(LZMA),0)
else
  DEF_FLAGS += -D_CompressPlugin_lzma -D_CompressPlugin_lzma2 -D_7ZIP_ST -I$(LZMA_PATH)
  ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
    DEF_FLAGS += -D_LZMA_DEC_OPT
  endif
  ifeq ($(VCD),0)
  else
    DEF_FLAGS += -D_CompressPlugin_7zXZ
  endif
endif
ifeq ($(ZSTD),0)
else
  DEF_FLAGS += -D_CompressPlugin_zstd -DZSTD_DISABLE_ASM -DZSTD_HAVE_WEAK_SYMBOLS=0 -DZSTD_TRACE=0 \
	    -I$(ZSTD_PATH) -I$(ZSTD_PATH)/common -I$(ZSTD_PATH)/decompress
endif

LOCAL_SRC_FILES  := $(Src_Files) $(Bz2_Files) $(Lzma_Files) $(Zstd_Files) $(Hdp_Files)
LOCAL_LDLIBS     := -llog -lz
LOCAL_CFLAGS     := -DANDROID_NDK -DNDEBUG $(DEF_FLAGS)
include $(BUILD_SHARED_LIBRARY)

