#include "../functions/system_registers.h"
#include <assert.h>

int main(void)
{
    assert(SREG_LEGACY_COUNT == 33U);
    assert(SREG_COUNT == 37U);
    assert(SREG_DHT22_TEMP == 0x0019);
    assert(SREG_DHT22_ERROR == 0x001B);
    assert(SREG_DS18B20_1_TEMP == 0x001C);
    assert(SREG_DS18B20_2_TEMP == 0x001D);
    assert(SREG_DS18B20_ERROR2 == 0x001F);
    assert(SREG_RESERVED_START == 0x0020);
    assert(SREG_NTC1_RAW_ADC == 0x0021);
    assert(SREG_NTC4_RAW_ADC == 0x0024);
    assert((SENSOR_ENABLE_SUPPORTED_MASK & ENABLE_DHT11) == 0U);
    assert((SENSOR_ENABLE_SUPPORTED_MASK & ENABLE_DS18B20_1) != 0U);
    assert((SENSOR_ENABLE_SUPPORTED_MASK & ENABLE_DS18B20_2) != 0U);
    assert(SENSOR_ENABLE_DEFAULT_MASK == 0x080FU);
    return 0;
}
