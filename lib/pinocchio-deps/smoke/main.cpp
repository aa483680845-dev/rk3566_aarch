#include <pinocchio/parsers/urdf.hpp>
#include <pinocchio/algorithm/rnea.hpp>
#include <cmath>
#include <iostream>

int main()
{
    const std::string xml = R"(
<robot name="smoke">
  <link name="base"/>
  <link name="arm">
    <inertial>
      <origin xyz="0 0 0"/><mass value="1"/>
      <inertia ixx="1" ixy="0" ixz="0" iyy="1" iyz="0" izz="1"/>
    </inertial>
  </link>
  <joint name="hinge" type="revolute">
    <parent link="base"/><child link="arm"/><axis xyz="0 0 1"/>
    <limit lower="-3.14" upper="3.14" effort="10" velocity="10"/>
  </joint>
</robot>)";
    pinocchio::Model model;
    pinocchio::urdf::buildModelFromXML(xml, model);
    if (model.nq != 1 || model.nv != 1) return 1;
    pinocchio::Data data(model);
    Eigen::VectorXd q = Eigen::VectorXd::Zero(model.nq);
    Eigen::VectorXd v = Eigen::VectorXd::Zero(model.nv);
    Eigen::VectorXd a = Eigen::VectorXd::Ones(model.nv);
    const Eigen::VectorXd tau = pinocchio::rnea(model, data, q, v, a);
    if (!tau.allFinite() || std::abs(tau[0] - 1.0) > 1e-10) return 2;
    std::cout << "URDF + RNEA passed: nq=" << model.nq << " tau=" << tau[0] << '\n';
}
