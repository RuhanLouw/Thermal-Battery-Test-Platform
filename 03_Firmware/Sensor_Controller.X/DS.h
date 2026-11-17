#ifndef DS_H
#define DS_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    DS_OK = 0,
    DS_TIMEOUT,
    DS_NO_DEVICE,
    DS_CRC_ERROR,
    DS_INVALID_DATA
} DS_error_t;

typedef struct {
    int16_t temp; /* degrees C x100 */
    DS_error_t error;
} DS_SENSOR_t;

typedef enum {
    DS_SYSTEM_IDLE,
    DS_SYSTEM_CONVERTING,
    DS_SYSTEM_READY
} ds_system_state_t;

static inline uint8_t DS_Crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0U;
    for (size_t i = 0; i < length; i++) {
        uint8_t value = data[i];
        for (uint8_t bit = 0; bit < 8U; bit++) {
            uint8_t mix = (uint8_t)((crc ^ value) & 1U);
            crc >>= 1;
            if (mix != 0U) {
                crc ^= 0x8CU;
            }
            value >>= 1;
        }
    }
    return crc;
}

static inline int16_t DS_RawToCentiDegrees(int16_t raw)
{
    int32_t scaled = (int32_t)raw * 25L;
    scaled += (scaled >= 0) ? 2L : -2L;
    return (int16_t)(scaled / 4L);
}

static inline DS_error_t DS_DecodeScratchpad(const uint8_t data[9], int16_t *temp)
{
    if (DS_Crc8(data, 8U) != data[8]) {
        return DS_CRC_ERROR;
    }
    int16_t raw = (int16_t)(((uint16_t)data[1] << 8) | data[0]);
    int16_t value = DS_RawToCentiDegrees(raw);
    if (value < -5500 || value > 12500) {
        return DS_INVALID_DATA;
    }
    *temp = value;
    return DS_OK;
}

void DS_Init(void);
void DS_StartConversion(void);
ds_system_state_t DS_GetSystemState(void);
DS_SENSOR_t DS_ReadSensor1(void);
DS_SENSOR_t DS_ReadSensor2(void);

#endif
