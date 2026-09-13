cmake_minimum_required(VERSION 3.22.1)

project("manekiminer")

add_library(
manekiminer
SHARED
native-lib.cpp
sha256.cpp)

# --- 新增：硬體加速與極限優化設定 ---
# 1. 開啟 O3 最高層級編譯器最佳化
target_compile_options(manekiminer PRIVATE -O3)

# 2. 若為 ARM64 架構 (現代手機)，強制開啟 Crypto 加密硬體指令集
if(${ANDROID_ABI} STREQUAL "arm64-v8a")
target_compile_options(manekiminer PRIVATE -march=armv8-a+crypto)
endif()
# ------------------------------------

find_library(
log-lib
log)

target_link_libraries(
manekiminer
${log-lib})