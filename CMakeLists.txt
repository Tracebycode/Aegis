cmake_minimum_required(VERSION 3.20)
project(Aegis VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler warning flags for Aegis targets
set(AEGIS_COMPILE_OPTIONS)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(AEGIS_COMPILE_OPTIONS
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -O3
    )
endif()

# Sanitizer options
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_BENCHMARKS "Build benchmark suite" ON)

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
endif()

if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -g)
    add_link_options(-fsanitize=thread)
endif()

include(GNUInstallDirs)
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Aegis core library
set(AEGIS_SOURCES
    src/common/time_utils.cpp
    src/common/hash.cpp
    src/storage/value.cpp
    src/storage/shard.cpp
    src/storage/sharded_hash_map.cpp
    src/protocol/command.cpp
    src/protocol/parser.cpp
    src/protocol/serializer.cpp
    src/ttl/ttl_manager.cpp
    src/persistence/aof_logger.cpp
    src/persistence/snapshot.cpp
    src/net/socket.cpp
    src/net/connection.cpp
    src/net/epoll_reactor.cpp
    src/net/server.cpp
)

add_library(aegis_core STATIC ${AEGIS_SOURCES})
target_include_directories(aegis_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(aegis_core PUBLIC Threads::Threads)
target_compile_options(aegis_core PRIVATE ${AEGIS_COMPILE_OPTIONS})

# Aegis server executable
add_executable(aegis_server src/main.cpp)
target_link_libraries(aegis_server PRIVATE aegis_core)
target_compile_options(aegis_server PRIVATE ${AEGIS_COMPILE_OPTIONS})

# Tests and Benchmarks via FetchContent
include(FetchContent)

if(BUILD_TESTS)
    enable_testing()
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(aegis_unit_tests
        tests/unit/storage_test.cpp
        tests/unit/protocol_test.cpp
        tests/unit/ttl_test.cpp
        tests/unit/persistence_test.cpp
        tests/unit/net_test.cpp
    )
    target_link_libraries(aegis_unit_tests PRIVATE aegis_core GTest::gtest_main)

    add_executable(aegis_integration_tests
        tests/integration/server_test.cpp
    )
    target_link_libraries(aegis_integration_tests PRIVATE aegis_core GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(aegis_unit_tests)
    gtest_discover_tests(aegis_integration_tests)
endif()

if(BUILD_BENCHMARKS)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)

    add_executable(storage_benchmark benchmarks/storage_benchmark.cpp)
    target_link_libraries(storage_benchmark PRIVATE aegis_core benchmark::benchmark)

    add_executable(protocol_benchmark benchmarks/protocol_benchmark.cpp)
    target_link_libraries(protocol_benchmark PRIVATE aegis_core benchmark::benchmark)
endif()
