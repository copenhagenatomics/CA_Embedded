#pragma once
#include "CAProtocol.h"

void HALundefined(const char *input);
void HALJumpToBootloader();
void CAPrintHeader();
void CAotpRead();

// analyse reason for boot and in case of SW reset jump to DFU SW update.
const char* CAonBoot();

// Generic handler for a CAProtocolCtx handler.
void CAhandleUserInputs(CAProtocolCtx* ctx, const char* startMsg);
