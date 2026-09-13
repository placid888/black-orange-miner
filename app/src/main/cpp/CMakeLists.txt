cmake_minimum_required(VERSION 3.22.1)

project("manekiminer")

add_library(
manekiminer
SHARED
native-lib.cpp
sha256.cpp)

find_library(
log-lib
log)

target_link_libraries(
manekiminer
${log-lib})