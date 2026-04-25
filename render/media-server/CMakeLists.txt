cmake_minimum_required(VERSION 3.16)
project(embedded_media_server LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include("${CMAKE_CURRENT_SOURCE_DIR}/../third-party/cmake/ZLMStatic.cmake")

add_executable(embedded_server
  src/main.cpp
)

target_link_libraries(embedded_server PRIVATE ZLM::mk_api)
