#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/gpio.h>
#include <string.h>

#include "bitmaps.h"
#include "oled.h"
#include "si2c.h"
#include "sys.h"
#include "timer.h"

uint8_t g_ucFlag = 0;
uint8_t g_ucBatValue = 0;

/*
 * ble mac get ble name
 */
void vCalu_BleName(uint8_t *pucMac, uint8_t *pucName) {
  uint8_t i;

  memcpy(pucName, BLE_ADV_NAME, BLE_ADV_NAME_LEN);

  for (i = 0; i < 4; i++) {
    pucName[BLE_ADV_NAME_LEN - 4 + i] += pucMac[i] % 20;
  }
}

bool sys_nfcState(void) {
  if (get_nfc_state() == 0) {
    delay_ms(1);
    if (get_nfc_state() == 0) {
      return true;
    }
  }
  return false;
}
bool sys_usbState(void) {
  if (get_usb_state()) {
    delay_ms(1);
    if (get_usb_state()) {
      return true;
    }
  }
  return false;
}

void sys_shutdown(void) {
  oledClear();
  oledDrawStringCenter(64, 30, "power off ...", FONT_STANDARD);
  oledRefresh();
  delay_ms(500);
  oledClear();
  oledRefresh();
  ble_power_off();
  stm32_power_off();
  scb_reset_system();
}

void sys_poweron(void) {
  uint32_t count = 0;

  while (1) {
    if (get_power_key_state()) {
      delay_ms(100);
      count++;
      if (count > 5) {
        oledClear();
        oledDrawStringCenter(64, 30, "power on...", FONT_STANDARD);
        oledRefresh();
        while (get_power_key_state())
          ;
        break;
      }
    } else if (sys_nfcState() || sys_usbState())
      break;
  }
  stm32_power_on();
  ble_power_on();
}
