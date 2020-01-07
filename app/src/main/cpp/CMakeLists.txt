cmake_minimum_required(VERSION 3.4.1)

include_directories(${CMAKE_SOURCE_DIR}/include)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${CMAKE_SOURCE_DIR}/../jniLibs/${CMAKE_ANDROID_ARCH_ABI}")


add_library(
        mediacore
        SHARED
        native-lib.cpp
        MediaCore.cpp
        JQueue.cpp)

find_library(
        log-lib
        log)

target_link_libraries(
        mediacore
        android
        avcodec
        avfilter
        avformat
        avutil
        swresample
        swscale
        OpenSLES
        ${log-lib})