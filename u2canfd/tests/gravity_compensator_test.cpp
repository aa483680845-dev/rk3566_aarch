#include "gravity/gravity_compensator.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Exception, typename Function>
void expect_throw(Function function)
{
    bool caught = false;
    try {
        function();
    } catch (const Exception&) {
        caught = true;
    }
    require(caught, "Expected exception was not thrown");
}

int main(int argc, char** argv)
{
    require(argc == 3, "Expected fixture and robot URDF path arguments");
    const std::string urdf_path = argv[1];

    gravity::GravityCompensator compensator(urdf_path, {20.0});
    require(compensator.dof() == 1, "Wrong DoF count");
    require(std::abs(compensator.compute({0.0})[0] + 9.81) < 1e-8,
            "Wrong gravity torque at zero angle");
    require(std::abs(compensator.compute({std::acos(-1.0) / 2})[0]) < 1e-8,
            "Gravity torque should vanish when the arm points down");

    gravity::GravityCompensator limited(urdf_path, {3.0});
    require(std::abs(limited.compute({0.0})[0] + 3.0) < 1e-8,
            "Negative torque was not clipped");
    require(std::abs(limited.compute({std::acos(-1.0)})[0] - 3.0) < 1e-8,
            "Positive torque was not clipped");

    expect_throw<std::invalid_argument>([&] { compensator.compute({}); });
    expect_throw<std::invalid_argument>([&] {
        compensator.compute({std::numeric_limits<double>::quiet_NaN()});
    });
    gravity::GravityCompensator disabled(urdf_path, {0.0});
    require(disabled.compute({0.0})[0] == 0.0,
            "Zero limit must disable negative gravity torque");
    require(disabled.compute({std::acos(-1.0)})[0] == 0.0,
            "Zero limit must disable positive gravity torque");
    expect_throw<std::invalid_argument>([&] {
        gravity::GravityCompensator invalid(urdf_path, {-1.0});
    });
    expect_throw<std::invalid_argument>([&] {
        gravity::GravityCompensator invalid(urdf_path,
            {std::numeric_limits<double>::infinity()});
    });
    expect_throw<std::invalid_argument>([&] {
        gravity::GravityCompensator invalid(urdf_path, {1.0, 1.0});
    });
    expect_throw<std::exception>([&] {
        gravity::GravityCompensator invalid(urdf_path + ".missing", {20.0});
    });

    gravity::GravityCompensator robot(argv[2], {3.0, 3.0, 3.0, 0.1, 0.1});
    require(robot.dof() == 5, "Robot URDF must have five DoF");
    robot.require_joint_order({"joint1", "joint2", "joint3", "joint4", "joint5"});
    expect_throw<std::invalid_argument>([&] {
        robot.require_joint_order({"joint2", "joint1", "joint3", "joint4", "joint5"});
    });
    const auto robot_torque = robot.compute({0.0, 0.0, 0.0, 0.0, 0.0});
    require(robot_torque.size() == 5, "Robot torque count must be five");
    const std::vector<double> limits{3.0, 3.0, 3.0, 0.1, 0.1};
    for (std::size_t i = 0; i < robot_torque.size(); ++i) {
        require(std::isfinite(robot_torque[i]), "Robot torque must be finite");
        require(std::abs(robot_torque[i]) <= limits[i], "Robot torque exceeds motor limit");
    }
}
