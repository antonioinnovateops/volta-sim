/**
 * Volta-Sim PWM Motor Example
 * Bare-metal STM32F407 firmware — TIM1 CH1 PWM on PA8, sweeps 0-100% duty.
 * Also reads ADC1 CH0 (PA0) to vary sweep speed.
 * No HAL, no vendor libraries — direct register access only.
 */

#include <stdint.h>

/* ---- RCC ---- */
#define RCC_BASE        0x40023800UL
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x44))

/* ---- GPIOA ---- */
#define GPIOA_BASE      0x40020000UL
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_AFRL      (*(volatile uint32_t *)(GPIOA_BASE + 0x20))
#define GPIOA_AFRH      (*(volatile uint32_t *)(GPIOA_BASE + 0x24))

/* ---- TIM1 (Advanced timer, APB2) ---- */
#define TIM1_BASE       0x40010000UL
#define TIM1_CR1        (*(volatile uint32_t *)(TIM1_BASE + 0x00))
#define TIM1_CCMR1      (*(volatile uint32_t *)(TIM1_BASE + 0x18))
#define TIM1_CCER       (*(volatile uint32_t *)(TIM1_BASE + 0x20))
#define TIM1_PSC        (*(volatile uint32_t *)(TIM1_BASE + 0x28))
#define TIM1_ARR        (*(volatile uint32_t *)(TIM1_BASE + 0x2C))
#define TIM1_CCR1       (*(volatile uint32_t *)(TIM1_BASE + 0x34))
#define TIM1_BDTR       (*(volatile uint32_t *)(TIM1_BASE + 0x44))

/* ---- ADC1 ---- */
#define ADC1_BASE       0x40012000UL
#define ADC1_SR         (*(volatile uint32_t *)(ADC1_BASE + 0x00))
#define ADC1_CR2        (*(volatile uint32_t *)(ADC1_BASE + 0x08))
#define ADC1_SQR3       (*(volatile uint32_t *)(ADC1_BASE + 0x34))
#define ADC1_DR         (*(volatile uint32_t *)(ADC1_BASE + 0x4C))

/* ---- SysTick ---- */
#define STK_CTRL        (*(volatile uint32_t *)0xE000E010)
#define STK_LOAD        (*(volatile uint32_t *)0xE000E014)
#define STK_VAL         (*(volatile uint32_t *)0xE000E018)

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

static void init_pwm(void)
{
    /* Enable GPIOA clock (bit 0 of AHB1ENR) */
    RCC_AHB1ENR |= (1U << 0);

    /* Enable TIM1 clock (bit 0 of APB2ENR) */
    RCC_APB2ENR |= (1U << 0);

    /* Configure PA8 as AF1 (TIM1_CH1):
     * MODER[17:16] = 10 (alternate function)
     * AFRH[3:0] = 0001 (AF1 = TIM1) — PA8 is in AFRH (pins 8-15)
     */
    GPIOA_MODER &= ~(3U << (8 * 2));
    GPIOA_MODER |=  (2U << (8 * 2));  /* AF mode */
    GPIOA_AFRH  &= ~(0xFU << 0);
    GPIOA_AFRH  |=  (1U << 0);        /* AF1 = TIM1 */

    /* Configure TIM1 for PWM mode 1 on CH1:
     * PSC = 15  -> timer clock = 16MHz / (15+1) = 1 MHz
     * ARR = 999 -> period = 1000 counts -> 1 kHz PWM
     */
    TIM1_PSC = 15;
    TIM1_ARR = 999;
    TIM1_CCR1 = 0;  /* Start at 0% duty */

    /* OC1M = 110 (PWM mode 1), OC1PE = 1 (preload enable) */
    TIM1_CCMR1 = (6U << 4) | (1U << 3);

    /* CC1E = 1 (enable CH1 output), CC1P = 0 (active high) */
    TIM1_CCER = (1U << 0);

    /* MOE = 1 (main output enable — required for TIM1 advanced timer) */
    TIM1_BDTR = (1U << 15);

    /* CEN = 1 (counter enable) */
    TIM1_CR1 = (1U << 0);
}

static void init_adc(void)
{
    /* Enable ADC1 clock (bit 8 of APB2ENR) */
    RCC_APB2ENR |= (1U << 8);

    /* Configure PA0 as analog input:
     * MODER[1:0] = 11 (analog mode)
     */
    GPIOA_MODER |= (3U << (0 * 2));

    /* Select channel 0 in SQR3 */
    ADC1_SQR3 = 0;

    /* Enable ADC (ADON bit) */
    ADC1_CR2 = (1U << 0);
}

static uint16_t read_adc(void)
{
    /* Start conversion (SWSTART bit) */
    ADC1_CR2 |= (1U << 30);

    /* Wait for EOC (bit 1 of SR) */
    while (!(ADC1_SR & (1U << 1)))
        ;

    /* Read result */
    return (uint16_t)(ADC1_DR & 0xFFF);
}

int main(void)
{
    /* Configure SysTick: 16 MHz HSI / 16000 = 1 kHz (1 ms tick) */
    STK_LOAD = 16000U - 1U;
    STK_VAL  = 0;
    STK_CTRL = (1U << 2) | (1U << 1) | (1U << 0);

    init_pwm();
    init_adc();

    /* Sweep duty cycle 0% -> 100% -> 0% continuously.
     * ADC reading on PA0 varies the sweep speed (10-50ms per step). */
    uint32_t duty = 0;
    int32_t direction = 1;

    for (;;) {
        /* Set new duty cycle */
        TIM1_CCR1 = duty;

        /* Read ADC to determine sweep delay */
        uint16_t adc_val = read_adc();
        /* Map 0-4095 to 10-50 ms */
        uint32_t delay = 10 + (adc_val * 40) / 4095;
        delay_ms(delay);

        /* Update sweep */
        duty += (uint32_t)(direction * 10);
        if (duty >= 1000) {
            duty = 999;
            direction = -1;
        } else if ((int32_t)duty <= 0) {
            duty = 0;
            direction = 1;
        }
    }
}
