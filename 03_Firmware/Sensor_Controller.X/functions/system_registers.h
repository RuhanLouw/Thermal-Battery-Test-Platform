#ifndef SYSTEM_REGISTERS_H
#define SYSTEM_REGISTERS_H

#include <stdint.h>

/* The original master contract remains fixed at 0x0000..0x0020. */
#define SREG_LEGACY_COUNT       33U
#define SREG_COUNT              37U
#define SENSOR_VALUE_INVALID INT16_MIN
#define SENSOR_RAW_INVALID      UINT16_MAX
#define SENSOR_NOT_FITTED_ERROR 1

extern volatile int16_t sys_regs[SREG_COUNT];
#define REG(address) sys_regs[(address)]

#define SREG_FIRMWARE           0x0000
#define SREG_UPTIME_L           0x0001
#define SREG_UPTIME_H           0x0002
#define SREG_STATUS             0x0003
#define SREG_SENSOR_ENABLE      0x0004
#define SREG_CHAMBER_INTERNAL   0x0005 /* degrees C x10 */
#define SREG_HEATING_ELEMENT    0x0006 /* degrees C x100 */
#define SREG_EVAPORATOR         0x0007 /* degrees C x100 */
#define SREG_SUPERHEAT          0x0008 /* degrees C x10 */
#define SREG_SUBCOOLING         0x0009 /* degrees C x10 */
#define SREG_COMMAND            0x000A
#define SREG_LOG_INTERVAL       0x000B
#define SREG_EXTERNAL           0x000C /* degrees C x100 */
#define SREG_NTC1_TEMP          0x000D
#define SREG_NTC2_TEMP          0x000E
#define SREG_NTC3_TEMP          0x000F
#define SREG_NTC4_TEMP          0x0010
#define SREG_NTC5_TEMP          0x0011
#define SREG_NTC6_TEMP          0x0012
#define SREG_NTC7_TEMP          0x0013
#define SREG_NTC8_TEMP          0x0014
#define SREG_NTC_ERROR_BITS     0x0015
#define SREG_KTYPE_TEMP         0x0016 /* degrees C x100 */
#define SREG_KTYPE_CJ_TEMP      0x0017 /* degrees C x100 */
#define SREG_KTYPE_ERROR        0x0018

/* Reserved legacy DHT addresses; no DHT sensor is fitted. */
#define SREG_DHT22_TEMP         0x0019
#define SREG_DHT22_HUMIDITY     0x001A
#define SREG_DHT22_ERROR        0x001B

#define SREG_DS18B20_1_TEMP     0x001C /* degrees C x100 */
#define SREG_DS18B20_2_TEMP     0x001D /* degrees C x100 */
#define SREG_DS18B20_ERROR1     0x001E
#define SREG_DS18B20_ERROR2     0x001F
#define SREG_RESERVED_START     0x0020

/* Calibration data appended after the legacy register block. */
#define SREG_NTC1_RAW_ADC       0x0021 /* averaged 12-bit ADC count */
#define SREG_NTC2_RAW_ADC       0x0022 /* averaged 12-bit ADC count */
#define SREG_NTC3_RAW_ADC       0x0023 /* averaged 12-bit ADC count */
#define SREG_NTC4_RAW_ADC       0x0024 /* averaged 12-bit ADC count */

#define STATUS_POLL_ACTIVE      (1U << 0)
#define STATUS_SYSTEM_READY     (1U << 1)
#define STATUS_ERROR_PRESENT    (1U << 2)
#define STATUS_LOGGING_ACTIVE   (1U << 3)
#define STATUS_RS485_OK         (1U << 4)

#define ENABLE_NTC1             (1U << 0)
#define ENABLE_NTC2             (1U << 1)
#define ENABLE_NTC3             (1U << 2)
#define ENABLE_NTC4             (1U << 3)
#define ENABLE_NTC5             (1U << 4)
#define ENABLE_NTC6             (1U << 5)
#define ENABLE_NTC7             (1U << 6)
#define ENABLE_NTC8             (1U << 7)
#define ENABLE_KTYPE            (1U << 8)
#define ENABLE_DHT11            (1U << 9) /* reserved; hardware not fitted */
#define ENABLE_DS18B20_1        (1U << 10)
#define ENABLE_DS18B20_2        (1U << 11)

#define SENSOR_ENABLE_SUPPORTED_MASK ( \
    ENABLE_NTC1 | ENABLE_NTC2 | ENABLE_NTC3 | ENABLE_NTC4 | \
    ENABLE_NTC5 | ENABLE_NTC6 | ENABLE_NTC7 | ENABLE_NTC8 | \
    ENABLE_KTYPE | ENABLE_DS18B20_1 | ENABLE_DS18B20_2)

#define SENSOR_ENABLE_DEFAULT_MASK ( \
    ENABLE_NTC1 | ENABLE_NTC2 | ENABLE_NTC3 | ENABLE_NTC4 | \
    ENABLE_DS18B20_2)

void SYS_REGS_INIT(void);
void enable_all_sensors(void);
void enable_sensor(uint16_t flag);
void disable_sensor(uint16_t flag);

#endif
