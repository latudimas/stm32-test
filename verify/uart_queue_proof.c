/* CBMC proof for the USART1 TX queue in src/uart.c. Run with `make verify`.
 *
 * Inductive: start from ANY valid queue (indices in range, arbitrary bytes),
 * do one uart_poll() or uart_putc(c), and check the FIFO spec. Since every
 * call sequence is a chain of such steps, this covers all of them:
 *   sent bytes ++ queue afterwards == queue before ++ bytes appended
 * (uart_putc('\n') appends "\r\n"). So nothing is lost, duplicated,
 * reordered or overwritten before it is sent, and indices stay in 0..511.
 *
 * Hardware assumption: TXE is unpredictable, but the transmitter is never
 * busy for more than MAX_BUSY consecutive SR reads (a real byte takes ~87 us).
 * Busy polls change nothing, so the bound only keeps tx()'s wait finite. */
#include <stdbool.h>
#include <stdint.h>

#define STM32F103_H /* replace the target MMIO definitions with the model below */
static uint32_t mmio_sink;
#define RCC_APB2ENR mmio_sink
#define RCC_APB2ENR_IOPAEN (1u << 2)
#define RCC_APB2ENR_USART1EN (1u << 14)
#define GPIO_CRH(base) mmio_sink
#define GPIOA_BASE 0u
#define GPIO_AF_PP_2MHZ 0xAu
#define USART1_BRR mmio_sink
#define USART1_CR1 mmio_sink
#define USART_CR1_UE (1u << 13)
#define USART_CR1_TE (1u << 3)
#define USART_SR_TXE (1u << 7)

uint32_t nondet_u32(void); /* No body: CBMC treats each result as any value. */
bool nondet_bool(void);
char nondet_char(void);

#define MAX_BUSY 2u
static uint32_t busy_reads;
static uint32_t read_sr(void)
{
    bool txe = nondet_bool() || busy_reads == MAX_BUSY;
    busy_reads = txe ? 0u : busy_reads + 1u;
    return txe ? USART_SR_TXE : 0u;
}
#define USART1_SR read_sr()

/* Every DR write lands in sent[], in order. */
#define MAX_SENT 4u
static uint32_t sent[MAX_SENT], n_sent;
static uint32_t *dr_write(void)
{
    __CPROVER_assert(n_sent < MAX_SENT, "one putc sends at most 4 bytes");
    return &sent[n_sent++];
}
#define USART1_DR (*dr_write())

uint32_t pclk2_hz;
#include "../src/uart.c"

static uint32_t queued(void)
{
    return (tx_head + TX_SIZE - tx_tail) % TX_SIZE;
}

int main(void)
{
    __CPROVER_havoc_object(tx_buf);
    tx_head = nondet_u32();
    tx_tail = nondet_u32();
    busy_reads = nondet_u32();
    __CPROVER_assume(tx_head < TX_SIZE && tx_tail < TX_SIZE);
    __CPROVER_assume(busy_reads <= MAX_BUSY);

    /* Remember the old queue: its front (what may be sent) and one arbitrary
     * slot p (standing for every slot) to check nothing gets clobbered. */
    uint32_t old_tail = tx_tail, old_len = queued();
    uint8_t front[MAX_SENT];
    for (uint32_t j = 0; j < MAX_SENT; j++)
        front[j] = tx_buf[(old_tail + j) % TX_SIZE];
    uint32_t p = nondet_u32();
    __CPROVER_assume(p < TX_SIZE);
    uint8_t old_p = tx_buf[p];

    uint8_t app[2];
    uint32_t app_len = 0;
    if (nondet_bool()) {
        uart_poll();
    } else {
        char c = nondet_char();
        uart_putc(c);
        if (c == '\n')
            app[app_len++] = '\r';
        app[app_len++] = (uint8_t)c;
    }

    __CPROVER_assert(tx_head < TX_SIZE && tx_tail < TX_SIZE, "indices stay in 0..511");
    __CPROVER_assert(n_sent <= old_len + app_len, "only queued bytes are sent");
    for (uint32_t j = 0; j < n_sent; j++)
        __CPROVER_assert(sent[j] == (j < old_len ? front[j] : app[j - old_len]),
                         "bytes are sent in FIFO order");
    __CPROVER_assert(tx_tail == (old_tail + n_sent) % TX_SIZE,
                     "tail advances once per sent byte");
    __CPROVER_assert(queued() == old_len + app_len - n_sent,
                     "no byte is lost or duplicated");

    uint32_t i = (p + TX_SIZE - tx_tail) % TX_SIZE; /* p's position in the queue */
    if (i < queued()) {
        uint32_t j = n_sent + i; /* ...and in old queue ++ appended bytes */
        __CPROVER_assert(tx_buf[p] == (j < old_len ? old_p : app[j - old_len]),
                         "queued bytes are not overwritten before being sent");
    }
    return 0;
}
