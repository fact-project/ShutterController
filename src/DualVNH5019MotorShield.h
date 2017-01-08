#ifndef DualVNH5019MotorShield_h
#define DualVNH5019MotorShield_h

#include <Arduino.h>
#include "tools.h"

class DualVNH5019MotorShield
{
  public:
    // CONSTRUCTORS
    DualVNH5019MotorShield(); // Default pin selection.

    // PUBLIC METHODS
    void init(); // Initialize TIMER 1, set the PWM to 20kHZ.
    void setM1Speed(int speed); // Set speed for M1.
    void setM2Speed(int speed); // Set speed for M2.
    void setMotorSpeed(int motor, int speed); // choose motor as int: 0 -> M1 and 1 -> M2
    int getMotorSpeed(int motor);
    void setSpeeds(int m1Speed, int m2Speed); // Set speed for both M1 and M2.
    void setM1Brake(int brake); // Brake M1.
    void setM2Brake(int brake); // Brake M2.
    void setBrakes(int m1Brake, int m2Brake); // Brake both M1 and M2.
    uint32_t get_mean(int motor, int samples);
    tools::mean_std_t get_mean_std(int motor, uint16_t samples);
    void ramp_to_speed_blocking(int motor, int speed);
    bool is_overcurrent(int motor);
    bool is_zerocurrent(int motor);

    // Each value is the real current value in the motors;
    // Define Current Limits in [A] - Offset is 500mA for no load on the motors
    // pushing coefficient ~100 g/mA
    // currents are measured in counts, 1 count corresponds to 3.4 mA
    const unsigned int _OverCurrent = 441 ; // i.e. ~1500mA .. ~100kg load.
    const unsigned int _ZeroCurrent = 73; // i.e. ~250mA


  private:
    unsigned char _INA1;
    unsigned char _INB1;
    static const unsigned char _PWM1 = 5;
    unsigned char _CS1;
    unsigned char _INA2;
    unsigned char _INB2;
    static const unsigned char _PWM2 = 6;
    unsigned char _CS2;

    int current_speed_M1 = 0;
    int current_speed_M2 = 0;
};

#endif