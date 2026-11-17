#ifndef CONTROL_APPLICATION_H
#define CONTROL_APPLICATION_H

#include <stdbool.h>
#include <stdint.h>

#define CONTROL_PI_TIMEOUT_MS 15000UL

void ControlApplication_Initialize(void);
void ControlApplication_Step1s(void);
void ControlApplication_OnPiRequest(uint32_t now_ms);
bool ControlApplication_CheckPiTimeout(uint32_t now_ms);

#endif
