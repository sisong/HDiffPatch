APP_PLATFORM := android-14
APP_CFLAGS += -s -Wno-error=format-security
APP_CFLAGS += -fvisibility=hidden -fvisibility-inlines-hidden
APP_CFLAGS += -ffunction-sections -fdata-sections
APP_LDFLAGS += -s -Wl,--gc-sections,--as-needed
APP_LDFLAGS += -Wl,-z,max-page-size=16384
APP_BUILD_SCRIPT := Android.mk
APP_ABI := arm64-v8a armeabi-v7a armeabi x86_64 x86
