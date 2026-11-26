#include <Arduino.h>
#include "UltrasonicSensor.h"

void setup()
{
    Serial.begin(115200);
    initUltrasonicSensor();

    Serial.println("Ultrasonic Sensor Test");
}

void loop()
{
    float distance = measureDistance();

    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");

    delay(500);
}