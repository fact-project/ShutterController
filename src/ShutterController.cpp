typedef enum {
  L_OPEN,
  L_OPENING,
  L_CLOSED,
  L_CLOSING,
  L_UNKNOWN
} lid_states_t;

typedef enum {
  S_BOTH_CLOSED,
  S_UPPER_OPENING,
  S_HALF_OPEN,
  S_LOWER_OPENING,
  S_BOTH_OPEN,
  S_LOWER_CLOSING,
  S_UPPER_CLOSING,
  S_UNKNOWN
} system_state_t;

system_state_t system_state = S_UNKNOWN;
lid_states_t lid_states[2] = {L_UNKNOWN, L_UNKNOWN};

const int _StartPoint      =     0;
const int _EndPoint        =  1024;

void MoveTo(int motor, double target_position);

#include "tools.h"
#include "DualVNH5019MotorShield.h"
#include "LinakHallSensor.h"

const int M1INA = 2;
const int M1INB = 3;
const int M2INA = 7;
const int M2INB = 8;
const int maximum_speed = 255;

DualVNH5019MotorShield md(
    M1INA,
    M1INB,
    A4,
    M2INA,
    M2INB,
    A5
    );

LinakHallSensor lh(A2, A3);

void setup()
{
  Serial.begin(9600);
  md.init();
  lh.init();
}

void send_status()
{
  tools::mean_std_t tmp;
  tmp = md.get_mean_std(0, 10);
  Serial.write((char*)&tmp, sizeof(tools::mean_std_t));

  tmp = md.get_mean_std(1, 10);
  Serial.write((char*)&tmp, sizeof(tools::mean_std_t));

  tmp = lh.get_mean_std(0, 10);
  Serial.write((char*)&tmp, sizeof(tools::mean_std_t));

  tmp = lh.get_mean_std(1, 10);
  Serial.write((char*)&tmp, sizeof(tools::mean_std_t));
}


void send_status_human_readable()
{
  for (int i=0; i<2; i++){
    tools::mean_std_t current = md.get_mean_std(i, 300);
    tools::mean_std_t position = lh.get_mean_std(i, 300);

    Serial.print("M");
    Serial.print(i);
    Serial.print(" I=");
    Serial.print(current.mean);
    Serial.print("+-");
    Serial.print(current.std);
    Serial.print(" pos=");
    Serial.print(position.mean);
    Serial.print("+-");
    Serial.print(position.std);
    Serial.print(" S:");
    Serial.print(md.getMotorSpeed(i));
    Serial.print(" ");

  }
  Serial.println();

}


char check_for_client_send_status_return_command()
{
  char command = 0;
  if (Serial.available() > 0) {
    command = Serial.read();
  }
  send_status_human_readable();
  Serial.print("command: ");
  Serial.println(command);
  return command;
}

int m0_speed = 0;
int m1_speed = 0;


void loop()
{
  char cmd = check_for_client_send_status_return_command();
  switch (cmd){
    case 0: break;
    case 'a': m0_speed = -255; break;
    case 's': m0_speed -= 32;  break;
    case 'd': m0_speed = 0;    break;
    case 'f': m0_speed += 32;  break;
    case 'g': m0_speed = 255;  break;

    case 'q': m1_speed = -255; break;
    case 'w': m1_speed -= 32;  break;
    case 'e': m1_speed = 0;    break;
    case 'r': m1_speed += 32;  break;
    case 't': m1_speed = 255;  break;
  }
  md.ramp_to_speed_blocking(0, m0_speed);
  md.ramp_to_speed_blocking(1, m1_speed);

  if( md.is_overcurrent(0) ){
    Serial.println("m0 overcurrent");
  }
  if( md.is_overcurrent(1) ){
    Serial.println("m1 overcurrent");
  }
}


void MoveTo(int motor, double target_position){
  tools::mean_std_t position = lh.get_mean_std(motor, 10);
  while(abs(target_position - position.mean) > 2*position.std)
  {
    if (target_position > position.mean)
    {
      md.ramp_to_speed_blocking(motor, maximum_speed);
    }
    else // if target_position < round(position.mean)
    {
      md.ramp_to_speed_blocking(motor, -maximum_speed);
    }

    if (md.is_overcurrent(motor)){
      break;
    }

    if (md.is_zerocurrent(motor)){
      break;
    }
    delay (10);
    position = lh.get_mean_std(motor, 10);
  }

  md.setMotorSpeed(motor, 0);
  return;
}

void open_both_sides(){
  MoveTo(1, _StartPoint);
  delay(100);
  MoveTo(0, _StartPoint);
}

void close_both_sides(){
  MoveTo(0, _EndPoint);
  delay(100);
  MoveTo(1, _EndPoint);
}

