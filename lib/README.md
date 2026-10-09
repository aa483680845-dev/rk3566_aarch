# 第三方依赖与 SDK

`lib/libusb`、`lib/pinocchio` 和 `lib/pinocchio-deps/src` 中的四个依赖是固定提交的 Git 子模块。普通 clone 无需初始化子模块，即可直接使用 `lib/artifacts` 中由主仓库跟踪的两架构完整开发 SDK。

| SDK | x86_64 | AArch64 |
| --- | --- | --- |
| libusb | `artifacts/libusb/stage-x86_64/usr` | `artifacts/libusb/stage-aarch64/usr` |
| Pinocchio 及依赖 | `artifacts/pinocchio/stage-x86_64/usr` | `artifacts/pinocchio/stage-aarch64/usr` |

每个前缀保留头文件、共享库与链接、包元数据；Pinocchio SDK 还包含 CMake 配置和 Eigen、Boost 等开发依赖。CMake 消费端可将相应的 Pinocchio 前缀传入 `CMAKE_PREFIX_PATH`。

需要重建时，先运行 `git submodule update --init --recursive`。libusb 使用 `bash lib/libusb-build.sh x86_64` 或 `bash lib/libusb-build.sh aarch64`；Pinocchio 使用 `bash lib/pinocchio-deps/build.sh x86_64` 或 `bash lib/pinocchio-deps/build.sh aarch64`。脚本把安装结果写回上述前缀。构建步骤见 `pinocchio-deps/BUILDING.md`。
