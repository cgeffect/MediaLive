cmake_minimum_required(VERSION 3.16)
project(encoder_pusher LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(SWSCALE REQUIRED IMPORTED_TARGET libswscale)

add_executable(encoder_pusher
    src/app/main.cpp
    src/core/task_config.cpp
    src/core/task_manager.cpp
    src/pipeline/frame_renderer.cpp
    src/pipeline/frame_render_pusher.cpp
    src/pipeline/pipeline_context.cpp
    src/pipeline/ffmpeg_utils.cpp
    src/pipeline/input_initializer.cpp
    src/pipeline/output_initializer.cpp
    src/pipeline/video_processor.cpp
    src/pipeline/audio_processor.cpp
)

target_include_directories(encoder_pusher PRIVATE
    src/core
    src/pipeline
)

target_link_libraries(encoder_pusher PRIVATE
    PkgConfig::AVFORMAT
    PkgConfig::AVCODEC
    PkgConfig::AVUTIL
    PkgConfig::SWSCALE
)
