#ifndef GRAVITY_COMPENSATOR_H
#define GRAVITY_COMPENSATOR_H

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace gravity {

// Angles and torques use URDF joint order, radians, and N.m.
// A single instance must be called from one control thread at a time.
class GravityCompensator {
public:
    GravityCompensator(const std::string& urdf_path,
                       const std::vector<double>& torque_limits_nm);
    ~GravityCompensator();

    GravityCompensator(const GravityCompensator&) = delete;
    GravityCompensator& operator=(const GravityCompensator&) = delete;

    std::size_t dof() const noexcept;
    void require_joint_order(const std::vector<std::string>& expected) const;
    std::vector<double> compute(const std::vector<double>& q_rad);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gravity

#endif
