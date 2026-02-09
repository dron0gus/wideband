#pragma once

#include <cstdint>

#include "hal.h"
#include "port.h"

void SendLinkAfrFormat(Configuration* configuration, uint8_t ch);
void SendLinkEgtFormat(Configuration* configuration, uint8_t ch);

void ProcessLinkCanMessage(const CANRxFrame* msg, Configuration* configuration, struct CanStatusData* statusData);