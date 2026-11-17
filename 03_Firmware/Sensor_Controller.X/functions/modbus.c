#define F_CPU 16000000UL

#include "modbus.h"
#include "definitions_sensor.h"
#include "system_registers.h"
#include "../mcc_generated_files/timer/tcb0.h"
#include "../mcc_generated_files/uart/usart1.h"
#include <stdbool.h>
#include <stdint.h>
#include <util/atomic.h>
#include <util/crc16.h>
#include <util/delay.h>

#define SLAVE_ADDRESS 2U
#define FC_READ_HOLDING_REGISTERS 0x03U
#define FC_READ_INPUT_REGISTERS 0x04U
#define FC_WRITE_MULTIPLE_REGISTERS 0x10U
#define EX_ILLEGAL_FUNCTION 0x01U
#define EX_ILLEGAL_DATA_ADDRESS 0x02U
#define EX_ILLEGAL_DATA_VALUE 0x03U
#define MAX_FRAME_SIZE 256U

static uint8_t rx_buffer[MAX_FRAME_SIZE];
static uint8_t tx_buffer[MAX_FRAME_SIZE];
static volatile uint16_t rx_index;
static volatile bool frame_ready;
static volatile bool rx_overflow;

static uint16_t modbus_crc16(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFFU;
    for (uint16_t i = 0; i < length; i++) {
        crc = _crc16_update(crc, data[i]);
    }
    return crc;
}

void RS485_TX_ENABLE(void)
{
    IO_PC3_SetHigh();
    _delay_us(1);
}

void RS485_RX_ENABLE(void)
{
    IO_PC3_SetLow();
}

void MB_Init(void)
{
    TCB0_Stop();
    UART1_RxCompleteCallbackRegister(modbus_receive_isr);
    UART1_TxCompleteCallbackRegister(modbus_transmit_isr);
    TCB0_CaptureCallbackRegister(modbus_timer_isr);
    TCB0_CAPTInterruptEnable();
    rx_index = 0U;
    frame_ready = false;
    rx_overflow = false;
    RS485_RX_ENABLE();
}

static void modbus_send_response(uint16_t length)
{
    uint16_t crc = modbus_crc16(tx_buffer, length);
    tx_buffer[length] = (uint8_t)(crc & 0xFFU);
    tx_buffer[length + 1U] = (uint8_t)(crc >> 8);

    TCB0_Stop();
    TX1_LED_SET();
    USART1.STATUS = USART_TXCIF_bm;
    RS485_TX_ENABLE();
    for (uint16_t i = 0; i < length + 2U; i++) {
        while (!UART1_IsTxReady()) {
        }
        UART1_Write(tx_buffer[i]);
    }
}

static void modbus_send_exception(uint8_t function, uint8_t code)
{
    tx_buffer[0] = SLAVE_ADDRESS;
    tx_buffer[1] = function | 0x80U;
    tx_buffer[2] = code;
    modbus_send_response(3U);
}

static void handle_read_registers(uint8_t function)
{
    uint16_t start = ((uint16_t)rx_buffer[2] << 8) | rx_buffer[3];
    uint16_t count = ((uint16_t)rx_buffer[4] << 8) | rx_buffer[5];

    if (count == 0U || count > 125U) {
        modbus_send_exception(function, EX_ILLEGAL_DATA_VALUE);
        return;
    }
    if (start >= SREG_COUNT || count > (uint16_t)(SREG_COUNT - start)) {
        modbus_send_exception(function, EX_ILLEGAL_DATA_ADDRESS);
        return;
    }

    tx_buffer[0] = SLAVE_ADDRESS;
    tx_buffer[1] = function;
    tx_buffer[2] = (uint8_t)(count * 2U);
    for (uint16_t i = 0; i < count; i++) {
        uint16_t value;
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            value = (uint16_t)REG(start + i);
        }
        tx_buffer[3U + i * 2U] = (uint8_t)(value >> 8);
        tx_buffer[4U + i * 2U] = (uint8_t)(value & 0xFFU);
    }
    modbus_send_response(3U + count * 2U);
}

static bool register_is_writable(uint16_t address, uint16_t value)
{
    if (address == SREG_STATUS) {
        return (value & (uint16_t)~STATUS_POLL_ACTIVE) == 0U;
    }
    if (address == SREG_SENSOR_ENABLE) {
        return (value & (uint16_t)~SENSOR_ENABLE_SUPPORTED_MASK) == 0U;
    }
    return address == SREG_LOG_INTERVAL;
}

static void write_register(uint16_t address, uint16_t value)
{
    if (address == SREG_STATUS) {
        REG(SREG_STATUS) = (int16_t)((REG(SREG_STATUS) & ~STATUS_POLL_ACTIVE) |
                                    (value & STATUS_POLL_ACTIVE));
    } else {
        REG(address) = (int16_t)value;
    }
}

static void handle_write_multiple_registers(uint16_t frame_length)
{
    uint16_t start = ((uint16_t)rx_buffer[2] << 8) | rx_buffer[3];
    uint16_t count = ((uint16_t)rx_buffer[4] << 8) | rx_buffer[5];
    uint8_t byte_count = rx_buffer[6];

    if (count == 0U || count > 123U || byte_count != count * 2U ||
        frame_length != 9U + byte_count) {
        modbus_send_exception(FC_WRITE_MULTIPLE_REGISTERS, EX_ILLEGAL_DATA_VALUE);
        return;
    }
    if (start >= SREG_COUNT || count > (uint16_t)(SREG_COUNT - start)) {
        modbus_send_exception(FC_WRITE_MULTIPLE_REGISTERS, EX_ILLEGAL_DATA_ADDRESS);
        return;
    }

    for (uint16_t i = 0; i < count; i++) {
        uint16_t value = ((uint16_t)rx_buffer[7U + i * 2U] << 8) |
                         rx_buffer[8U + i * 2U];
        if (!register_is_writable(start + i, value)) {
            modbus_send_exception(FC_WRITE_MULTIPLE_REGISTERS, EX_ILLEGAL_DATA_VALUE);
            return;
        }
    }
    for (uint16_t i = 0; i < count; i++) {
        uint16_t value = ((uint16_t)rx_buffer[7U + i * 2U] << 8) |
                         rx_buffer[8U + i * 2U];
        write_register(start + i, value);
    }

    for (uint8_t i = 0; i < 6U; i++) {
        tx_buffer[i] = rx_buffer[i];
    }
    modbus_send_response(6U);
}

void modbus_process(void)
{
    uint16_t length = 0U;
    bool overflow = false;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if (!frame_ready) {
            length = 0U;
            overflow = false;
        } else {
            length = rx_index;
            overflow = rx_overflow;
        }
    }
    if (length == 0U) {
        return;
    }

    if (!overflow && length >= 4U && rx_buffer[0] == SLAVE_ADDRESS) {
        uint16_t received_crc = rx_buffer[length - 2U] |
                                ((uint16_t)rx_buffer[length - 1U] << 8);
        if (received_crc == modbus_crc16(rx_buffer, length - 2U)) {
            uint8_t function = rx_buffer[1];
            if ((function == FC_READ_HOLDING_REGISTERS ||
                 function == FC_READ_INPUT_REGISTERS) && length == 8U) {
                handle_read_registers(function);
            } else if (function == FC_WRITE_MULTIPLE_REGISTERS && length >= 11U) {
                handle_write_multiple_registers(length);
            } else {
                modbus_send_exception(function, EX_ILLEGAL_FUNCTION);
            }
        } else {
            ERROR_LED_TOGGLE();
        }
    }

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        rx_index = 0U;
        rx_overflow = false;
        frame_ready = false;
    }
}

void modbus_receive_isr(void)
{
    uint8_t value = UART1_Read();
    if (!frame_ready) {
        if (rx_index < MAX_FRAME_SIZE) {
            rx_buffer[rx_index++] = value;
        } else {
            rx_overflow = true;
        }
    }
    RX1_LED_SET();
    TCB0_Start();
    TCB0_CounterSet(0U);
}

void modbus_transmit_isr(void)
{
    RS485_RX_ENABLE();
    TX1_LED_nSET();
}

void modbus_timer_isr(void)
{
    TCB0_Stop();
    if (rx_index > 0U || rx_overflow) {
        frame_ready = true;
    }
    RX1_LED_nSET();
}
