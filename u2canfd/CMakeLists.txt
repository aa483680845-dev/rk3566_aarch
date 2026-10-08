cmake_minimum_required(VERSION 3.16)
project(dm_main LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

execute_process(
        COMMAND "${CMAKE_CXX_COMPILER}" -dumpmachine
        OUTPUT_VARIABLE COMPILER_TARGET
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE COMPILER_TARGET_RESULT
)

if(NOT COMPILER_TARGET_RESULT EQUAL 0)
    message(FATAL_ERROR "Cannot determine compiler target")
elseif(COMPILER_TARGET MATCHES "^aarch64-")
    set(U2CAN_ARCH aarch64)
elseif(COMPILER_TARGET MATCHES "^x86_64-")
    set(U2CAN_ARCH x86_64)
else()
    message(FATAL_ERROR "Unsupported compiler target: ${COMPILER_TARGET}")
endif()

if(U2CAN_ARCH STREQUAL "aarch64")
    set(DM_DEVICE_LIBRARY
            "${CMAKE_CURRENT_SOURCE_DIR}/lib/aarch64/libdm_device.so")
elseif(U2CAN_ARCH STREQUAL "x86_64")
    set(DM_DEVICE_LIBRARY
            "${CMAKE_CURRENT_SOURCE_DIR}/lib/linux/libdm_device.so")
else()
    message(FATAL_ERROR "Unsupported architecture: ${U2CAN_ARCH}")
endif()

set(LIBUSB_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/../lib/libusb/stage-${U2CAN_ARCH}/usr")

if(NOT EXISTS "${LIBUSB_ROOT}/include/libusb-1.0/libusb.h")
    message(FATAL_ERROR "libusb header not found: ${LIBUSB_ROOT}")
endif()
if(NOT EXISTS "${LIBUSB_ROOT}/lib/libusb-1.0.so")
    message(FATAL_ERROR "libusb library not found: ${LIBUSB_ROOT}")
endif()

add_library(project_libusb SHARED IMPORTED)
set_target_properties(project_libusb PROPERTIES
        IMPORTED_LOCATION "${LIBUSB_ROOT}/lib/libusb-1.0.so"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBUSB_ROOT}/include"
)

add_library(dm_device SHARED IMPORTED)
set_target_properties(dm_device PROPERTIES
        IMPORTED_LOCATION "${DM_DEVICE_LIBRARY}"
)

add_executable(dm_main
        src/main.cpp
        src/protocol/damiao.cpp
        src/protocol/usb_class.cpp
)
target_include_directories(dm_main PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib"
)
target_link_libraries(dm_main PRIVATE dm_device project_libusb)

add_executable(dev_sn src/dev_sn.cpp)
target_link_libraries(dev_sn PRIVATE project_libusb)
