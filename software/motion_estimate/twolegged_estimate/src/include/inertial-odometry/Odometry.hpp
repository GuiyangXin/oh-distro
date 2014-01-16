#ifndef __ODOMETRY_H__
#define __ODOMETRY_H__

#include <iostream>
#include <inertial-odometry/InertialOdometry_Types.hpp>
#include <inertial-odometry/OrientationComputer.hpp>
#include <inertial-odometry/VP_Mechanization.hpp>
#include <inertial-odometry/IMUCompensation.hpp>

#define USE_TRIGNOMETRIC_EXMAP

namespace InertialOdometry {

  class Odometry {
    private:
		OrientationComputer orc;
		VP_Mechanization avp;
		Parameters param;
		//InertialOdomOutput output_state; // State memory should only exist in the attitude and vp objects -- this should not be here

		// Store state memory
		DynamicState state;
		
		void UpdateStructState(InertialOdomOutput &output_data);// ??


		InertialOdomOutput PropagatePrediction_wo_IMUCompensation(IMU_dataframe* _imu, const Eigen::Quaterniond &orient);


		Eigen::Matrix<double, 3, 3> Expmap(Eigen::Vector3d const &w);
	  
    public:
		EIGEN_MAKE_ALIGNED_OPERATOR_NEW

		IMUCompensation imu_compensator;

		// Constructor with standard parameters
		Odometry();
		// Propagate the internal state registers with new IMU data
		DynamicState PropagatePrediction(IMU_dataframe* _imu, const Eigen::Quaterniond &orient);
		
		DynamicState getDynamicState();

		void setPositionState(const Eigen::Vector3d &P_set);

		void setVelocityState(const Eigen::Vector3d &V_set);

		Eigen::Matrix3d C_bw();
  };

}

#endif
