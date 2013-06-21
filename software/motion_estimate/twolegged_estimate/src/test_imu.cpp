/*
 * test_imu.cpp
 *
 *  Created on: May 24, 2013
 *      Author: drc
 */
#include <iostream>
#include <csignal>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/assign/std/vector.hpp>
#include <lcm/lcm-cpp.hpp>
#include <lcm/lcm.h>
#include "lcmtypes/drc_lcmtypes.hpp"
#include <model-client/model-client.hpp>
#include <lcmtypes/bot_core.hpp>
#include <Eigen/Dense>
#include "QuaternionLib.h"

#define PI  3.14159265358979323


using namespace std;
using namespace InertialOdometry;

void signalHandler( int signum ){
  cout << "Interrupt signal (" << signum << ") received.\n";

  // cleanup and close up
  try{
    cout << "Exiting.\n";
  } catch (std::exception &e){
    std::cout << "Exception occurred during close out\n";
  }
  // terminate program

  exit(signum);

}



class HandleIMU {
private:
	boost::shared_ptr<lcm::LCM> _lcm;

	Eigen::Quaterniond imu_orientation;
	double max;
	unsigned long long prev_msg_utime;
	unsigned long long fall_utime;
	

public:
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW

	HandleIMU(boost::shared_ptr<lcm::LCM> &_lcm) : _lcm(_lcm) {
		cout << "New handler\n";
		max = 0;
		_lcm->subscribe("TRUE_ROBOT_STATE",&HandleIMU::true_state_handler,this);
		_lcm->subscribe("TORSO_IMU",&HandleIMU::torso_state_handler,this);
		_lcm->subscribe("HEAD_IMU",&HandleIMU::head_imu_handler,this);

		prev_msg_utime = 0;
		fall_utime = 0;

	}

	void true_state_handler(const lcm::ReceiveBuffer* rbuf,
							const std::string& channel,
							const  drc::robot_state_t* msg) {

		Eigen::Quaterniond tp_q(msg->origin_position.rotation.w,
								msg->origin_position.rotation.x,
								msg->origin_position.rotation.y,
								msg->origin_position.rotation.z);


		//cout << "" << (msg->utime - prev_msg_utime)/1000. << endl;

		prev_msg_utime = msg->utime;

		Eigen::Vector3d err_angles;

		Eigen::Matrix3d C;
		C = q2C(tp_q);

		err_angles = C2e(C) - q2e_new(imu_orientation);

		if (err_angles.norm()>max) {
			max = err_angles.norm();
		}

		if (err_angles.norm() > 1E-2) {
			//cout << fixed << max*57.29 << " | " << err_angles.norm()*57.29 << endl;
		}
		//cout << "Receiving state\n";

		//cout << "num_joints: " << msg->num_joints << endl;




		if (false) {
			// This is for printing the feet and had forces and torques to screen
			std::cout << "vals, " << msg->utime << ", ";

			for (int i=0;i<msg->contacts.num_contacts;i++) {
				std::cout << msg->contacts.contact_force[i].x << ", " << msg->contacts.contact_force[i].y << ", " << msg->contacts.contact_force[i].z << ", ";
			}

			for (int i=0;i<msg->contacts.num_contacts;i++) {
				std::cout << msg->contacts.contact_torque[i].x << ", " << msg->contacts.contact_torque[i].y << ", " << msg->contacts.contact_torque[i].z << ", ";
			}

			std::cout << std::endl;

		}



	}

	void head_imu_handler(const lcm::ReceiveBuffer* rbuf, const std::string& channel, const  drc::imu_t* msg) {

		std::cout << "himu, " << msg->utime << ", ";
		std::cout << msg->linear_acceleration[0] << ", "  << msg->linear_acceleration[1] << ", " << msg->linear_acceleration[2] << std::endl;

	}

	void torso_state_handler(const lcm::ReceiveBuffer* rbuf, const std::string& channel, const  drc::imu_t* msg) {

		Eigen::Quaterniond q(msg->orientation[0],msg->orientation[1],msg->orientation[2],msg->orientation[3]);

		imu_orientation = q;

		Eigen::Vector4d check_conv;
		Eigen::Quaterniond __q;

		__q = e2q(q2e_new(q));

		//std::cout <<  "IMU_ANGLES: " << q2e_new(q).transpose() << std::endl;

		Eigen::Vector3d E;
		E = q2e_new(q);

		// fall detector

		if (false) {
			if (E(1) > 20*PI/180. || E(0) > 20*PI/180.) {
				//std::cout << "Falling\n";
				if ((msg->utime - fall_utime) > 1200000) {
					std::cout << "Timeout.\n";
					fall_utime = msg->utime;
					system("notify-send ROBOT_FELL_OVER");
				}
			}
		}

		//std::cout << msg->linear_acceleration[0] << ", "  << msg->linear_acceleration[1] << ", " << msg->linear_acceleration[2] << std::endl;

		check_conv(0) = __q.w() - q.w();
		check_conv(1) = __q.x() - q.x();
		check_conv(2) = __q.y() - q.y();
		check_conv(3) = __q.z() - q.z();


		//cout << "Checking additive norm~: " << check_conv.norm() << endl;

		//cout << "Receiving IMU\n";

	}

};

int main() {

  //Eigen::Matrix3d test;

  //test << 1,2,3,4,5,6,7,8,9;

  //cout << test(2,1) << endl;

  //cout << "Got here\n";


  // register signal SIGINT and signal handler
  signal(SIGINT, signalHandler);

  system("notify-send FallDetectorStarted");

  boost::shared_ptr<lcm::LCM> lcm(new lcm::LCM);
  if(!lcm->good())
    return 1;

  HandleIMU handler(lcm);


  while(0 == lcm->handle());

  return 0;
}









