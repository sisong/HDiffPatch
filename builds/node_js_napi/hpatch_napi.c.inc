// hpatch_napi.c.inc
// Created by sisong on 2023-11-19.

//1. #include "napi/native_api.h" or #include <node_api.h> ?
//2. #inlcude "hpatch_napi.c.inc" or copy code to your C\C++ file
//3. run napi_module_register(&hpatchzModule); or NAPI_MODULE(hpatchz,hpatchzInit); or NODE_API_MODULE(hpatchz,hpatchzInit); ?

#include "../../builds/android_ndk_jni_mk/hpatch.h"
#include "../../libHDiffPatch/HPatch/patch_types.h"
#include "../../file_for_patch.h"
#ifdef __cplusplus
extern "C" {
#endif
    #define HPATCH_OPTIONS_ERROR 1
    #define _check_(v)  do { if (!(v)) { result=HPATCH_OPTIONS_ERROR; goto _clear; }; } while(0)
    #define _check_status(st)  _check_((st)==napi_ok)
    #define __j2cstr_(jstr,cstr) do { size_t _n; _check_status(napi_get_value_string_utf8(env,jstr,cstr,sizeof(cstr),&_n)); } while(0)
    #define _check_j2cstr(jstr,cstr)  do { _check_(jstr); __j2cstr_(jstr,cstr); } while(0)
    #define _check_jn2cstr(jstr,cstr) do { if (jstr) __j2cstr_(jstr,cstr); else *cstr=0; } while(0)

    //patch:(oldFileName: String, diffFileName: String, newFileName: String, cacheMemory?: number) => number;
    static napi_value NodeJs_napi_com_github_sisong_hpatchz(napi_env env, napi_callback_info info){
        napi_value args[4]={0};
        int  result=0;
        int64_t cacheMemory;
        {
            size_t  argc=4;
            _check_status(napi_get_cb_info(env,info,&argc,args,0,0));

            if (argc==3)
                cacheMemory=-1;
            else if (argc==4)
                _check_status(napi_get_value_int64(env,args[3],&cacheMemory));
            else
                _check_(hpatch_FALSE);
        }
        {
            char cOldFileName   [hpatch_kPathMaxSize];
            char cDiffFileName  [hpatch_kPathMaxSize];
            char cOutNewFileName[hpatch_kPathMaxSize];

            _check_jn2cstr(args[0],cOldFileName);
            _check_j2cstr(args[1],cDiffFileName);
            _check_j2cstr(args[2],cOutNewFileName);
            result=hpatchz(cOldFileName,cDiffFileName,cOutNewFileName,cacheMemory);
        }
    _clear:
        {
            napi_value jsResult;
            _check_status(napi_create_int32(env,result,&jsResult));
            return jsResult;
        }
    }

static napi_value hpatchzInit(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        { "patch", 0, NodeJs_napi_com_github_sisong_hpatchz, 0, 0, 0, napi_default, 0 }
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}

static napi_module hpatchzModule = {
    .nm_version = HDIFFPATCH_VERSION_NUMBER,
    .nm_flags = 0,
    .nm_filename = 0,
    .nm_register_func = hpatchzInit,
    .nm_modname = "hpatchz",
    .nm_priv = ((void*)0),
    .reserved = { 0 },
};

#ifdef __cplusplus
}
#endif

