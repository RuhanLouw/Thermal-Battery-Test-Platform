#ifndef MODBUS_H
#define MODBUS_H

void MB_Init(void);
void modbus_process(void);
void RS485_TX_ENABLE(void);
void RS485_RX_ENABLE(void);
void modbus_receive_isr(void);
void modbus_transmit_isr(void);
void modbus_timer_isr(void);

#endif
