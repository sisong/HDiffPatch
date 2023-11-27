//copy all files to your native project's C\CPP path
#include "napi\native_api.h"
#include "../../../../../HDiffPatch/builds/node_js_napi/hpatch_napi.c.inc"

EXTERN_C_START
__attribute__((constructor)) void RegisterHPatchzModule(void)
{
    napi_module_register(&hpatchzModule);
}
EXTERN_C_END
