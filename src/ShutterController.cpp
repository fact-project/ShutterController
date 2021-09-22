#include "tools.h"
#include "DualVNH5019MotorShield.h"
#include "LinakHallSensor.h"
#include <stdio.h> // for function sprintf

typedef enum {
  S_BOTH_CLOSED,     // 0
  S_UPPER_OPENING,   // 1
  S_HALF_OPEN,       // 2
  S_LOWER_OPENING,   // 3
  S_BOTH_OPEN,       // 4
  S_LOWER_CLOSING,   // 5
  S_UPPER_CLOSING,   // 6
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
      md.adjust_to_speed(1, 255);
      system_state = S_UPPER_OPENING;
      break;
    case S_UPPER_OPENING: break;
    case S_HALF_OPEN:
      md.adjust_to_speed(0, 255);
      system_state = S_LOWER_OPENING;
      break;
    case S_LOWER_OPENING: break;
    case S_BOTH_OPEN: break;
    case S_LOWER_CLOSING:
      md.adjust_to_speed(0, 255);
      system_state = S_LOWER_OPENING;
      break;
    case S_UPPER_CLOSING:
      md.adjust_to_speed(1, 255);
      system_state = S_UPPER_OPENING;
      break;
  }
}

void close_both_sides(){
  switch (system_state){
    case S_BOTH_CLOSED: break;
    case S_UPPER_OPENING:
      md.adjust_to_speed(1, -255);
      system_state = S_UPPER_CLOSING;
      break;
    case S_HALF_OPEN:
      system_state = S_UPPER_CLOSING;
      md.adjust_to_speed(1, -255);
      break;
    case S_LOWER_OPENING:
      system_state = S_LOWER_CLOSING;
      md.adjust_to_speed(0, -255);
      break;
    case S_BOTH_OPEN:
      system_state = S_LOWER_CLOSING;
      md.adjust_to_speed(0, -255);
      break;
    case S_LOWER_CLOSING: break;
    case S_UPPER_CLOSING: break;
  }
}

void stop_all(){
  md.ramp_to_speed_blocking(0, 0);
  md.ramp_to_speed_blocking(1, 0);
}

void check_motor_current(){
  switch (system_state){
    case S_BOTH_CLOSED:
    case S_BOTH_OPEN:
    case S_HALF_OPEN:
      // nothing is moving, so we don't care
      break;
    case S_UPPER_CLOSING:
      if (md.is_overcurrent(1)){
        system_state = S_BOTH_CLOSED;
        Serial.println("M1 OVERCURRENT: UPPER_CLOSING -> BOTH CLOSED");
      } else if (md.is_zerocurrent(1)){
        system_state = S_BOTH_CLOSED;
        Serial.println("M1 0-zero-current: UPPER_CLOSING -> BOTH CLOSED");
      }
      break;
    case S_UPPER_OPENING:
      if (md.is_overcurrent(1)){
        system_state = S_BOTH_CLOSED;
        Serial.println("M1 OVERCURRENT: UPPER_OPENING -> HALF OPEN");
      } else if (md.is_zerocurrent(1)){
        system_state = S_BOTH_CLOSED;
        Serial.println("M1 0-zero-current: UPPER_OPENING -> HALF OPEN");
      }
      break;
    case S_LOWER_OPENING:
      if (md.is_overcurrent(1)){
        system_state = S_BOTH_CLOSED;
        Serial.println("M0 OVERCURRENT: LOWER_OPENING -> BOTH OPEN");
      } else if (md.is_zerocurrent(1)){
        system_state = S_BOTH_CLOSED;
        Serial.println("M0 0-zero-current: LOWER_OPENING -> BOTH OPEN");
      }
      break;
    case S_LOWER_CLOSING:
      if (md.is_overcurrent(1)){
        system_state = S_BOTH_CLOSED;
        Serial.println("M0 OVERCURRENT: LOWER_CLOSING -> HALF_OPEN");
      } else if (md.is_zerocurrent(1)){
        system_state = S_BOTH_CLOSED;
        Serial.println("M0 0-zero-current: LOWER_CLOSING -> HALF_OPEN");
      }
      break;
  }
}


void send_status_human_readable(char foo)
{
  char formatted_string[128];
  for (int i=0; i<2; i++){
    tools::mean_std_t current = md.get_mean_std(i, 300);
    tools::mean_std_t position = lh.get_mean_std(i, 300);
    sprintf(formatted_string, "M%1d: I=%4ld+-%4ld pos=%4ld+-%4ld S=%3d cmd=%c system_state=%d  %c",
      i, // motor id
      (long)current.mean, (long)current.std,
      (long)position.mean, (long)position.std,
      md.getMotorSpeed(i),
      current_cmd,
      system_state,
      foo
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
  } else if (current_cmd == 'x'){
    stop_all();
  }
  send_status_human_readable('-');
  check_motor_current();
  send_status_human_readable('|');
  fetch_new_command();
}
