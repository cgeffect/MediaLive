# Import installed ZLMediaKit static C API library as target ZLM::mk_api

set(_ZLM_ROOT_DEFAULT "${CMAKE_CURRENT_LIST_DIR}/../install/zlm")
set(ZLM_ROOT "${_ZLM_ROOT_DEFAULT}" CACHE PATH "Path to installed ZLMediaKit static artifacts")

set(_ZLM_LIB "${ZLM_ROOT}/lib/libmk_api.a")
set(_ZLM_LIB_CORE "${ZLM_ROOT}/lib/libzlmediakit.a")
set(_ZLM_LIB_TOOLKIT "${ZLM_ROOT}/lib/libzltoolkit.a")
set(_ZLM_LIB_CODEC "${ZLM_ROOT}/lib/libext-codec.a")
set(_ZLM_LIB_MOV "${ZLM_ROOT}/lib/libmov.a")
set(_ZLM_LIB_MPEG "${ZLM_ROOT}/lib/libmpeg.a")
set(_ZLM_LIB_FLV "${ZLM_ROOT}/lib/libflv.a")
set(_ZLM_LIB_JSONCPP "${ZLM_ROOT}/lib/libjsoncpp.a")
set(_ZLM_INC "${ZLM_ROOT}/include")

if(NOT EXISTS "${_ZLM_LIB}")
  message(FATAL_ERROR "ZLMediaKit static lib not found: ${_ZLM_LIB}. Run render/third-party/build.sh first.")
endif()

if(NOT EXISTS "${_ZLM_INC}")
  message(FATAL_ERROR "ZLMediaKit headers not found: ${_ZLM_INC}")
endif()

find_package(Threads REQUIRED)

add_library(ZLM::mk_api STATIC IMPORTED GLOBAL)
set_target_properties(ZLM::mk_api PROPERTIES
  IMPORTED_LOCATION "${_ZLM_LIB}"
  INTERFACE_INCLUDE_DIRECTORIES "${_ZLM_INC}"
  INTERFACE_LINK_LIBRARIES
    "${_ZLM_LIB_CORE};${_ZLM_LIB_CODEC};${_ZLM_LIB_TOOLKIT};${_ZLM_LIB_MOV};${_ZLM_LIB_MPEG};${_ZLM_LIB_FLV};${_ZLM_LIB_JSONCPP};${_ZLM_LIB_CORE};${_ZLM_LIB_TOOLKIT};Threads::Threads")
