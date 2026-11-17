cmake_minimum_required(VERSION 3.21)

project(Winux
    VERSION 0.1.0
    DESCRIPTION "Experimental native Windows host runtime for a future user-mode Linux kernel port"
    LANGUAGES C CXX
)

if(NOT WIN32)
    message(FATAL_ERROR "The current Winux prototype targets native Windows only.")
endif()

if(NOT MSVC)
    message(FATAL_ERROR "The current SEH fault boundary requires the Microsoft C compiler (MSVC).")
endif()

option(WINUX_BUILD_TESTS "Build Winux runtime tests" ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(winux_host STATIC
    src/host/win32_platform.c
    src/host/fault_dispatcher.c
    src/runtime/runtime.c
)

target_include_directories(winux_host
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/host
)

target_compile_definitions(winux_host PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if(MSVC)
    target_compile_options(winux_host PRIVATE /W4)
endif()

add_executable(winux
    src/cli/main.cpp
    src/integration/runtime_controller.cpp
)

target_include_directories(winux PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/integration
)

target_link_libraries(winux PRIVATE winux_host)

if(MSVC)
    target_compile_options(winux PRIVATE /W4 /permissive-)
endif()

install(TARGETS winux RUNTIME DESTINATION .)

if(WINUX_BUILD_TESTS)
    enable_testing()

    add_executable(winux_runtime_tests
        tests/runtime_lifecycle.cpp
    )

    target_link_libraries(winux_runtime_tests PRIVATE winux_host)
    target_include_directories(winux_runtime_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/host
    )

    if(MSVC)
        target_compile_options(winux_runtime_tests PRIVATE /W4 /permissive-)
    endif()

    add_test(NAME runtime_lifecycle COMMAND winux_runtime_tests)
endif()
