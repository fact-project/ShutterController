#include "DualVNH5019MotorShield.h"

// Constructors ////////////////////////////////////////////////////////////////

DualVNH5019MotorShield::DualVNH5019MotorShield()
{
  //Pin map
  _INA1 = 2;
  _INB1 = 3;
  _CS1 = A4;
  _PWM1 = 5;
  _INA2 = 7;
  _INB2 = 8;
  _CS2 = A5;
  _PWM2 = 6;

  current_speed[0] = 0;
  current_speed[1] = 0;

}

// Public Methods //////////////////////////////////////////////////////////////
void DualVNH5019MotorShield::init()
{
// Define pinMode for the pins and set the frequency for timer1.

  pinMode(_INA1,OUTPUT);
  pinMode(_INB1,OUTPUT);
  pinMode(_PWM1,OUTPUT);
  pinMode(_CS1,INPUT);
  pinMode(_INA2,OUTPUT);
  pinMode(_INB2,OUTPUT);
  pinMode(_PWM2,OUTPUT);
  pinMode(_CS2,INPUT);

}

void DualVNH5019MotorShield::setM1Speed(int speed)
{
  setSpeed_any(_INA1, _INB1, _PWM1, speed);
  current_speed[0] = speed;
}

void DualVNH5019MotorShield::setM2Speed(int speed)
{
  setSpeed_any(_INA2, _INB2, _PWM2, speed);
  current_speed[1] = speed;
}

// Set speed for motor, speed is a number betwenn -255 and 255
void
DualVNH5019MotorShield::setSpeed_any (int ina, int inb, int pwm, int speed)
{
  unsigned char reverse = 0;
  if (speed < 0)
  {
    speed = -speed;  // make speed a positive quantity
    reverse = 1;  // preserve the direction
  }
  if (speed > 255)  // Max
    speed = 255;
  analogWrite(pwm, speed);
  if (speed == 0)
  {
    digitalWrite(ina,LOW);   // Make the motor coast no
    digitalWrite(inb,LOW);   // matter which direction it is spinning.
  }
  else if (reverse)
  {
    digitalWrite(ina,LOW);
    digitalWrite(inb,HIGH);
  }
  else
  {
    digitalWrite(ina,HIGH);
    digitalWrite(inb,LOW);
  }
}

void DualVNH5019MotorShield::setMotorSpeed(int motor, int speed){ // choose motor as int: 0 -> M1 and 1 -> M2
  if(motor == 0){
    setM1Speed(speed);
  } else {
    setM2Speed(speed);
  }
}

int DualVNH5019MotorShield::getMotorSpeed(int motor){
  return current_speed[motor];
}

// Set speed for motor 1 and 2
void DualVNH5019MotorShield::setSpeeds(int m1Speed, int m2Speed)
{
  setM1Speed(m1Speed);
  setM2Speed(m2Speed);
}

// Brake motor 1, brake is a number between 0 and 255
void DualVNH5019MotorShield::setM1Brake(int brake)
{
  // normalize brake
  if (brake < 0)
  {
    brake = -brake;
  }
  if (brake > 255)  // Max brake
    brake = 255;
  digitalWrite(_INA1, LOW);
  digitalWrite(_INB1, LOW);
  analogWrite(_PWM1, brake);
}

// Brake motor 2, brake is a number between 0 and 255
void DualVNH5019MotorShield::setM2Brake(int brake)
{
  // normalize brake
  if (brake < 0)
  {
    brake = -brake;
  }
  if (brake > 255)  // Max brake
    brake = 255;
  digitalWrite(_INA2, LOW);
  digitalWrite(_INB2, LOW);
  analogWrite(_PWM2, brake);
}

// Brake motor 1 and 2, brake is a number between 0 and 255
void DualVNH5019MotorShield::setBrakes(int m1Brake, int m2Brake)
{
  setM1Brake(m1Brake);
  setM2Brake(m2Brake);
}

// a factor 10 preamp has been placed, so this:
// 5V / 1024 ADC counts / 144 mV per A = 34 mA per count
// is now really this:
// 5V / 1024 ADC counts / 1440 mV per A = 3.4 mA per count

// In order to keep using integers, and avoid overflows, when multiplicating
// 1023 with 34, I convert to long .. divide by 10 .. and convert back to uint
// The error of this is below 5% as soon as we measure more than 10mA

/*
 returns the mean
 of the current consumed by the motor in units of 3.4mA per LSB.
*/
uint32_t DualVNH5019MotorShield::get_mean(int motor, int samples)
{
  if (motor == 0)
  {
    return tools::get_mean(_CS1, samples);
  }
  else
  {
    return tools::get_mean(_CS2, samples);
  }
}

/*
 returns the mean and the variance (not std deviation!)
 of the current consumed by the motor in units of 3.4mA per LSB.
*/
tools::mean_std_t DualVNH5019MotorShield::get_mean_std(int motor, uint16_t samples){
  if (motor == 0){
    return tools::get_mean_std(_CS1, samples);
  }
  else
  {
    return tools::get_mean_std(_CS2, samples);
  }
}

void DualVNH5019MotorShield::ramp_to_speed_blocking(int motor, int speed)
{
  while (getMotorSpeed(motor) != speed){
    if (getMotorSpeed(motor) < speed){
      setMotorSpeed(motor, getMotorSpeed(motor) + 1);
    } else {
      setMotorSpeed(motor, getMotorSpeed(motor) - 1);
    }
    delay(1);
  }
}

bool DualVNH5019MotorShield::is_overcurrent(int motor)
{

  if (get_mean(motor, 10) > _OverCurrent)
  {
    setMotorSpeed(motor, 0);
    return true;
  }
  else
  {
    return false;
  }

}


bool DualVNH5019MotorShield::is_zerocurrent(int motor)
{
  if (get_mean(motor, 10) < _ZeroCurrent)
  {
    setMotorSpeed(motor, 0);
    return true;
  }
  else
  {
    return false;
  }

}

