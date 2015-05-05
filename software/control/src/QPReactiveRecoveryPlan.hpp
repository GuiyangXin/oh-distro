#include "drake/RigidBodyManipulator.h"
#include "drake/Polynomial.h"
#include "drake/PiecewisePolynomial.h"
#include "control/ExponentialForm.hpp"
#include "drake/QPCommon.h"
#include "drake/lcmt_qp_controller_input.hpp"
#include <lcm/lcm-cpp.hpp>

enum SupportLogicType {
  REQUIRE_SUPPORT, ONLY_IF_FORCE_SENSED, ONLY_IF_KINEMATIC, KINEMATIC_OR_SENSED, PREVENT_SUPPORT
};


#define QP_REACTIVE_RECOVERY_VERTICES_PER_FOOT 4

enum FootID {RIGHT, LEFT};

std::map<FootID, std::string> footIDToName = {std::make_pair(RIGHT, "right"),
                                              std::make_pair(LEFT, "left")};

std::map<std::string, FootID> footNameToID = {std::make_pair("right", RIGHT),
                                              std::make_pair("left", LEFT)};

std::map<FootID, FootID> otherFoot = {std::make_pair(RIGHT, LEFT),
                                      std::make_pair(LEFT, RIGHT)};

struct BangBangIntercept {
  double tf;
  double tswitch;
  double u;
};

typedef Matrix<double, 7, 1> XYZQuat;

struct FootState {
  Isometry3d pose;
  XYZQuat velocity;
  bool contact;
  double terrain_height;
  Vector3d terrain_normal;
};

struct InterceptPlan {
  double tf;
  double tswitch;
  Isometry3d pose_next;
  Isometry3d icp_next;
  Isometry3d cop;
  FootID swing_foot;
  FootID stance_foot;
  Isometry3d stance_pose;
  double error;
};


typedef std::map<FootID, FootState> FootStateMap;
typedef std::map<FootID, Matrix<double, 3, 4>> VertMap;

struct BipedDescription {
  std::map<FootID, Matrix<double, 3, 4>> reachable_vertices;
  std::map<FootID, Matrix<double, 3, QP_REACTIVE_RECOVERY_VERTICES_PER_FOOT>> foot_vertices;
  double u_max; // foot acceleration bounds used for computing ICP intercepts
  double omega; // characteristic frequency of the linear inverted pendulum system. Defined as sqrt(g / height)
};

BipedDescription getAtlasDefaults() {
  BipedDescription biped;
  biped.reachable_vertices[RIGHT] << -0.35, 0.35, 0.35, -0.35,
                                    -0.2, -0.2, -0.45, -0.45,
                                    0, 0, 0, 0;
  biped.reachable_vertices[LEFT] << -0.35, 0.35, 0.35, -0.35,
                                    0.2, 0.2, 0.45, 0.45,
                                    0, 0, 0, 0;
  biped.foot_vertices[RIGHT] << -0.12, 0.12, 0.12, -0.12,
                               -0.06, -0.06, 0.06, 0.06,
                               0, 0, 0, 0;
  biped.foot_vertices[LEFT] << -0.12, 0.12, 0.12, -0.12,
                              -0.06, -0.06, 0.06, 0.06,
                              0, 0, 0, 0;
  biped.omega = sqrt(9.81 / 1.098);
  biped.u_max = 20;
  return biped;
}
  
class QPReactiveRecoveryPlan {

  private:
    std::unique_ptr<PiecewisePolynomial<double>> last_swing_plan;
    InterceptPlan last_intercept_plan;
    double t_start = 0;
    bool initialized = false;
    BipedDescription biped;
    std::map<FootID, int> foot_frame_ids;
    std::map<FootID, int> foot_body_ids;
    VectorXd q_des;
    MatrixXd S;
    RobotPropertyCache robot_property_cache;

    void findFootSoleFrames();
    FootStateMap getFootStates(const VectorXd &v, const std::vector<bool>& contact_force_detected);
    void getCaptureInput(double t_global, const FootStateMap &foot_states, const Isometry3d &icp, drake::lcmt_qp_controller_input &qp_input);
    void getInterceptInput(double t_global, const FootStateMap &foot_states, drake::lcmt_qp_controller_input &qp_input);
    std::shared_ptr<lcm::LCM> LCMHandle;
    void initLCM();
    void publishForVisualization(double t_global, const Isometry3d &icp);
    void setupQPInputDefaults(double t_global, drake::lcmt_qp_controller_input &qp_input);
    void encodeSupportData(const int body_id, const FootState &foot_state, const SupportLogicType &support_logic, drake::lcmt_support_data &support_data);
    void encodeBodyMotionData(int body_or_frame_id, PiecewisePolynomial<double> spline, drake::lcmt_body_motion_data &body_motion);
    std::unique_ptr<PiecewisePolynomial<double>> straightToGoalTrajectory(double t_global, const InterceptPlan &intercept_plan, const std::map<FootID, FootState> &foot_states);
    std::unique_ptr<PiecewisePolynomial<double>> upOverAndDownTrajectory(double t_global, const InterceptPlan &intercept_plan, const FootStateMap &foot_states);
    Matrix3Xd heelToeContacts(int body_id);

	public:
    RigidBodyManipulator* robot;
    double capture_max_flyfoot_height = 0.05;
    double capture_shrink_factor = 0.8;
    double desired_icp_offset = 0.1;
    double min_step_duration = 0.4;
    double foot_hull_cop_shrink_factor = 0.5;
    double max_considerable_foot_swing = 0.15;
    double post_execution_delay = 0.4;
    double mu = 0.5;
    double pelvis_height_above_sole = 0.84;
    double swing_height_above_terrain = 0.03;

    QPReactiveRecoveryPlan(RigidBodyManipulator *robot, const RobotPropertyCache &rpc);
    QPReactiveRecoveryPlan(RigidBodyManipulator *robot, const RobotPropertyCache &rpc, BipedDescription biped);

		static VectorXd closestPointInConvexHull(const Ref<const VectorXd> &x, const Ref<const MatrixXd> &V);

    static Isometry3d closestPoseInConvexHull(const Isometry3d &pose, const Ref<const MatrixXd> &V);

    static std::vector<double> expIntercept(const ExponentialForm &expform, double l0, double ld0, double u, int degree);

    // a polynomial representing position as a function of time subject to initial position x0, initial velocity xd0, final velocity of 0, and acceleration of +u followed by acceleration of -u. 
    static Polynomial<double> bangBangPolynomial(double x0, double xd0, double u);

    // bang-bang policy intercepts from initial state [x0, xd0] to final state [xf, 0] at max acceleration u_max
    static std::vector<BangBangIntercept> bangBangIntercept(double x0, double xd0, double xf, double u_max);

    static Isometry3d getTWorldToLocal(const Isometry3d &icp, const Isometry3d &cop);

    static double getMinTimeToXprimeAxis(const FootState foot_state, const BipedDescription &biped, Isometry3d &T_world_to_local);

    static ExponentialForm icpTrajectory(double x_ic, double x_cop, double omega);

    static std::unique_ptr<PiecewisePolynomial<double>> freeKnotTimesSpline(double t0, double tf, const Ref<const MatrixXd> &xs, const Ref<const VectorXd> xd0, const Ref<const VectorXd> xdf);

    std::unique_ptr<PiecewisePolynomial<double>> swingTrajectory(double t_global, const InterceptPlan &intercept_plan, const std::map<FootID, FootState> &foot_states);

    void resetInitialization();

    double icpError(const Ref<const Vector2d> &r_ic, const FootStateMap &foot_states, const VertMap &foot_vertices);

    bool isICPCaptured(const Ref<const Vector2d> &r_ic, const FootStateMap &foot_states, const VertMap &foot_vertices);

    std::vector<InterceptPlan> getInterceptsWithCoP(const FootID &swing_foot, const std::map<FootID, FootState> &foot_states, const Isometry3d &icp, const Isometry3d &cop);

    std::vector<InterceptPlan> getInterceptPlansForFoot(const FootID &swing_foot, const std::map<FootID, FootState> &foot_states, const Isometry3d &icp);

    std::vector<InterceptPlan> getInterceptPlans(const std::map<FootID, FootState> &foot_states, const Isometry3d &icp);

    drake::lcmt_qp_controller_input getQPControllerInput(double t_global, const VectorXd &q, const VectorXd &v, const std::vector<bool>& contact_force_detected);
    void publishQPControllerInput(double t_global, const VectorXd &q, const VectorXd &v, const std::vector<bool>& contact_force_detected);

    Vector2d getICP(const VectorXd &v);

    void setS(const MatrixXd &S) {
      this->S = S;
    }

    void setRobot(RigidBodyManipulator *robot);

    void setQDes(const Ref<const VectorXd> &q_des) {
      this->q_des = q_des;
    }
};

