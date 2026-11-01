/*
 * LCD.h
 *
 *  Created on: Oct 30, 2026
 *      Author: Robert Cromer
 */

#ifndef LCD_H_
#define LCD_H_

void TIM2Init(void);

void TIM2_DelayMs(uint32_t);

void initializeLCDPins(void);

void sendNibble(uint8_t, uint8_t);

void sendCommand(uint8_t);

void sendData(uint8_t);

int BSRRShift(uint8_t);

void LCDInit(void);

void sendString(char*);

#endif /* LCD_H_ */

