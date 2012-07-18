/*
 *		Copyright (c) 2012 Alex Norman.  All rights reserved.
 *
 *		This file is part of rs485_leds
 *		
 *		rs485_leds is free software: you can redistribute it and/or modify it
 *		under the terms of the GNU General Public License as published by the
 *		Free Software Foundation, either version 3 of the License, or (at your
 *		option) any later version.
 *		
 *		rs485_leds is distributed in the hope that it will be useful, but
 *		WITHOUT ANY WARRANTY; without even the implied warranty of
 *		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 *		Public License for more details.
 *		
 *		You should have received a copy of the GNU General Public License auint16_t
 *		with rs485_leds.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <inttypes.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>

ISR(TIMER1_COMPA_vect) {
   PORTB ^= (_BV(PINB6) | _BV(PINB5));
   TCNT1 = 0;
}

int main(void) {
   //set up output
   DDRB |= _BV(PINB6) | _BV(PINB5);;
   PORTB = _BV(PINB6);

   //set up timer
   //prescaler
   TCCR1B =  _BV(CS11);

   OCR1A = 0xFFF;
   TIMSK1 = _BV(TOIE1) | _BV(OCIE1A);

   sei();

   while(1);
}
