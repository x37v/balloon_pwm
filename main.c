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
#include <util/atomic.h>
#include "gamma.h"

#define STAGE_COUNTS 16
#define PROGRAM_SUBDIV 32 
#define STAGE_BPRESS_THRESH 16

#define LED_R_PIN PINA4
#define LED_G_PIN PINA3
#define LED_B_PIN PINA2

#define BUTTON_PIN PINA7
#define IGNITE_PIN PINA0

#define BUZZER_PIN0 PINA6
#define BUZZER_PIN1 PINA5

#define TRUE 1
#define FALSE 0

volatile uint8_t led_count = 0;

//when do you turn them off
volatile uint8_t led_r_off = 0;
volatile uint8_t led_g_off = 0;
volatile uint8_t led_b_off = 0;

volatile uint16_t rollover_count = 0;
volatile uint8_t rollover = FALSE;


#define FOUR_BIT_LED

#define ROLLOVER_INC 64

#define NUM_COLORSETS 5
static uint8_t color_sets[NUM_COLORSETS * 3] = {
  255, 255, 0,
  255, 0, 0,
  255, 0, 170,
  0, 255, 0,
  0, 1, 255
};

static uint8_t colors_off[3] = {0, 0, 0};

void enable_buzzer(uint16_t v);
void disable_buzzer(void);

#ifdef FOUR_BIT_LED
ISR(TIM0_OVF_vect) {
#else
ISR(TIM0_COMPA_vect) {
#endif
  TCNT0 = 0;

#ifdef FOUR_BIT_LED
  led_count += 16;
  rollover_count += ROLLOVER_INC;
#else
  led_count++;
  rollover_count += ROLLOVER_INC >> 4;
#endif

  if (rollover_count == 0)
    rollover = TRUE;

  if (led_count >= led_r_off)
    PORTA &= ~(_BV(LED_R_PIN));
  else if (led_count == 0)
    PORTA |= _BV(LED_R_PIN);
  if (led_count >= led_g_off)
    PORTA &= ~(_BV(LED_G_PIN));
  else if (led_count == 0)
    PORTA |= _BV(LED_G_PIN);
  if (led_count >= led_b_off)
    PORTA &= ~(_BV(LED_B_PIN));
  else if (led_count == 0)
    PORTA |= _BV(LED_B_PIN);
}

void init_buzzer() {
  //set up timer 1
  TCCR1A = 0;
  TCCR1B = 0; 
  TCCR1C = 0; 
  TCNT1L = 0;
  OCR1AL = 0xF;

  //CTC mode, resets clock when it meets OCR1A
  TCCR1B = _BV(WGM12) | _BV(CS10) | _BV(CS11);

  TIMSK1 = 0;
}

void enable_buzzer(uint16_t v) {
  //toggles OC1[A,B] when the timer reaches the top
  TCCR1A = _BV(COM1A1) | _BV(COM1A0) |  _BV(COM1B0);
  TCCR1C = _BV(FOC1A); //force one of the outputs to high
  OCR1AH = v >> 8;
  OCR1AL = v & 0xFF;
  TCNT1 = 0;
}

void disable_buzzer(void) {
  TCCR1A = 0;
}

void init_timer0(void) {
  TCCR0A = 0; //normal mode
  TCCR0B = _BV(CS00); //no prescale
#ifdef FOUR_BIT_LED
  //OCR0A = 16 << 4;
  TIMSK0 = _BV(TOIE0); //enable interrupts
#else
  OCR0A = 16;
  TIMSK0 = _BV(OCIE0A); //enable interrupts for output compare match a
#endif

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
  DDRA |= _BV(BUZZER_PIN0) | _BV(BUZZER_PIN1);

  //set buzzer pin
  PORTA |= _BV(BUZZER_PIN0);

  //button is input [0], set pullup
  PORTA |= _BV(BUTTON_PIN);
}

inline void set_led(uint8_t* rgb) {
  led_r_off = *rgb;
  led_g_off = *(rgb + 1);
  led_b_off = *(rgb + 2);
}

static uint8_t stage = 0;
static uint8_t stage_subdiv = 0;
static uint8_t stage_bpress = 0;
static uint8_t stage_new = TRUE;
static uint8_t stage_state = 0;
static uint32_t program_timeout = 0;
static uint32_t led_timeout = 0;
static uint8_t color_idx = 0;

void update_stage() {
  stage++;
  stage_subdiv = 0;
  stage_bpress = 0;
  stage_new = TRUE;
}

void exec_stage(uint32_t program_time) {
  switch(stage) {
    case 0:
      if (stage_new)
        stage_state = 0;

      if (stage_new || program_time == program_timeout) {
        if (stage_state == 0) {
          color_idx = (color_idx + 1) % NUM_COLORSETS;
          set_led(color_sets + 3 * color_idx);
          enable_buzzer(((rand() & 0xF) + 1) << 10);
          program_timeout = program_time + 2 + (rand() & 0x3F);
          led_timeout = program_time + 1;
          if (rand() & 0xFF >= 75) //66% of the time turn off
            stage_state = 1;
        } else {
          disable_buzzer();
          program_timeout = program_time + 8 + (rand() & 0xF);
          stage_state = 0;
        }
      }

      if (program_time == led_timeout)
        set_led(colors_off);
      break;
    case 4:
    default:
      break;
  }

  stage_new = FALSE;
}

int main(void) {
  static uint8_t button_down_index = 0;
  static uint8_t button_down_history = 0;
  static uint8_t button_down_last = FALSE;

  static uint32_t program_time = 0;

  wdt_disable();

  init_io();
  init_timer0();
  init_buzzer();

  sei();
  while(1) {
    //debounce button
    if (!(PINA & _BV(BUTTON_PIN)))
      button_down_history |= (1 << button_down_index);
    else
      button_down_history &= ~(1 << button_down_index);
    button_down_index = (button_down_index + 1) % 8;

    if (button_down_history == 0 && button_down_last) {
      //button is now up
      button_down_last = FALSE;

    } else if (button_down_history == 0xFF && !button_down_last) {
      //button is now down
      button_down_last = TRUE;

      /*
      //update 
      stage_bpress++;
      if (stage_bpress >= STAGE_BPRESS_THRESH)
        update_stage();
        */
      stage_new = TRUE;
    }

    //deal with timed events
    if (rollover) {
      program_time++;
      ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        rollover = FALSE;
        rollover_count = 0;
      }
      if (program_time % PROGRAM_SUBDIV == 0) {
        stage_subdiv++;
        if (stage_subdiv >= STAGE_COUNTS)
          update_stage();
      }
      exec_stage(program_time);
    }
  }
}

