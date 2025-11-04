cmake_minimum_required(VERSION 3.16)

project(fujitsu_airstage_uart LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(fujitsu_airstage
    src/packet.cpp
    src/messages.cpp
    src/capture_reader.cpp
    src/register_db.cpp
)

target_include_directories(fujitsu_airstage
    PUBLIC
        include
)

target_compile_features(fujitsu_airstage PUBLIC cxx_std_20)

add_executable(fujitsu_dump
    src/dump_packets.cpp
)

target_link_libraries(fujitsu_dump PRIVATE fujitsu_airstage)

