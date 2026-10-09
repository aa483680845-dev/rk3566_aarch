#include "protocol/damiao.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "Usage: save_zero [motor-id 1-6]\n"
                  << "With no motor ID, saves the current position of motors 1-6 as zero.\n";
        return 0;
    }
    if (argc > 2) {
        std::cerr << "Usage: save_zero [motor-id 1-6]\n";
        return 2;
    }

    std::vector<int> motor_ids;
    if (argc == 1) {
        motor_ids = {1, 2, 3, 4, 5, 6};
    } else {
        try {
            std::size_t parsed = 0;
            const int motor_id = std::stoi(argv[1], &parsed);
            if (parsed != std::string(argv[1]).size() || motor_id < 1 || motor_id > 6) {
                throw std::invalid_argument("motor ID out of range");
            }
            motor_ids.push_back(motor_id);
        } catch (const std::exception&) {
            std::cerr << "Motor ID must be an integer from 1 to 6.\n";
            return 2;
        }
    }

    try {
        std::vector<damiao::DmActData> motors;
        motors.reserve(motor_ids.size());
        for (int motor_id : motor_ids) {
            motors.push_back({
                motor_id <= 3 ? damiao::DM4310 : damiao::DMH3510,
                damiao::MIT_MODE,
                static_cast<uint16_t>(motor_id),
                static_cast<uint16_t>(0x10 + motor_id),
                CHANNEL0
            });
        }
        damiao::Motor_Control control(
            DEV_USB2CANFD_DUAL, 1000000, 1000000,
            "24B3E941BE0474C0E833BFF8F3C6EB68", &motors);
        for (int motor_id : motor_ids) {
            control.set_zero_position(*control.getMotor(CHANNEL0, motor_id));
            std::cout << "Sent save-zero command to motor " << motor_id << ".\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } catch (const std::exception& error) {
        std::cerr << "Failed to save zero: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
