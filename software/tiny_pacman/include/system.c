// ===================================================================================
// Basic System Functions for CH32V003                                        * v1.2 *
// ===================================================================================
//
// This file must be included!!!!
//
// References:
// -----------
// - CNLohr ch32v003fun: https://github.com/cnlohr/ch32v003fun
// - WCH Nanjing Qinheng Microelectronics: http://wch.cn
//
// 2023 by Stefan Wagner:   https://github.com/wagiminator

#include "system.h"

// ===================================================================================
// Setup Microcontroller (this function is called automatically at startup)
// ===================================================================================
void SYS_init(void) {
  // Init system clock
  #if SYS_CLK_INIT > 0
  CLK_init();
  #endif

  // Init SYSTICK
  #if SYS_TICK_INIT > 0
  STK_init();
  #endif

  // Enable GPIO
  #if SYS_GPIO_EN > 0
  RCC->APB2PCENR |= RCC_IOPAEN | RCC_IOPCEN | RCC_IOPDEN;
  #endif
}

// ===================================================================================
// System Clock Functions
// ===================================================================================

// Init internal oscillator (non PLL) as system clock source
void CLK_init_HSI(void) {
  FLASH->ACTLR  = FLASH_ACTLR_LATENCY_0;                        // 0 cycle latency
  RCC->INTR     = 0x009F0000;                                   // clear ready flags
  RCC->CFGR0    = CLK_DIV;                                      // set clock divider
  RCC->CTLR     = RCC_HSION | ((HSITRIM) << 3);                 // use HSI, Only
}

// Init internal oscillator with PLL as system clock source
void CLK_init_HSI_PLL(void) {
  FLASH->ACTLR  = FLASH_ACTLR_LATENCY_1;                        // 1 cycle latency
  RCC->INTR     = 0x009F0000;                                   // clear ready flags
  RCC->CFGR0    = CLK_DIV | RCC_PLLSRC_HSI_Mul2;                // set PLL and clock divider
  RCC->CTLR     = RCC_HSION | RCC_PLLON | ((HSITRIM) << 3);     // use HSI, but enable PLL
  while((RCC->CTLR & RCC_PLLRDY) == 0);                                     // wait till PLL is ready
  RCC->CFGR0 = (RCC->CFGR0 & ((uint32_t)~(RCC_SW))) | (uint32_t)RCC_SW_PLL; // select PLL as system clock source
  while((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x08);                // wait till PLL is used as system clock source
}

// Init external crystal (non PLL) as system clock source
void CLK_init_HSE(void) {
  RCC->APB2PCENR |= RCC_AFIOEN;                                 // enable auxiliary clock module
  AFIO->PCFR1    |= AFIO_PCFR1_PA12_REMAP;                      // pins PA1-PA2 for external crystal
  FLASH->ACTLR    = FLASH_ACTLR_LATENCY_0;                      // 0 cycle latency
  RCC->CTLR       = RCC_HSION | RCC_HSEON | RCC_PLLON;          // enable HSE and keep HSI+PLL on
  while(!(RCC->CTLR & RCC_HSERDY));                             // wait till HSE is ready
  RCC->CFGR0      = RCC_HPRE_DIV1 | RCC_SW_HSE;                 // HCLK = SYSCLK = APB1 and use HSE for system clock
  while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x04);   // wait till HSE is used as system clock source
  RCC->CTLR       = RCC_HSEON;                                  // turn off HSI + PLL.
}

// Init external crystal (PLL) as system clock source
void CLK_init_HSE_PLL(void) {
  RCC->APB2PCENR |= RCC_AFIOEN;                                 // enable auxiliary clock module
  AFIO->PCFR1    |= AFIO_PCFR1_PA12_REMAP;                      // pins PA1-PA2 for external crystal
  RCC->CTLR       = RCC_HSION | RCC_HSEON | RCC_PLLON;          // enable HSE and keep HSI+PLL on
  while(!(RCC->CTLR & RCC_HSERDY));                             // wait till HSE is ready
  RCC->CFGR0      = RCC_SW_HSE | RCC_HPRE_DIV1;                 // HCLK = SYSCLK = APB1 and use HSE for system clock
  FLASH->ACTLR    = FLASH_ACTLR_LATENCY_1;                      // 1 cycle latency
  RCC->CTLR       = RCC_HSEON;                                  // turn off PLL and HSI
  RCC->CFGR0      = RCC_SW_HSE | RCC_HPRE_DIV1 | RCC_PLLSRC_HSE_Mul2; // use PLL with HSE
  RCC->CTLR       = RCC_HSEON | RCC_PLLON;                      // turn PLL back on..
  while((RCC->CTLR & RCC_PLLRDY) == 0);                         // wait till PLL is ready
  RCC->CFGR0 = RCC_SW_PLL | RCC_HPRE_DIV1 | RCC_PLLSRC_HSE_Mul2;// select PLL as system clock source
  while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x08);   // wait till PLL is used as system clock source
}

// Setup pin PC4 for MCO (output, push-pull, 50MHz, auxiliary)
void MCO_init(void) {
  RCC->APB2PCENR |=   RCC_AFIOEN | RCC_IOPCEN;
  GPIOC->CFGLR   &= ~(0b1111<<(4<<2));
  GPIOC->CFGLR   |=   0b1011<<(4<<2);
}

// ===================================================================================
// Delay Functions
// ===================================================================================

// Wait n counts of SysTick
void DLY_ticks(uint32_t n) {
  uint32_t end = STK->CNT + n;
  while(((int32_t)(STK->CNT - end)) < 0 );
}

// ===================================================================================
// Independent Watchdog Timer (IWDG) Functions
// ===================================================================================

// Start independent watchdog timer (IWDG) with given time in milliseconds (max 8191).
// Once the IWDG has been started, it cannot be disabled, only reloaded (feed).
// It can be stopped by disabling the internal low-speed clock (LSI).
void IWDG_start(uint16_t ms) {
  LSI_enable();                         // enable internal low-speed clock (LSI)
  IWDG->CTLR = 0x5555;                  // allow register modification
  while(IWDG->STATR & IWDG_PVU);        // wait for clock register to be ready
  IWDG->PSCR = 0b111;                   // set LSI clock prescaler 256
  while(IWDG->STATR & IWDG_RVU);        // wait for reload register to be ready
  IWDG->RLDR = ms >> 1;                 // set watchdog counter reload value
  IWDG->CTLR = 0xAAAA;                  // load reload value into watchdog counter
  IWDG->CTLR = 0xCCCC;                  // enable IWDG
}

// Reload watchdog counter with n milliseconds, n<=8191
void IWDG_reload(uint16_t ms) {
  IWDG->CTLR = 0x5555;                  // allow register modification
  while(IWDG->STATR & IWDG_RVU);        // wait for reload register to be ready
  IWDG->RLDR = ms >> 1;                 // set watchdog counter reload value
  IWDG->CTLR = 0xAAAA;                  // load reload value into watchdog counter
}

// ===================================================================================
// Sleep Functions
// ===================================================================================

// Init automatic wake-up timer
void AWU_init(void) {
  LSI_enable();                         // enable internal low-speed clock (LSI)
  EXTI->EVENR |= ((uint32_t)1<<9);      // enable AWU event
  EXTI->FTENR |= ((uint32_t)1<<9);      // enable AWU falling edge triggering
  RCC->APB1PCENR |= RCC_PWREN;          // enable power module
  PWR->AWUCSR = PWR_AWUCSR_AWUEN;       // enable automatic wake-up timer
}

// Put device into sleep, wake up by interrupt
void SLEEP_WFI_now(void) {
  PWR->CTLR &= ~PWR_CTLR_PDDS;          // set power-down mode to sleep
  __WFI();                              // wait for interrupt
}

// Put device into sleep, wake up by event
void SLEEP_WFE_now(void) {
  PWR->CTLR &= ~PWR_CTLR_PDDS;          // set power-down mode to sleep
  __WFE();                              // wait for event
}

// Put device into standby (deep sleep), wake up interrupt
void STDBY_WFI_now(void) {
  PWR->CTLR   |= PWR_CTLR_PDDS;         // set power-down mode to standby (deep sleep)
  PFIC->SCTLR |= PFIC_SLEEPDEEP;        // set deep sleep mode
  __WFI();                              // wait for interrupt
  PFIC->SCTLR &= ~PFIC_SLEEPDEEP;       // unset deep sleep mode
}

// Put device into standby (deep sleep), wake up event
void STDBY_WFE_now(void) {
  PWR->CTLR   |= PWR_CTLR_PDDS;         // set power-down mode to standby (deep sleep)
  PFIC->SCTLR |= PFIC_SLEEPDEEP;        // set deep sleep mode
  __WFE();                              // wait for event
  PFIC->SCTLR &= ~PFIC_SLEEPDEEP;       // unset deep sleep mode
}

// ===================================================================================
// C++ Support
// Based on CNLohr ch32v003fun: https://github.com/cnlohr/ch32v003fun
// ===================================================================================
#ifdef __cplusplus
// This is required to allow pure virtual functions to be defined.
extern void __cxa_pure_virtual() { while (1); }

// These magic symbols are provided by the linker.
extern void (*__preinit_array_start[]) (void) __attribute__((weak));
extern void (*__preinit_array_end[]) (void) __attribute__((weak));
extern void (*__init_array_start[]) (void) __attribute__((weak));
extern void (*__init_array_end[]) (void) __attribute__((weak));

void __libc_init_array(void) {
  uint32_t count, i;

  count = __preinit_array_end - __preinit_array_start;
  for (i = 0; i < count; i++) __preinit_array_start[i]();

  count = __init_array_end - __init_array_start;
  for (i = 0; i < count; i++) __init_array_start[i]();
}
#endif

// ===================================================================================
// C version of CH32V003 Startup .s file from WCH
// Based on CNLohr ch32v003fun: https://github.com/cnlohr/ch32v003fun
// ===================================================================================
int main(void) __attribute__((used));

extern uint32_t _sbss;
extern uint32_t _ebss;
extern uint32_t _data_lma;
extern uint32_t _data_vma;
extern uint32_t _edata;

// If you don't override a specific handler, it will just spin forever.
void DefaultIRQHandler(void) { while(1); }

// This makes it so that all of the interrupt handlers just alias to
// DefaultIRQHandler unless they are individually overridden.
#define DUMMY_HANDLER __attribute__((section(".text.vector_handler"), weak, alias("DefaultIRQHandler"), used))
DUMMY_HANDLER void NMI_Handler(void);
DUMMY_HANDLER void HardFault_Handler(void);
DUMMY_HANDLER void SysTick_Handler(void);
DUMMY_HANDLER void SW_Handler(void);
DUMMY_HANDLER void WWDG_IRQHandler(void);
DUMMY_HANDLER void PVD_IRQHandler(void);
DUMMY_HANDLER void FLASH_IRQHandler(void);
DUMMY_HANDLER void RCC_IRQHandler(void);
DUMMY_HANDLER void EXTI7_0_IRQHandler(void);
DUMMY_HANDLER void AWU_IRQHandler(void);
DUMMY_HANDLER void DMA1_Channel1_IRQHandler(void);
DUMMY_HANDLER void DMA1_Channel2_IRQHandler(void);
DUMMY_HANDLER void DMA1_Channel3_IRQHandler(void);
DUMMY_HANDLER void DMA1_Channel4_IRQHandler(void);
DUMMY_HANDLER void DMA1_Channel5_IRQHandler(void);
DUMMY_HANDLER void DMA1_Channel6_IRQHandler(void);
DUMMY_HANDLER void DMA1_Channel7_IRQHandler(void);
DUMMY_HANDLER void ADC1_IRQHandler(void);
DUMMY_HANDLER void I2C1_EV_IRQHandler(void);
DUMMY_HANDLER void I2C1_ER_IRQHandler(void);
DUMMY_HANDLER void USART1_IRQHandler(void);
DUMMY_HANDLER void SPI1_IRQHandler(void);
DUMMY_HANDLER void TIM1_BRK_IRQHandler(void);
DUMMY_HANDLER void TIM1_UP_IRQHandler(void);
DUMMY_HANDLER void TIM1_TRG_COM_IRQHandler(void);
DUMMY_HANDLER void TIM1_CC_IRQHandler(void);
DUMMY_HANDLER void TIM2_IRQHandler(void);

void InterruptVector(void)        __attribute__((naked, section(".init"), weak, alias("InterruptVectorDefault")));
void InterruptVectorDefault(void) __attribute__((naked, section(".init")));

void InterruptVectorDefault(void) {
	asm volatile( "\n\
	.align  2\n\
	.option   push;\n\
	.option   norvc;\n\
	j handle_reset\n\
	.word   0\n\
	.word   NMI_Handler               /* NMI Handler */                    \n\
	.word   HardFault_Handler         /* Hard Fault Handler */             \n\
	.word   0\n\
	.word   0\n\
	.word   0\n\
	.word   0\n\
	.word   0\n\
	.word   0\n\
	.word   0\n\
	.word   0\n\
	.word   SysTick_Handler           /* SysTick Handler */                \n\
	.word   0\n\
	.word   SW_Handler                /* SW Handler */                     \n\
	.word   0\n\
	/* External Interrupts */                                              \n\
	.word   WWDG_IRQHandler           /* Window Watchdog */                \n\
	.word   PVD_IRQHandler            /* PVD through EXTI Line detect */   \n\
	.word   FLASH_IRQHandler          /* Flash */                          \n\
	.word   RCC_IRQHandler            /* RCC */                            \n\
	.word   EXTI7_0_IRQHandler        /* EXTI Line 7..0 */                 \n\
	.word   AWU_IRQHandler            /* AWU */                            \n\
	.word   DMA1_Channel1_IRQHandler  /* DMA1 Channel 1 */                 \n\
	.word   DMA1_Channel2_IRQHandler  /* DMA1 Channel 2 */                 \n\
	.word   DMA1_Channel3_IRQHandler  /* DMA1 Channel 3 */                 \n\
	.word   DMA1_Channel4_IRQHandler  /* DMA1 Channel 4 */                 \n\
	.word   DMA1_Channel5_IRQHandler  /* DMA1 Channel 5 */                 \n\
	.word   DMA1_Channel6_IRQHandler  /* DMA1 Channel 6 */                 \n\
	.word   DMA1_Channel7_IRQHandler  /* DMA1 Channel 7 */                 \n\
	.word   ADC1_IRQHandler           /* ADC1 */                           \n\
	.word   I2C1_EV_IRQHandler        /* I2C1 Event */                     \n\
	.word   I2C1_ER_IRQHandler        /* I2C1 Error */                     \n\
	.word   USART1_IRQHandler         /* USART1 */                         \n\
	.word   SPI1_IRQHandler           /* SPI1 */                           \n\
	.word   TIM1_BRK_IRQHandler       /* TIM1 Break */                     \n\
	.word   TIM1_UP_IRQHandler        /* TIM1 Update */                    \n\
	.word   TIM1_TRG_COM_IRQHandler   /* TIM1 Trigger and Commutation */   \n\
	.word   TIM1_CC_IRQHandler        /* TIM1 Capture Compare */           \n\
	.word   TIM2_IRQHandler           /* TIM2 */                           \n\
	.option   pop;\n");
}

void handle_reset(void) {
  uint32_t *src, *dst;

  asm volatile( "\n\
    .option push\n\
    .option norelax\n\
    la gp, __global_pointer$\n\
    .option pop\n\
    la sp, _eusrstack\n"
    #if __GNUC__ > 10
"   .option arch, +zicsr\n"
    #endif

    // Setup the interrupt vector, processor status and INTSYSCR
"   li a0, 0x80\n\
    csrw mstatus, a0\n\
    li a3, 0x3\n\
    ;csrw 0x804, a3\n\
    la a0, InterruptVector\n\
    or a0, a0, a3\n\
    csrw mtvec, a0\n" 
    : : : "a0", "a3", "memory"
  );

  // Clear BSS
  dst = &_sbss;
  while(dst < &_ebss) *dst++ = 0;

  // Copy data from FLASH to RAM
  src = &_data_lma;
  dst = &_data_vma;
  while(dst < &_edata) *dst++ = *src++;

  // C++ Support
  #ifdef __cplusplus
  __libc_init_array();
  #endif

  // Init system
  SYS_init();

  // Set mepc to be main as the root app
  asm volatile(
"   csrw mepc, %[main]\n"
"   mret\n" : : [main]"r"(main)
  );
}
