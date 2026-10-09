#include "protocol/damiao.h"
#include "gravity/gravity_compensator.h"
#include "config/controller_config.h"
#include <array>
#include <chrono>
#include <csignal>
#include <mutex>
#include <stdexcept>

// 原子标志，用于安全地跨线程修改
std::atomic<bool> running(true);

// Ctrl+C 触发的信号处理函数
void signalHandler(int signum) {

    running = false;
    //_exit(EXIT_FAILURE);
    std::cerr << "\nInterrupt signal (" << signum << ") received.\n";
}


std::shared_ptr<damiao::Motor_Control> control;
std::shared_ptr<damiao::Motor_Control> control2;
std::mutex m_mutex;
std::array<bool, 5> feedback_received{};

void process_data(std::shared_ptr<damiao::Motor_Control> con, usb_rx_frame_t* frame)
{ 
  static auto uint_to_float = [](uint16_t x, float xmin, float xmax, uint8_t bits) -> float {
        float span = xmax - xmin;
        float data_norm = float(x) / ((1 << bits) - 1);
        float data = data_norm * span + xmin;
        
        return data;
    };
  //frame->head.can_id;这个是mst_id不是can_id
  uint32_t canID =frame->head.can_id;
  uint8_t ch =frame->head.channel;
    
  if( con->getRWSFlag()==true&&   con->getMotorsByChannel(ch)->find(canID) !=  con->getMotorsByChannel(ch)->end())
  {//这是发送保存参数或者写参数或者读参数返回的数据
      if(frame->payload[2]==0x33 || frame->payload[2]==0x55 || frame->payload[2]==0xAA)
      {//发的是读参数或写参数命令，返回对应寄存器参数
          if(frame->payload[2]==0x33 || frame->payload[2]==0x55)
          {//写参数或者读参数返回
                con->receive_param(&frame->payload[0],ch);  
                con->getRWSFlag()=false;
          }
          con->getRWSFlag()=false;            
      }      
  }
  else
  {
      if (frame->head.dlc < 6) return;
      int err = (int(frame->payload[0]) >> 4) & 0x0F;
      //这是正常返回的位置速度力矩数据
      uint16_t q_uint = (uint16_t(frame->payload[1]) << 8) | frame->payload[2];
      uint16_t dq_uint = (uint16_t(frame->payload[3]) << 4) | (frame->payload[4] >> 4);
      uint16_t tau_uint = (uint16_t(frame->payload[4] & 0xf) << 8) | frame->payload[5];

      if(  con->getMotorsByChannel(ch)->find(canID) ==  con->getMotorsByChannel(ch)->end())
      {
          return;
      }
      
      auto m =   con->getMotorsByChannel(ch)->find(canID);
      auto limit_param_receive = m->second->get_limit_param();
      float receive_q = uint_to_float(q_uint, -limit_param_receive.Q_MAX, limit_param_receive.Q_MAX, 16);

      float receive_dq = uint_to_float(dq_uint, -limit_param_receive.DQ_MAX, limit_param_receive.DQ_MAX, 12);
      float receive_tau = uint_to_float(tau_uint, -limit_param_receive.TAU_MAX, limit_param_receive.TAU_MAX, 12);
      m->second->receive_data(receive_q, receive_dq, receive_tau, err); 

      m->second->updateTimeInterval();
      const uint16_t motor_id = m->second->GetCanId();
      if (con == control && ch == CHANNEL0 && motor_id >= 1 && motor_id <= 5 && err == 0) {
          feedback_received[motor_id - 1] = true;
      }
  } 
 
}

void canframeCallback(usb_rx_frame_t* frame)
{ 
  std::lock_guard<std::mutex> lock(m_mutex);
  process_data(control,frame);  
}


void canframeCallback2(usb_rx_frame_t* frame)
{ 
  std::lock_guard<std::mutex> lock(m_mutex);
  process_data(control2,frame);  
}

int main(int argc, char** argv)
{
  using clock = std::chrono::steady_clock;
  using duration = std::chrono::duration<double>;

  std::signal(SIGINT, signalHandler);
  try 
  {
      if (argc != 1 && (argc != 3 || std::string(argv[1]) != "--config")) {
          throw std::invalid_argument("Usage: dm_main [--config <path>]");
      }
      const auto config = controller_config::load(argc == 3 ?
          std::filesystem::path(argv[2]) : controller_config::default_path());
      uint16_t canid1 = 0x01;
      uint16_t mstid1 = 0x11;
      uint16_t canid2 = 0x02;
      uint16_t mstid2 = 0x12;
      uint16_t canid3 = 0x03;
      uint16_t mstid3 = 0x13;
      uint16_t canid4 = 0x04;
      uint16_t mstid4 = 0x14;
      uint16_t canid5 = 0x05;
      uint16_t mstid5 = 0x15;
      uint16_t canid6 = 0x06;
      uint16_t mstid6 = 0x16;
      
      std::vector<damiao::DmActData> init_data;

      init_data.push_back(damiao::DmActData{.motorType = damiao::DM4310,
                                            .mode = damiao::MIT_MODE,
                                            .can_id=canid1,
                                            .mst_id=mstid1,
                                            .channel=CHANNEL0 });

      init_data.push_back(damiao::DmActData{.motorType = damiao::DM4310,
                                            .mode = damiao::MIT_MODE,
                                            .can_id=canid2,
                                            .mst_id=mstid2,
                                            .channel=CHANNEL0 });

      init_data.push_back(damiao::DmActData{.motorType = damiao::DM4310,
                                            .mode = damiao::MIT_MODE,
                                            .can_id=canid3,
                                            .mst_id=mstid3,
                                            .channel=CHANNEL0 });

      init_data.push_back(damiao::DmActData{.motorType = damiao::DMH3510,
                                            .mode = damiao::MIT_MODE,
                                            .can_id=canid4,
                                            .mst_id=mstid4,
                                            .channel=CHANNEL0 });

      init_data.push_back(damiao::DmActData{.motorType = damiao::DMH3510,
                                            .mode = damiao::MIT_MODE,
                                            .can_id=canid5,
                                            .mst_id=mstid5,
                                            .channel=CHANNEL0 });

      init_data.push_back(damiao::DmActData{.motorType = damiao::DMH3510,
                                            .mode = damiao::MIT_MODE,
                                            .can_id=canid6,
                                            .mst_id=mstid6,
                                            .channel=CHANNEL0 });
      
        const std::array<uint16_t, 5> joint_motor_ids{canid1, canid2, canid3, canid4, canid5};
        std::vector<double> joint_limits;
        for (std::size_t i = 0; i < joint_motor_ids.size(); ++i)
            joint_limits.push_back(config.motors[i].torque_limit_nm);
        gravity::GravityCompensator compensator(config.urdf_path.string(), joint_limits);
        if (compensator.dof() != joint_motor_ids.size()) {
            throw std::runtime_error("Robot URDF must have five active joints");
        }
        compensator.require_joint_order({"joint1", "joint2", "joint3", "joint4", "joint5"});
        control = std::make_shared<damiao::Motor_Control>(
            config.device_type, config.nominal_baud, config.data_baud,
            config.serial_number, &init_data);
        //接收回调函数注册
        device_hook_to_rec(control->getUSBHw()->getDeviceHandle(),canframeCallback);

        control->enable_all();//使能该接口下的所有电机
        //control2->enable_all();//使能该接口下的所有电机
      bool compensation_started = false;
      bool feedback_wait_logged = false;
      while (running)
      {
        const duration desired_duration(0.005); // 计算期望周期
        auto current_time = clock::now();
        std::vector<double> joint_positions;
        joint_positions.reserve(joint_motor_ids.size());
        bool feedback_ready = true;
        {
          std::lock_guard<std::mutex> lock(m_mutex);
          for (std::size_t i = 0; i < joint_motor_ids.size(); ++i) {
            // if (control->getMotor(CHANNEL0, joint_motor_ids[i])->Get_Err() != 0) {
            //   throw std::runtime_error("Motor fault reported by motor " +
            //                            std::to_string(joint_motor_ids[i]));
            // }
            if (!feedback_received[i]) {
              feedback_ready = false;
            }
            joint_positions.push_back(control->getMotor(CHANNEL0, joint_motor_ids[i])->Get_Position());
          }
        }
        std::vector<double> gravity_torque(joint_motor_ids.size(), 0.0);
        if (feedback_ready) {
          gravity_torque = compensator.compute(joint_positions);
          if (!compensation_started) {
            std::cerr << "Feedback received from motors 1-5; gravity compensation started.\n";
          }
          compensation_started = true;
        } else if (!feedback_wait_logged) {
          std::cerr << "Waiting for motor feedback; sending zero feedforward torque.\n";
          feedback_wait_logged = true;
        }
        control->control_mit(*control->getMotor(CHANNEL0,canid1), 0.0, config.motors[0].kd, 0.0, 0.0, gravity_torque[0]);
        control->control_mit(*control->getMotor(CHANNEL0,canid2), 0.0, config.motors[1].kd, 0.0, 0.0, gravity_torque[1]);
        control->control_mit(*control->getMotor(CHANNEL0,canid3), 0.0, config.motors[2].kd, 0.0, 0.0, gravity_torque[2]);
        control->control_mit(*control->getMotor(CHANNEL0,canid4), 0.0, config.motors[3].kd, 0.0, 0.0, gravity_torque[3]);
        control->control_mit(*control->getMotor(CHANNEL0,canid5), 0.0, config.motors[4].kd, 0.0, 0.0, gravity_torque[4]);
        control->control_mit(*control->getMotor(CHANNEL0,canid6), 0.0, config.motors[5].kd, 0.0, 0.0, 0.0);

        for(uint16_t id = 1;id<=6;id++)
        {
          float pos=control->getMotor(CHANNEL0,id)->Get_Position();
          float vel=control->getMotor(CHANNEL0,id)->Get_Velocity();
          float tau=control->getMotor(CHANNEL0,id)->Get_tau();
          uint8_t err=control->getMotor(CHANNEL0,id)->Get_Err();
          double time=control->getMotor(CHANNEL0,id)->getTimeInterval();
          std::cerr<<"id is: "<<id<<" pos: "<<pos<<" vel: "<<vel<<" effort: "<<tau<<" err: "<<err<<std::dec<<" time(s): "<<time<<std::endl;
        }
        const auto sleep_till = current_time + std::chrono::duration_cast<clock::duration>(desired_duration);
        std::this_thread::sleep_until(sleep_till);    
      }

      std::cout << "The program exited safely." << std::endl;
  }
  catch (const std::exception& e) {
      std::cerr << "Error: hardware interface exception: " << e.what() << std::endl;
      return 1;
  }

  return 0;
}
