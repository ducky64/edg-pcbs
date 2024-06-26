#include <mbed.h>

// #include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_hal_tsc.h"
#include "stm32l4xx.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_rcc.h"
#include "stm32l4xx_ll_crs.h"
#include "stm32l4xx_ll_system.h"
#include "stm32l4xx_ll_pwr.h"
#include "stm32l4xx_ll_utils.h"
#include "stm32l4xx_ll_cortex.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_usart.h"
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_tim.h"
#include "stm32l4xx_ll_rng.h"
#include "stm32l4xx_ll_spi.h"
#include "stm32l4xx_ll_usb.h"
#include "stm32l4xx_ll_exti.h"
#include "stm32l4xx_hal_pcd.h"
#include "stm32l4xx_hal.h"


DigitalOut LedR(A1);
DigitalOut LedG(A2);
DigitalOut LedB(A3);


// COPIED FROM https://github.com/solokeys/solo1/blob/master/targets/stm32l432/src/sense.c
//
void tsc_set_electrode(uint32_t channel_ids)
{
    TSC->IOCCR = (channel_ids);
}

void tsc_start_acq(void)
{
    TSC->CR &= ~(TSC_CR_START);

    TSC->ICR = TSC_FLAG_EOA | TSC_FLAG_MCE;

    // Set IO output to output push-pull low
    TSC->CR &= (~TSC_CR_IODEF);

    TSC->CR |= TSC_CR_START;
}

void tsc_wait_on_acq(void)
{
    while ( ! (TSC->ISR & TSC_FLAG_EOA) ) ;

    if ( TSC->ISR & TSC_FLAG_MCE ) {
      volatile uint8_t failed = 1;  // TODO: can set a breakpoint here if it overflows
    }
}

uint32_t tsc_read(uint32_t indx)
{
    return TSC->IOGXCR[indx];
}

#define ELECTRODE_0     TSC_GROUP2_IO1
#define ELECTRODE_1     TSC_GROUP2_IO2

uint32_t tsc_read_button(uint32_t index)
{
    switch(index)
    {
        case 0:
            tsc_set_electrode(ELECTRODE_0);
            break;
        case 1:
            tsc_set_electrode(ELECTRODE_1);
            break;
    }
    tsc_start_acq();
    tsc_wait_on_acq();
    volatile uint32_t tsc_value = 0;  // used to be able to set a breakpoint and inspect values
    tsc_value = tsc_read(1);
    return tsc_value < 600;  // TODO: needs recalibration
}



int main() {
  LedR = 0;
  LedG = 0;
  LedB = 0;
  wait_us(500*1000);
  LedR = 1;
  LedG = 1;
  LedB = 1;

  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

  // COPIED FROM https://github.com/solokeys/solo1/blob/master/targets/stm32l432/src/sense.c
  //
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);

  LL_GPIO_InitTypeDef GPIO_InitStruct;
  // Enable TSC clock
  RCC->AHB1ENR |= (1<<16);

  /** TSC GPIO Configuration
  PA4   ------> Channel 1
  PA5   ------> Channel 2
  */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_5|LL_GPIO_PIN_4;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_9;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /** TSC GPIO Configuration
  PA6   ------> sampling cap
  */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_6;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  // Channel IOs
  uint32_t channel_ios = TSC_GROUP2_IO1 | TSC_GROUP2_IO2;

  // enable
  TSC->CR = TSC_CR_TSCE;

  TSC->CR |= (TSC_CTPH_8CYCLES |
                          TSC_CTPL_10CYCLES |
                          (uint32_t)(1 << TSC_CR_SSD_Pos) |
                          TSC_SS_PRESC_DIV1 |
                          TSC_PG_PRESC_DIV16 |
                          TSC_MCV_2047 |
                          TSC_SYNC_POLARITY_FALLING |
                          TSC_ACQ_MODE_NORMAL);

  // Schmitt trigger and hysteresis
  TSC->IOHCR = (uint32_t)(~(channel_ios | 0 | TSC_GROUP2_IO3));

  // Sampling IOs
  TSC->IOSCR = TSC_GROUP2_IO3;

  // Groups
  uint32_t grps = 0x02;
  TSC->IOGCSR = grps;

  TSC->IER &= (uint32_t)(~(TSC_IT_EOA | TSC_IT_MCE));
  TSC->ICR = (TSC_FLAG_EOA | TSC_FLAG_MCE);


  while (1) {
    LedG = tsc_read_button(0);
    LedR = tsc_read_button(1);
  }

  return 0;
}
