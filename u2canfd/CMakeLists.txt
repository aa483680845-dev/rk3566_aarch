cmake_minimum_required(VERSION 3.16)
project(dm_main LANGUAGES CXX)
include(CTest)

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

# Use the Pinocchio installation built for the selected compiler target.
set(PROJECT_PINOCCHIO_PREFIX
        "${CMAKE_CURRENT_SOURCE_DIR}/../lib/pinocchio/stage-${U2CAN_ARCH}/usr")
if(NOT EXISTS "${PROJECT_PINOCCHIO_PREFIX}/lib/cmake/pinocchio/pinocchioConfig.cmake")
    message(FATAL_ERROR "Pinocchio installation not found: ${PROJECT_PINOCCHIO_PREFIX}")
endif()
list(PREPEND CMAKE_PREFIX_PATH "${PROJECT_PINOCCHIO_PREFIX}")
find_package(pinocchio REQUIRED CONFIG)

add_library(gravity_compensator SHARED src/gravity/gravity_compensator.cpp)
target_include_directories(gravity_compensator PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(gravity_compensator PUBLIC pinocchio::pinocchio)

# libusb 在项目上级目录中按架构分别暂存。
set(LIBUSB_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/../lib/libusb/stage-${U2CAN_ARCH}/usr")

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
        src/config/controller_config.cpp
        src/protocol/damiao.cpp
        src/protocol/usb_class.cpp
)
target_include_directories(dm_main PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib"
        "${PROJECT_PINOCCHIO_PREFIX}/include"
)
target_link_libraries(dm_main PRIVATE dm_device project_libusb gravity_compensator)

# 设备序列号工具只依赖 libusb。
add_executable(dev_sn src/dev_sn.cpp)
target_link_libraries(dev_sn PRIVATE project_libusb)

if(U2CAN_ARCH STREQUAL "x86_64")
    add_executable(save_zero
            src/save_zero.cpp
            src/protocol/damiao.cpp
            src/protocol/usb_class.cpp)
    target_include_directories(save_zero PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/lib")
    target_link_libraries(save_zero PRIVATE dm_device project_libusb)
endif()

if(BUILD_TESTING AND U2CAN_ARCH STREQUAL "x86_64")
    add_executable(controller_config_test
            tests/controller_config_test.cpp src/config/controller_config.cpp)
    target_include_directories(controller_config_test PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${PROJECT_PINOCCHIO_PREFIX}/include")
    add_test(NAME controller_config_test COMMAND controller_config_test
            "${CMAKE_CURRENT_SOURCE_DIR}/config/controller.ini"
            "${CMAKE_CURRENT_SOURCE_DIR}/model/robot_urdf/robot.urdf")
    add_executable(gravity_compensator_test tests/gravity_compensator_test.cpp)
    target_link_libraries(gravity_compensator_test PRIVATE gravity_compensator)
    add_test(NAME gravity_compensator_test COMMAND gravity_compensator_test
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/gravity_one_joint.urdf"
            "${CMAKE_CURRENT_SOURCE_DIR}/model/robot_urdf/robot.urdf")
endif()
