#include "../DS.h"
#include <assert.h>
#include <stdint.h>

int main(void)
{
    const uint8_t valid[9] = {
        0x50U, 0x05U, 0x4BU, 0x46U, 0x7FU, 0xFFU, 0x0CU, 0x10U, 0x1CU
    };
    uint8_t corrupt[9] = {
        0x50U, 0x05U, 0x4BU, 0x46U, 0x7FU, 0xFFU, 0x0CU, 0x10U, 0x1DU
    };
    int16_t temperature = 0;

    assert(DS_Crc8(valid, 8U) == valid[8]);
    assert(DS_DecodeScratchpad(valid, &temperature) == DS_OK);
    assert(temperature == 8500);
    assert(DS_DecodeScratchpad(corrupt, &temperature) == DS_CRC_ERROR);
    assert(DS_RawToCentiDegrees(401) == 2506);
    assert(DS_RawToCentiDegrees(-162) == -1013);
    assert(DS_RawToCentiDegrees(1360) == 8500);
    return 0;
}
