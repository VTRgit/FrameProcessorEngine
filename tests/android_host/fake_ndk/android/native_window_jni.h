// Host-test stand-in for the NDK header of the same name (subset, same signature).
#pragma once
#include <android/native_window.h>
#include <jni.h>
#ifdef __cplusplus
extern "C" {
#endif
jobject ANativeWindow_toSurface(JNIEnv* env, ANativeWindow* window);
#ifdef __cplusplus
}
#endif
