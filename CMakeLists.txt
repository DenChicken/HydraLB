cmake_minimum_required(VERSION 4.2)

set(CMAKE_C_COMPILER gcc-16)
set(CMAKE_CXX_COMPILER g++-16)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE INTERNAL "")

project(HydraLB VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(HYDRALB_WARNINGS
    $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:
        -Wall
        -Wextra
        -Werror
        -Wunused-variable
        -Wunused-function
        -Wunused-value
        -Wunused-result
        -Wuninitialized
        -Wshadow
        -Wswitch-default
        -Wswitch-enum
        -Wformat=2
        -Wrestrict
        -Wcast-align
        -Wold-style-cast
        -Woverloaded-virtual
    >
)

add_library(hydralb_core)

target_sources(hydralb_core
    PUBLIC
        FILE_SET CXX_MODULES FILES
            src/common/network.cppm
            src/common/config.cppm
            src/algorithms/maglev.cppm
)

set_target_properties(hydralb_core PROPERTIES COMPILE_WARNING_AS_ERROR ON)
target_compile_options(hydralb_core PRIVATE ${HYDRALB_WARNINGS})
target_compile_features(hydralb_core PUBLIC cxx_std_23)
set_property(TARGET hydralb_core PROPERTY CXX_MODULE_STD ON)

add_library(hydralb_data)

target_sources(hydralb_data
    PUBLIC
        FILE_SET CXX_MODULES FILES
            src/dpdk/eal.cppm
            src/dpdk/mempool.cppm
            src/dpdk/packet.cppm
            src/dpdk/ethdev.cppm
            src/dpdk/dpdk.cppm
            src/data/pipeline_node.cppm
            src/data/pipeline.cppm
            src/data/nodes/pcap_ingress.cppm
            src/data/nodes/pcap_egress.cppm
            src/data/nodes/lb.cppm
            src/data/bootstrap.cppm
            src/data/dispatcher.cppm
            src/data/data.cppm
)

set_target_properties(hydralb_data PROPERTIES COMPILE_WARNING_AS_ERROR ON)
target_compile_options(hydralb_data PRIVATE ${HYDRALB_WARNINGS})
target_compile_features(hydralb_data PUBLIC cxx_std_23)
set_property(TARGET hydralb_data PROPERTY CXX_MODULE_STD ON)
target_link_libraries(hydralb_data PUBLIC hydralb_core)

add_executable(${PROJECT_NAME} main.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_WARNING_AS_ERROR ON)
target_compile_options(${PROJECT_NAME} PRIVATE ${HYDRALB_WARNINGS})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_MODULE_STD ON)
target_link_libraries(${PROJECT_NAME} PRIVATE hydralb_data)


# DPDK

find_package(PkgConfig REQUIRED)

set(DPDK_INSTALL_DIR "${CMAKE_BINARY_DIR}/externals/dpdk-install")

file(GLOB_RECURSE PC_FILE "${DPDK_INSTALL_DIR}/*/libdpdk.pc")

if(NOT PC_FILE)
    message(FATAL_ERROR
        "DPDK not built. Run: bash scripts/build_dpdk.sh"
    )
endif()

get_filename_component(DPDK_PKGCONFIG_DIR "${PC_FILE}" DIRECTORY)
set(ENV{PKG_CONFIG_PATH} "${DPDK_PKGCONFIG_DIR}")
pkg_check_modules(DPDK REQUIRED IMPORTED_TARGET libdpdk)

target_link_libraries(hydralb_data PUBLIC PkgConfig::DPDK)
target_include_directories(hydralb_data SYSTEM PUBLIC ${DPDK_INCLUDE_DIRS})


# Tests

set(GOOGLETEST_DIR "${CMAKE_SOURCE_DIR}/externals/googletest")

if(NOT EXISTS "${GOOGLETEST_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "googletest not initialized. Run: git submodule update --init --recursive"
    )
endif()

enable_testing()

set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory("${GOOGLETEST_DIR}" EXCLUDE_FROM_ALL SYSTEM)

add_executable(hydralb_tests tests/maglev_test.cpp)

set_target_properties(hydralb_tests PROPERTIES COMPILE_WARNING_AS_ERROR ON)
target_compile_options(hydralb_tests PRIVATE ${HYDRALB_WARNINGS})
target_compile_features(hydralb_tests PRIVATE cxx_std_23)
set_property(TARGET hydralb_tests PROPERTY CXX_MODULE_STD ON)
target_link_libraries(hydralb_tests PRIVATE hydralb_core GTest::gtest_main)

add_test(NAME hydralb_tests COMMAND hydralb_tests)


# PCAP copy

file(COPY pcap DESTINATION ${CMAKE_BINARY_DIR})
