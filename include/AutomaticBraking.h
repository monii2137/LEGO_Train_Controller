#ifndef AUTOMATIC_BRAKING_H
#define AUTOMATIC_BRAKING_H

void initializeAutomaticBraking();

void updateAutomaticBraking(int trainID, float distance);

bool isObstacleDetected(int trainID);
bool isSensorFault(int trainID);

#endif