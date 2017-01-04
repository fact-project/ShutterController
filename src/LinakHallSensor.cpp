#include "LinakHallSensor.h"

LinakHallSensor::LinakHallSensor(unsigned char S1, unsigned char S2)
{
  _S1 = S1;
  _S2 = S2;
}

void LinakHallSensor::init()
{
  pinMode(_S1, INPUT);
  pinMode(_S2, INPUT);
}

unsigned int LinakHallSensor::getS1()
{
  return analogRead(_S1);
}

unsigned int LinakHallSensor::getS2()
{
  return analogRead(_S2);
}

unsigned int LinakHallSensor::get(int sensor_id)
{
  if (sensor_id==0){
    return getS1();
  } else {
    return getS2();
  }
}

double LinakHallSensor::get_mean(int motor, int samples)
{
  double tmp = 0;
  for (int j=0;j<samples;j++) {
    tmp += get(motor);
  }
  return tmp/samples;
}