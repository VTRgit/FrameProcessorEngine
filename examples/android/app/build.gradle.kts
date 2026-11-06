plugins {
    id("com.android.application")
}

android {
    namespace = "com.vtrsoftware.fpe.camerademo"
    compileSdk = 37
    ndkVersion = "30.0.16248370"

    defaultConfig {
        applicationId = "com.vtrsoftware.fpe.camerademo"
        minSdk = 26          // AImageReader_newWithUsage, ANativeWindow_toSurface
        targetSdk = 36
        versionCode = 1
        versionName = "0.2.0"
        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "4.1.2"
        }
    }
}
