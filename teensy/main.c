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
#include <util/atomic.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <implementations/lufa_midi/midi_usb.h>
#include <avr/wdt.h>

void pitchbend_callback(MidiDevice * device, uint8_t channel, uint8_t lsb, uint8_t msb);

int main(void) {
   MidiDevice midi_device;

   sei();
   wdt_disable();

   //setup the device
   midi_usb_init(&midi_device);
   midi_register_pitchbend_callback(&midi_device, pitchbend_callback);

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

   //set up output
   DDRB |= _BV(PINB6) | _BV(PINB5);

   TIMSK1 = 0;

   while(1)
      midi_device_process(&midi_device);
}

void pitchbend_callback(MidiDevice * device, uint8_t channel, uint8_t lsb, uint8_t msb) {
   uint16_t combined = lsb | ((uint16_t)msb << 7);
   midi_send_pitchbend(device, channel, (int16_t)combined - 8192);

   combined = 1 + (16384 -  combined);
   ATOMIC_BLOCK(ATOMIC_FORCEON) {
      OCR1AH = (combined >> 8);
      OCR1AL = combined & 0xFF;
      //TCNT1L = 0; 
   }
}

