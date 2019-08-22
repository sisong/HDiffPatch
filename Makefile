# args
LZMA     := 1
DIR_DIFF := 1
MD5      := 0
MT       := 0


HPATCH_OBJ := \
    libHDiffPatch/HPatch/patch.o \
    file_for_patch.o \
    dirDiffPatch/dir_patch/dir_patch.o
ifeq ($(DIR_DIFF),0)
else
  HPATCH_OBJ += \
    dirDiffPatch/dir_patch/res_handle_limit.o \
    dirDiffPatch/dir_patch/ref_stream.o \
    dirDiffPatch/dir_patch/new_stream.o \
    libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.o
endif


HDIFF_OBJ := \
    libHDiffPatch/HDiff/diff.o \
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
    dirDiffPatch/dir_diff/dir_diff.o
endif
ifeq ($(MT),0)
else
  HDIFF_OBJ += \
    libParallel/parallel_import.o \
    libParallel/parallel_channel.o \
    compress_parallel.o
endif


DEF_FLAGS := \
    -O3 -DNDEBUG -D_FILE_OFFSET_BITS=64 \
    -D_IS_NEED_ORIGINAL=1 \
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
    DEF_FLAGS += -D_ChecksumPlugin_md5 -I'../libmd5'
  endif
endif
ifeq ($(LZMA),0)
else
  DEF_FLAGS += \
    -D_CompressPlugin_lzma -I'../lzma/C' \
    -D_CompressPlugin_lzma2 -I'../lzma/C'
endif
ifeq ($(MT),0)
  DEF_FLAGS += \
    -D_7ZIP_ST \
    -D_IS_USED_MULTITHREAD=0 
else
  DEF_FLAGS += \
    -D_IS_USED_MULTITHREAD=1 \
    -D_IS_USED_PTHREAD=1
endif


PATCH_LINK := -lz -lbz2
DIFF_LINK  := $(PATCH_LINK)
ifeq ($(MT),0)
else
  DIFF_LINK += -lpthread
endif


CFLAGS   += $(DEF_FLAGS) 
CXXFLAGS += $(DEF_FLAGS)

.PHONY: all install clean

all: md5Lib lzmaLib libhdiffpatch.a hdiffz hpatchz

ifeq ($(DIR_DIFF),0)
  MD5_OBJ     :=
  md5Lib      :
else
  ifeq ($(MD5),0)
    MD5_OBJ     :=
    md5Lib      :
  else
    MD5_OBJ     := 'md5.o'
    md5Lib      : # https://sourceforge.net/projects/libmd5-rfc  https://github.com/sisong/libmd5
	$(CC) -c $(CFLAGS) '../libmd5/md5.c'
  endif
endif

ifeq ($(LZMA),0)
  LZMA_DEC_OBJ :=
  LZMA_OBJ     :=
  lzmaLib      :
else
  LZMA_DEC_OBJ := 'LzmaDec.o' 'Lzma2Dec.o' 
  LZMA_OBJ     := 'LzFind.o' 'LzmaEnc.o' 'Lzma2Enc.o' $(LZMA_DEC_OBJ)
  LZMA_SRC     := '../lzma/C/LzFind.c' '../lzma/C/LzmaDec.c' '../lzma/C/LzmaEnc.c' \
		     '../lzma/C/Lzma2Dec.c' '../lzma/C/Lzma2Enc.c'
  ifeq ($(MT),0)  
  else  # download from https://github.com/sisong/lzma/tree/pthread
    LZMA_OBJ += 'LzFindMt.o' 'MtCoder.o' 'MtDec.o' 'ThreadsP.o'
    LZMA_SRC += '../lzma/C/LzFindMt.c' '../lzma/C/MtCoder.c' \
		   '../lzma/C/MtDec.c' '../lzma/C/ThreadsP.c' 
  endif
  lzmaLib: # https://www.7-zip.org/sdk.html  https://github.com/sisong/lzma
	$(CC) -c $(CFLAGS) $(LZMA_SRC)
endif

libhdiffpatch.a: $(HDIFF_OBJ)
	$(AR) rcs $@ $^

hdiffz: 
	$(CXX) hdiffz.cpp libhdiffpatch.a $(MD5_OBJ) $(LZMA_OBJ) $(CXXFLAGS) $(DIFF_LINK) -o hdiffz
hpatchz: 
	$(CC) hpatchz.c $(HPATCH_OBJ) $(MD5_OBJ) $(LZMA_DEC_OBJ) $(CFLAGS) $(PATCH_LINK) -o hpatchz

RM := rm -f
INSTALL_X := install -m 0755
INSTALL_BIN := $(DESTDIR)/usr/local/bin

clean:
	$(RM) libhdiffpatch.a hdiffz hpatchz $(HDIFF_OBJ) $(MD5_OBJ) $(LZMA_OBJ)

install: all
	$(INSTALL_X) hdiffz $(INSTALL_BIN)/hdiffz
	$(INSTALL_X) hpatchz $(INSTALL_BIN)/hpatchz

uninstall:
	$(RM)  $(INSTALL_BIN)/hdiffz  $(INSTALL_BIN)/hpatchz
