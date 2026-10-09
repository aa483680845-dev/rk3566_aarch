#include <pinocchio/fwd.hpp>
#include "gravity/gravity_compensator.h"

#include <pinocchio/algorithm/rnea.hpp>
#include <pinocchio/parsers/urdf.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace gravity {
namespace {

pinocchio::Model load_model(const std::string& urdf_path)
{
    if (urdf_path.empty()) {
        throw std::invalid_argument("URDF path must not be empty");
    }
    pinocchio::Model model;
    // 只解析动力学模型；重力计算不需要 URDF 中的视觉或碰撞几何。
    pinocchio::urdf::buildModel(urdf_path, model);
    return model;
}

} // namespace

struct GravityCompensator::Impl {
    Impl(const std::string& urdf_path, const std::vector<double>& limits)
        : model(load_model(urdf_path)), data(model), torque_limits_nm(limits)
    {
        // q 的维度是 nq，输出扭矩的维度是 nv。这个简化接口要求二者相同，
        // 这样调用方可以按 URDF 关节顺序逐项传入角度、取回扭矩。
        if (model.nv == 0 || model.nq != model.nv) {
            throw std::invalid_argument("URDF must describe fixed-base, one-DoF joints with nq == nv");
        }
        if (torque_limits_nm.size() != static_cast<std::size_t>(model.nv)) {
            throw std::invalid_argument("Torque limit count must equal the model DoF count");
        }
        for (double limit : torque_limits_nm) {
            if (!std::isfinite(limit) || limit < 0.0) {
                throw std::invalid_argument("Torque limits must be non-negative and finite");
            }
        }
    }

    pinocchio::Model model;
    // Pinocchio 在 Data 中保存计算中间量；复用它，避免每周期重新创建。
    pinocchio::Data data;
    std::vector<double> torque_limits_nm;
};

GravityCompensator::GravityCompensator(const std::string& urdf_path,
                                       const std::vector<double>& torque_limits_nm)
    : impl_(std::make_unique<Impl>(urdf_path, torque_limits_nm))
{
}

GravityCompensator::~GravityCompensator() = default;

std::size_t GravityCompensator::dof() const noexcept
{
    return static_cast<std::size_t>(impl_->model.nv);
}

void GravityCompensator::require_joint_order(const std::vector<std::string>& expected) const
{
    const auto& names = impl_->model.names;
    if (names.size() != expected.size() + 1 ||
        !std::equal(expected.begin(), expected.end(), names.begin() + 1)) {
        throw std::invalid_argument("URDF active joint order does not match motor 1-5");
    }
}

std::vector<double> GravityCompensator::compute(const std::vector<double>& q_rad)
{
    if (q_rad.size() != static_cast<std::size_t>(impl_->model.nq)) {
        throw std::invalid_argument("Joint position count must equal the model nq");
    }
    for (double q : q_rad) {
        if (!std::isfinite(q)) {
            throw std::invalid_argument("Joint positions must be finite");
        }
    }

    // Map 直接引用输入数组，避免将每周期的关节角再复制到 Eigen 向量。
    const Eigen::Map<const Eigen::VectorXd> q(q_rad.data(), q_rad.size());
    // g(q) 是维持当前姿态所需的广义重力扭矩；无需速度和加速度输入。
    const auto& gravity_torque =
        pinocchio::computeGeneralizedGravity(impl_->model, impl_->data, q);
    std::vector<double> torque(impl_->torque_limits_nm.size());
    for (std::size_t i = 0; i < torque.size(); ++i) {
        const double value = gravity_torque[static_cast<Eigen::Index>(i)];
        if (!std::isfinite(value)) {
            throw std::runtime_error("Pinocchio returned a non-finite gravity torque");
        }
        const double limit = impl_->torque_limits_nm[i];
        // MIT 编码函数本身不做限幅，先限制到对应电机可发送的扭矩范围。
        torque[i] = std::clamp(value, -limit, limit);
    }
    return torque;
}

} // namespace gravity
