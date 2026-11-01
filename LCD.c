
#include <stdint.h>
#include "LCD.h"
#include "stm32f7xx.h"

void TIM2Init(void){
	// enable TIM2 clock
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

	// Setting the prescaler for the clock to 1 kHZ
	TIM2->PSC = (16000 - 1);

	// Auto-reload register cleared to allow scaling inside the delay function
	TIM2->ARR = 0;

	// Forcing an update to latch prescaler into registers
	TIM2->EGR |= TIM_EGR_UG;
	TIM2->SR = 0; // resets for the timer to start

}


void TIM2_DelayMs(uint32_t miliseconds){
	if (miliseconds == 0) return;


	TIM2->CNT = 0; // Resets counTer to 0
	TIM2->ARR = miliseconds; // Sets the duration in miliseconds
	TIM2->CR1 |= TIM_CR1_CEN; // Enables counter

	// Polls the Update Interrupt Flag (UIF) until the counter hits the ARR target match
	while (!(TIM2->SR & TIM_SR_UIF));

	TIM2->SR &= ~TIM_SR_UIF; // Clears the UIF flag for the next execution
	TIM2->CR1 &= ~TIM_CR1_CEN; // Stops TIM2 to save power
}



/**
 * Initializes the following pins for the LCD:
 * 	-RS: D0 (C7)
 * 	-R/W: D1 (C6)
 * 	-Enable: D2 (G6)
 *
 * 	-Data 4: D12 (B14)
 * 	-Data 5: D13 (I1)
 * 	-Data 6: D14 (B9)
 * 	-Data 7: D15 (B8)
 */
void initializeLCDPins(void) {
	// Initializes D0 (C7)
	GPIOC->MODER &= ~(3UL << (7 * 2));
	GPIOC->MODER |= (1UL << (7 * 2));

	// INitializes D1 (C6)
	GPIOC->MODER &= ~(3UL << (6 * 2));
	GPIOC->MODER |= (1UL << (6 * 2));
	GPIOC->BSRR |= (1UL << (6 + 16));


	// Initializes D2 (G6)
	GPIOG->MODER &= ~(3UL << (6 * 2));
	GPIOG->MODER |= (1UL << (6 * 2));

	// Initializes D12 (B14)
	GPIOB->MODER &= ~(3UL << (14 * 2));
	GPIOB->MODER |= (1UL << 14 * 2);

	// Initializes D13 (I1)
	GPIOI->MODER &= ~(3UL << (1 * 2));
	GPIOI->MODER |= (1UL << (1 * 2));

	// INitializes D14 (B9)
	GPIOB->MODER &= ~(3UL << (9 * 2));
	GPIOB->MODER |= (1UL << (9 * 2));

	// Initializes D15 (B8)
	GPIOB->MODER &= ~(3UL << (8 * 2));
	GPIOB->MODER |= (1UL << (8 * 2));


	// initializing data pins to high speed pin mode
	GPIOC->OSPEEDR |= (0x3 << (7 * 2));
	GPIOC->OSPEEDR |= (0x3 << (6 * 2));
	GPIOG->OSPEEDR |= (0x3 << (6 * 2));
	GPIOB->OSPEEDR |= (0x3 << (14 * 2));
	GPIOI->OSPEEDR |= (0x3 << (1 * 2));
	GPIOB->OSPEEDR |= (0x3 << (9 * 2));
	GPIOB->OSPEEDR |= (0x3 << (8 * 2));
}



/**
 *
 */
void sendNibble(uint8_t nibble, uint8_t RS){
	// Establishes RS pin line state
	GPIOC->BSRR |= (1 << (7 + BSRRShift(RS)));

	// Parses Data bits 0 - 3
	// Assigns bit 0 -> D4 -> PB14
	GPIOB->BSRR |= (1 << (14 + BSRRShift(nibble & 1UL)));

	// Assigns bit 1 -> D5 -> PI1
	GPIOI->BSRR |= (1 << (1 + BSRRShift((nibble >> 1) & 1UL)));

	// Assigns bit 2 -> D6 -> PB9
	GPIOB->BSRR |= (1 << (9 + BSRRShift((nibble >> 2) & 1UL)));

	// Assigns bit 3 -> D7 -> PB8
	GPIOB->BSRR |= (1 << (8 + BSRRShift((nibble >> 3)& 1UL)));

	// Cycle enable pulse to send data
	GPIOG->BSRR |= (1 << (6 + BSRRShift(1))); // Sets enable high
	TIM2_DelayMs(2);
	GPIOG->BSRR |= (1 << (6 + BSRRShift(0))); // Sets enable high
	TIM2_DelayMs(2);
}


int BSRRShift(uint8_t outputBit){
	if (outputBit){
		return 0;
	} else {
		return 16;
	}
}

void sendCommand(uint8_t command){
	sendNibble(command >> 4, 0); // Transmit Upper Nibble
	sendNibble(command & 0x0F, 0); // Transmit Lower nibble
}

void sendData(uint8_t data) {
	sendNibble(data >> 4, 1);
	sendNibble(data & 0x0F, 1);
}



void LCDInit(void){
	// initializes the TIM2 clock
#ifndef TIM2_INITIALIZED
	TIM2Init();
#endif
	initializeLCDPins();

	TIM2_DelayMs(100);

	// standard setup for HD44780 display
	sendNibble(0x03, 0);
	TIM2_DelayMs(2);
	sendNibble(0x03, 0);
	TIM2_DelayMs(10);
	sendNibble(0x03, 0);
	TIM2_DelayMs(2);
	sendNibble(0x02, 0); // Activates 4-bit projile structure
	TIM2_DelayMs(2);

	// Opational profile parameters
	sendCommand(0x28); // Configures 4-bit mode with 2 lines of selection layout and a 5X8 font
	sendCommand(0x0C); // Display ON, cursor block hidden
	sendCommand(0x06);
	sendCommand(0x01);

}

void sendString(char *str){
	while(*str) {
		sendData(*str++);
	}
}

