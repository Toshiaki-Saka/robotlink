# RobotLink test suite.
# Each test is a standalone executable registered with CTest. The tests depend
# on the `robotlink` interface library, so the generated dynamics header is
# built automatically before they compile.

set(ROBOTLINK_TESTS
    test_rk4
    test_dynamics
)

foreach(test_name IN LISTS ROBOTLINK_TESTS)
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE robotlink)
    target_include_directories(${test_name} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}")  # for test_harness.hpp

    if(MSVC)
        target_compile_options(${test_name} PRIVATE /W3)
    else()
        target_compile_options(${test_name} PRIVATE -Wall -Wextra)
    endif()

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
