# C++ 例程 / u2canfd

## 概述
- 本目录是 C++ 示例入口，面向 USB 转 CANFD 的编译与运行流程。
- 具体步骤已迁移到 [WORKFLOW.md](WORKFLOW.md)。

## 文档 / 资源
- [WORKFLOW.md](WORKFLOW.md)
- [include/](include/)
- [lib/](lib/)
- [src/](src/)
- [CMakeLists.txt](CMakeLists.txt)

## 快速开始
- 先阅读 [WORKFLOW.md](WORKFLOW.md)。

## 保存电机零点（x86_64）
- 构建：`cmake --build cmake-build-release-x86 --target save_zero`
- 将六个电机停在要设为零位的姿态，直接运行 `./cmake-build-release-x86/save_zero`，会依次向 1–6 号电机各发送一次 `0xFE` 保存零点指令。仍可传入 1–6 的单个 `motor-id`，只处理指定电机。
- 工具使用代码中固定的双路 USB-CANFD 设备序列号、CHANNEL0 和 1 Mbit/s 波特率；它不读取 `controller.ini`。发送后不读取电机确认回执，需在设备上核对零点。

## 重力补偿库
- `gravity_compensator` 是独立的共享库，公开头文件为 `include/gravity/gravity_compensator.h`。`dm_main` 的前五轴主循环已接入重力补偿，第六轴维持原指令。
- 创建 `gravity::GravityCompensator(urdf_path, torque_limits_nm)` 时加载固定底座 URDF。限值按 URDF 关节顺序传入，可从对应电机的 `get_limit_param().TAU_MAX` 取得。
- 每个控制周期将同顺序的关节角（rad）传给 `compute(q_rad)`；返回的扭矩（N·m）已按各轴限值限幅，可逐项转换为 `float` 传入 `control_mit` 的最后一个参数。`dof()` 可用于核对模型关节数量。
- 扭矩限值允许设为 0，此时对应关节的重力补偿前馈扭矩为 0；负数和非有限值仍会报错。
- 默认前馈重力补偿扭矩限幅为电机 1–3 各 3 N·m、电机 4–6 各 0 N·m；电机 6 始终发送 0 N·m 前馈扭矩。MIT 的速度阻尼项仍可能额外产生扭矩，这些限值不改变电机内部限幅。
- 模型要求 `nq == nv`，电机角度和扭矩须与 URDF 的关节方向一致。缺少 URDF、输入数量或数值不合法时抛出异常。一个库实例只供一个控制线程调用。
- `dm_main` 每次启动读取可执行文件上一级的 `config/controller.ini`：开发机构建为 `u2canfd/config/controller.ini`，RK3566 部署包为 `u2canfd-rk3566-aarch64/config/controller.ini`。可用 `dm_main --config /path/to/controller.ini` 或 `./run.sh --config /path/to/controller.ini` 指定其他配置；修改后重启程序即可生效，无需重编译。
- 配置的 `[model] urdf_path` 可填绝对路径；相对路径以 INI 所在目录为基准。默认值为 `../model/robot_urdf/robot.urdf`。`[device]` 配置 `device_type`（`DEV_USB2CANFD` 或 `DEV_USB2CANFD_DUAL`）、`serial_number`、`nominal_baud`、`data_baud`；`[motor1]` 至 `[motor6]` 各配置 `torque_limit_nm` 与 `kd`。电机 1–5 分别对应 `joint1`–`joint5`，电机 6 只使用 `kd`。限幅必须在对应电机的 `TAU_MAX` 内，`kd` 范围为 0–5。
- 程序会在连接 USB 前校验全部配置、加载 URDF 并确认五自由度。使能后立即进入主循环，在五轴首次有效反馈齐全前发送零补偿扭矩；此处不检查反馈超时。`save_zero` 不读取此配置。
- RK3566 包带有程序所需的项目共享库和交叉编译器 C++ 运行库，仍依赖设备系统的 glibc 与动态加载器。部署前请核对目标系统的 glibc 版本与交叉编译产物所需版本。

## 状态
- ZH: 主版
- EN: Translation pending
- TBD: 编译与设备检测细节保留在 WORKFLOW.md
