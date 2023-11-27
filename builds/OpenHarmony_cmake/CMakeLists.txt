# the minimum version of CMake.
cmake_minimum_required(VERSION 3.4.1)
project(hpatchz)
set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include)

# args
set(ZLIB    1)
set(LZMA    1)
set(ZSTD    1)
set(BROTLI  0)
set(VCD     1)
# if open BSD,must open BZIP2
set(BSD     1)
set(BZIP2   1)

set(DEF_FLAGS  "-s -Wno-error=format-security \
                -fvisibility=hidden  \
                -ffunction-sections -fdata-sections \
                -ffat-lto-objects -flto \
                -Os -DNDEBUG -D_LARGEFILE_SOURCE \
                -D_IS_NEED_CACHE_OLD_BY_COVERS=0 -D_IS_NEED_DEFAULT_CompressPlugin=0 \
                ")
set(LINK_FLAGS  "-s -Wl,--gc-sections,--as-needed")

set(HDP_PATH "../../../../../../HDiffPatch")
set(Src_Files ${HDP_PATH}/builds/android_ndk_jni_mk/hpatch.c
              ${HDP_PATH}/file_for_patch.c
              ${HDP_PATH}/libHDiffPatch/HPatch/patch.c )

if (${BSD} EQUAL 0)
  set(DEF_FLAGS  "${DEF_FLAGS} -D_IS_NEED_BSDIFF=0")
else()
  set(DEF_FLAGS  "${DEF_FLAGS} -D_IS_NEED_BSDIFF=1")
  set(Src_Files  ${Src_Files} ${HDP_PATH}/bsdiff_wrapper/bspatch_wrapper.c)
endif()

if (${VCD} EQUAL 0)
  set(DEF_FLAGS  "${DEF_FLAGS} -D_IS_NEED_VCDIFF=0")
else()
  set(DEF_FLAGS  "${DEF_FLAGS} -D_IS_NEED_VCDIFF=1")
  set(Src_Files  ${Src_Files}
        ${HDP_PATH}/vcdiff_wrapper/vcpatch_wrapper.c
        ${HDP_PATH}/libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.c )
endif()

if (${ZLIB} EQUAL 0)
else()
  set(DEF_FLAGS  "${DEF_FLAGS} -D_CompressPlugin_zlib")
  set(LINK_FLAGS "${LINK_FLAGS} -lz")
endif()

if (${BZIP2} EQUAL 0)
else()
  # http://www.bzip.org  https://github.com/sisong/bzip2
  set(BZ2_PATH ${HDP_PATH}/../bzip2)
  set(DEF_FLAGS  "${DEF_FLAGS} -D_CompressPlugin_bz2 -DBZ_NO_STDIO")
  include_directories(${BZ2_PATH})
  set(Src_Files  ${Src_Files}
        ${BZ2_PATH}/blocksort.c
        ${BZ2_PATH}/bzlib.c
        ${BZ2_PATH}/compress.c
        ${BZ2_PATH}/crctable.c
        ${BZ2_PATH}/decompress.c
        ${BZ2_PATH}/huffman.c
        ${BZ2_PATH}/randtable.c )
endif()

if (${LZMA} EQUAL 0)
else()
  # https://github.com/sisong/lzma
  set(LZMA_PATH  ${HDP_PATH}/../lzma/C/)
  set(DEF_FLAGS  "${DEF_FLAGS} -D_CompressPlugin_lzma -D_CompressPlugin_lzma2 -DZ7_ST")
  include_directories(${LZMA_PATH})
  string(FIND ${CMAKE_LIBRARY_ARCHITECTURE} "aarch64" arm64Pos)
  if (${arm64Pos} GREATER_EQUAL 0)
    ENABLE_LANGUAGE(ASM)
    set(DEF_FLAGS  "${DEF_FLAGS} -DZ7_LZMA_DEC_OPT")
  endif()
  set(Src_Files  ${Src_Files}
        ${LZMA_PATH}/LzmaDec.c
        ${LZMA_PATH}/Lzma2Dec.c )
  if (${arm64Pos} GREATER_EQUAL 0)
    set(Src_Files  ${Src_Files} ${LZMA_PATH}/../Asm/arm64/LzmaDecOpt.S )
  endif()
  if (${VCD} EQUAL 0)
  else()
    set(DEF_FLAGS  "${DEF_FLAGS} -D_CompressPlugin_7zXZ -D_IS_TRY_USE_HW_SHA=0")
    set(Src_Files  ${Src_Files}
          ${LZMA_PATH}/7zCrc.c
          ${LZMA_PATH}/7zCrcOpt.c
          ${LZMA_PATH}/Bra.c
          ${LZMA_PATH}/Bra86.c
          ${LZMA_PATH}/BraIA64.c
          ${LZMA_PATH}/Delta.c
          ${LZMA_PATH}/Sha256.c
          ${LZMA_PATH}/Sha256Opt.c
          ${LZMA_PATH}/Xz.c
          ${LZMA_PATH}/XzCrc64.c
          ${LZMA_PATH}/XzCrc64Opt.c
          ${LZMA_PATH}/XzDec.c
          ${LZMA_PATH}/CpuArch.c )
  endif()
endif()

if (${ZSTD} EQUAL 0)
else()
  # https://github.com/sisong/zstd
  set(ZSTD_PATH  ${HDP_PATH}/../zstd/lib/)
  set(DEF_FLAGS  "${DEF_FLAGS} -D_CompressPlugin_zstd \
      -DZSTD_HAVE_WEAK_SYMBOLS=0 -DZSTD_TRACE=0 -DZSTD_DISABLE_ASM=1 -DZSTDLIB_VISIBLE= -DZSTDLIB_HIDDEN= \
		  -DDYNAMIC_BMI2=0 -DZSTD_LEGACY_SUPPORT=0 -DZSTD_LIB_DEPRECATED=0 -DHUF_FORCE_DECOMPRESS_X1=1 \
		  -DZSTD_FORCE_DECOMPRESS_SEQUENCES_SHORT=1 -DZSTD_NO_INLINE=1 -DZSTD_STRIP_ERROR_STRINGS=1 -DZSTDERRORLIB_VISIBILITY=")
  include_directories(${ZSTD_PATH} ${ZSTD_PATH}/common ${ZSTD_PATH}/decompress)
  set(Src_Files  ${Src_Files}
        ${ZSTD_PATH}/common/debug.c
        ${ZSTD_PATH}/common/entropy_common.c
        ${ZSTD_PATH}/common/error_private.c
        ${ZSTD_PATH}/common/fse_decompress.c
        ${ZSTD_PATH}/common/xxhash.c
        ${ZSTD_PATH}/common/zstd_common.c
        ${ZSTD_PATH}/decompress/huf_decompress.c
        ${ZSTD_PATH}/decompress/zstd_ddict.c
        ${ZSTD_PATH}/decompress/zstd_decompress.c
        ${ZSTD_PATH}/decompress/zstd_decompress_block.c )
endif()

if (${BROTLI} EQUAL 0)
else()
  # https://github.com/google/brotli
  set(BROTLI_PATH ${HDP_PATH}/../brotli/c/)
  set(DEF_FLAGS  "${DEF_FLAGS} -D_CompressPlugin_brotli")
  include_directories(${BROTLI_PATH}/include)
  set(Src_Files  ${Src_Files}
        ${BROTLI_PATH}/common/constants.c
        ${BROTLI_PATH}/common/context.c
        ${BROTLI_PATH}/common/dictionary.c
        ${BROTLI_PATH}/common/platform.c
        ${BROTLI_PATH}/common/shared_dictionary.c
        ${BROTLI_PATH}/common/transform.c
        ${BROTLI_PATH}/dec/bit_reader.c
        ${BROTLI_PATH}/dec/decode.c
        ${BROTLI_PATH}/dec/huffman.c
        ${BROTLI_PATH}/dec/state.c )
endif()


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${DEF_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${LINK_FLAGS}")
add_library(hpatchz SHARED hpatchz_oh.c ${Src_Files})
target_link_libraries(hpatchz PUBLIC libace_napi.z.so)
