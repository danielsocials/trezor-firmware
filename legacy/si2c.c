#include <errno.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/rcc.h>
#include <stdio.h>
#include <string.h>

#include "buttons.h"
#include "common.h"
#include "si2c.h"
#include "sys.h"
#include "timer.h"
#include "usart.h"

uint8_t i2c_data_in[SI2C_BUF_MAX_LEN];
uint16_t i2c_data_inlen;
bool i2c_recv_done = false;
uint8_t i2c_data_out[SI2C_BUF_MAX_LEN];
uint32_t i2c_data_outlen;

bool bCheckNFC(void) { return sys_nfcState(); }

void i2c_slave_init(void) {
  rcc_periph_clock_enable(RCC_I2C2);
  rcc_periph_clock_enable(RCC_GPIOB);

  i2c_reset(I2C2);

  gpio_set_output_options(GPIO_SI2C_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ,
                          GPIO_SI2C_SCL | GPIO_SI2C_SDA);
  gpio_mode_setup(GPIO_SI2C_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE,
                  GPIO_SI2C_SCL | GPIO_SI2C_SDA);
  gpio_set_af(GPIO_SI2C_PORT, GPIO_AF4, GPIO_SI2C_SCL | GPIO_SI2C_SDA);
  i2c_peripheral_disable(I2C2);
  /*	//HSI is at 2Mhz */
  i2c_set_fast_mode(I2C2);
  i2c_set_speed(I2C2, i2c_speed_fm_400k, 32);
  /*	//addressing mode*/
  i2c_set_own_7bit_slave_address(I2C2, SI2C_ADDR);
  i2c_enable_ack(I2C2);

  // use interrupt
  i2c_enable_interrupt(I2C2,
                       I2C_CR2_ITBUFEN | I2C_CR2_ITEVTEN | I2C_CR2_ITERREN);

  // I2C_CR1(I2C2) |= I2C_CR1_NOSTRETCH;
  i2c_peripheral_enable(I2C2);

  // set NVIC
  nvic_set_priority(NVIC_I2C2_EV_IRQ, 0);
  nvic_enable_irq(NVIC_I2C2_EV_IRQ);

  i2c_enable_ack(I2C2);

  memset(i2c_data_out, 0x00, SI2C_BUF_MAX_LEN);
  i2c_data_outlen = 0;
}
static void i2c_delay(void) {
  volatile uint32_t i = 1000;
  while (i--)
    ;
}
void i2c2_ev_isr() {
  uint32_t sr1, sr2;
  static uint8_t dir = 0;  // 0-receive 1-send
  static uint32_t index = 0;
  sr1 = I2C_SR1(I2C2);
  if (sr1 & I2C_SR1_ADDR) {  // EV1
    sr2 = I2C_SR2(I2C2);     // clear flag
    dir = sr2 & I2C_SR2_TRA;
  }
  if (sr1 & I2C_SR1_RxNE) {  // EV2
    i2c_data_in[i2c_data_inlen++] = i2c_get_data(I2C2);
    index = 0;
  }
  if (dir & I2C_SR2_TRA) {
    if (sr1 & I2C_SR1_TxE) {  // EV3 ev3-1
      if (i2c_data_outlen > 0) {
        i2c_send_data(I2C2, i2c_data_out[index++]);
        do {
          i2c_delay();
          sr1 = I2C_SR1(I2C2);
        } while ((!(sr1 & I2C_SR1_BTF)) && (!((sr1 & I2C_SR1_AF))));
        i2c_data_outlen--;
        if (i2c_data_outlen == 0) {
          SET_COMBUS_HIGH();
        }
      } else {
        i2c_send_data(I2C2, '#');
      }
    } else if (sr1 & I2C_SR1_BTF) {
      i2c_send_data(I2C2, i2c_data_out[index++]);
    }
  }
  if (sr1 & I2C_SR1_STOPF) {  // EV4
    I2C_CR1(I2C2) |= I2C_CR1_PE;
    i2c_recv_done = true;
    SET_COMBUS_HIGH();
  }
  if (sr1 & I2C_SR1_AF) {  // EV4
    I2C_SR1(I2C2) &= ~I2C_SR1_AF;
  }
}
void i2c2_er_isr(void) {}

void i2cSlaveResponse(uint8_t *pucStr, uint32_t usStrLen) {
  uint32_t len;
  volatile uint32_t i;
  memcpy(i2c_data_out, pucStr, usStrLen);
  len = usStrLen - 64;
  if (len) {
    for (i = 0; i < (len / 64); i++) {
      memcpy(i2c_data_out + 64 + i * 63, pucStr + (i + 1) * 64 + 1, 63);
    }
  }

  i2c_data_outlen = (i2c_data_out[5] << 24) + (i2c_data_out[6] << 16) +
                    (i2c_data_out[7] << 8) + i2c_data_out[8] + 9;
  SET_COMBUS_LOW();
}
