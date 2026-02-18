#pragma once

#include <cstdint>

#include "hal.h"
#include "port.h"

void SendRusefiFormat(Configuration* configuration, uint8_t ch);
void SendRusefiEgtFormat(Configuration* configuration);

void ProcessRusefiCanMessage(const CANRxFrame* msg, Configuration* configuration, struct CanStatusData* statusData);