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

const int M1INA = 2;
const int M1INB = 3;
const int M2INA = 7;
const int M2INB = 8;

DualVNH5019MotorShield md(
    M1INA,
    M1INB,
    A4,
    M2INA,
    M2INB,
    A5
    );

LinakHallSensor lh(A2, A3);

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


void send_status_human_readable()
{
  char formatted_string[128];
  sprintf(formatted_string, "cmd=%c system_state=%d",
    current_cmd,
    system_state
  );
  formatted_string[127] = 0;
  Serial.println(formatted_string);

  for (int i=0; i<2; i++){
    tools::mean_std_t current = md.get_mean_std(i, 300);
    tools::mean_std_t position = lh.get_mean_std(i, 300);
    sprintf(formatted_string, "M%1d: I=%5ld+-%5ld pos=%5ld+-%5ld S=%3d",
      i, // motor id
      (long)current.mean, (long)current.std,
      (long)position.mean, (long)position.std,
      md.getMotorSpeed(i)
      );
    formatted_string[127] = 0;
    Serial.println(formatted_string);
  }
}

void fetch_new_command()
{
  if (Serial.available() > 0) {
    current_cmd = Serial.read();
  }
}

void setup()
{
  Serial.begin(9600);
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
  check_motor_current();
  send_status_human_readable();
  fetch_new_command();
}
