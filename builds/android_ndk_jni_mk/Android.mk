LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := hpatchz

# args
LZMA  := 1
ZSTD  := 0

ifeq ($(LZMA),0)
  Lzma_Files :=
else
  # https://github.com/sisong/lzma
  LZMA_PATH  := $(LOCAL_PATH)/../../../lzma/C/
  Lzma_Files := $(LZMA_PATH)/LzmaDec.c  \
                $(LZMA_PATH)/Lzma2Dec.c
 ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
  Lzma_Files += $(LZMA_PATH)/../Asm/arm64/LzmaDecOpt.S
 endif
endif

ifeq ($(ZSTD),0)
  Zstd_Files :=
else
  # https://github.com/facebook/zstd
  ZSTD_PATH  := $(LOCAL_PATH)/../../../zstd/lib
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

Src_Files := $(LOCAL_PATH)/hpatch_jni.c \
             $(LOCAL_PATH)/hpatch.c

DEF_FLAGS := -Os -D_IS_NEED_BSDIFF=0 -D_IS_NEED_CACHE_OLD_BY_COVERS=0 -D_IS_NEED_DEFAULT_CompressPlugin=0
DEF_FLAGS += -D_CompressPlugin_zlib
ifeq ($(LZMA),0)
else
  DEF_FLAGS += -D_CompressPlugin_lzma -D_CompressPlugin_lzma2 -D_7ZIP_ST -I$(LZMA_PATH)
  ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
    DEF_FLAGS += -D_LZMA_DEC_OPT
  endif
endif
ifeq ($(ZSTD),0)
else
  DEF_FLAGS += -D_CompressPlugin_zstd -DZSTD_DISABLE_ASM -DZSTD_HAVE_WEAK_SYMBOLS=0 -DZSTD_TRACE=0 \
	    -I$(ZSTD_PATH) -I$(ZSTD_PATH)/common -I$(ZSTD_PATH)/decompress
endif

LOCAL_SRC_FILES  := $(Src_Files) $(Lzma_Files) $(Zstd_Files) $(Hdp_Files)
LOCAL_LDLIBS     := -llog -lz
LOCAL_CFLAGS     := -DANDROID_NDK -DNDEBUG $(DEF_FLAGS)
include $(BUILD_SHARED_LIBRARY)

