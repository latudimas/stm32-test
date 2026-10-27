/*
 * Startup code: vector table + Reset_Handler.
 * Vector table layout: PM0056 §2.3.4 "Vector table" (Cortex-M3 system exceptions).
 * Device IRQs (RM0008 §10.1.2, entries 16+) are left out until we enable one.
 */
#include <stdint.h>

/* Symbols defined in stm32f103.ld */
extern uint32_t _estack;
extern uint32_t _sidata, _sdata, _edata;
extern uint32_t _sbss, _ebss;

int main(void);

void Reset_Handler(void);

/* Any unexpected exception lands here; halt so it's obvious in GDB. */
void Default_Handler(void)
{
    for (;;) {
    }
}

/* Index = exception number. 0 is the initial stack pointer, not a handler. */
__attribute__((section(".isr_vector"), used))
static void (*const vector_table[16])(void) = {
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
    Default_Handler,          /* 15: SysTick                 */
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
