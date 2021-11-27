LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := hpatchz

# args
LZMA  := 1

ifeq ($(LZMA),0)
  Lzma_Files :=
else
  # https://github.com/sisong/lzma
  LZMA_PATH  := $(LOCAL_PATH)/../../../lzma/C/
  Lzma_Files := $(LZMA_PATH)/LzmaDec.c  \
                $(LZMA_PATH)/Lzma2Dec.c
 ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
  Lzma_Files += $(LOCAL_PATH)/../../../lzma/Asm/arm64/LzmaDecOpt.S
 endif
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

LOCAL_SRC_FILES  := $(Src_Files) $(Lzma_Files) $(Hdp_Files)
LOCAL_LDLIBS     := -llog -lz
LOCAL_CFLAGS     := -DANDROID_NDK -DNDEBUG $(DEF_FLAGS)
include $(BUILD_SHARED_LIBRARY)

