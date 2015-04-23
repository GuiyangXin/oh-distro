#include "GamepadPublisher.h"
#include <limits>

using namespace std;
using namespace drc;

GamepadPublisher::GamepadPublisher(string const & device_name, string const & channel_name)
: m_channel_name(channel_name), m_joystick(device_name) { }

void GamepadPublisher::publish()
{
  js_event jse;
  int rc = m_joystick.read_joystick_event(jse);
  if (rc == JOYSTICK_SUCCESS && (jse.type == GAMEPAD_DISCRETE || jse.type == GAMEPAD_CONTINUOUS)) {
    gamepad_cmd_t gamepad_cmd = build_message(jse);
    printf("Event: time %8u, value %8hd, type: %3u, axis/button: %u\n", jse.time, jse.value, jse.type, jse.number);  
    m_lcm.publish(m_channel_name, &gamepad_cmd);
  }
}

gamepad_cmd_t GamepadPublisher::build_message(js_event const & jse) const
{ 
  gamepad_cmd_t gamepad_cmd;
  gamepad_cmd.utime = jse.time;

  switch (jse.type) {
    case GAMEPAD_DISCRETE:
      process_discrete_event(jse, gamepad_cmd);
      break;
    case GAMEPAD_CONTINUOUS:
      process_continuous_event(jse, gamepad_cmd);
      break;
    default:
      break;
  }
  return gamepad_cmd;
}

void GamepadPublisher::process_discrete_event(js_event const & jse, gamepad_cmd_t & gamepad_cmd) const
{
  switch (jse.number) { 
    case GAMEPAD_BUTTON_A:
      gamepad_cmd.button_a = jse.value;
      break;
    case GAMEPAD_BUTTON_B:
      gamepad_cmd.button_b = jse.value;
      break;
    case GAMEPAD_BUTTON_X:
      gamepad_cmd.button_x = jse.value;
      break;
    case GAMEPAD_BUTTON_Y:
      gamepad_cmd.button_y = jse.value;
      break;
    case GAMEPAD_BUTTON_START:
      gamepad_cmd.button_start = jse.value;
      break;
    case GAMEPAD_BUTTON_BACK:
      gamepad_cmd.button_back = jse.value;
      break;
    case GAMEPAD_BUTTON_LB:
      gamepad_cmd.button_lb = jse.value;
      break;
    case GAMEPAD_BUTTON_RB:
      gamepad_cmd.button_rb = jse.value;
      break;
    case GAMEPAD_BUTTON_JL:
      gamepad_cmd.button_thumbpad_left = jse.value;
      break;
    case GAMEPAD_BUTTON_JR:
      gamepad_cmd.button_thumbpad_right = jse.value;
      break;
    default:
      break;
  }
}


void GamepadPublisher::process_continuous_event(js_event const & jse, gamepad_cmd_t & gamepad_cmd) const
{
  static const float scaleFactor = static_cast<float>((1<<15) - 1);
  float scaledValue = (-0.5*jse.value / scaleFactor);

  switch (jse.number) { 
    case GAMEPAD_DPAD_X:
      gamepad_cmd.button_dpad_x = jse.value;
      break;
    case GAMEPAD_DPAD_Y:
      gamepad_cmd.button_dpad_y = jse.value;
      break;
    case GAMEPAD_LEFT_X:
      gamepad_cmd.thumbpad_left_x = -scaledValue;
      break;
    case GAMEPAD_LEFT_Y:
      gamepad_cmd.thumbpad_left_y = scaledValue;
      break;
    case GAMEPAD_RIGHT_X:
      gamepad_cmd.thumbpad_right_x = -scaledValue;
      break;
    case GAMEPAD_RIGHT_Y:
      gamepad_cmd.thumbpad_right_y = scaledValue;
      break;
    case GAMEPAD_TRIGGER_L:
      gamepad_cmd.trigger_left = jse.value / 255.0f;
      break;
    case GAMEPAD_TRIGGER_R:
      gamepad_cmd.trigger_right = jse.value / 255.0f;
      break;
    default:
      break;
  }
}