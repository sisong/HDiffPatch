HPATCH_OBJ := \
    libHDiffPatch/HPatch/patch.o

HDIFF_OBJ := \
    libHDiffPatch/HDiff/diff.o \
    libHDiffPatch/HDiff/private_diff/bytes_rle.o \
    libHDiffPatch/HDiff/private_diff/suffix_string.o \
    libHDiffPatch/HDiff/private_diff/compress_detect.o \
    libHDiffPatch/HDiff/private_diff/limit_mem_diff/digest_matcher.o \
    libHDiffPatch/HDiff/private_diff/limit_mem_diff/stream_serialize.o \
    libHDiffPatch/HDiff/private_diff/limit_mem_diff/adler_roll.o \
    libHDiffPatch/HDiff/private_diff/libdivsufsort/divsufsort64.o \
    libHDiffPatch/HDiff/private_diff/libdivsufsort/divsufsort.o \
    $(HPATCH_OBJ)

CFLAGS     += -O3
CXXFLAGS   += -O3

.PHONY: all clean

all: libhdiffpatch.a hdiffz hpatchz unit_test diff_demo patch_demo 

libhdiffpatch.a: $(HDIFF_OBJ) 
	$(AR) rcs $@ $^

hdiffz: 
	c++ hdiffz.cpp libhdiffpatch.a -lbz2 -lz -o hdiffz
hpatchz: 
	cc hpatchz.c $(HPATCH_OBJ) -lbz2 -lz -o hpatchz
unit_test: libhdiffpatch.a
diff_demo: libhdiffpatch.a
patch_demo: $(HPATCH_OBJ)

clean:
	rm -f hdiffz hpatchz unit_test diff_demo patch_demo libhdiffpatch.a $(HDIFF_OBJ)
