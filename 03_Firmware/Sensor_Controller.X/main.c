#define F_CPU 16000000UL

#include "mcc_generated_files/system/system.h"
#include "functions/definitions_sensor.h"
#include "functions/temp_sensors.h"
#include "functions/system_registers.h"
#include "functions/modbus.h"
#include "DS.h"
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdint.h>

#define NTC_SENSOR_COUNT 8U
#define NTC_SAMPLE_COUNT 20U

typedef struct {
    NTC_SENSOR_t ntcs[NTC_SENSOR_COUNT];
    KTYPE_SENSOR_t ktype;
    DS_SENSOR_t ds18b20[2];
} SYS_SENSORS;

static SYS_SENSORS sys_sensors;
static volatile uint32_t uptime_seconds;
static volatile bool sensor_poll_due;

static void initialize_sensor_values(void)
{
    for (uint8_t i = 0; i < NTC_SENSOR_COUNT; i++) {
        sys_sensors.ntcs[i].temp = SENSOR_VALUE_INVALID;
        sys_sensors.ntcs[i].raw_adc = SENSOR_RAW_INVALID;
        sys_sensors.ntcs[i].error = 1U;
    }
    sys_sensors.ktype.temp = SENSOR_VALUE_INVALID;
    sys_sensors.ktype.cold_junction = SENSOR_VALUE_INVALID;
    sys_sensors.ktype.error = KTYPE_TIMEOUT_ERROR;
    for (uint8_t i = 0; i < 2U; i++) {
        sys_sensors.ds18b20[i].temp = SENSOR_VALUE_INVALID;
        sys_sensors.ds18b20[i].error = DS_TIMEOUT;
    }
}

static void system_init(void)
{
    for (uint8_t i = 0; i < SREG_COUNT; i++) {
        sys_regs[i] = 0;
    }
    initialize_sensor_values();
    REG(SREG_FIRMWARE) = 103; /* v1.03 */
    REG(SREG_STATUS) = STATUS_POLL_ACTIVE | STATUS_SYSTEM_READY;
    REG(SREG_SENSOR_ENABLE) = SENSOR_ENABLE_DEFAULT_MASK;

    /* These legacy slots remain so the master's 33-register map stays stable. */
    REG(SREG_DHT22_TEMP) = SENSOR_VALUE_INVALID;
    REG(SREG_DHT22_HUMIDITY) = SENSOR_VALUE_INVALID;
    REG(SREG_DHT22_ERROR) = SENSOR_NOT_FITTED_ERROR;

    tempSensors_init();
    MB_Init();
    KTYPE_Init();
    DS_Init();
}

static void update_sensor_registers(void)
{
    uint16_t enabled = (uint16_t)REG(SREG_SENSOR_ENABLE);
    uint16_t ntc_errors = 0U;
    for (uint8_t i = 0; i < NTC_SENSOR_COUNT; i++) {
        REG(SREG_NTC1_TEMP + i) = sys_sensors.ntcs[i].temp;
        if ((enabled & (ENABLE_NTC1 << i)) && sys_sensors.ntcs[i].error != 0U) {
            ntc_errors |= (uint16_t)(1U << i);
        }
    }
    REG(SREG_NTC_ERROR_BITS) = (int16_t)ntc_errors;
    REG(SREG_NTC1_RAW_ADC) = (int16_t)sys_sensors.ntcs[0].raw_adc;
    REG(SREG_NTC2_RAW_ADC) = (int16_t)sys_sensors.ntcs[1].raw_adc;
    REG(SREG_NTC3_RAW_ADC) = (int16_t)sys_sensors.ntcs[2].raw_adc;
    REG(SREG_NTC4_RAW_ADC) = (int16_t)sys_sensors.ntcs[3].raw_adc;

    REG(SREG_KTYPE_TEMP) = sys_sensors.ktype.temp;
    REG(SREG_KTYPE_CJ_TEMP) = sys_sensors.ktype.cold_junction;
    REG(SREG_KTYPE_ERROR) = (int16_t)sys_sensors.ktype.error;
    REG(SREG_DS18B20_1_TEMP) = sys_sensors.ds18b20[0].temp;
    REG(SREG_DS18B20_2_TEMP) = sys_sensors.ds18b20[1].temp;
    REG(SREG_DS18B20_ERROR1) = (int16_t)sys_sensors.ds18b20[0].error;
    REG(SREG_DS18B20_ERROR2) = (int16_t)sys_sensors.ds18b20[1].error;

    REG(SREG_CHAMBER_INTERNAL) = get_chamber_temp();
    REG(SREG_HEATING_ELEMENT) = sys_sensors.ktype.temp;
    REG(SREG_EVAPORATOR) = SENSOR_VALUE_INVALID;
    REG(SREG_SUPERHEAT) = SENSOR_VALUE_INVALID;
    REG(SREG_SUBCOOLING) = SENSOR_VALUE_INVALID;
    REG(SREG_EXTERNAL) = sys_sensors.ds18b20[1].temp;

    if (ntc_errors != 0U ||
        ((enabled & ENABLE_KTYPE) && sys_sensors.ktype.error != KTYPE_OK) ||
        ((enabled & ENABLE_DS18B20_1) && sys_sensors.ds18b20[0].error != DS_OK) ||
        ((enabled & ENABLE_DS18B20_2) && sys_sensors.ds18b20[1].error != DS_OK)) {
        REG(SREG_STATUS) |= STATUS_ERROR_PRESENT;
    } else {
        REG(SREG_STATUS) &= (int16_t)~STATUS_ERROR_PRESENT;
    }
}

static void poll_sensors(void)
{
    if (!sensor_poll_due || !(REG(SREG_STATUS) & STATUS_POLL_ACTIVE)) {
        return;
    }
    sensor_poll_due = false;
    MEASURE_LED_SET();

    for (uint8_t i = 0; i < NTC_SENSOR_COUNT; i++) {
        if (REG(SREG_SENSOR_ENABLE) & (ENABLE_NTC1 << i)) {
            sys_sensors.ntcs[i] = read_ntc((uint8_t)(i + 1U), NTC_SAMPLE_COUNT);
        } else {
            sys_sensors.ntcs[i].temp = SENSOR_VALUE_INVALID;
            sys_sensors.ntcs[i].raw_adc = SENSOR_RAW_INVALID;
            sys_sensors.ntcs[i].error = SENSOR_NOT_FITTED_ERROR;
        }
    }
    if ((REG(SREG_SENSOR_ENABLE) & ENABLE_KTYPE) &&
        KTYPE_check_state() == KTYPE_READ_READY) {
        sys_sensors.ktype = read_ktype();
        KTYPE_start_conversion();
    }
    if (DS_GetSystemState() == DS_SYSTEM_READY) {
        if (REG(SREG_SENSOR_ENABLE) & ENABLE_DS18B20_1) {
            sys_sensors.ds18b20[0] = DS_ReadSensor1();
        }
        if (REG(SREG_SENSOR_ENABLE) & ENABLE_DS18B20_2) {
            sys_sensors.ds18b20[1] = DS_ReadSensor2();
        }
        DS_StartConversion();
    }

    update_sensor_registers();
    MEASURE_LED_nSET();
}

static void pit_callback(void)
{
    RUN_LED_TOGGLE();
    uptime_seconds++;
    REG(SREG_UPTIME_L) = (int16_t)(uptime_seconds & 0xFFFFUL);
    REG(SREG_UPTIME_H) = (int16_t)(uptime_seconds >> 16);
    sensor_poll_due = true;
}

int main(void)
{
    SYSTEM_Initialize();
    RTC_SetPITIsrCallback(pit_callback);
    system_init();
    RS485_RX_ENABLE();
    RUN_LED_SET();
    sei();

    while (1) {
        modbus_process();
        poll_sensors();
    }
}
