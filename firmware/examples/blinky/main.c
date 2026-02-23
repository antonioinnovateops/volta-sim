/**
 * Volta-Sim Blinky Example
 * Minimal bare-metal STM32F407 firmware — toggles PD12 (green LED) every 500ms.
 * No HAL, no vendor libraries — direct register access only.
 */

#include <stdint.h>

/* ---- RCC ---- */
#define RCC_BASE        0x40023800UL
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30))

/* ---- GPIOD ---- */
#define GPIOD_BASE      0x40020C00UL
#define GPIOD_MODER     (*(volatile uint32_t *)(GPIOD_BASE + 0x00))
#define GPIOD_ODR       (*(volatile uint32_t *)(GPIOD_BASE + 0x14))

/* ---- SysTick ---- */
#define STK_CTRL        (*(volatile uint32_t *)0xE000E010)
#define STK_LOAD        (*(volatile uint32_t *)0xE000E014)
#define STK_VAL         (*(volatile uint32_t *)0xE000E018)

/* Pin definitions */
#define LED_GREEN       12  /* PD12 */

/* Millisecond counter incremented by SysTick_Handler */
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

int main(void)
{
    /* Enable GPIOD peripheral clock (bit 3 of AHB1ENR) */
    RCC_AHB1ENR |= (1U << 3);

    /* Configure PD12 as general-purpose output (MODER bits [25:24] = 01) */
    GPIOD_MODER &= ~(3U << (LED_GREEN * 2));
    GPIOD_MODER |=  (1U << (LED_GREEN * 2));

    /* Configure SysTick: 16 MHz HSI / 16000 = 1 kHz (1 ms tick) */
    STK_LOAD = 16000U - 1U;
    STK_VAL  = 0;
    STK_CTRL = (1U << 2) | (1U << 1) | (1U << 0); /* CLKSOURCE | TICKINT | ENABLE */

    for (;;) {
        GPIOD_ODR ^= (1U << LED_GREEN);
        delay_ms(500);
    }
}
