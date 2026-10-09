cmake_minimum_required(VERSION 3.16)
project(dm_main LANGUAGES CXX)

# 两个可执行文件统一使用 C++17。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查询当前 C++ 编译器的目标三元组，而不是使用构建主机的架构。
# 这样 CLion 的本机和 aarch64 交叉编译配置可以自动选择各自的依赖库。
execute_process(
        COMMAND "${CMAKE_CXX_COMPILER}" -dumpmachine
        OUTPUT_VARIABLE COMPILER_TARGET
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE COMPILER_TARGET_RESULT
)

# 目前只支持 x86_64 和 aarch64 Linux 目标。
if(NOT COMPILER_TARGET_RESULT EQUAL 0)
    message(FATAL_ERROR "Cannot determine compiler target")
elseif(COMPILER_TARGET MATCHES "^aarch64-")
    set(U2CAN_ARCH aarch64)
elseif(COMPILER_TARGET MATCHES "^x86_64-")
    set(U2CAN_ARCH x86_64)
else()
    message(FATAL_ERROR "Unsupported compiler target: ${COMPILER_TARGET}")
endif()

# 达妙设备库已预编译，按目标架构选择对应的 .so 文件。
if(U2CAN_ARCH STREQUAL "aarch64")
    set(DM_DEVICE_LIBRARY
            "${CMAKE_CURRENT_SOURCE_DIR}/lib/aarch64/libdm_device.so")
elseif(U2CAN_ARCH STREQUAL "x86_64")
    set(DM_DEVICE_LIBRARY
            "${CMAKE_CURRENT_SOURCE_DIR}/lib/linux/libdm_device.so")
else()
    message(FATAL_ERROR "Unsupported architecture: ${U2CAN_ARCH}")
endif()

# libusb SDK 由主仓库管理，按目标架构选择。
set(LIBUSB_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/../lib/artifacts/libusb/stage-${U2CAN_ARCH}/usr")

# 配置阶段检查头文件和链接用的共享库，缺失时立即报错。
if(NOT EXISTS "${LIBUSB_ROOT}/include/libusb-1.0/libusb.h")
    message(FATAL_ERROR "libusb header not found: ${LIBUSB_ROOT}")
endif()
if(NOT EXISTS "${LIBUSB_ROOT}/lib/libusb-1.0.so")
    message(FATAL_ERROR "libusb library not found: ${LIBUSB_ROOT}")
endif()

# 将已有共享库声明为导入目标，供下面的可执行文件链接。
add_library(project_libusb SHARED IMPORTED)
set_target_properties(project_libusb PROPERTIES
        IMPORTED_LOCATION "${LIBUSB_ROOT}/lib/libusb-1.0.so"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBUSB_ROOT}/include"
)

add_library(dm_device SHARED IMPORTED)
set_target_properties(dm_device PROPERTIES
        IMPORTED_LOCATION "${DM_DEVICE_LIBRARY}"
)

# 主程序依赖达妙设备库和 libusb。
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

# 设备序列号工具只依赖 libusb。
add_executable(dev_sn src/dev_sn.cpp)
target_link_libraries(dev_sn PRIVATE project_libusb)
