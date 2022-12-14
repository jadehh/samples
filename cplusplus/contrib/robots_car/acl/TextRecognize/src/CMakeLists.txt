# Copyright (c) Huawei Technologies Co., Ltd. 2019. All rights reserved.

# CMake lowest version requirement
cmake_minimum_required(VERSION 3.5.1)

# project information
project(TextRecognize)

# Compile options
add_compile_options(-std=c++11)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "../../../out")
set(CMAKE_CXX_FLAGS_DEBUG "-fPIC -O0 -g -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-fPIC -O2 -Wall")

add_definitions(-DENABLE_DVPP_INTERFACE)
set(INC_PATH $ENV{DDK_PATH})

if (NOT DEFINED ENV{DDK_PATH})
    set(INC_PATH "/usr/local/Ascend")
    message(STATUS "set default INC_PATH: ${INC_PATH}")
else ()
    message(STATUS "env INC_PATH: ${INC_PATH}")
endif()

set(LIB_PATH $ENV{NPU_HOST_LIB})

if (NOT DEFINED ENV{NPU_HOST_LIB})
    set(LIB_PATH "/usr/local/Ascend/runtime/lib64/stub/")
    message(STATUS "set default LIB_PATH: ${LIB_PATH}")
else ()
    message(STATUS "env LIB_PATH: ${LIB_PATH}")
endif()

# Header path
include_directories(
    $ENV{HOME}/ascend_ddk/arm/include/
    $ENV{HOME}/ascend_ddk/arm/include/opencv4
    ${INC_PATH}/runtime/include/
    ../inc/

    $ENV{HOME}/ascend_ddk/arm/ros/melodic/include     #ROS include
    $ENV{HOME}/ascend_ddk/arm/usr/include             #opencv 3.2.0 include
)

if(target STREQUAL "Simulator_Function")
    add_compile_options(-DFUNC_SIM)
endif()

# add host lib path
link_directories(
    ${LIB_PATH}
    $ENV{HOME}/ascend_ddk/arm/lib/
    ${INC_PATH}/compiler/lib64

    $ENV{HOME}/ascend_ddk/arm/ros/melodic/lib   #ROS lib
    /usr/lib/aarch64-linux-gnu                  #OpenCV lib etc.
)

add_executable(main
        utils.cpp
        model_process.cpp
        PracticalSocket.cpp
        clipper.cpp
        text_recognize.cpp
        dvpp_process.cpp
        dvpp_resize.cpp
        dvpp_jpegd.cpp
        main.cpp)

if(target STREQUAL "Simulator_Function")
    target_link_libraries(main funcsim jsoncpp)
else()
    target_link_libraries(main ascendcl acl_dvpp stdc++ opencv_core opencv_imgproc opencv_imgcodecs dl rt jsoncpp

                #ROS lib
                libcv_bridge.so
                libimage_transport.so
                libroscpp.so  librosconsole.so  libroscpp_serialization.so librostime.so                     #ROS
                libxmlrpcpp.so libcpp_common.so librosconsole_log4cxx.so librosconsole_backend_interface.so  #ROS
                libmessage_filters.so libclass_loader.so libroslib.so librospack.so
                )
endif()

install(TARGETS main DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
