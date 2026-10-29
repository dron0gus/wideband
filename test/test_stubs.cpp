#include "fault.h"
#include "can.h"

void SetFault(int, wbo::Fault)
{
}

float testRemoteBatteryVoltage = 13.7f;

float GetRemoteBatteryVoltage()
{
    return testRemoteBatteryVoltage;
}
