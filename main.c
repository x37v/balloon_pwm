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
void cc_callback(MidiDevice * device, uint8_t channel, uint8_t num, uint8_t value);

int main(void) {
   MidiDevice midi_device;

   sei();
   wdt_disable();

   //setup the device
   midi_usb_init(&midi_device);
   midi_register_pitchbend_callback(&midi_device, pitchbend_callback);
   //midi_register_cc_callback(&midi_device, cc_callback);

   //set up output
   DDRB |= _BV(PINB6) | _BV(PINB5);
   PORTB = 0;

   //set up timer
   
   // timer0 off
   TCCR0A = 0;
   TCCR0B = 0;
   TCNT0 = 0;

   // timer1 off
   TCCR1A = 0;
   TCCR1B = 0; 
   TCCR1C = 0; 
   TCNT1L = 0;

   OCR0A = 0;  // green  invert
   OCR0B = 0;  // red    invert
   OCR1AL = 0;  // blue    invert
   OCR1BL = 0xFF;  // all    invert

   //timer1 on, 8 bit PWM mode
   //TCCR1A = 0xB1;
   //TCCR1B = 0x0B;
   
   //CTC mode, resets clock when it meets OCR1A
   TCCR1A = _BV(COM1A0) | _BV(COM1B1) | _BV(COM1B0);
   TCCR1B = _BV(WGM12) | _BV(CS10) | _BV(CS11);

   TIMSK1 = 0;

   while(1)
      midi_device_process(&midi_device);
}

void cc_callback(MidiDevice * device, uint8_t channel, uint8_t num, uint8_t value) {
   midi_send_cc(device, channel, num, value);
   OCR1A = 0xF + ((uint16_t)value << 7);
}

void pitchbend_callback(MidiDevice * device, uint8_t channel, uint8_t lsb, uint8_t msb) {
   uint16_t combined = lsb | ((uint16_t)msb << 7);
   combined = 0xF + combined;

   ATOMIC_BLOCK(ATOMIC_FORCEON) {
      OCR1AH = (combined >> 8);
      OCR1AL = combined;
      TCNT1 = 0; 
   }


   midi_send_pitchbend(device, channel, (int16_t)combined - 8192);
}

