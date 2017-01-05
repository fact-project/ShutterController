enum states_t {
  _UNKNOWN        =  0,
  _CLOSED         =  1,
  _OPEN           =  2,
  _STEADY         =  3,
  _MOVING         =  4,
  _CLOSING        =  5,
  _OPENING        =  6,
  _JAMMED         =  7,
  _MOTOR_FAULT    =  8,
  _POWER_PROBLEM  =  9,
  _OVER_CURRENT   = 10
};

const int _StartPoint      =     0;
const int _EndPoint        =  1024;

states_t _LidStatus[2]      = {_UNKNOWN, _UNKNOWN};

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
    M1INA, // INA1 : "M1INA"
    M1INB, // INB1 : "M1INB"
    A4, // CS1: "M1CS"
    M2INA, // INA2 : "M2INA" - original 7
    M2INB, // INB2 : "M2INB" - original 8
    A5 // CS2 : "M2CS"
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
      _LidStatus[motor] = _CLOSING;
    }
    else // if target_position < round(position.mean)
    {
      md.ramp_to_speed_blocking(motor, -maximum_speed);
      _LidStatus[motor] = _OPENING;
    }

    if (md.is_overcurrent(motor)){
      _LidStatus[motor] = _OVER_CURRENT;
      break;
    }

    if (md.is_zerocurrent(motor)){
      if (_LidStatus[motor] == _CLOSING){
        _LidStatus[motor] == _CLOSED;
      }
      else
      {
        _LidStatus[motor] == _OPEN;
      }
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

