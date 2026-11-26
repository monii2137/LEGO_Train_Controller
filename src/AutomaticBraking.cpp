#include <Arduino.h>
#include "AutomaticBraking.h"

static const int TRAIN_COUNT = 3;
static const float BRAKING_DISTANCE = 30.0f;
static const int MAX_FAILED_MEASUREMENTS = 3;

static bool obstacleDetected[TRAIN_COUNT];
static bool sensorFault[TRAIN_COUNT];
static int failedMeasurements[TRAIN_COUNT];

void initializeAutomaticBraking()
{
    for (int i = 0; i < TRAIN_COUNT; i++)
    {
        obstacleDetected[i] = false;
        sensorFault[i] = true;
        failedMeasurements[i] = 0;
    }
}

void updateAutomaticBraking(int trainID, float distance)
{
    if (trainID < 1 || trainID > TRAIN_COUNT)
        return;

    int index = trainID - 1;

    if (!isfinite(distance) || distance <= 0.0f)
    {
        if (failedMeasurements[index] < MAX_FAILED_MEASUREMENTS)
            failedMeasurements[index]++;

        if (failedMeasurements[index] >= MAX_FAILED_MEASUREMENTS)
        {
            sensorFault[index] = true;

            Serial.printf("Train %d: Sensor fault!\n", trainID);
        }

        return;
    }

    failedMeasurements[index] = 0;
    sensorFault[index] = false;

    obstacleDetected[index] = (distance <= BRAKING_DISTANCE);

    Serial.printf("Train %d distance: %.2f cm\n",
                  trainID, distance);
}

bool isObstacleDetected(int trainID)
{
    if (trainID < 1 || trainID > TRAIN_COUNT)
        return true;

    return obstacleDetected[trainID - 1] ||
           sensorFault[trainID - 1];
}

bool isSensorFault(int trainID)
{
    if (trainID < 1 || trainID > TRAIN_COUNT)
        return true;

    return sensorFault[trainID - 1];
}