#include "config/controller_config.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

int main(int argc, char** argv) {
    require(argc == 3, "expected config and URDF paths");
    const fs::path source = argv[1];
    require(controller_config::default_path() == source, "default config path");
    const fs::path dir = fs::temp_directory_path() / "u2canfd-config-test";
    fs::create_directories(dir);
    const fs::path file = dir / "controller.ini";
    const std::string original = [&] {
        std::ifstream in(source);
        return std::string(std::istreambuf_iterator<char>(in), {});
    }();
    auto load_text = [&](const std::string& contents) {
        std::ofstream out(file);
        out << contents;
        out.close();
        return controller_config::load(file);
    };
    auto rejects = [&](const std::string& contents) {
        try { load_text(contents); } catch (const std::exception&) { return; }
        throw std::runtime_error("invalid config was accepted");
    };
    const auto config = load_text(original);
    require(config.urdf_path == (dir / "../model/robot_urdf/robot.urdf").lexically_normal(), "relative URDF path");
    require(config.device_type == DEV_USB2CANFD_DUAL, "device type");
    require(config.serial_number == "24B3E941BE0474C0E833BFF8F3C6EB68", "SN");
    require(config.nominal_baud == 1000000 && config.data_baud == 1000000, "baud rates");
    for (int i = 0; i < 6; ++i) {
        require(config.motors[i].torque_limit_nm == (i < 3 ? 3.0 : 0.0), "motor torque mapping");
        require(std::abs(config.motors[i].kd - (i < 5 ? 0.2 : 0.0)) < 1e-9, "motor kd mapping");
    }
    auto distinct_text = original;
    const double expected_limits[] = {1, 2, 3, 0.4, 0.5, 0.6};
    for (int i = 0; i < 6; ++i) {
        const auto section_start = distinct_text.find("[motor" + std::to_string(i + 1) + "]");
        const auto torque_start = distinct_text.find("torque_limit_nm=", section_start) + 16;
        const auto torque_end = distinct_text.find('\n', torque_start);
        distinct_text.replace(torque_start, torque_end - torque_start, std::to_string(expected_limits[i]));
        const auto kd_start = distinct_text.find("kd=", torque_start) + 3;
        const auto kd_end = distinct_text.find('\n', kd_start);
        distinct_text.replace(kd_start, kd_end - kd_start, std::to_string((i + 1) * 0.1));
    }
    const auto distinct = load_text(distinct_text);
    for (int i = 0; i < 6; ++i) {
        require(std::abs(distinct.motors[i].torque_limit_nm - expected_limits[i]) < 1e-9, "distinct motor torque mapping");
        require(std::abs(distinct.motors[i].kd - ((i + 1) * 0.1)) < 1e-9, "distinct motor kd mapping");
    }
    auto absolute_text = original;
    const std::string relative_urdf = "../model/robot_urdf/robot.urdf";
    absolute_text.replace(absolute_text.find(relative_urdf), relative_urdf.size(), argv[2]);
    const auto custom = load_text(absolute_text);
    require(custom.urdf_path == fs::path(argv[2]), "absolute URDF path");
    rejects("");
    rejects(original + "\n[extra]\nx=1\n");
    rejects(original + "\n[motor1]\nkd=1\n");
    rejects(original + "\nnominal_baud=2\n");
    auto replace = [&](const std::string& key, const std::string& value) {
        auto changed = original;
        const auto at = changed.find(key);
        require(at != std::string::npos, "fixture key missing");
        changed.replace(at, key.size(), value);
        rejects(changed);
    };
    replace("serial_number=24B3E941BE0474C0E833BFF8F3C6EB68", "serial_number=");
    replace("device_type=DEV_USB2CANFD_DUAL", "device_type=invalid");
    replace("nominal_baud=1000000", "nominal_baud=0");
    replace("kd=0.2", "kd=5.1");
    replace("torque_limit_nm=3", "torque_limit_nm=-1");
    replace("torque_limit_nm=3", "torque_limit_nm=11");
    replace("torque_limit_nm=0", "torque_limit_nm=2");
    replace("torque_limit_nm=3", "torque_limit_nm=nan");
    replace("kd=0.2", "kd=-0.1");
    replace("kd=0.2", "kd=inf");
    replace("kd=0.2", "");
    replace("kd=0.2", "damping=0.2");
    replace("kd=0.2", "kd=0.2\nkd=0.3");
    replace("data_baud=1000000", "data_baud=oops");
    replace("serial_number=24B3E941BE0474C0E833BFF8F3C6EB68", "wrong_key=abc");
    fs::remove(file);
    fs::remove(dir);
}
