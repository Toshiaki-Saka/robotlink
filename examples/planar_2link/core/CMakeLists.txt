# core/CMakeLists.txt — builds the TLM core as a shared library.
cmake_minimum_required(VERSION 3.16)
project(tlm_core LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

add_library(tlm_core SHARED src/tlm_core.cpp)

target_include_directories(tlm_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_definitions(tlm_core PRIVATE TLM_CORE_BUILD)

if(MSVC)
    target_compile_options(tlm_core PRIVATE /W4 /permissive- /O2)
    target_compile_definitions(tlm_core PUBLIC NOMINMAX)
else()
    target_compile_options(tlm_core PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release>:-O3>
    )
endif()

set_target_properties(tlm_core PROPERTIES
    OUTPUT_NAME tlm_core
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# Regression test, registered with CTest. Guarded to the top-level project so
# frontend builds (which pull core in via add_subdirectory) don't drag it in.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_executable(tlm_core_smoke tools/smoke_test.cpp)
    target_link_libraries(tlm_core_smoke PRIVATE tlm_core)
    add_test(NAME tlm_core_smoke COMMAND tlm_core_smoke)
endif()

install(TARGETS tlm_core
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
install(FILES include/tlm_core.h DESTINATION include)
