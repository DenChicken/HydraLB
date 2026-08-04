cmake_minimum_required(VERSION 4.2)

set(CMAKE_C_COMPILER gcc-16)
set(CMAKE_CXX_COMPILER g++-16)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE INTERNAL "")

project(HydraLB VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
    PUBLIC
        FILE_SET CXX_MODULES FILES
            src/common/network.cppm
            src/common/config.cppm
            src/algorithms/maglev.cppm
            src/dpdk/eal.cppm
            src/dpdk/mempool.cppm
            src/dpdk/packet.cppm
            src/dpdk/ethdev.cppm
            src/dpdk/dpdk.cppm
            src/data/pipeline_node.cppm
            src/data/pipeline.cppm
            src/data/nodes/pcap_ingress.cppm
            src/data/nodes/pcap_egress.cppm
            src/data/nodes/parser.cppm
            src/data/nodes/route.cppm
            src/data/nodes/encap.cppm
            src/data/bootstrap.cppm
            src/data/dispatcher.cppm
            src/data/data.cppm
    PRIVATE
        main.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_MODULE_STD ON)

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

target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::DPDK)
target_include_directories(${PROJECT_NAME} PRIVATE ${DPDK_INCLUDE_DIRS})
