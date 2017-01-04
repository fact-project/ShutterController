#include "DualVNH5019MotorShield.h"

// Constructors ////////////////////////////////////////////////////////////////

DualVNH5019MotorShield::DualVNH5019MotorShield()
{
  //Pin map
  _INA1 = 2;
  _INB1 = 4;
  _CS1 = A0;
  _INA2 = 7;
  _INB2 = 8;
  _CS2 = A1;
}

DualVNH5019MotorShield::DualVNH5019MotorShield(unsigned char INA1, unsigned char INB1, unsigned char CS1,
                                               unsigned char INA2, unsigned char INB2, unsigned char CS2)
{
  //Pin map
  //PWM1 and PWM2 cannot be remapped because the library assumes PWM is on timer1
  _INA1 = INA1;
  _INB1 = INB1;
  _CS1 = CS1;
  _INA2 = INA2;
  _INB2 = INB2;
  _CS2 = CS2;
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
  #if defined(__AVR_ATmega168__)|| defined(__AVR_ATmega328P__) || defined(__AVR_ATmega32U4__)
  /*
  // Timer 1 configuration
  // prescaler: clockI/O / 1
  // outputs enabled
  // phase-correct PWM
  // top of 255
  //
  // PWM frequency calculation
  // 16MHz / 1 (prescaler) / 2 (phase-correct) / 400 (top) = 20kHz
  TCCR1A = 0b10100000;
  TCCR1B = 0b00010001;
  ICR1 = 255;
  */
  // Timer 0 configuration
  // prescaler: clockI/O / 1
  // outputs enabled
  // phase-correct PWM
  //
  // PWM frequency calculation
  // 16MHz / 1 (prescaler) / 2 (phase-correct) / 255 = 31kHz
  TCCR0A = 0b10100001;
  TCCR0B = 0b00000001;


  #endif
}
// Set speed for motor 1, speed is a number betwenn -255 and 255
void DualVNH5019MotorShield::setM1Speed(int speed)
{
  unsigned char reverse = 0;
  current_speed_M1 = speed;
  if (speed < 0)
  {
    speed = -speed;  // Make speed a positive quantity
    reverse = 1;  // Preserve the direction
  }
  if (speed > 255)  // Max PWM dutycycle
    speed = 255;
  #if defined(__AVR_ATmega168__)|| defined(__AVR_ATmega328P__) || defined(__AVR_ATmega32U4__)
  OCR0B = speed;
  #else
  analogWrite(_PWM1, speed);
  #endif
  if (speed == 0)
  {
    digitalWrite(_INA1,LOW);   // Make the motor coast no
    digitalWrite(_INB1,LOW);   // matter which direction it is spinning.
  }
  else if (reverse)
  {
    digitalWrite(_INA1,LOW);
    digitalWrite(_INB1,HIGH);
  }
  else
  {
    digitalWrite(_INA1,HIGH);
    digitalWrite(_INB1,LOW);
  }
}

// Set speed for motor 2, speed is a number betwenn -255 and 255
void DualVNH5019MotorShield::setM2Speed(int speed)
{
  unsigned char reverse = 0;
  current_speed_M2 = speed;
  if (speed < 0)
  {
    speed = -speed;  // make speed a positive quantity
    reverse = 1;  // preserve the direction
  }
  if (speed > 255)  // Max
    speed = 255;
  #if defined(__AVR_ATmega168__)|| defined(__AVR_ATmega328P__) || defined(__AVR_ATmega32U4__)
  OCR0A = speed;
  #else
  analogWrite(_PWM2, speed);
  #endif
  if (speed == 0)
  {
    digitalWrite(_INA2,LOW);   // Make the motor coast no
    digitalWrite(_INB2,LOW);   // matter which direction it is spinning.
  }
  else if (reverse)
  {
    digitalWrite(_INA2,LOW);
    digitalWrite(_INB2,HIGH);
  }
  else
  {
    digitalWrite(_INA2,HIGH);
    digitalWrite(_INB2,LOW);
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
  if(motor == 0){
    return current_speed_M1;
  } else {
    return current_speed_M2;
  }
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
  #if defined(__AVR_ATmega168__)|| defined(__AVR_ATmega328P__) || defined(__AVR_ATmega32U4__)
  OCR0B = brake;
  #else
  analogWrite(_PWM1, brake);
  #endif
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
  #if defined(__AVR_ATmega168__)|| defined(__AVR_ATmega328P__) || defined(__AVR_ATmega32U4__)
  OCR0A = brake;
  #else
  analogWrite(_PWM2, brake);
  #endif
}

// Brake motor 1 and 2, brake is a number between 0 and 255
void DualVNH5019MotorShield::setBrakes(int m1Brake, int m2Brake)
{
  setM1Brake(m1Brake);
  setM2Brake(m2Brake);
}

// Return motor 1 current value in milliamps.
unsigned int DualVNH5019MotorShield::getM1CurrentMilliamps()
{
  // 5V / 1024 ADC counts / 144 mV per A = 34 mA per count
  return analogRead(_CS1) * 34;
}

// Return motor 2 current value in milliamps.
unsigned int DualVNH5019MotorShield::getM2CurrentMilliamps()
{
  // 5V / 1024 ADC counts / 144 mV per A = 34 mA per count
  return analogRead(_CS2) * 34;
}

unsigned int DualVNH5019MotorShield::getCurrentMilliamps(int motor)
{
  if (motor == 0) {
    return getM1CurrentMilliamps();
  } else {
    return getM2CurrentMilliamps();
  }
}

unsigned int DualVNH5019MotorShield::get_mean(int motor, int samples)
{
  unsigned int tmp = 0;
  for (int j=0;j<samples;j++) {
    tmp += getCurrentMilliamps(motor);
  }
  return tmp/samples;
}

tools::mean_std_t DualVNH5019MotorShield::get_mean_std(int motor, int samples){
  tools::mean_std_t tmp;
  tmp.mean = 0.;
  tmp.std = 0.;

  unsigned int foo;
  for (int i=0; i<samples; i++)
  {
    foo = getCurrentMilliamps(motor);
    tmp.mean += foo;
    tmp.std += foo * foo;
  }
  tmp.mean /= samples;
  tmp.std = sqrt(tmp.std / samples - tmp.mean * tmp.mean);

  return tmp;
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

