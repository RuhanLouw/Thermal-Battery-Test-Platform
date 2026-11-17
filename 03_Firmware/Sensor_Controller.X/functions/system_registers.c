#include "system_registers.h"

volatile int16_t sys_regs[SREG_COUNT] = {0};

void SYS_REGS_INIT(void)
{
    for (uint8_t i = 0; i < SREG_COUNT; i++) {
        sys_regs[i] = 0;
    }
}

void enable_all_sensors(void)
{
    REG(SREG_SENSOR_ENABLE) = SENSOR_ENABLE_SUPPORTED_MASK;
}

void enable_sensor(uint16_t flag)
{
    REG(SREG_SENSOR_ENABLE) |= (int16_t)(flag & SENSOR_ENABLE_SUPPORTED_MASK);
}

void disable_sensor(uint16_t flag)
{
    REG(SREG_SENSOR_ENABLE) &= (int16_t)~flag;
}
