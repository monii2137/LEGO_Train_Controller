#include "UltrasonicSensor.h"

const int TRIG_PIN = D0;
const int ECHO_PIN = D1;

void initUltrasonicSensor()
{
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    digitalWrite(TRIG_PIN, LOW);
}

float measureDistance()
{
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);

    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);

    digitalWrite(TRIG_PIN, LOW);

    unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000);
    Serial.print("Echo duration: ");
    Serial.print(duration);
    Serial.println(" us");

        if (duration == 0)
        {
            return -1.0f;
        }

        float distance = (duration * 0.0343f) / 2.0f;
        return distance;

        

}  