#include <SPI.h>
#include <Ethernet.h>

#include "ShutterController.h"

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

EthernetServer server(80);

byte _mac[] = { 0x90, 0xA2, 0xDA, 0x00, 0x5C, 0x91 };
IPAddress _ip(10, 0, 100, 36);
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
    Serial.println("Using hard-coded ip...");
    Ethernet.begin(_mac, _ip);

  Serial.print("My IP address is ");
  Serial.println(Ethernet.localIP());

  server.begin();

  md.init();
  lh.init();
}

void send_status(EthernetClient & client)
{
  tools::mean_std_t tmp;
  tmp = md.get_mean_std(0, 10);
  client.write((char*)&tmp, sizeof(tools::mean_std_t));

  tmp = md.get_mean_std(1, 10);
  client.write((char*)&tmp, sizeof(tools::mean_std_t));

  tmp = lh.get_mean_std(0, 10);
  client.write((char*)&tmp, sizeof(tools::mean_std_t));

  tmp = lh.get_mean_std(1, 10);
  client.write((char*)&tmp, sizeof(tools::mean_std_t));
}


void send_status_human_readable(EthernetClient & client)
{
  for (int i=0; i<2; i++){
    tools::mean_std_t current = md.get_mean_std(i, 100);
    tools::mean_std_t position = lh.get_mean_std(i, 100);

    client.print("M");
    client.print(i);
    client.print(" I=");
    client.print(current.mean);
    client.print("+-");
    client.print(current.std);
    client.print(" pos=");
    client.print(position.mean);
    client.print("+-");
    client.print(position.std);
    client.print(" S:");
    client.print(md.getMotorSpeed(i));
    client.print(" ");

  }
  client.println();

}


char check_for_client_send_status_return_command()
{
  EthernetClient client = server.available();
  char command = 0;
  if (client) {
    int avail = client.available();
    client.println(avail);
    if (avail > 1){
      command = client.read();
    }
    send_status_human_readable(client);
    client.print("command: ");
    client.println(command);

    //client.stop();
  }
  return command;
}

void loop()
{
  char cmd = check_for_client_send_status_return_command();
  switch (cmd){
    case 0: break;
    case 'a': md.ramp_to_speed_blocking(0, -20); break;
    case 's': md.ramp_to_speed_blocking(0, -10); break;
    case 'd': md.ramp_to_speed_blocking(0, 0); break;
    case 'f': md.ramp_to_speed_blocking(0, 10); break;
    case 'g': md.ramp_to_speed_blocking(0, 20); break;

    case 'q': md.ramp_to_speed_blocking(1, -20); break;
    case 'w': md.ramp_to_speed_blocking(1, -10); break;
    case 'e': md.ramp_to_speed_blocking(1, 0); break;
    case 'r': md.ramp_to_speed_blocking(1, 10); break;
    case 't': md.ramp_to_speed_blocking(1, 20); break;
  }
  md.is_overcurrent(0);
  md.is_overcurrent(1);
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

