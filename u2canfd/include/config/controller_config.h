#ifndef CONTROLLER_CONFIG_H
#define CONTROLLER_CONFIG_H

#include <cstddef>
#include "protocol/pub_user.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

namespace controller_config {

struct Motor {
    double torque_limit_nm;
    double kd;
};

struct Config {
    std::filesystem::path urdf_path;
    device_def_t device_type;
    std::string serial_number;
    std::uint32_t nominal_baud;
    std::uint32_t data_baud;
    std::array<Motor, 6> motors;
};

std::filesystem::path default_path();
Config load(const std::filesystem::path& path);

} // namespace controller_config
#endif
