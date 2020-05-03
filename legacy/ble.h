#ifndef __BLE_H__
#define __BLE_H__

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

bool ble_connect_state(void);
bool ble_name_state(void);
uint8_t *ble_get_name(void);
void ble_request_name(void);
void ble_reset(void);
void ble_uart_poll(void);

#endif
