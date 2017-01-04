#ifndef LinakHallSensor_h
#define LinakHallSensor_h

#include <Arduino.h>
#include "tools.h"

class LinakHallSensor
{
  public:
    // User-defined pin selection.
    LinakHallSensor(unsigned char S1=A2, unsigned char S2=A3);

    void init();
    unsigned int getS1();
    unsigned int getS2();
    unsigned int get(int sensor_id);
    double get_mean(int motor, int samples);
    tools::mean_std_t get_mean_std(int motor, int samples);

  private:
    unsigned char _S1;
    unsigned char _S2;
};

#endif // LinakHallSensor_h