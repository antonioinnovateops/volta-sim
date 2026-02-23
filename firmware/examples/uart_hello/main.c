/**
 * Volta-Sim UART Hello Example
 * Prints "Hello from Volta!" on USART2 every second.
 * Bare-metal — no HAL, direct register access.
 */

#include <stdint.h>

/* ---- RCC ---- */
#define RCC_BASE        0x40023800UL
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x40))

/* ---- GPIOA (PA2 = USART2 TX) ---- */
#define GPIOA_BASE      0x40020000UL
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_AFRL      (*(volatile uint32_t *)(GPIOA_BASE + 0x20))

/* ---- USART2 ---- */
#define USART2_BASE     0x40004400UL
#define USART2_SR       (*(volatile uint32_t *)(USART2_BASE + 0x00))
#define USART2_DR       (*(volatile uint32_t *)(USART2_BASE + 0x04))
#define USART2_BRR      (*(volatile uint32_t *)(USART2_BASE + 0x08))
#define USART2_CR1      (*(volatile uint32_t *)(USART2_BASE + 0x0C))

/* USART SR bits */
#define USART_SR_TXE    (1U << 7)

/* ---- SysTick ---- */
#define STK_CTRL        (*(volatile uint32_t *)0xE000E010)
#define STK_LOAD        (*(volatile uint32_t *)0xE000E014)
#define STK_VAL         (*(volatile uint32_t *)0xE000E018)

static volatile uint32_t tick;

void SysTick_Handler(void)
{
    tick++;
}

static void delay_ms(uint32_t ms)
{
    uint32_t start = tick;
    while ((tick - start) < ms)
        ;
}

static void usart2_init(void)
{
    /* Enable GPIOA clock (bit 0) and USART2 clock (bit 17 of APB1ENR) */
    RCC_AHB1ENR |= (1U << 0);
    RCC_APB1ENR |= (1U << 17);

    /* PA2 alternate function mode (MODER bits [5:4] = 10) */
    GPIOA_MODER &= ~(3U << (2 * 2));
    GPIOA_MODER |=  (2U << (2 * 2));

    /* PA2 -> AF7 (USART2_TX): AFRL bits [11:8] = 0111 */
    GPIOA_AFRL &= ~(0xFU << (2 * 4));
    GPIOA_AFRL |=  (7U   << (2 * 4));

    /* Baud rate: 16 MHz / 115200 ≈ 138.89 → BRR = 0x008B (mantissa=8, frac=11) */
    USART2_BRR = 0x008B;

    /* Enable USART, TX only */
    USART2_CR1 = (1U << 13) | (1U << 3);  /* UE | TE */
}

static void usart2_putc(char c)
{
    while (!(USART2_SR & USART_SR_TXE))
        ;
    USART2_DR = (uint32_t)c;
}

static void usart2_puts(const char *s)
{
    while (*s)
        usart2_putc(*s++);
}

int main(void)
{
    usart2_init();

    /* SysTick: 16 MHz / 16000 = 1 kHz */
    STK_LOAD = 16000U - 1U;
    STK_VAL  = 0;
    STK_CTRL = (1U << 2) | (1U << 1) | (1U << 0);

    for (;;) {
        usart2_puts("Hello from Volta!\r\n");
        delay_ms(1000);
    }
}
