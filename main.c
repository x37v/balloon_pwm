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
#include "gamma.h"

#define LED_R_PIN PINA4
#define LED_G_PIN PINA3
#define LED_B_PIN PINA2

#define BUTTON_PIN PINA7
#define IGNITE_PIN PINA0

#define TRUE 1
#define FALSE 0

volatile uint8_t led_count = 0;

//when do you turn them off
volatile uint8_t led_r_off = 0;
volatile uint8_t led_g_off = 0;
volatile uint8_t led_b_off = 0;

//ISR(TIM0_OVF_vect) {
ISR(TIM0_COMPA_vect) {
  TCNT0 = 0;
  led_count++;

  if (led_count == led_r_off)
    PORTA &= ~(_BV(LED_R_PIN));
  else if (led_count == 0)
    PORTA |= _BV(LED_R_PIN);
  if (led_count == led_g_off)
    PORTA &= ~(_BV(LED_G_PIN));
  else if (led_count == 0)
    PORTA |= _BV(LED_G_PIN);
  if (led_count == led_b_off)
    PORTA &= ~(_BV(LED_B_PIN));
  else if (led_count == 0)
    PORTA |= _BV(LED_B_PIN);
}

void enable_buzzer(void) {
  //set up timer 1
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

  TIMSK1 = 0;
}

void init_timer0(void) {
  //set up timer 0
  TCCR0A = 0; //normal mode
  TCCR0B = _BV(CS00); //no prescale
  OCR0A = 16;

  //TIMSK0 = _BV(TOIE0); //enable interrupts
  TIMSK0 = _BV(OCIE0A); //enable interrupts for output compare match a
  TIFR0 = _BV(OCF0A);
}

void init_io(void) {
  //enable pullups
  MCUCR &= ~_BV(PUD);

  DDRA = 0;

  //LED OUT
  DDRA |= _BV(LED_R_PIN) | _BV(LED_G_PIN) | _BV(LED_B_PIN);

  //igniter
  DDRA |= _BV(IGNITE_PIN);

  //set up output PWM
  DDRA |= _BV(PINA6) | _BV(PINA5);

  //button is input [0], set pullup
  PORTA |= _BV(BUTTON_PIN);
}

int main(void) {
  wdt_disable();

  init_io();
  init_timer0();
  enable_buzzer();

  sei();

  uint8_t r = 0;
  uint8_t g = 23;
  uint8_t b = 64;

  uint8_t button_down_index = 0;
  uint8_t button_down_history = 0;
  uint8_t button_down_last = FALSE;

  //PORTA |= _BV(LED_R_PIN);
  while(1) {

    //debounce button
    if (!(PINA & _BV(BUTTON_PIN)))
      button_down_history |= (1 << button_down_index);
    else
      button_down_history &= ~(1 << button_down_index);
    button_down_index = (button_down_index + 1) % 8;

    if (button_down_history == 0) { //up
      if (button_down_last) {
        //led_r_off = 0xFF;
        //button state changed
        button_down_last = FALSE;
      }
    } else if (button_down_history == 0xFF) { //down
      if (!button_down_last) {
        //button state changed
        button_down_last = TRUE;
        if (led_r_off)
          led_r_off = 0;
        else
          led_r_off = 0xFF;
      }
    }

      ////_delay_ms(1);
      //r++;
      //g++;
      //b++;
      //led_r_off = gamma_correct(r);
      //led_g_off = gamma_correct(g);
      //led_b_off = gamma_correct(b);

  }
}

