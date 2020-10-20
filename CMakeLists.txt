cmake_minimum_required(VERSION 3.0.0)
project(gs VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-D__STDC_CONSTANT_MACROS")

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

set(GST_INC_DIR "/usr/include/gstreamer-1.0")
set(SYS_INC_DIR "/usr/include")

set(GST_LIB_DIR "/usr/lib/x86_64-linux-gnu/gstreamer-1.0")
set(SYS_LIB_DIR "/usr/lib/x86_64-linux-gnu")

include_directories(
    ${GST_INC_DIR}
    ${SYS_INC_DIR}
    "/usr/include/glib-2.0"
    "/usr/lib/x86_64-linux-gnu/glib-2.0/include"
    ${PROJECT_SOURCE_DIR}/inc
)

link_directories(
    ${GST_LIB_DIR}
    ${SYS_LIB_DIR}
)

aux_source_directory(${PROJECT_SOURCE_DIR}/src DIR_SRCS)
add_executable(
    gs
    main.cpp
    ${DIR_SRCS}
)

target_link_libraries(
    gs
    -lpthread
    -lgstreamer-1.0
    -lgobject-2.0
    -lglib-2.0
)
