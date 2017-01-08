#include "tools.h"
#include "DualVNH5019MotorShield.h"
#include "LinakHallSensor.h"
#include <stdio.h> // for function sprintf

typedef enum {
  S_BOTH_CLOSED,
  S_UPPER_OPENING,
  S_HALF_OPEN,
  S_LOWER_OPENING,
  S_BOTH_OPEN,
  S_LOWER_CLOSING,
  S_UPPER_CLOSING,
} system_state_t;

system_state_t system_state = S_BOTH_OPEN;
char current_cmd = 'x';

DualVNH5019MotorShield md;
LinakHallSensor lh;

void open_both_sides(){
  switch (system_state){
    case S_BOTH_CLOSED:
      md.ramp_to_speed_blocking(1, 255);
      system_state = S_UPPER_OPENING;
      break;
    case S_UPPER_OPENING: break;
    case S_HALF_OPEN:
      md.ramp_to_speed_blocking(0, 255);
      system_state = S_LOWER_OPENING;
      break;
    case S_LOWER_OPENING: break;
    case S_BOTH_OPEN: break;
    case S_LOWER_CLOSING:
      md.ramp_to_speed_blocking(0, 255);
      system_state = S_LOWER_OPENING;
      break;
    case S_UPPER_CLOSING:
      md.ramp_to_speed_blocking(1, 255);
      system_state = S_UPPER_OPENING;
      break;
  }
}

void close_both_sides(){
  switch (system_state){
    case S_BOTH_CLOSED: break;
    case S_UPPER_OPENING:
      md.ramp_to_speed_blocking(1, -255);
      system_state = S_UPPER_CLOSING;
      break;
    case S_HALF_OPEN:
      system_state = S_UPPER_CLOSING;
      md.ramp_to_speed_blocking(1, -255);
      break;
    case S_LOWER_OPENING:
      system_state = S_LOWER_CLOSING;
      md.ramp_to_speed_blocking(0, -255);
      break;
    case S_BOTH_OPEN:
      system_state = S_LOWER_CLOSING;
      md.ramp_to_speed_blocking(0, -255);
      break;
    case S_LOWER_CLOSING: break;
    case S_UPPER_CLOSING: break;
  }
}

void check_motor_current(){
  switch (system_state){
    case S_BOTH_CLOSED:
    case S_BOTH_OPEN:
    case S_HALF_OPEN:
      // nothing is moving, so we don't care
      break;
    case S_UPPER_CLOSING:
      if (md.is_overcurrent(1) || md.is_zerocurrent(1)){
        system_state = S_BOTH_CLOSED;
      }
      break;
    case S_UPPER_OPENING:
      if (md.is_overcurrent(1) || md.is_zerocurrent(1)){
        system_state = S_HALF_OPEN;
      }
      break;
    case S_LOWER_OPENING:
      if (md.is_overcurrent(0) || md.is_zerocurrent(0)){
        system_state = S_BOTH_OPEN;
      }
      break;
    case S_LOWER_CLOSING:
      if (md.is_overcurrent(0) || md.is_zerocurrent(0)){
        system_state = S_HALF_OPEN;
      }
      break;
  }
}

void send_status_fixed_binary()
{
  struct message_t {
      tools::mean_std_t inner_motor_current;
      tools::mean_std_t outer_motor_current;

      tools::mean_std_t inner_motor_position;
      tools::mean_std_t outer_motor_position;

      int16_t inner_motor_speed;
      int16_t outer_motor_speed;

      byte current_cmd;
      byte system_state;
  };

  message_t msg;

  msg.inner_motor_current = md.get_mean_std(0, 300);  // ~30ms
  msg.outer_motor_current = md.get_mean_std(1, 300);  // ~30ms

  msg.inner_motor_position = lh.get_mean_std(0, 300);  // ~30ms
  msg.outer_motor_position = lh.get_mean_std(1, 300);  // ~30ms

  msg.inner_motor_speed = md.getMotorSpeed(0);
  msg.outer_motor_speed = md.getMotorSpeed(1);

  msg.current_cmd = current_cmd;
  msg.system_state = system_state;

  uint16_t checksum = tools::checksum_fletcher16((byte*)&msg, sizeof(msg));
  Serial.write((byte*)&msg, sizeof(msg));
  Serial.write((byte*)&checksum, sizeof(checksum));
}

void fetch_new_command()
{
  if (Serial.available() > 0) {
    current_cmd = Serial.read();
  }
}

void setup()
{
  Serial.begin(115200);
  md.init();
  lh.init();
}


void loop()
{
  if (current_cmd == 'o'){
    open_both_sides();
  } else if (current_cmd == 'c'){
    close_both_sides();
  }
  send_status_fixed_binary();
  check_motor_current();
  send_status_fixed_binary();
  fetch_new_command();
}
