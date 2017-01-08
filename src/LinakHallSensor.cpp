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

uint32_t LinakHallSensor::get_mean(int motor, int samples)
{
  if (motor == 0){
    return tools::get_mean(_S1, samples);
  }
  else
  {
    return tools::get_mean(_S2, samples);
  }
}

tools::mean_std_t LinakHallSensor::get_mean_std(int motor, int samples)
{
  if (motor == 0){
    return tools::get_mean_std(_S1, samples);
  }
  else
  {
    return tools::get_mean_std(_S2, samples);
  }
}
