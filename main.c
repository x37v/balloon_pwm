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
#include <implementations/lufa_midi/midi_usb.h>
#include <avr/wdt.h>

SIGNAL(TIMER1_COMPA_vect) {
   PORTB ^= (_BV(PINB6) | _BV(PINB5));
   TCNT1 = 0;
}

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
   DDRB |= _BV(PINB6) | _BV(PINB5);;
   PORTB = _BV(PINB6);

   //set up timer
   //prescaler
   TCCR1B =  _BV(CS11) | _BV(CS10);

   OCR1A = 0xF;
   TIMSK1 = _BV(TOIE1) | _BV(OCIE1A);


   while(1)
      midi_device_process(&midi_device);
}

void cc_callback(MidiDevice * device, uint8_t channel, uint8_t num, uint8_t value) {
   midi_send_cc(device, channel, num, value);
   OCR1A = 0xF + ((uint16_t)value << 7);
}

void pitchbend_callback(MidiDevice * device, uint8_t channel, uint8_t lsb, uint8_t msb) {
   uint16_t combined = lsb | ((uint16_t)msb << 7);
   OCR1A = 0xF + combined;
   midi_send_pitchbend(device, channel, (int16_t)combined - 8192);
}

