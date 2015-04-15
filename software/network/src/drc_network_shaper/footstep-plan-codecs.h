#ifndef FOOTSTEPPLANCODECS20130517H
#define FOOTSTEPPLANCODECS20130517H

#include "custom-codecs.h"

#include "lcmtypes/drc/walking_plan_request_t.hpp"

#include "footstep-plan-analogs.pb.h"

class FootStepPlanCodec : public CustomChannelCodec
{
  public:
    FootStepPlanCodec(const std::string loopback_channel = "");
        
    bool encode(const std::vector<unsigned char>& lcm_data, std::vector<unsigned char>* transmit_data);
      
    bool decode(std::vector<unsigned char>* lcm_data, const std::vector<unsigned char>& transmit_data);

    
  private:
    goby::acomms::DCCLCodec* dccl_;
        
        
};





#endif
