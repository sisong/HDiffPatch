HDIFF_OBJ := \
    libHDiffPatch/HDiff/diff.o \
    libHDiffPatch/HDiff/private_diff/bytes_rle.o \
    libHDiffPatch/HDiff/private_diff/suffix_string.o \
    libHDiffPatch/HDiff/private_diff/libdivsufsort/divsufsort64.o \
    libHDiffPatch/HDiff/private_diff/libdivsufsort/divsufsort.o

HPATCH_OBJ := \
    libHDiffPatch/HPatch/patch.o

CFLAGS      += -Wall -Werror -O3
CXXFLAGS    += -Wall -Werror -O3

.PHONY: all clean

all: libhdiffpatch.a diff_demo patch_demo unit_test

libhdiffpatch.a: $(HDIFF_OBJ) $(HPATCH_OBJ)
	$(AR) rcs $@ $^

diff_demo: libhdiffpatch.a
patch_demo: $(HPATCH_OBJ)
unit_test: libhdiffpatch.a

clean:
	rm -f diff_demo patch_demo unit_test libhdiffpatch.a $(HDIFF_OBJ) $(HPATCH_OBJ)
