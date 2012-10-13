/*
 *		Copyright (c) 2012 Alex Norman.  All rights reserved.
 */

#include <inttypes.h>
#include <avr/io.h>
#include <util/delay.h>
#include <util/atomic.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <avr/wdt.h>

//verify
#define LED_R_PIN PINA4
#define LED_G_PIN PINA3
#define LED_B_PIN PINA2

int main(void) {
   sei();
   wdt_disable();

   //set up timer
   TCCR1A = 0;
   TCCR1B = 0; 
   TCCR1C = 0; 
   TCNT1L = 0;
   OCR1AL = 0xF;
   
   //toggles OC1[A,B] when the timer reaches the top
   //TCCR1A = _BV(COM1A0) |  _BV(COM1B0);
   TCCR1A = _BV(COM1A1) | _BV(COM1A0) |  _BV(COM1B0);

   //CTC mode, resets clock when it meets OCR1A
   TCCR1B = _BV(WGM12) | _BV(CS10) | _BV(CS11);

   TCCR1C = _BV(FOC1A); //force one of the outputs to high

   //set up output PWM
   DDRA |= _BV(PINA6) | _BV(PINA5);

   //LED OUT
   DDRA |= _BV(LED_R_PIN) | _BV(LED_G_PIN) | _BV(LED_B_PIN);

   //igniter
   DDRA |= _BV(PINA0);

   TIMSK1 = 0;

   PORTA |= _BV(LED_B_PIN);

   while(1) {
   }
}

