#ifndef DualVNH5019MotorShield_h
#define DualVNH5019MotorShield_h

#include <Arduino.h>
#include "tools.h"

class DualVNH5019MotorShield
{
  public:

    DualVNH5019MotorShield();

    void init();
    void setMotorSpeed(int motor, int speed); // choose motor as int: 0 -> M1 and 1 -> M2
    int getMotorSpeed(int motor);
    uint32_t get_mean(int motor, int samples);
    tools::mean_std_t get_mean_std(int motor, uint16_t samples);
    void ramp_to_speed_blocking(int motor, int speed);
    bool is_overcurrent(int motor);
    bool is_zerocurrent(int motor);

    // Each value is the real current value in the motors;
    // Define Current Limits in [A] - Offset is 500mA for no load on the motors
    // pushing coefficient ~100 g/mA
    // currents are measured in counts, 1 count corresponds to 3.4 mA
    const unsigned int _OverCurrent = 441 ;
    const unsigned int _ZeroCurrent = 100;


  private:
    void setSpeed_any (int ina, int inb, int pwm, int speed);

    unsigned char _INA1;
    unsigned char _INB1;
    unsigned char _PWM1;
    unsigned char _CS1;
    unsigned char _INA2;
    unsigned char _INB2;
    unsigned char _PWM2;
    unsigned char _CS2;

    int current_speed[2];
};

#endif
