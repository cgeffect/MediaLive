
cmake_minimum_required(VERSION 3.0)
project(hello)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNO_CRYPTO")
set(CMAKE_SYSTEM_PROCESSOR "arm64")
set(CMAKE_OSX_ARCHITECTURES "arm64")

file(GLOB rtmp_source *.c)
add_library(rtmp STATIC ${rtmp_source})
