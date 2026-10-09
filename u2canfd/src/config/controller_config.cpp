#include "config/controller_config.h"
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace controller_config {
namespace {

using Tree = boost::property_tree::ptree;

const Tree& section(const Tree& tree, const std::string& name,
                    const std::set<std::string>& expected) {
    const auto child = tree.get_child_optional(name);
    if (!child) throw std::invalid_argument("Missing section [" + name + "]");
    if (!child->data().empty()) throw std::invalid_argument("Invalid section [" + name + "]");
    for (const auto& entry : *child) {
        if (!expected.count(entry.first) || !entry.second.empty())
            throw std::invalid_argument("Unknown or invalid field " + name + "." + entry.first);
    }
    for (const auto& key : expected) {
        if (!child->get_optional<std::string>(key))
            throw std::invalid_argument("Missing field " + name + "." + key);
    }
    return *child;
}

std::uint32_t baud(const Tree& tree, const std::string& key) {
    const auto value = tree.get<std::string>(key);
    if (value.empty() || value[0] == '-') throw std::invalid_argument("Invalid " + key);
    std::size_t used = 0;
    try {
        const auto number = std::stoull(value, &used);
        if (used == value.size() && number > 0 && number <= std::numeric_limits<std::uint32_t>::max())
            return static_cast<std::uint32_t>(number);
    } catch (const std::exception&) {}
    throw std::invalid_argument("Invalid " + key);
}

double finite_number(const Tree& tree, const std::string& key, double maximum) {
    const auto value = tree.get<std::string>(key);
    std::size_t used = 0;
    try {
        const auto number = std::stod(value, &used);
        if (used == value.size() && std::isfinite(number) && number >= 0 && number <= maximum)
            return number;
    } catch (const std::exception&) {}
    throw std::invalid_argument("Invalid " + key);
}

} // namespace

std::filesystem::path default_path() {
    return (std::filesystem::read_symlink("/proc/self/exe").parent_path().parent_path()
            / "config/controller.ini").lexically_normal();
}

Config load(const std::filesystem::path& path) {
    Tree tree;
    boost::property_tree::ini_parser::read_ini(path.string(), tree);
    const std::set<std::string> sections{"model", "device", "motor1", "motor2", "motor3", "motor4", "motor5", "motor6"};
    for (const auto& entry : tree) {
        if (!sections.count(entry.first))
            throw std::invalid_argument("Unknown section or root field: " + entry.first);
    }
    const auto& model = section(tree, "model", {"urdf_path"});
    const auto& device = section(tree, "device", {"device_type", "serial_number", "nominal_baud", "data_baud"});
    Config config{};
    const auto urdf = std::filesystem::path(model.get<std::string>("urdf_path"));
    if (urdf.empty()) throw std::invalid_argument("Empty model.urdf_path");
    config.urdf_path = urdf.is_absolute() ? urdf :
        (std::filesystem::absolute(path).parent_path() / urdf).lexically_normal();
    const auto type = device.get<std::string>("device_type");
    if (type == "DEV_USB2CANFD") config.device_type = DEV_USB2CANFD;
    else if (type == "DEV_USB2CANFD_DUAL") config.device_type = DEV_USB2CANFD_DUAL;
    else throw std::invalid_argument("Invalid device.device_type");
    config.serial_number = device.get<std::string>("serial_number");
    if (config.serial_number.find_first_not_of(" \t\r\n") == std::string::npos)
        throw std::invalid_argument("Empty device.serial_number");
    config.nominal_baud = baud(device, "nominal_baud");
    config.data_baud = baud(device, "data_baud");
    for (std::size_t i = 0; i < config.motors.size(); ++i) {
        const auto& motor = section(tree, "motor" + std::to_string(i + 1), {"torque_limit_nm", "kd"});
        // Motors 1-3 are DM4310 (10 N.m); motors 4-6 are DMH3510 (1 N.m).
        config.motors[i] = {finite_number(motor, "torque_limit_nm", i < 3 ? 10.0 : 1.0),
                            finite_number(motor, "kd", 5.0)};
    }
    return config;
}

} // namespace controller_config
