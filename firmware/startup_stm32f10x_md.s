/**
  * @file      startup_stm32f10x_md.s
  * @brief     STM32F10x 中容量器件 (STM32F103C8T6) GCC 启动文件
  *
  * 内容:
  *   - 中断向量表 (16 个内核异常 + 43 个外设中断)
  *   - 复位流程: 装栈 -> 拷贝 .data -> 清零 .bss
  *               -> SystemInit (72MHz PLL) -> __libc_init_array -> main
  *   - 所有中断默认弱定义到 Default_Handler (死循环)
  */
.syntax unified
.cpu cortex-m3
.fpu softvfp
.thumb

.global g_pfnVectors
.global Default_Handler

/* 链接脚本提供的初始化段地址 */
.word _sidata
.word _sdata
.word _edata
.word _sbss
.word _ebss

.section .text.Reset_Handler, "ax", %progbits
.weak Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
  /* 装载栈顶 (向量表[0] 亦由硬件自动装载, 此处再显式设置一次) */
  ldr   r0, =_estack
  mov   sp, r0

  /* 拷贝 .data 段: Flash(_sidata) -> RAM(_sdata.._edata) */
  ldr   r0, =_sdata
  ldr   r1, =_edata
  ldr   r2, =_sidata
  movs  r3, #0
  b     LoopCopyDataInit

CopyDataInit:
  ldr   r4, [r2, r3]
  str   r4, [r0, r3]
  adds  r3, r3, #4

LoopCopyDataInit:
  adds  r4, r0, r3
  cmp   r4, r1
  bcc   CopyDataInit

  /* 清零 .bss 段 (_sbss.._ebss) */
  ldr   r2, =_sbss
  ldr   r4, =_ebss
  movs  r3, #0
  b     LoopFillZerobss

FillZerobss:
  str   r3, [r2]
  adds  r2, r2, #4

LoopFillZerobss:
  cmp   r2, r4
  bcc   FillZerobss

  /* 系统时钟初始化 (HSE 8MHz -> PLL x9 -> 72MHz) */
  bl    SystemInit
  /* 静态构造 */
  bl    __libc_init_array
  /* 进入应用 */
  bl    main
  bx    lr

.size Reset_Handler, .-Reset_Handler

.section .text.Default_Handler, "ax", %progbits
Default_Handler:
Infinite_Loop:
  b     Infinite_Loop
.size Default_Handler, .-Default_Handler

/* ------------------------------------------------------------------ */
/* 各中断的弱别名 (被 stm32f10x_it.c 中的强定义覆盖)                    */
/* ------------------------------------------------------------------ */
.weak NMI_Handler
.thumb_set NMI_Handler, Default_Handler

.weak HardFault_Handler
.thumb_set HardFault_Handler, Default_Handler

.weak MemManage_Handler
.thumb_set MemManage_Handler, Default_Handler

.weak BusFault_Handler
.thumb_set BusFault_Handler, Default_Handler

.weak UsageFault_Handler
.thumb_set UsageFault_Handler, Default_Handler

.weak SVC_Handler
.thumb_set SVC_Handler, Default_Handler

.weak DebugMon_Handler
.thumb_set DebugMon_Handler, Default_Handler

.weak PendSV_Handler
.thumb_set PendSV_Handler, Default_Handler

.weak SysTick_Handler
.thumb_set SysTick_Handler, Default_Handler

.weak WWDG_IRQHandler
.thumb_set WWDG_IRQHandler, Default_Handler

.weak PVD_IRQHandler
.thumb_set PVD_IRQHandler, Default_Handler

.weak TAMPER_IRQHandler
.thumb_set TAMPER_IRQHandler, Default_Handler

.weak RTC_IRQHandler
.thumb_set RTC_IRQHandler, Default_Handler

.weak FLASH_IRQHandler
.thumb_set FLASH_IRQHandler, Default_Handler

.weak RCC_IRQHandler
.thumb_set RCC_IRQHandler, Default_Handler

.weak EXTI0_IRQHandler
.thumb_set EXTI0_IRQHandler, Default_Handler

.weak EXTI1_IRQHandler
.thumb_set EXTI1_IRQHandler, Default_Handler

.weak EXTI2_IRQHandler
.thumb_set EXTI2_IRQHandler, Default_Handler

.weak EXTI3_IRQHandler
.thumb_set EXTI3_IRQHandler, Default_Handler

.weak EXTI4_IRQHandler
.thumb_set EXTI4_IRQHandler, Default_Handler

.weak DMA1_Channel1_IRQHandler
.thumb_set DMA1_Channel1_IRQHandler, Default_Handler

.weak DMA1_Channel2_IRQHandler
.thumb_set DMA1_Channel2_IRQHandler, Default_Handler

.weak DMA1_Channel3_IRQHandler
.thumb_set DMA1_Channel3_IRQHandler, Default_Handler

.weak DMA1_Channel4_IRQHandler
.thumb_set DMA1_Channel4_IRQHandler, Default_Handler

.weak DMA1_Channel5_IRQHandler
.thumb_set DMA1_Channel5_IRQHandler, Default_Handler

.weak DMA1_Channel6_IRQHandler
.thumb_set DMA1_Channel6_IRQHandler, Default_Handler

.weak DMA1_Channel7_IRQHandler
.thumb_set DMA1_Channel7_IRQHandler, Default_Handler

.weak ADC1_2_IRQHandler
.thumb_set ADC1_2_IRQHandler, Default_Handler

.weak USB_HP_CAN1_TX_IRQHandler
.thumb_set USB_HP_CAN1_TX_IRQHandler, Default_Handler

.weak USB_LP_CAN1_RX0_IRQHandler
.thumb_set USB_LP_CAN1_RX0_IRQHandler, Default_Handler

.weak CAN1_RX1_IRQHandler
.thumb_set CAN1_RX1_IRQHandler, Default_Handler

.weak CAN1_SCE_IRQHandler
.thumb_set CAN1_SCE_IRQHandler, Default_Handler

.weak EXTI9_5_IRQHandler
.thumb_set EXTI9_5_IRQHandler, Default_Handler

.weak TIM1_BRK_IRQHandler
.thumb_set TIM1_BRK_IRQHandler, Default_Handler

.weak TIM1_UP_IRQHandler
.thumb_set TIM1_UP_IRQHandler, Default_Handler

.weak TIM1_TRG_COM_IRQHandler
.thumb_set TIM1_TRG_COM_IRQHandler, Default_Handler

.weak TIM1_CC_IRQHandler
.thumb_set TIM1_CC_IRQHandler, Default_Handler

.weak TIM2_IRQHandler
.thumb_set TIM2_IRQHandler, Default_Handler

.weak TIM3_IRQHandler
.thumb_set TIM3_IRQHandler, Default_Handler

.weak TIM4_IRQHandler
.thumb_set TIM4_IRQHandler, Default_Handler

.weak I2C1_EV_IRQHandler
.thumb_set I2C1_EV_IRQHandler, Default_Handler

.weak I2C1_ER_IRQHandler
.thumb_set I2C1_ER_IRQHandler, Default_Handler

.weak I2C2_EV_IRQHandler
.thumb_set I2C2_EV_IRQHandler, Default_Handler

.weak I2C2_ER_IRQHandler
.thumb_set I2C2_ER_IRQHandler, Default_Handler

.weak SPI1_IRQHandler
.thumb_set SPI1_IRQHandler, Default_Handler

.weak SPI2_IRQHandler
.thumb_set SPI2_IRQHandler, Default_Handler

.weak USART1_IRQHandler
.thumb_set USART1_IRQHandler, Default_Handler

.weak USART2_IRQHandler
.thumb_set USART2_IRQHandler, Default_Handler

.weak USART3_IRQHandler
.thumb_set USART3_IRQHandler, Default_Handler

.weak EXTI15_10_IRQHandler
.thumb_set EXTI15_10_IRQHandler, Default_Handler

.weak RTC_Alarm_IRQHandler
.thumb_set RTC_Alarm_IRQHandler, Default_Handler

.weak USBWakeUp_IRQHandler
.thumb_set USBWakeUp_IRQHandler, Default_Handler

/* ------------------------------------------------------------------ */
/* 中断向量表 (STM32F103 中容量: 16 + 43 = 59 项)                        */
/* ------------------------------------------------------------------ */
.section .isr_vector, "a", %progbits
.type g_pfnVectors, %object
.size g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
  .word _estack                    /* 0x000: 初始栈顶 */
  .word Reset_Handler              /* 0x004: 复位 */
  .word NMI_Handler                /* 0x008 */
  .word HardFault_Handler          /* 0x00C */
  .word MemManage_Handler          /* 0x010 */
  .word BusFault_Handler           /* 0x014 */
  .word UsageFault_Handler         /* 0x018 */
  .word 0                          /* 0x01C: 保留 */
  .word 0                          /* 0x020: 保留 */
  .word 0                          /* 0x024: 保留 */
  .word 0                          /* 0x028: 保留 */
  .word SVC_Handler                /* 0x02C */
  .word DebugMon_Handler           /* 0x030 */
  .word 0                          /* 0x034: 保留 */
  .word PendSV_Handler             /* 0x038 */
  .word SysTick_Handler            /* 0x03C */
  /* 外设中断 0..42 */
  .word WWDG_IRQHandler            /* 0x040: IRQ 0  */
  .word PVD_IRQHandler             /* 0x044: IRQ 1  */
  .word TAMPER_IRQHandler          /* 0x048: IRQ 2  */
  .word RTC_IRQHandler             /* 0x04C: IRQ 3  */
  .word FLASH_IRQHandler           /* 0x050: IRQ 4  */
  .word RCC_IRQHandler             /* 0x054: IRQ 5  */
  .word EXTI0_IRQHandler           /* 0x058: IRQ 6  */
  .word EXTI1_IRQHandler           /* 0x05C: IRQ 7  PA1 时序触发 */
  .word EXTI2_IRQHandler           /* 0x060: IRQ 8  */
  .word EXTI3_IRQHandler           /* 0x064: IRQ 9  */
  .word EXTI4_IRQHandler           /* 0x068: IRQ 10 */
  .word DMA1_Channel1_IRQHandler   /* 0x06C: IRQ 11 */
  .word DMA1_Channel2_IRQHandler   /* 0x070: IRQ 12 */
  .word DMA1_Channel3_IRQHandler   /* 0x074: IRQ 13 */
  .word DMA1_Channel4_IRQHandler   /* 0x078: IRQ 14 */
  .word DMA1_Channel5_IRQHandler   /* 0x07C: IRQ 15 */
  .word DMA1_Channel6_IRQHandler   /* 0x080: IRQ 16 */
  .word DMA1_Channel7_IRQHandler   /* 0x084: IRQ 17 */
  .word ADC1_2_IRQHandler          /* 0x088: IRQ 18 */
  .word USB_HP_CAN1_TX_IRQHandler  /* 0x08C: IRQ 19 */
  .word USB_LP_CAN1_RX0_IRQHandler /* 0x090: IRQ 20 */
  .word CAN1_RX1_IRQHandler        /* 0x094: IRQ 21 */
  .word CAN1_SCE_IRQHandler        /* 0x098: IRQ 22 */
  .word EXTI9_5_IRQHandler         /* 0x09C: IRQ 23 */
  .word TIM1_BRK_IRQHandler        /* 0x0A0: IRQ 24 */
  .word TIM1_UP_IRQHandler         /* 0x0A4: IRQ 25 */
  .word TIM1_TRG_COM_IRQHandler    /* 0x0A8: IRQ 26 */
  .word TIM1_CC_IRQHandler         /* 0x0AC: IRQ 27 */
  .word TIM2_IRQHandler            /* 0x0B0: IRQ 28 */
  .word TIM3_IRQHandler            /* 0x0B4: IRQ 29 */
  .word TIM4_IRQHandler            /* 0x0B8: IRQ 30 TIM4 时序时基 */
  .word I2C1_EV_IRQHandler         /* 0x0BC: IRQ 31 */
  .word I2C1_ER_IRQHandler         /* 0x0C0: IRQ 32 */
  .word I2C2_EV_IRQHandler         /* 0x0C4: IRQ 33 */
  .word I2C2_ER_IRQHandler         /* 0x0C8: IRQ 34 */
  .word SPI1_IRQHandler            /* 0x0CC: IRQ 35 */
  .word SPI2_IRQHandler            /* 0x0D0: IRQ 36 */
  .word USART1_IRQHandler          /* 0x0D4: IRQ 37 USART1 命令口 */
  .word USART2_IRQHandler          /* 0x0D8: IRQ 38 */
  .word USART3_IRQHandler          /* 0x0DC: IRQ 39 */
  .word EXTI15_10_IRQHandler       /* 0x0E0: IRQ 40 */
  .word RTC_Alarm_IRQHandler       /* 0x0E4: IRQ 41 */
  .word USBWakeUp_IRQHandler       /* 0x0E8: IRQ 42 */

.section .note.GNU-stack, "", %progbits
