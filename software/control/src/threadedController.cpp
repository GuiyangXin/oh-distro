#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>
#include <sys/select.h>
#include "drake/lcmt_qp_controller_input.hpp"
#include "drc/controller_status_t.hpp"
#include "drc/robot_state_t.hpp"
#include "drc/atlas_behavior_command_t.hpp"
#include <lcm/lcm-cpp.hpp>
#include "drake/QPCommon.h"
#include "RobotStateDriver.hpp"
#include "AtlasCommandDriver.hpp"
#include "FootContactDriver.hpp"


namespace {

struct ThreadedControllerOptions {
  std::string atlas_command_channel;
  std::string atlas_behavior_channel;
  int max_infocount; // If we see info < 0 more than max_infocount times, freeze Atlas. Set to -1 to disable freezing.
};

std::atomic<bool> done(false);

std::atomic<bool> newInputAvailable(false);
std::atomic<bool> newStateAvailable(false);

std::mutex pointerMutex;

std::shared_ptr<RobotStateDriver> state_driver;
std::shared_ptr<AtlasCommandDriver> command_driver;
std::shared_ptr<FootContactDriver> foot_contact_driver;
Matrix<bool, Dynamic, 1> b_contact_force;

class SolveArgs {
public:

  NewQPControllerData *pdata;

  std::shared_ptr<DrakeRobotState> robot_state;
  std::shared_ptr<drake::lcmt_qp_controller_input> qp_input;
  std::shared_ptr<QPControllerOutput> qp_output;

  Matrix<bool, Dynamic, 1> b_contact_force;
  std::shared_ptr<QPControllerDebugData> debug;

  int info;
};

SolveArgs solveArgs;

drc::atlas_behavior_command_t atlas_behavior_msg;

int infocount = 0;

class LCMHandler {

public:

  std::thread ThreadHandle;
  std::shared_ptr<lcm::LCM> LCMHandle;
  bool ShouldStop;

  LCMHandler()
  {
    this->ShouldStop = false;
    this->InitLCM();
  }

  void InitLCM()
  {
    this->LCMHandle = std::shared_ptr<lcm::LCM>(new lcm::LCM);
    if(!this->LCMHandle->good())
    {
      std::cout << "ERROR: lcm is not good()" << std::endl;
    }
  }

  bool WaitForLCM(double timeout)
  {
    int lcmFd = this->LCMHandle->getFileno();

    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = timeout * 1e6;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(lcmFd, &fds);

    int status = select(lcmFd + 1, &fds, 0, 0, &tv);
    if (status == -1 && errno != EINTR)
    {
      printf("select() returned error: %d\n", errno);
    }
    else if (status == -1 && errno == EINTR)
    {
      printf("select() interrupted\n");
    }
    return (status > 0 && FD_ISSET(lcmFd, &fds));
  }

  void ThreadLoopWithSelect()
  {

    std::cout << "ThreadLoopWithSelect " << std::this_thread::get_id() << std::endl;

    while (!this->ShouldStop)
    {
      const double timeoutInSeconds = 0.3;
      bool lcmReady = this->WaitForLCM(timeoutInSeconds);

      if (this->ShouldStop)
      {
        break;
      }

      if (lcmReady)
      {
        if (this->LCMHandle->handle() != 0)
        {
          printf("lcm->handle() returned non-zero\n");
          break;
        }
      }
    }

    std::cout << "ThreadLoopWithSelect ended " << std::this_thread::get_id() << std::endl;

  }

  void ThreadLoop()
  {
    while (!this->ShouldStop)
    {
      if (this->LCMHandle->handle() != 0)
      {
        printf("lcm->handle() returned non-zero\n");
        break;
      }
    }
  }

  bool IsRunning()
  {
    return this->ThreadHandle.joinable();
  }

  void Start()
  {
    std::cout << "LCMHandler start... " << std::this_thread::get_id() << std::endl;
    if (this->IsRunning())
    {
      std::cout << "already running lcm thread. " << std::this_thread::get_id() << std::endl;
      return;
    }

    this->ShouldStop = false;
    this->ThreadHandle = std::thread(&LCMHandler::ThreadLoopWithSelect, this);
  }

  void Stop()
  {
    this->ShouldStop = true;
    this->ThreadHandle.join();
  }

};


class LCMControlReceiver {
public:

  LCMControlReceiver(LCMHandler* lh)
  {
    this->lcmHandler = lh;
  }

  void InitSubscriptions()
  {
    lcm::Subscription* sub;

    sub = this->lcmHandler->LCMHandle->subscribe("QP_CONTROLLER_INPUT", &LCMControlReceiver::inputHandler, this);
    sub->setQueueCapacity(1);

    sub = this->lcmHandler->LCMHandle->subscribe("EST_ROBOT_STATE", &LCMControlReceiver::onRobotState, this);
    sub->setQueueCapacity(1);

    sub = this->lcmHandler->LCMHandle->subscribe("FOOT_CONTACT_ESTIMATE", &LCMControlReceiver::onFootContact, this);
    sub->setQueueCapacity(1);
  }

  void inputHandler(const lcm::ReceiveBuffer* rbuf, const std::string& channel, const drake::lcmt_qp_controller_input* msg)
  {
    //std::cout << "received qp_input on lcm thread " << std::this_thread::get_id() << std::endl;

    std::shared_ptr<drake::lcmt_qp_controller_input> msgCopy(new drake::lcmt_qp_controller_input);
    *msgCopy = *msg;

    // std::cout << "got input msg with param set name: " << msgCopy->param_set_name << std::endl;
    pointerMutex.lock();
    solveArgs.qp_input = msgCopy;
    pointerMutex.unlock();
    newInputAvailable = true;
  }

  void onRobotState(const lcm::ReceiveBuffer* rbuf, const std::string& channel, const drc::robot_state_t* msg)
  {
    //std::cout << "received robotstate on lcm thread " << std::this_thread::get_id() << std::endl;

    std::shared_ptr<DrakeRobotState> state(new DrakeRobotState);

    int nq = solveArgs.pdata->r->num_positions;
    int nv = solveArgs.pdata->r->num_velocities;
    state->q = VectorXd::Zero(nq);
    state->qd = VectorXd::Zero(nv);

    state_driver->decode(msg, state.get());

    // std::cout << "decoded robot state " << state->t << std::endl;

    pointerMutex.lock();
    solveArgs.robot_state = state;
    pointerMutex.unlock();
    newStateAvailable = true;
  }

  void onFootContact(const lcm::ReceiveBuffer* rbuf, const std::string& channel, const drc::foot_contact_estimate_t* msg)
  {
    Matrix<bool, Dynamic, 1> b_contact_force = Matrix<bool, Dynamic, 1>::Zero(solveArgs.pdata->r->num_bodies);

    foot_contact_driver->decode(msg, b_contact_force);

    pointerMutex.lock();
    solveArgs.b_contact_force = b_contact_force;
    pointerMutex.unlock();
  }


  LCMHandler* lcmHandler;
};


LCMHandler lcmHandler;
LCMControlReceiver controlReceiver(&lcmHandler);

bool isOutputSafe(QPControllerOutput &qp_output) {
  for (int i=0; i < qp_output.q_ref.size(); i++) {
    if (std::isnan(qp_output.q_ref(i))) {
      std::cout << "Error: NaN in q_ref" << std::endl;
      return false;
    }
    if (std::isnan(qp_output.qd_ref(i))) {
      std::cout << "Error: NaN in qd_ref" << std::endl;
      return false;
    }
    if (std::isnan(qp_output.qdd(i))) {
      std::cout << "Error: NaN in qdd" << std::endl;
      return false;
    }
    if (std::isinf(qp_output.q_ref(i))) {
      std::cout << "Error: Inf in q_ref" << std::endl;
      return false;
    }
    if (std::isinf(qp_output.qd_ref(i))) {
      std::cout << "Error: Inf in qd_ref" << std::endl;
      return false;
    }
    if (std::isinf(qp_output.qdd(i))) {
      std::cout << "Error: Inf in qdd" << std::endl;
      return false;
    }
  }
  for (int i=0; i < qp_output.u.size(); i++) {
    if (std::isnan(qp_output.u(i))) {
      std::cout << "Error: NaN in u" << std::endl;
      return false;
    }
    if (std::isinf(qp_output.u(i))) {
      std::cout << "Error: Inf in u" << std::endl;
      return false;
    }
  }
  return true;
}

void threadLoop(std::shared_ptr<ThreadedControllerOptions> ctrl_opts)
{

  QPControllerOutput qp_output;
  done = false;

  while (!done) {

    //std::cout << "waiting for new data... " << std::this_thread::get_id() << std::endl;

    while (!newStateAvailable || !newInputAvailable) {
      std::this_thread::yield();
    }

    auto begin = std::chrono::high_resolution_clock::now();


    // copy pointers
    pointerMutex.lock();
    std::shared_ptr<DrakeRobotState> robot_state = solveArgs.robot_state;
    std::shared_ptr<drake::lcmt_qp_controller_input> qp_input = solveArgs.qp_input;
    b_contact_force = solveArgs.b_contact_force;
    pointerMutex.unlock();

    // newInputAvailable = false;
    newStateAvailable = false;

    // std::cout << "calling solve " << std::endl;

    if (qp_input->be_silent) {
      // Act as a dummy controller, produce no ATLAS_COMMAND, and reset all integrator states
      solveArgs.pdata->state.t_prev = robot_state->t;
      solveArgs.pdata->state.vref_integrator_state = VectorXd::Zero(solveArgs.pdata->state.vref_integrator_state.size());
      solveArgs.pdata->state.q_integrator_state = VectorXd::Zero(solveArgs.pdata->state.q_integrator_state.size());
      infocount = 0;
    } else {
      int info = setupAndSolveQP(solveArgs.pdata, qp_input, *robot_state, b_contact_force, &qp_output, solveArgs.debug);

      if (!isOutputSafe(qp_output)) {
        atlas_behavior_msg.utime = 0;
        atlas_behavior_msg.command = "freeze";
        lcmHandler.LCMHandle->publish(ctrl_opts->atlas_behavior_channel, &atlas_behavior_msg);
      }

      if (info < 0 && ctrl_opts->max_infocount > 0) {
        infocount++;
        std::cout << "Incremented infocount to " << infocount << std::endl;
        if (infocount >= ctrl_opts->max_infocount) {
          if (infocount == ctrl_opts->max_infocount) {
            std::cout << "Infocount exceeded. Freezing Atlas!" << std::endl;
          }
          atlas_behavior_msg.utime = 0;
          atlas_behavior_msg.command = "freeze";
          lcmHandler.LCMHandle->publish(ctrl_opts->atlas_behavior_channel, &atlas_behavior_msg);
        }
      } else {
        infocount = 0;
      }
      // std::cout << "u: " << qp_output.u << std::endl;
      // std::cout << "q: " << qp_output.q_ref << std::endl;
      // std::cout << "qd: " << qp_output.qd_ref << std::endl;
      // std::cout << "qdd: " << qp_output.qdd << std::endl;

      AtlasParams *params; 
      std::map<string,AtlasParams>::iterator it;
      it = solveArgs.pdata->param_sets.find(qp_input->param_set_name);
      if (it == solveArgs.pdata->param_sets.end()) {
        mexWarnMsgTxt("Got a param set I don't recognize! Using standing params instead");
        it = solveArgs.pdata->param_sets.find("standing_hardware");
        if (it == solveArgs.pdata->param_sets.end()) {
          params = nullptr;
          mexErrMsgTxt("Could not fall back to standing parameters either. I have to give up here.");
        }
      }
      params = &(it->second);
      // publish ATLAS_COMMAND
      drc::atlas_command_t* command_msg = command_driver->encode(robot_state->t, &qp_output, params->hardware);
      lcmHandler.LCMHandle->publish(ctrl_opts->atlas_command_channel, command_msg);
    }

    typedef std::chrono::duration<float> float_seconds;
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<float_seconds>(end - begin);
    //std::cout << "solve speed: " << elapsed.count() << " ms.  " << 1.0 /elapsed.count() << " hz." << std::endl;

  }

}




void controllerLoop(NewQPControllerData *pdata, std::shared_ptr<ThreadedControllerOptions> ctrl_opts)
{
  state_driver.reset(new RobotStateDriver(pdata->state_coordinate_names));
  command_driver.reset(new AtlasCommandDriver(&pdata->input_joint_names, pdata->state_coordinate_names));
  foot_contact_driver.reset(new FootContactDriver(pdata->rpc.body_ids));

  solveArgs.pdata = pdata;
  solveArgs.b_contact_force = Matrix<bool, Dynamic, 1>::Zero(pdata->r->num_bodies);

  // std::cout << "pdata num bodies: " << pdata->r->num_bodies << std::endl;

  lcmHandler.Start();
  controlReceiver.InitSubscriptions();

  std::cout << "starting control loop... " << std::endl;

  threadLoop(ctrl_opts);
}














} // end anonymous namespace




