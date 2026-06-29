cmake_minimum_required(VERSION 3.20)
project(RobotLink
    VERSION 0.1.0
    DESCRIPTION "Symbolic dynamics derivation, C++ code generation, and simulation of a 3-DOF robot arm"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to a Release build if the user did not specify one (single-config generators).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(ROBOTLINK_BUILD_TESTS "Build the RobotLink test suite" ON)

# ── Python (for build-time dynamics code generation) ─────────────────────────
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(GENERATED_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/generated")
set(GENERATED_HPP  "${GENERATED_DIR}/dynamics_generated.hpp")
set(DERIVE_SCRIPT  "${CMAKE_CURRENT_SOURCE_DIR}/python/derive_and_export.py")

file(MAKE_DIRECTORY "${GENERATED_DIR}")

add_custom_command(
    OUTPUT  "${GENERATED_HPP}"
    COMMAND "${Python3_EXECUTABLE}" "${DERIVE_SCRIPT}" --output "${GENERATED_HPP}"
    DEPENDS "${DERIVE_SCRIPT}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Deriving robot dynamics and generating C++ header (may take ~1-3 min on first build)..."
    VERBATIM
)
add_custom_target(generate_dynamics DEPENDS "${GENERATED_HPP}")

# ── Eigen ────────────────────────────────────────────────────────────────────
# By default Eigen 3.4 is fetched automatically. To build offline or against a
# system install, configure with:  -DROBOTLINK_USE_SYSTEM_EIGEN=ON
option(ROBOTLINK_USE_SYSTEM_EIGEN "Use an installed Eigen3 instead of FetchContent" OFF)

if(ROBOTLINK_USE_SYSTEM_EIGEN)
    find_package(Eigen3 3.4 REQUIRED NO_MODULE)
else()
    include(FetchContent)
    FetchContent_Declare(
        eigen
        GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
        GIT_TAG        3.4.0
        GIT_SHALLOW    TRUE
    )
    set(EIGEN_BUILD_DOC        OFF CACHE BOOL "" FORCE)
    set(EIGEN_BUILD_PKGCONFIG  OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING          OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(eigen)
endif()

# ── robotlink: header-only interface library (shared by sim and tests) ───────
add_library(robotlink INTERFACE)
add_dependencies(robotlink generate_dynamics)
target_include_directories(robotlink INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${GENERATED_DIR}"
)
target_link_libraries(robotlink INTERFACE Eigen3::Eigen)
target_compile_features(robotlink INTERFACE cxx_std_20)

# ── Simulation executable ────────────────────────────────────────────────────
add_executable(robot_sim src/main.cpp)
target_link_libraries(robot_sim PRIVATE robotlink)
target_compile_definitions(robot_sim PRIVATE
    OUTPUT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/output"
)

if(MSVC)
    target_compile_options(robot_sim PRIVATE /W3 /O2)
else()
    target_compile_options(robot_sim PRIVATE -Wall -Wextra -O2)
endif()

# ── Absorbed example: planar 2-link arm (former `tlm` repo) ──────────────────
# Self-contained C ABI core (no Eigen / no generated dynamics), built from this
# root so the shared GUI launcher and the cross-example test can find it.
option(ROBOTLINK_BUILD_PLANAR_2LINK "Build the absorbed planar 2-link arm example" ON)
if(ROBOTLINK_BUILD_PLANAR_2LINK)
    add_subdirectory(examples/planar_2link/core)
endif()

# ── Tests ────────────────────────────────────────────────────────────────────
if(ROBOTLINK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)

    if(ROBOTLINK_BUILD_PLANAR_2LINK)
        add_test(NAME planar_2link_python
                 COMMAND ${Python3_EXECUTABLE}
                         ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_planar_2link.py)
        if(WIN32)
            set_tests_properties(planar_2link_python PROPERTIES
                ENVIRONMENT "PATH=$<TARGET_FILE_DIR:tlm_core>;$ENV{PATH}")
        endif()
    endif()
endif()

# ── Install ──────────────────────────────────────────────────────────────────
include(GNUInstallDirs)
install(TARGETS robot_sim RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# ── Summary ──────────────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "RobotLink ${PROJECT_VERSION} configured.")
message(STATUS "  Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Tests      : ${ROBOTLINK_BUILD_TESTS}")
message(STATUS "Build steps:")
message(STATUS "  1. cmake -B build -S .")
message(STATUS "  2. cmake --build build         (generates dynamics header first)")
message(STATUS "  3. ctest --test-dir build      (run the test suite)")
message(STATUS "  4. ./build/robot_sim           (writes output/sim_results.csv)")
message(STATUS "  5. python python/visualizer.py")
message(STATUS "")
