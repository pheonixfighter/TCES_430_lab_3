#include <stdint.h>
#include "stm32f7xx.h"
#if !defined(__SOFT_FP__) && defined(__ARM_FP)
	#warning "FPU is not initialized, but the project is compiling for an FPU. Please initialize the FPU before use."
#endif

// Define states for our sequence state machine
typedef enum {
	BLINK_LED2, // Flash LED 2 on PB4 (5 times)
	BLINK_LED3, // Flash LED 3 on PG7 (8 times)
	PAUSE_STATE // 2-second pause before restarting
} SequenceState;

// Volatile variable to pass the halt state between the main loop and the ISR
volatile uint8_t exti_triggered = 0;


void simple_delay(uint32_t count) {
	while (count--) {
			__asm__("NOP");
	}
}


int main(void) {
	// ------------------------------------------------------------
	// 1. CLOCK ENABLES
	// ------------------------------------------------------------
	// Bit 8 = GPIOI, Bit 1 = GPIOB, Bit 6 = GPIOG, Bit 7 = GPIOH
	 RCC->AHB1ENR |= (1UL << 8) | (1UL << 1) | (1UL << 6) | (1UL << 7);
	// Bit 14 enables SYSCFG clock (for routing GPIO to EXTI)
	 RCC->APB2ENR |= (1UL << 14);
	// Bit 0 enables TIM2 clock on the APB1 bus
	 RCC->APB1ENR |= (1UL << 0);

	// ------------------------------------------------------------
	// 2. GPIO CONFIGURATION
	// ------------------------------------------------------------
	// LED 1 (PI3) -> Output
	 GPIOI->MODER &= ~(3UL << (3 * 2));
	 GPIOI->MODER |= (1UL << (3 * 2));
	 GPIOI->OTYPER &= ~(1UL << 3);
	// Switch 2 (PI2) -> Input with Pull-up
	 GPIOI->MODER &= ~(3UL << (2 * 2));
	 GPIOI->PUPDR &= ~(3UL << (2 * 2));
	 GPIOI->PUPDR |= (1UL << (2 * 2));
	// LED 2 (PB4) -> Output
	 GPIOB->MODER &= ~(3UL << (4 * 2));
	 GPIOB->MODER |= (1UL << (4 * 2));
	 GPIOB->OTYPER &= ~(1UL << 4);
	// LED 3 (PG7) -> Output
	 GPIOG->MODER &= ~(3UL << (7 * 2));
	 GPIOG->MODER |= (1UL << (7 * 2));
	 GPIOG->OTYPER &= ~(1UL << 7);
	 // Switch 2 (PI0, D5) -> Input with output
	 GPIOI->MODER &= ~(3UL);
	 GPIOI->PUPDR &= ~(3UL);
	 GPIOI->PUPDR |= (1UL);
	 // LED 4 (PH6, D6) -> Push-Pull output
	 GPIOH->MODER &= 0;
	 GPIOH->MODER |= (1UL << (2 * 6));
	 GPIOH->OTYPER &= ~(1UL << 6);

	// ------------------------------------------------------------
	// 3. EXTI INTERRUPT & PRIORITY CONFIGURATION (PI2)
	// ------------------------------------------------------------
	// Route Port I Pin 2 to EXTI2 (Bits [11:8] in EXTICR[0] gets 0x8)
	 SYSCFG->EXTICR[0] &= ~(0xFUL << 8);
	 SYSCFG->EXTICR[0] |= (0x8UL << 8);
	 EXTI->IMR |= (1UL << 2); // Unmask line 2
	 EXTI->FTSR |= (1UL << 2); // Fall on button press (GND)
	 EXTI->RTSR &= ~(1UL << 2); // Disable rise trigger
	// EXTI2 is IRQ number 8 in the NVIC table.
	// NVIC->IP is a byte array indexed by IRQ number, EXTI2’s priority byte is IP[8]
	// Only upper 4 bits are implemented (16 levels)
	 // lower number = higher priority
	 NVIC->IP[8] |= (5UL << 4); // Assign Priority 5 to IRQ8
	// Enable IRQ8 (EXTI2); bit 8 of NVIC_ISER0
	 NVIC->ISER[0] |= (1UL << 8); //Enable IRQ 8


	// ------------------------------------------------------------
	// 4. EXTI INTERRUPT & PRIORITY CONFIGURATION (PI0, or D5 on the board)
	// ------------------------------------------------------------
	 // Route Port I Pin 0 to EXTI0 (Bits [3:0] in EXTICT[0] gets 0x8
	 SYSCFG->EXTICR[0] &= ~(0xFUL); // no shifting since we are working with EXTI0
	 SYSCFG->EXTICR[0] |= (0x8UL);
	 EXTI->IMR |= (1UL); // Unmasks line 0
	 EXTI->FTSR |= 1UL; // Fall on button press
	 EXTI->RTSR &= ~(1UL); // Disable rise trigger
	 // EXTI0 is the IRQ number 6 in the NVIC table
	 // NVIC->IP is a byte array indexed by IRQ number. EXTI0's priority byte is IP[0]
	 // Only upper 4 bits are implemented (16 levels), and lower # = higher priority
	 NVIC->IP[6] |= (2UL << 4);
	 // Enable IRQ6
	 NVIC->ISER[0] |= (1UL << 6); // Enable IRQ 6


	// ------------------------------------------------------------
	// 5. TIMER 2 CONFIGURATION (~250ms update window)
	// ------------------------------------------------------------
	TIM2->PSC = 15999; //Assumes 16MHz timer clock
	TIM2->ARR = 250; //250ms
	TIM2->SR &= ~(1UL << 0);
	TIM2->CR1 |= (1UL << 0);


	SequenceState current_state = BLINK_LED2;
	uint8_t toggle_count = 0;
	uint8_t pin_state = 0;
	while (1) {
		// ------------------------------------------------------------
		// SEQUENTIAL STATE MACHINE (Driven by TIM2 ticks)
		// ------------------------------------------------------------
		if (TIM2->SR & (1UL << 0)) {
			TIM2->SR &= ~(1UL << 0); // Clear timer update flag
			// If the priority interrupt is running, halt state transitions
			if (exti_triggered) {
				continue;
			}

			switch (current_state) {

			case BLINK_LED2:
				if (toggle_count < 10) { // 5 blinks = 10 toggles
				 pin_state = !pin_state;
				if (pin_state) GPIOB->BSRR = (1UL << 4);
				else GPIOB->BSRR = (1UL << (4 + 16));
				toggle_count++;
				 } else {
				 GPIOB->BSRR = (1UL << (4 + 16));
				 toggle_count = 0;
				 pin_state = 0;
				 current_state = BLINK_LED3;
				}
				break;
			case BLINK_LED3:
				if (toggle_count < 16) { // 8 blinks = 16 toggles
				 pin_state = !pin_state;
				if (pin_state) GPIOG->BSRR = (1UL << 7);
				else GPIOG->BSRR = (1UL << (7 + 16));
				 toggle_count++;
				 } else {
				 GPIOG->BSRR = (1UL << (7 + 16));
				 toggle_count = 0;
				pin_state = 0;
				 TIM2->ARR = 2000; // Switch to 2-second pause duration
				 current_state = PAUSE_STATE;
				 }
				break;
			case PAUSE_STATE:
				TIM2->ARR = 250; // Restore standard blink interval speed
				current_state = BLINK_LED2;
				break;
			}
		 }
	}
}
	// ------------------------------------------------------------
	// 6. INTERRUPT SERVICE ROUTINE (ISR) FOR EXTI2
	// ------------------------------------------------------------
void EXTI2_IRQHandler(void) {
	if (EXTI->PR & (1UL << 2)) {
		EXTI->PR = (1UL << 2); // Clear EXTI pending flag
		exti_triggered = 1; // Raise halt flag for the main background thread
		// Execute the emergency sequence: 3 blinks on LED 1 (PI3)
		// 3 complete blinks = 6 alternate toggles
		for (int i = 0; i < 6; i++) {
			if (i % 2 == 0) {
			 GPIOI->BSRR = (1UL << 3); // Turn LED 1 ON
			} else {
			 GPIOI->BSRR = (1UL << (3 + 16)); // Turn LED 1 OFF
			}
			simple_delay(800000);
		}
		EXTI->PR = (1UL << 2); // Clear EXTI pending flag
		NVIC->ICPR[0] = (1UL << 8); // Clear NVIC Pending bit for IRQ8
		exti_triggered = 0; // Drop halt flag, allowing LED 2/3 to resume instantly
	}
}

void EXTI0_IRQHandler(void) {
	if (EXTI->PR & 1UL) {
		EXTI->PR = 1UL;  // Clears EXTI pending flag
		exti_triggered = 1; // Raise halt flag for the main background thread
		// Executes the response for triggering the I0 interrupt
		// Will blink LED_4 5 times
		for (int i = 0; i < 10; i++){
			if (i % 2 == 0) {
				GPIOH->BSRR = (1UL << 6);
			} else {
				GPIOH->BSRR = (1UL << (6 + 16));
			}
			simple_delay(800000);
		}
		EXTI->PR = (1UL);
		NVIC->ICPR[0] = (1UL << 6); // Clears NVIC pending bit for IRQ 6
		exti_triggered = 0;
	}
}


