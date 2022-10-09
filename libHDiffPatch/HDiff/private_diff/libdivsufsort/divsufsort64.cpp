#define BUILD_DIVSUFSORT64
#define HAVE_CONFIG_H 1
# include "divsufsort64.h"
typedef saidx64_t saidx_t;
typedef saidx_t sastore_t;
# define divsufsort divsufsort64
# define divsufsort_version divsufsort64_version
# define sssort sssort64
# define trsort trsort64

#include "divsufsort_private.h"
#include "divsufsort.c.inc.h"
#include "sssort.c.inc.h"
#include "trsort.c.inc.h"
