#include <iostream>

#include "rng.hpp"

#include "particle.hpp"
#include "../pfgrasp.hpp"
#include <drc_utils/BotWrapper.hpp>

#define VERBOSE_TXT 0

// STATUS OF NOISE DRIFT:
// when vo fails, last dvo is scaled to give dvo with added noise
// - each particle has the same vo except for the added noise.
// TODO:
// smooth the velocity using a fraction of the previous velocity e.g. 50:50


void Particle::MoveParticle(rng *pRng){
  double sigma_u = 0.1;
  this->state.position(0) = pRng->Normal(this->state.position[0], sigma_u);
  this->state.position(1) = pRng->Normal(this->state.position[1], sigma_u);
  this->state.position(2) = pRng->Normal(this->state.position[2], sigma_u);
  return ;
}
  

void Particle::InitializeState(rng *pRng, double init_weight, const void* userdata){
  const PFGrasp* pfg = (const PFGrasp*)userdata;
  double bound = pfg->bound;
  double hBound = pfg->bound / 2; // half bound
  double x_cam[3] = {pRng->Uniform(-hBound,hBound), pRng->Uniform(-hBound,hBound), pRng->Uniform(0,bound)};
  double x_world[3];

  BotFrames* bf = pfg->botWrapper_->getBotFrames();
  bot_frames_transform_vec(bf, pfg->options_.cameraChannelName.c_str(), "local", x_cam, x_world);

  this->state.position(0) = x_world[0];
  this->state.position(1) = x_world[1];
  this->state.position(2) = x_world[2];

  this->logweight = init_weight;
}  

double deg2rad(const double deg){
  const double PI = 3.14159265358979323846;
  return ((deg)*((PI)/(180.0)));
}

double Particle::GetLogLikelihood(rng *pRng, const void* userdata){
  const double obs_error = deg2rad(5);
  const PFGrasp* pfg = (const PFGrasp*)userdata;

  if(pfg->options_.debug) std::cout << "dbg-GetLogLikelihood1"<< pfg<< std::endl;
  double bound = pfg->bound;
  if(pfg->options_.debug) std::cout << "dbg-GetLogLikelihood1"<< pfg->bound<< std::endl;
  double hbound = bound/2;
  // projection matrix
  if(pfg->options_.debug) std::cout << "dbg-GetLogLikelihood2" << std::endl;
  BotCamTrans* HandCamTrans = pfg->warpedCamTrans;
  if(pfg->options_.debug) std::cout << "dbg-GetLogLikelihood3" << std::endl;

  float a = pfg->bearing_a_, b = pfg->bearing_b_;
  //int64_t utime = pfg->img_utime_;


  if(pfg->options_.debug) std::cout << "dbg-GetLogLikelihood3 a:"<< a << std::endl;
  if(a==0 && b==0) return 1; // no observation

  BotTrans bt = pfg->localToCam_;

  if(pfg->options_.debug) std::cout << "dbg-GetLogLikelihood4" << std::endl;
  double *x_world = this->state.position.data();
  double x_cam[3];
  bot_trans_apply_vec(&bt, x_world, x_cam);

  if(pfg->options_.debug) std::cout << "dbg-GetLogLikelihood5" << std::endl;
  double pix[3];
  bot_camtrans_project_point(HandCamTrans, x_cam, pix);
  double du = (pix[0]-bot_camtrans_get_principal_x(HandCamTrans)) / bot_camtrans_get_focal_length_x(HandCamTrans);
  double dv = (pix[1]-bot_camtrans_get_principal_y(HandCamTrans)) / bot_camtrans_get_focal_length_y(HandCamTrans);
  
  if(pfg->options_.debug) std::cout << "dbg-du" <<du<<" dv" << dv <<  std::endl;
  if(pfg->options_.debug) std::cout << "dbg-GetLogLikelihood6" << std::endl;
  double p = 0;
  if(x_cam[2] >= 0 && x_cam[2] < bound && fabs(x_cam[0]) < hbound && fabs(x_cam[1]) < hbound) {
    p = pRng->NormalPdf(atan(a)-atan(du), 0, obs_error) * pRng->NormalPdf(atan(b)-atan(dv), 0, obs_error);
  }

  if(pfg->options_.debug) std::cout << "dbg-GetLogLikelihood7" << std::endl;
  return log(p);
}
