LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := hpatchz

# args
ZLIB  := 1
LZMA  := 1
ZSTD  := 1
BROTLI:= 0
VCD   := 0
# if open BSD,must open BZIP2
BSD   := 0
BZIP2 := 0

DEF_FLAGS := -Os -DANDROID_NDK -DNDEBUG -D_LARGEFILE_SOURCE \
             -D_IS_NEED_CACHE_OLD_BY_COVERS=0 -D_IS_NEED_DEFAULT_CompressPlugin=0
LINK_FLAGS:= -llog

HDP_PATH  := $(LOCAL_PATH)/../../
Src_Files := $(HDP_PATH)/builds/android_ndk_jni_mk/hpatch_jni.c \
             $(HDP_PATH)/builds/android_ndk_jni_mk/hpatch.c \
             $(HDP_PATH)/file_for_patch.c \
             $(HDP_PATH)/libHDiffPatch/HPatch/patch.c

ifeq ($(BSD),0)
  DEF_FLAGS += -D_IS_NEED_BSDIFF=0
else
  DEF_FLAGS += -D_IS_NEED_BSDIFF=1
  Src_Files += $(HDP_PATH)/bsdiff_wrapper/bspatch_wrapper.c
endif

ifeq ($(VCD),0)
  DEF_FLAGS += -D_IS_NEED_VCDIFF=0
else
  DEF_FLAGS += -D_IS_NEED_VCDIFF=1
  Src_Files += $(HDP_PATH)/vcdiff_wrapper/vcpatch_wrapper.c \
               $(HDP_PATH)/libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.c
endif

ifeq ($(ZLIB),0)
else
  DEF_FLAGS += -D_CompressPlugin_zlib
  LINK_FLAGS+= -lz
endif

ifeq ($(BZIP2),0)
else
  # http://www.bzip.org  https://github.com/sisong/bzip2
  BZ2_PATH  :=  $(HDP_PATH)/../bzip2/
  DEF_FLAGS += -D_CompressPlugin_bz2 -DBZ_NO_STDIO -I$(BZ2_PATH)
  Src_Files +=  $(BZ2_PATH)/blocksort.c \
                $(BZ2_PATH)/bzlib.c \
                $(BZ2_PATH)/compress.c \
                $(BZ2_PATH)/crctable.c \
                $(BZ2_PATH)/decompress.c \
                $(BZ2_PATH)/huffman.c \
                $(BZ2_PATH)/randtable.c
endif

ifeq ($(LZMA),0)
else
  # https://github.com/sisong/lzma
  LZMA_PATH := $(HDP_PATH)/../lzma/C/
  DEF_FLAGS += -D_CompressPlugin_lzma -D_CompressPlugin_lzma2 -DZ7_ST -I$(LZMA_PATH)
  ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
    DEF_FLAGS += -DZ7_LZMA_DEC_OPT
  endif
  Src_Files += $(LZMA_PATH)/LzmaDec.c \
               $(LZMA_PATH)/Lzma2Dec.c
  ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
    Src_Files += $(LZMA_PATH)/../Asm/arm64/LzmaDecOpt.S
  endif
  ifeq ($(VCD),0)
  else
    DEF_FLAGS += -D_CompressPlugin_7zXZ
    Src_Files +=$(LZMA_PATH)/7zCrc.c \
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
else
  # https://github.com/sisong/zstd
  ZSTD_PATH  := $(HDP_PATH)/../zstd/lib/
  DEF_FLAGS += -D_CompressPlugin_zstd -I$(ZSTD_PATH) -I$(ZSTD_PATH)/common -I$(ZSTD_PATH)/decompress \
      -DZSTD_HAVE_WEAK_SYMBOLS=0 -DZSTD_TRACE=0 -DZSTD_DISABLE_ASM=1 -DZSTDLIB_VISIBLE= -DZSTDLIB_HIDDEN= \
		  -DDYNAMIC_BMI2=0 -DZSTD_LEGACY_SUPPORT=0 -DZSTD_LIB_DEPRECATED=0 -DHUF_FORCE_DECOMPRESS_X1=1 \
		  -DZSTD_FORCE_DECOMPRESS_SEQUENCES_SHORT=1 -DZSTD_NO_INLINE=1 -DZSTD_STRIP_ERROR_STRINGS=1 -DZSTDERRORLIB_VISIBILITY=
  Src_Files +=$(ZSTD_PATH)/common/debug.c \
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

ifeq ($(BROTLI),0)
else
  # https://github.com/google/brotli
  BROTLI_PATH := $(HDP_PATH)/../brotli/c/
  DEF_FLAGS += -D_CompressPlugin_brotli -I$(BROTLI_PATH)/include
  Src_Files +=$(BROTLI_PATH)/common/constants.c \
              $(BROTLI_PATH)/common/context.c \
              $(BROTLI_PATH)/common/dictionary.c \
              $(BROTLI_PATH)/common/platform.c \
              $(BROTLI_PATH)/common/shared_dictionary.c \
              $(BROTLI_PATH)/common/transform.c \
              $(BROTLI_PATH)/dec/bit_reader.c \
              $(BROTLI_PATH)/dec/decode.c \
              $(BROTLI_PATH)/dec/huffman.c \
              $(BROTLI_PATH)/dec/state.c
endif


LOCAL_SRC_FILES := $(Src_Files)
LOCAL_LDLIBS    := $(LINK_FLAGS)
LOCAL_CFLAGS    := $(DEF_FLAGS)
include $(BUILD_SHARED_LIBRARY)

