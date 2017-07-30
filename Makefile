HDIFF_OBJ := \
    libHDiffPatch/HDiff/diff.o \
    libHDiffPatch/HDiff/private_diff/bytes_rle.o \
    libHDiffPatch/HDiff/private_diff/suffix_string.o \
    libHDiffPatch/HDiff/private_diff/compress_detect.o \
    libHDiffPatch/HDiff/private_diff/libdivsufsort/divsufsort64.o \
    libHDiffPatch/HDiff/private_diff/libdivsufsort/divsufsort.o

HPATCH_OBJ := \
    libHDiffPatch/HPatch/patch.o

CFLAGS      += -O3 -lbz2  -lz
CXXFLAGS   += -O3 -lbz2 -lz

.PHONY: all clean

all: libhdiffpatch.a hdiffz hpatchz unit_test diff_demo patch_demo 

libhdiffpatch.a: $(HDIFF_OBJ) $(HPATCH_OBJ)
	$(AR) rcs $@ $^

hdiffz: libhdiffpatch.a
hpatchz: $(HPATCH_OBJ)
unit_test: libhdiffpatch.a
diff_demo: libhdiffpatch.a
patch_demo: $(HPATCH_OBJ)

clean:
	rm -f hdiffz hpatchz unit_test diff_demo patch_demo libhdiffpatch.a $(HDIFF_OBJ) $(HPATCH_OBJ)
