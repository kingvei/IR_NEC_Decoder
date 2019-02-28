/*
 * ir.c
 *
 *  Created on: 22.02.2019
 *      Author: slawek
 *
 *  Oryginalnie: 25 wrz 2014 na m168
 */

#include "ir_conf.h"
#include "ir.h"


//długość trwania impulsów danych (suma stanów H+L) dla ramek typu CONST_LENGTH
//   ___________   _   __   _   __   _   _           ______     _                                    ______
// _|           |_| |_|  |_| |_|  |_| |_| |_________|      |___| |__________________________________|      |___|
//  ^--header-----^-0-^--1-^-0-^--1-^-0-^ ^         ^--repeat--^                                    ^--repeat--^
//                                        \ptrail                \ptrail
//  ^----CONST_LENGTH:frame--("gap" wg.lirc.org)----^------------frame------------------------------^

//   ms                             upc                 ferguson           rcv-200e
//1.nagłówek                         13.44                  13.49              13.50
//2.bit 0                           1.11-1.13              1.11-1.13         1.11-1.13
//3.bit 1                           2.23-2.25              2.23-2.26          2.24-2.26
//4.powtórzenie                       11.04                11.02-11.15        11.18-11.20
//5.długość ramki danych (z przerwą po) 107.54            107.83-107.86       107.88
//6.długość "ramki" powtórzenia   107.31-107.34           107.70-107.78       107.76
//7.max puls (6-4)                     96.3                    96.76            96.58

//czas trwania w µs
//nagłówek  ~13.44ms
#define HEADER_US	13500
//bit 0 ~1,12ms
#define BIT_ZERO_US	1150
//bit 1 ~2,24ms
#define BIT_ONE_US	2250
//impuls powtórzenia ~11,05 ms
#define REPEAT_US	11100
//tolerancja nagłówka, bitów i powtórzenia +/-us
#define MARGIN_US	100
//długość całej ramki w us
#define FRAME_US	107750	//było 107600
//tolerancja dlugości ramki +/-us
#define FRAME_MARGIN_US	350

//*************************************
//ilość bitów danych (bez nagłówka).
#define IR_NUM_OF_BITS 32


//najdłuższy impuls nie powinien wystąpić przed poniższym stanem licznika
#define IR_LONG_CNT	60000U

//sprawdzamy czy najdłuższy impuls zmieści się w zakresie licznika (unsigned int)
#if ( (( FRAME_US + FRAME_MARGIN_US - REPEAT_US + MARGIN_US ) * IR_FREQ/1000000UL ) / ( IR_PRESKALER ) ) > UINT16_MAX
#error "za niski preskaler, najdłuższy interwał nie mieści się w zakresie licznika"
#endif
//sprawdzamy czy zakres licznika jest w pełni wykorzystany
#if ( (( FRAME_US + FRAME_MARGIN_US - REPEAT_US + MARGIN_US ) * IR_FREQ/1000000UL ) / ( IR_PRESKALER ) ) < IR_LONG_CNT
#warning "Obniż preskaler"
#endif

//*** stałe czasowe składowych impulsów w tickach zegara
#define HEADER		(unsigned int)(( HEADER_US/1000.0 * IR_FREQ ) / ( IR_PRESKALER * 1000UL ))
#define BIT_ZERO	(unsigned int)(( BIT_ZERO_US/1000.0 * IR_FREQ ) / ( IR_PRESKALER * 1000UL ))
#define BIT_ONE		(unsigned int)(( BIT_ONE_US/1000.0 * IR_FREQ ) / ( IR_PRESKALER * 1000UL ))
#define REPEAT		(unsigned int)(( REPEAT_US/1000.0 * IR_FREQ ) / ( IR_PRESKALER * 1000UL ))
#define MARGIN		(unsigned int)(( MARGIN_US/1000.0 * IR_FREQ/1000.0 ) / ( IR_PRESKALER ))
#define FRAME		(unsigned int)(( FRAME_US/1000.0 * IR_FREQ ) / ( IR_PRESKALER * 1000UL ))
#define FRAME_MARGIN	(unsigned int)(( FRAME_MARGIN_US/1000.0 * IR_FREQ ) / ( IR_PRESKALER * 1000UL ))


//definicja stanów maszyny stanów. Stany WAIT_FOR_x trzeba rozumieć jako
//"Oczekiwanie na zbocze opadające kończące puls x"
enum state_def { IDLE, WAIT_FOR_HEADER, WAIT_FOR_BIT, WAIT_FOR_REPEAT_GAP, WAIT_FOR_REPEAT_PULSE };

static enum state_def state; //bieżacy stan maszyny


//********************* FUNKCJE ************************

//******* funkcja wywoływana w przerwaniu Capt
void ir_decode(uint16_t pulse)
{
	static uint16_t msg_len; //czas trwania dotychczas wczytanego headera i bitów danych - do obliczenia repeat_gap przy adresach extended
	static uint8_t bits_to_read; //ile pozostało bitów do odczytania
	static uint32_t data;	//odczytane dane


	switch( state )
	{
		case IDLE:	//czekalim na pierwszy wykryty puls ever
			state = WAIT_FOR_HEADER;
			break;
		case WAIT_FOR_HEADER :

			if( pulse >= HEADER - MARGIN && pulse <= HEADER + MARGIN /*&& !overflowed*/) //wykryto nagłówek - zaczynamy zabawę
			{
				state = WAIT_FOR_BIT;
				bits_to_read = IR_NUM_OF_BITS;
				data = 0L;  //przygotuj czyste pole na nowe dane
				msg_len = pulse; //obliczanie faktycznej długości ramki danych
			}
			break;

		case WAIT_FOR_BIT :

			if( ( pulse >= BIT_ZERO - MARGIN && pulse <= BIT_ZERO + MARGIN /* && !overflowed*/) ||
				( pulse >= BIT_ONE - MARGIN && pulse <= BIT_ONE + MARGIN /* && !overflowed*/) )
			{
				data >>= 1;
				if( pulse >= BIT_ONE - MARGIN && pulse <= BIT_ONE + MARGIN )
				{
					data |= ~(UINT32_MAX>>1); //ustaw najstarszejszy bit
				}
				if(!--bits_to_read) //wszystkie bity przeczytane przejdź do następnego stanu
				{
					state = WAIT_FOR_REPEAT_GAP;
					ir_callback(data, data>>16, false);
				}
				msg_len += pulse; //obliczenie faktycznej długości komunikatu
			}
			else  //wystąpił impuls o nieoczekiwanej długości - reset maszyny stanów
				state = WAIT_FOR_HEADER;

			break;

		case WAIT_FOR_REPEAT_GAP :

			if( pulse >= FRAME - FRAME_MARGIN - msg_len && pulse <= FRAME + FRAME_MARGIN - msg_len /* && !overflowed*/) //odejmowanie tylko dla komunikatów typu CONST_LENGTH
			{
				state = WAIT_FOR_REPEAT_PULSE;
			}
			else
			{
				state = WAIT_FOR_HEADER;
			}
			break;

		case WAIT_FOR_REPEAT_PULSE :

			if( pulse >= REPEAT - FRAME_MARGIN && pulse <= REPEAT + FRAME_MARGIN  /*&& !overflowed*/)
			{
				state = WAIT_FOR_REPEAT_GAP;
				ir_callback(data, data>>16, true);
				msg_len = pulse;
			}
			else
			{
				state = WAIT_FOR_HEADER;
			}
			break;

	} //switch

} //end of ir_capture

//funkcja wywoływana w przerwaniu od przepełnienia licznika
void ir_reset(void)
{
	state = IDLE;
}


//callback wywoływany po otrzymaniu nowych danych
__attribute__((weak))
void ir_callback(uint16_t address, uint16_t command, bool repeat)
{
	(void)address;
	(void)command;
	(void)repeat;
}


bool ir_isxored(uint16_t command)
{
	return  ( UINT8_MAX == ( (uint8_t)(command>>8) ^ (uint8_t)(command) ) );
}

