# Pinocchio 双架构构建

构建原始 Pinocchio 源码提交 `f3d7c0e25`（`v4.1.0-314-gf3d7c0e25`），Release、C++17、共享库、URDF 支持。Python 绑定、碰撞检测、示例和完整单元测试关闭。

## 安装目录

- x86_64：`../pinocchio/stage-x86_64/usr`
- AArch64：`../pinocchio/stage-aarch64/usr`

每个前缀包含 `include`、`lib` 和 CMake 包配置；主要库是 `libpinocchio_default.so` 和 `libpinocchio_parsers.so`。依赖也安装在同一前缀内。

依赖版本：Boost 1.83.0、Eigen 3.4.0（本机已安装的纯头文件库）、urdfdom 4.0.1、urdfdom_headers 1.1.2、console_bridge 1.0.2、tinyxml2 10.0.0。`src` 保存下载的依赖源码。Pinocchio 的 `cmake` 子模块固定在 `648efca10ffc9081c633e5d23d8b069e89eee341`。

## 重新构建

在项目根目录运行：

```bash
bash lib/pinocchio-deps/build.sh x86_64
bash lib/pinocchio-deps/build.sh aarch64
```

默认每种架构使用两个编译任务；可用 `PINOCCHIO_BUILD_JOBS` 调整。脚本复用现有源码和 Boost 的 `b2`，不需要重新下载。x86_64 使用 `/usr/bin/g++-13`，AArch64 使用 `aarch64-toolchain.cmake` 指定的 Arm GNU 13.3 工具链。

## 在 CMake 项目中使用

配置项目时，将对应架构的安装前缀传入 `CMAKE_PREFIX_PATH`，然后：

```cmake
find_package(pinocchio REQUIRED CONFIG)
target_link_libraries(your_target PRIVATE pinocchio::pinocchio)
```

`smoke` 是独立的消费端示例：读取单关节 URDF，并验证 RNEA 的计算结果。构建 AArch64 消费端时也必须使用交叉工具链。

部署 RK3566 时需要一起提供 `stage-aarch64/usr/lib` 中的运行库，以及与工具链兼容的 C++ 运行库。必要时将该目录加入 `LD_LIBRARY_PATH`。头文件及 `lib/cmake` 用于开发和链接，不是运行时必需文件。

## 本次验证（2026-10-09）

- 两种架构均完成 Release 编译、安装，以及独立 CMake 消费端的编译链接。
- x86_64 实际运行 `smoke-build-x86_64/pinocchio_smoke`，输出 `URDF + RNEA passed: nq=1 tau=1`。
- 每套安装目录中的 13 个实际共享库均核对了 ELF 架构，所需的非系统共享库全部存在于对应安装目录。
- AArch64 消费端在 `smoke-build-aarch64/pinocchio_smoke`，尚未在 RK3566 实机运行。
