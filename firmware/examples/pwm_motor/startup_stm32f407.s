/**
 * Minimal startup for STM32F407 — vector table + Reset_Handler.
 * Only the vectors needed for blinky (Reset, SysTick) are populated.
 */

    .syntax unified
    .cpu    cortex-m4
    .thumb

/* ---- Stack ---- */
    .section .isr_vector, "a", %progbits
    .align  2
    .globl  __isr_vector
__isr_vector:
    .word   _estack             /*  0: Initial SP */
    .word   Reset_Handler       /*  1: Reset */
    .word   Default_Handler     /*  2: NMI */
    .word   Default_Handler     /*  3: HardFault */
    .word   Default_Handler     /*  4: MemManage */
    .word   Default_Handler     /*  5: BusFault */
    .word   Default_Handler     /*  6: UsageFault */
    .word   0                   /*  7: Reserved */
    .word   0                   /*  8: Reserved */
    .word   0                   /*  9: Reserved */
    .word   0                   /* 10: Reserved */
    .word   Default_Handler     /* 11: SVCall */
    .word   Default_Handler     /* 12: DebugMon */
    .word   0                   /* 13: Reserved */
    .word   Default_Handler     /* 14: PendSV */
    .word   SysTick_Handler     /* 15: SysTick */

/* ---- Reset Handler ---- */
    .section .text
    .align  2
    .globl  Reset_Handler
    .type   Reset_Handler, %function
Reset_Handler:
    /* Copy .data from flash to SRAM */
    ldr     r0, =_sdata
    ldr     r1, =_edata
    ldr     r2, =_sidata
.copy_data:
    cmp     r0, r1
    bge     .zero_bss
    ldr     r3, [r2], #4
    str     r3, [r0], #4
    b       .copy_data

    /* Zero .bss */
.zero_bss:
    ldr     r0, =_sbss
    ldr     r1, =_ebss
    movs    r3, #0
.zero_loop:
    cmp     r0, r1
    bge     .call_main
    str     r3, [r0], #4
    b       .zero_loop

.call_main:
    bl      main
    b       .

/* ---- Default Handler (infinite loop) ---- */
    .globl  Default_Handler
    .type   Default_Handler, %function
Default_Handler:
    b       .

    .weak   SysTick_Handler
    .thumb_set SysTick_Handler, Default_Handler
