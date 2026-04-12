#include <SoftwareSerial.h>
#include <Servo.h>

const byte rxPin = 2;
const byte txPin = 3;
const byte pwmLeftPin = 5;
const unsigned int PWM_MIN = 1000;
const unsigned int PWM_MAX = 2000;
const unsigned int PWM_IDLE = 1500;

SoftwareSerial pippiSerial (rxPin, txPin);
Servo pippiServoLeft;

void setup()
{
  pippiSerial.begin(9600);
  pippiServoLeft.attach(pwmLeftPin, PWM_MIN, PWM_MAX);
  pippiServoLeft.writeMicroseconds(PWM_IDLE);
}

void loop()
{
  if (pippiSerial.available() > 0)
  {
    char c = pippiSerial.read();

    if (c == 'L')
    {
      int pulseLeft = pippiSerial.parseInt();

      if (pulseLeft <= PWM_MAX && pulseLeft >= PWM_MIN)
      {
        pippiServoLeft.writeMicroseconds(pulseLeft);
        pippiSerial.print("A");
      }
    }

  }
}
