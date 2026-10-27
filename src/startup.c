/*
 * Startup code: vector table + Reset_Handler.
 * Vector table layout: PM0056 §2.3.4 "Vector table" (Cortex-M3 system exceptions)
 * followed by the device IRQs from RM0008 §10.1.2.
 */
#include <stdint.h>

/* Symbols defined in stm32f103.ld */
extern uint32_t _estack;
extern uint32_t _sidata, _sdata, _edata;
extern uint32_t _sbss, _ebss;

int main(void);

void Reset_Handler(void);
void SysTick_Handler(void);          /* systick.c */

/* Any unexpected exception lands here; halt so it's obvious in GDB. */
void Default_Handler(void)
{
    for (;;) {
    }
}

/* Weak: i2c_hw.c overrides it; with the bit-bang driver it falls back to
 * Default_Handler (never fires, the DMA interrupt is never enabled there). */
void DMA1_Channel6_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));

/* Index = exception number. 0 is the initial stack pointer, not a handler.
 * 16.. are device IRQs for medium-density STM32F103 (RM0008 §10.1.2 Table 63,
 * IRQ 0..42). Unused ones go to Default_Handler; a stray enabled IRQ halts there. */
__attribute__((section(".isr_vector"), used))
static void (*const vector_table[16 + 43])(void) = {
    (void (*)(void))&_estack, /*  0: initial SP              */
    Reset_Handler,            /*  1: Reset                   */
    Default_Handler,          /*  2: NMI                     */
    Default_Handler,          /*  3: HardFault               */
    Default_Handler,          /*  4: MemManage               */
    Default_Handler,          /*  5: BusFault                */
    Default_Handler,          /*  6: UsageFault              */
    0, 0, 0, 0,               /*  7-10: reserved             */
    Default_Handler,          /* 11: SVCall                  */
    Default_Handler,          /* 12: DebugMonitor            */
    0,                        /* 13: reserved                */
    Default_Handler,          /* 14: PendSV                  */
    SysTick_Handler,          /* 15: SysTick                 */
    Default_Handler,          /* 16: IRQ  0 WWDG          */
    Default_Handler,          /* 17: IRQ  1 PVD           */
    Default_Handler,          /* 18: IRQ  2 TAMPER        */
    Default_Handler,          /* 19: IRQ  3 RTC           */
    Default_Handler,          /* 20: IRQ  4 FLASH         */
    Default_Handler,          /* 21: IRQ  5 RCC           */
    Default_Handler,          /* 22: IRQ  6 EXTI0         */
    Default_Handler,          /* 23: IRQ  7 EXTI1         */
    Default_Handler,          /* 24: IRQ  8 EXTI2         */
    Default_Handler,          /* 25: IRQ  9 EXTI3         */
    Default_Handler,          /* 26: IRQ 10 EXTI4         */
    Default_Handler,          /* 27: IRQ 11 DMA1_Channel1 */
    Default_Handler,          /* 28: IRQ 12 DMA1_Channel2 */
    Default_Handler,          /* 29: IRQ 13 DMA1_Channel3 */
    Default_Handler,          /* 30: IRQ 14 DMA1_Channel4 */
    Default_Handler,          /* 31: IRQ 15 DMA1_Channel5 */
    DMA1_Channel6_IRQHandler, /* 32: IRQ 16 DMA1_Channel6 */
    Default_Handler,          /* 33: IRQ 17 DMA1_Channel7 */
    Default_Handler,          /* 34: IRQ 18 ADC1_2        */
    Default_Handler,          /* 35: IRQ 19 USB_HP_CAN_TX */
    Default_Handler,          /* 36: IRQ 20 USB_LP_CAN_RX0*/
    Default_Handler,          /* 37: IRQ 21 CAN_RX1       */
    Default_Handler,          /* 38: IRQ 22 CAN_SCE       */
    Default_Handler,          /* 39: IRQ 23 EXTI9_5       */
    Default_Handler,          /* 40: IRQ 24 TIM1_BRK      */
    Default_Handler,          /* 41: IRQ 25 TIM1_UP       */
    Default_Handler,          /* 42: IRQ 26 TIM1_TRG_COM  */
    Default_Handler,          /* 43: IRQ 27 TIM1_CC       */
    Default_Handler,          /* 44: IRQ 28 TIM2          */
    Default_Handler,          /* 45: IRQ 29 TIM3          */
    Default_Handler,          /* 46: IRQ 30 TIM4          */
    Default_Handler,          /* 47: IRQ 31 I2C1_EV       */
    Default_Handler,          /* 48: IRQ 32 I2C1_ER       */
    Default_Handler,          /* 49: IRQ 33 I2C2_EV       */
    Default_Handler,          /* 50: IRQ 34 I2C2_ER       */
    Default_Handler,          /* 51: IRQ 35 SPI1          */
    Default_Handler,          /* 52: IRQ 36 SPI2          */
    Default_Handler,          /* 53: IRQ 37 USART1        */
    Default_Handler,          /* 54: IRQ 38 USART2        */
    Default_Handler,          /* 55: IRQ 39 USART3        */
    Default_Handler,          /* 56: IRQ 40 EXTI15_10     */
    Default_Handler,          /* 57: IRQ 41 RTCAlarm      */
    Default_Handler,          /* 58: IRQ 42 USBWakeUp     */
};

void Reset_Handler(void)
{
    /* Copy .data initial values from flash to RAM. */
    uint32_t *src = &_sidata;
    for (uint32_t *dst = &_sdata; dst < &_edata;) {
        *dst++ = *src++;
    }

    /* Zero .bss. */
    for (uint32_t *dst = &_sbss; dst < &_ebss;) {
        *dst++ = 0;
    }

    main();

    /* main() should never return; if it does, park here. */
    for (;;) {
    }
}
