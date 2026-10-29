#include "status.h"
#include "can.h"

void SetStatus(int, wbo::Status)
{
}

float testRemoteBatteryVoltage = 13.7f;

float GetRemoteBatteryVoltage()
{
    return testRemoteBatteryVoltage;
}
