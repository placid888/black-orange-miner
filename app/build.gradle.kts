plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.manekiminer.app"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.manekiminer.app"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"
        
        ndk {
            abiFilters.add("arm64-v8a")
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}