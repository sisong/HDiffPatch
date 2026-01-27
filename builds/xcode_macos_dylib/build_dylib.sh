#!/bin/bash
# Build libhpatch.dylib for macOS
# Created for HDiffPatch

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
OUT_DIR="$SCRIPT_DIR"

ZLIB_PATH="../zlib"
LZMA_PATH="../lzma/C"
ZSTD_PATH="../zstd/lib"

cd "$ROOT_DIR"

CFLAGS="-Os -DNDEBUG -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 \
    -D_IS_NEED_ALL_CompressPlugin=0 \
    -D_IS_NEED_DEFAULT_CompressPlugin=0 \
    -D_IS_NEED_DIR_DIFF_PATCH=0 \
    -D_IS_USED_MULTITHREAD=0 \
    -D_IS_NEED_BSDIFF=0 \
    -D_IS_NEED_VCDIFF=0 \
    -D_IS_NEED_CACHE_OLD_BY_COVERS=0 \
    -D_IS_NEED_CACHE_OLD_ALL=1 \
    -D_CompressPlugin_zlib -I$ZLIB_PATH \
    -D_CompressPlugin_lzma -D_CompressPlugin_lzma2 -DZ7_ST -I$LZMA_PATH \
    -D_CompressPlugin_zstd -DZSTD_HAVE_WEAK_SYMBOLS=0 -DZSTD_TRACE=0 -DZSTD_DISABLE_ASM=1 \
    -DZSTDLIB_HIDDEN= -DZSTDLIB_VISIBLE= -DZDICTLIB_VISIBLE= -DZSTDERRORLIB_VISIBLE= \
    -DDYNAMIC_BMI2=0 -DZSTD_LEGACY_SUPPORT=0 -DZSTD_LIB_DEPRECATED=0 -DHUF_FORCE_DECOMPRESS_X1=1 \
    -DZSTD_FORCE_DECOMPRESS_SEQUENCES_SHORT=1 -DZSTD_NO_INLINE=1 -DZSTD_STRIP_ERROR_STRINGS=1 \
    -I$ZSTD_PATH -I$ZSTD_PATH/common -I$ZSTD_PATH/decompress \
    -fPIC -fvisibility=default"

HPATCH_SRCS="
    libHDiffPatch/HPatch/patch.c
    file_for_patch.c
    $LZMA_PATH/LzmaDec.c
    $LZMA_PATH/Lzma2Dec.c
    $ZSTD_PATH/common/debug.c
    $ZSTD_PATH/common/entropy_common.c
    $ZSTD_PATH/common/error_private.c
    $ZSTD_PATH/common/fse_decompress.c
    $ZSTD_PATH/common/xxhash.c
    $ZSTD_PATH/common/zstd_common.c
    $ZSTD_PATH/decompress/huf_decompress.c
    $ZSTD_PATH/decompress/zstd_ddict.c
    $ZSTD_PATH/decompress/zstd_decompress.c
    $ZSTD_PATH/decompress/zstd_decompress_block.c
    $ZLIB_PATH/adler32.c
    $ZLIB_PATH/crc32.c
    $ZLIB_PATH/inffast.c
    $ZLIB_PATH/inflate.c
    $ZLIB_PATH/inftrees.c
    $ZLIB_PATH/trees.c
    $ZLIB_PATH/zutil.c
"

WRAPPER_SRC="builds/android_ndk_jni_mk/hpatch.c"

echo "Building libhpatchz.dylib..."

clang -dynamiclib \
    -install_name @rpath/libhpatchz.dylib \
    $CFLAGS \
    $HPATCH_SRCS \
    $WRAPPER_SRC \
    -o "$OUT_DIR/libhpatchz.dylib"

cp builds/android_ndk_jni_mk/hpatch.h "$OUT_DIR/hpatchz.h"

echo "Done! Output:"
echo "  $OUT_DIR/libhpatchz.dylib"
echo "  $OUT_DIR/hpatchz.h"
