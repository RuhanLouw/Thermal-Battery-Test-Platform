#ifndef TEMP_SENSORS_H
#define TEMP_SENSORS_H

#include "../mcc_generated_files/system/pins.h"
#include "../mcc_generated_files/system/port.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define V_REF 3.0f
#define devider_12b 4096.0f
#define R_FIXED 10000.0f
#define R_25 10000.0f
#define BETA 3977.0f
#define T_25 298.15f

#define _A() IO_PC2_SetHigh()
#define _B() IO_PF5_SetHigh()
#define _C() IO_PF4_SetHigh()
#define _nA() IO_PC2_SetLow()
#define _nB() IO_PF5_SetLow()
#define _nC() IO_PF4_SetLow()
#define SCK_HIGH() IO_PA6_SetHigh()
#define SCK_LOW() IO_PA6_SetLow()
#define READ_MISO() IO_PA5_GetValue()
#define ENABLE_NTC_MUX() IO_PD7_SetHigh()
#define DISABLE_NTC_MUX() IO_PD7_SetLow()
#define ENABLE_KTYPE_PIN() IO_PD1_SetLow()
#define DISABLE_KTYPE_PIN() IO_PD1_SetHigh()

typedef enum {
    KTYPE_IDLE = 0,
    KTYPE_CONVERTING,
    KTYPE_READ_READY,
    KTYPE_TIMEOUT
} KTYPE_STATE_t;

typedef enum {
    KTYPE_OK = 0,
    KTYPE_SCV,
    KTYPE_SCG,
    KTYPE_OC,
    KTYPE_TIMEOUT_ERROR
} KTYPE_ERROR_t;

typedef struct {
    int16_t temp; /* degrees C x10 */
    uint16_t raw_adc; /* averaged 12-bit ADC count */
    uint8_t error;
} NTC_SENSOR_t;

typedef struct {
    int16_t temp;          /* degrees C x100 */
    int16_t cold_junction; /* degrees C x100 */
    KTYPE_ERROR_t error;
} KTYPE_SENSOR_t;

void tempSensors_init(void);
void CS_NTC(uint8_t ntc_num);
uint16_t mcp3201_read_bitbang(uint8_t ntc_num);
float readAvg_NTC(uint8_t ntc_num, uint8_t num_reads);
float getTempFromRaw_NTC(float raw_adc);
float getTemp_NTC(uint8_t ntc_num, uint8_t num_reads);
NTC_SENSOR_t read_ntc(uint8_t ntc_number, uint8_t ntc_poll_number);

KTYPE_STATE_t KTYPE_start_conversion(void);
KTYPE_STATE_t KTYPE_check_state(void);
void KTYPE_bitbang(uint8_t *buffer);
KTYPE_ERROR_t readKTypeSensor(float *thermo, float *junc);
KTYPE_SENSOR_t read_ktype(void);
void KTYPE_Init(void);
void KTYPE_timer_CapCallBack(void);

int16_t get_chamber_temp(void);
int16_t get_superheat_temp(void);
int16_t get_subcool_temp(void);

#endif
