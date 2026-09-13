android {
    namespace = "com.manekiminer.app"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.manekiminer.app"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"
        
        // 鎖定 CPU 架構 (Poco M5 支援 arm64-v8a)
        ndk {
            abiFilters.add("arm64-v8a")
        }
    }

    // 連結 CMakeLists.txt
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}