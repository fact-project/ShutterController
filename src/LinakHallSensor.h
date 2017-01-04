#ifndef LinakHallSensor_h
#define LinakHallSensor_h

#include <Arduino.h>

class LinakHallSensor
{
  public:
    // User-defined pin selection.
    LinakHallSensor(unsigned char S1=A2, unsigned char S2=A3);

    void init();
    unsigned int getS1();
    unsigned int getS2();
    unsigned int get(int sensor_id);


  private:
    unsigned char _S1;
    unsigned char _S2;
};

#endif // LinakHallSensor_h