# args
DIR_DIFF := 1
MT       := 1
LZMA     := 1
ZSTD     := 1
MD5      := 1


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


HDIFF_OBJ := \
    hdiffz_import_patch.o \
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
    dirDiffPatch/dir_diff/dir_diff.o \
    dirDiffPatch/dir_diff/dir_diff_tools.o \
    dirDiffPatch/dir_diff/dir_manifest.o
endif
ifeq ($(MT),0)
else
  HDIFF_OBJ += \
    libParallel/parallel_import.o \
    libParallel/parallel_channel.o \
    compress_parallel.o
endif


DEF_FLAGS := \
    -Os -DNDEBUG -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 \
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
ifeq ($(ZSTD),0)
else
  DEF_FLAGS += \
    -D_CompressPlugin_zstd -I'../zstd/lib' \
    -D_CompressPlugin_zstd -I'../zstd/lib/common' \
    -D_CompressPlugin_zstd -I'../zstd/lib/compress' \
    -D_CompressPlugin_zstd -I'../zstd/lib/decompress'
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

CFLAGS   += $(DEF_FLAGS) 
CXXFLAGS += $(DEF_FLAGS)

.PHONY: all install clean

all: md5Lib lzmaLib zstdLib libhdiffpatch.a hdiffz hpatchz

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
  LZMA_SRC     := '../lzma/C/LzmaDec.c' '../lzma/C/Lzma2Dec.c' \
		              '../lzma/C/LzFind.c' '../lzma/C/LzmaEnc.c' '../lzma/C/Lzma2Enc.c'
  ifeq ($(MT),0)  
  else  
    LZMA_OBJ += 'LzFindMt.o' 'MtCoder.o' 'MtDec.o' 'Threads.o'
    LZMA_SRC += '../lzma/C/LzFindMt.c' '../lzma/C/MtCoder.c' \
		   '../lzma/C/MtDec.c' '../lzma/C/Threads.c' 
  endif
  lzmaLib: # https://github.com/sisong/lzma
	$(CC) -c $(CFLAGS) $(LZMA_SRC)
endif

ifeq ($(ZSTD),0)
  ZSTD_DEC_OBJ :=
  ZSTD_OBJ     :=
  zstdLib      :
else  
  ZSTD_DEC_OBJ := 'debug.o' 'entropy_common.o' 'error_private.o'\
                  'fse_decompress.o' 'xxhash.o' 'zstd_common.o' \
                  'huf_decompress.o' 'zstd_ddict.o' \
                  'zstd_decompress.o' 'zstd_decompress_block.o'
  ZSTD_OBJ     := 'fse_compress.o' 'hist.o' 'huf_compress.o' \
                  'zstd_compress.o' 'zstd_compress_literals.o' 'zstd_compress_sequences.o' \
                  'zstd_compress_superblock.o' 'zstd_double_fast.o' 'zstd_fast.o' \
                  'zstd_lazy.o' 'zstd_ldm.o' 'zstd_opt.o'  $(ZSTD_DEC_OBJ)
  ZSTD_SRC     := '../zstd/lib/common/debug.c' '../zstd/lib/common/entropy_common.c' '../zstd/lib/common/error_private.c' \
                  '../zstd/lib/common/fse_decompress.c' '../zstd/lib/common/xxhash.c' '../zstd/lib/common/zstd_common.c' \
                  '../zstd/lib/decompress/huf_decompress.c' '../zstd/lib/decompress/zstd_ddict.c' \
                  '../zstd/lib/decompress/zstd_decompress.c' '../zstd/lib/decompress/zstd_decompress_block.c' \
                  '../zstd/lib/compress/fse_compress.c' '../zstd/lib/compress/hist.c' '../zstd/lib/compress/huf_compress.c' \
                  '../zstd/lib/compress/zstd_compress.c' '../zstd/lib/compress/zstd_compress_literals.c' '../zstd/lib/compress/zstd_compress_sequences.c' \
                  '../zstd/lib/compress/zstd_compress_superblock.c' '../zstd/lib/compress/zstd_double_fast.c' '../zstd/lib/compress/zstd_fast.c' \
                  '../zstd/lib/compress/zstd_lazy.c' '../zstd/lib/compress/zstd_ldm.c' '../zstd/lib/compress/zstd_opt.c' 
                  
  ifeq ($(MT),0)  
  else  
    ZSTD_OBJ += 'pool.o' 'threading.o' 'zstdmt_compress.o'
    ZSTD_SRC += '../zstd/lib/common/pool.c' '../zstd/lib/common/threading.c' '../zstd/lib/compress/zstdmt_compress.c'
  endif
  zstdLib: # https://github.com/facebook/zstd
	$(CC) -c $(CFLAGS) $(ZSTD_SRC)
endif

libhdiffpatch.a: $(HDIFF_OBJ)
	$(AR) rcs $@ $^

hdiffz: 
	$(CXX) hdiffz.cpp libhdiffpatch.a $(MD5_OBJ) $(LZMA_OBJ) $(ZSTD_OBJ) $(CXXFLAGS) $(DIFF_LINK) -o hdiffz
hpatchz: 
	$(CC) hpatchz.c $(HPATCH_OBJ) $(MD5_OBJ) $(LZMA_DEC_OBJ) $(ZSTD_DEC_OBJ) $(CFLAGS) $(PATCH_LINK) -o hpatchz

RM := rm -f
INSTALL_X := install -m 0755
INSTALL_BIN := $(DESTDIR)/usr/local/bin

clean:
	$(RM) libhdiffpatch.a hdiffz hpatchz $(HDIFF_OBJ) $(MD5_OBJ) $(LZMA_OBJ) $(ZSTD_OBJ)

install: all
	$(INSTALL_X) hdiffz $(INSTALL_BIN)/hdiffz
	$(INSTALL_X) hpatchz $(INSTALL_BIN)/hpatchz

uninstall:
	$(RM)  $(INSTALL_BIN)/hdiffz  $(INSTALL_BIN)/hpatchz
