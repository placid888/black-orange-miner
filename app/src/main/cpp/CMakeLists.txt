# 設定 CMake 最小版本要求
cmake_minimum_required(VERSION 3.22.1)

# 設定專案名稱
project("manekiminer")

# 新增動態連結庫 (預留給 SHA-256 與 Stratum 協定)
add_library(
manekiminer
SHARED
native-lib.cpp)

# 連結 Android 內建 Log 函式庫，方便底層除錯
find_library(
log-lib
log)

# 將內建函式庫連結至專案
target_link_libraries(
manekiminer
${log-lib})