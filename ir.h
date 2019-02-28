/*
 * ir.h
 *
 *  Created on: 22.02.2019
 *      Author: slawek
 */

#ifndef IR_H_
#define IR_H_

#include <stdint.h>
#include <stdbool.h>

/*
 * Funkcja wołana w przerwaniu capture. W parametrze przekazać wartość
 * rejestru capture (TIMx_CCRy).
 * Dla odebranego pakietu danych wywołuje ir_callback
 */
extern void ir_capture(uint16_t pulse);

/*
 * Funkcja wołana w przerwaniu od przepełnienia licznika
 */
extern void ir_ovf(void);


/*
 * Funkcja wołane po odbraniu zestawu danych.
 * W pierwszym wywołaniu dla nowego zestawu repeat == false
 * przy każdym kolejnym wywołaniu dla powtórzenia repeat == true
 */
extern void ir_callback(uint16_t address, uint16_t command, bool repeat);

/*
 * True jeśli starszy bajt jest dopełnieniem młodszego
 * Adres rozszerzony (16bitowy) zwróci false
 */
extern bool ir_isxored(uint16_t data);


#endif /* IR_H_ */
