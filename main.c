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

#define STAGE_COUNTS 28
#define PROGRAM_SUBDIV 32 
#define STAGE_BPRESS_THRESH 75
#define USE_REAL_RAND 0

#ifndef RANDOM_SEED
#define RANDOM_SEED 125
#endif

typedef enum {
  INTRO = 0,
  CLICK,
  CLICK_BURSTS,
  DRONE_ONE,
  DRONE_ALL,
  DRONE_ALL2,
  DRONE_ALL3,
  DRONE_CLICK,
  DRONE_CLICK2,
  CLICK_FINISH,
  DONE
} stage_t;

#define INITIAL_STAGE INTRO

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

#ifdef INITIAL_STAGE
static uint8_t stage = INITIAL_STAGE;
#else
static uint8_t stage = 0;
#endif
static uint8_t stage_subdiv = 0;
static uint8_t stage_bpress = 0;
static uint8_t stage_new = TRUE;
static uint8_t stage_state = 0;
static uint8_t bpress_new = FALSE;

static uint8_t program_timeout = 0;
static uint8_t led_timeout = 0;

#define FOUR_BIT_LED

#define ROLLOVER_INC 128

#define NUM_COLORSETS 8
#define NUM_COLORSETS_MOD (NUM_COLORSETS - 1)
static uint8_t color_sets[NUM_COLORSETS * 3] = {
  255, 255, 0,
  255, 0, 0,
  255, 0, 170,
  0, 255, 0,
  0, 1, 255,

  //repeats
  255, 255, 0,
  255, 0, 0,
  255, 0, 170,
};

#define NUM_DRONES 16
#define NUM_DRONES_MOD (NUM_DRONES - 1)
//y = -0.477272727272727 * x + 7821.70454545455
static uint16_t drones[NUM_DRONES] = {
  307, // <= 15746
  197, // <= 15976
  229, // <= 15908
  119, // <= 16139
  148, // <= 16079
  132, // <= 16112
  297, // <= 15767
  98, // <= 16183
  393, // <= 15565
  458, // <= 15427

  //repeats
  2 + 307, // <= 15746
  2 + 197, // <= 15976
  2 + 229, // <= 15908
  2 + 119, // <= 16139
  2 + 148, // <= 16079
  2 + 132 // <= 16112
};

#if USE_REAL_RAND
#define a_rand rand

#else

static inline uint8_t a_rand() {
#if 1
  static uint8_t r = RANDOM_SEED;
  r += 31;
  return r;
#else
  //https://gist.github.com/3921043
  static int a = 6101;
  static int b = 997;
  static int c = 5583;

  uint8_t t = a + b + c;
  a = b;
  b = c;
  c = t;
  return t;
#endif
}
#endif

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

inline void init_buzzer(void) {
  //set up timer 1
  TCCR1A = 0;
  TCCR1B = 0; 
  TCCR1C = 0; 
  TCNT1L = 0;
  OCR1AL = 0xF;

  //CTC mode, resets clock when it meets OCR1A
  //TCCR1B = _BV(WGM12) | _BV(CS10) | _BV(CS11);
  TCCR1B = _BV(WGM12) | _BV(CS11);

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

inline void disable_buzzer(void) {
  TCCR1A = 0;
}

inline void init_timer0(void) {
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

inline void init_io(void) {
  //LED OUT
  DDRA = _BV(LED_R_PIN) | _BV(LED_G_PIN) | _BV(LED_B_PIN)
    | _BV(IGNITE_PIN) //igniter
    | _BV(BUZZER_PIN0) | _BV(BUZZER_PIN1); //set up output PWM

  PORTA = _BV(BUTTON_PIN); //button is input [0], set pullup
}

static void leds_set(uint8_t timeout) {
  uint8_t offset = 3 * ((a_rand() + 1) & NUM_COLORSETS_MOD);
  led_r_off = color_sets[offset];
  led_g_off = color_sets[offset + 1];
  led_b_off = color_sets[offset + 2];

  led_timeout = timeout == 0 ? 1 : timeout;
}

static void leds_off(void) {
  led_r_off = 0;
  led_g_off = 0;
  led_b_off = 0;
  led_timeout = 0;
}

static void leds_update(uint8_t time) {
  if (led_timeout && time == led_timeout)
    leds_off();
}

static void ignite(int doit) {
  if (doit)
    PORTA |= _BV(IGNITE_PIN);
  else
    PORTA &= ~_BV(IGNITE_PIN);
}

void update_stage(void) {
  stage++;
  stage_subdiv = 0;
  stage_bpress = 0;
  stage_new = TRUE;
}

inline static void exec_stage(uint8_t program_time) {
#ifdef TIME_STAGE
  if (stage_new) {
    enable_buzzer(drones[stage & NUM_DRONES_MOD]);
    leds_set(program_timeout);
  }
#else
  switch(stage) {
    case DONE:
      leds_off();
      disable_buzzer();
      break;
    case DRONE_CLICK:
    case DRONE_CLICK2:
      if (bpress_new || stage_new || program_time == program_timeout) {
        uint8_t r = a_rand() & 0x3;
        //big timeout range
        //random color selections
        if ((a_rand() & 0xF) > 4)
          program_timeout = program_time + 1;
        else
          program_timeout = program_time + (a_rand() & 0x7F);

        switch (r) {
          case 0:
            stage_state = 0;
            enable_buzzer(drones[a_rand() & NUM_DRONES_MOD]);
            leds_set(program_timeout);
            break;
          case 1:
            stage_state = 1;
            disable_buzzer();
            leds_off();
            break;
          case 2:
          default:
            stage_state = 2;
            enable_buzzer(((a_rand() & 0x1F) + 1) << 11);
            leds_set(1);
            break;
        }
      } else if (stage_state == 2 && led_timeout == 0) {
        //flash on clicks
        if ((a_rand() & 0xF) > 9) {
          leds_set(program_time + 1);
        }
      }
      break;
    case DRONE_ALL3:
      if (stage_new) {
        leds_set(program_timeout);
        enable_buzzer(drones[stage_state & NUM_DRONES_MOD]);
        ignite(TRUE);
        _delay_ms(500);
        ignite(FALSE);
        leds_off();
      }
    case DRONE_ALL:
    case DRONE_ALL2:
      if (stage_new || program_time == program_timeout) {
        program_timeout = program_time + 60 + (a_rand() & 0x3F);
        if (stage_new || stage_state > 4) {
          enable_buzzer(drones[a_rand() & NUM_DRONES_MOD]);
        }
        leds_set(program_time + 6);
        led_timeout = 0;
        stage_state = 0;
      } else if (bpress_new) {
        leds_set(program_time + 4);
        led_timeout = 0;
        stage_state += 1;
      }
      break;
    case DRONE_ONE:
      if (stage_new || program_time == program_timeout) {
        enable_buzzer(drones[0]);
        leds_set(program_timeout);
        led_timeout = 0;
      } else if (bpress_new) {
        program_timeout = program_time + 1 + (a_rand() & 0x2);
        disable_buzzer();
        leds_set(program_timeout);
        led_timeout = 0;
      }
      break;
    case CLICK_BURSTS:
      //use fall through to get clicks
      if (bpress_new || stage_new || program_time == program_timeout) {
        if (stage_state == 0 && (a_rand() & 0xFF) < 140) {
          program_timeout = program_time + 1 + (a_rand() & 0x7);
          enable_buzzer(drones[0]);
          leds_set(program_timeout);
          stage_state = 1;
          break;
        }
      }
      //allow fall through!
    case CLICK_FINISH:
    case CLICK:
      if (stage_new)
        stage_state = 0;

      if (bpress_new || stage_new || program_time == program_timeout) {
        if (stage_state == 0) {
          enable_buzzer(((a_rand() & 0xF) + 1) << 12);
          program_timeout = program_time + 2 + (a_rand() & 0x3F);
          leds_set(program_time + 1);
          if ((a_rand() & 0xFF) >= 75) //66% of the time turn off
            stage_state = 1;
        } else {
          disable_buzzer();
          program_timeout = program_time + 8 + (a_rand() & 0xF);
          stage_state = 0;
        }
      }
      break;
    case INTRO:
      if (stage_new)
        stage_state = 0;

      if (bpress_new) {
        if (stage_state == 0) {
          leds_set(program_time + 100);
        } else if (stage_state == 1) {
          leds_off();
        }
        stage_state++;
        if (stage_state >= 6)
          update_stage();
      }

      break;
  }
#endif

  leds_update(program_time);

  bpress_new = 
    stage_new = FALSE;
}

int main(void) {
  static uint8_t button_down_index = 0;
  static uint8_t button_down_history = 0;
  static uint8_t button_down_last = FALSE;

  static uint8_t program_time = 0;

#if USE_REAL_RAND
  srand(RANDOM_SEED);
#endif

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

      bpress_new = TRUE;

      //update 
      stage_bpress++;
      if (stage_bpress >= STAGE_BPRESS_THRESH)
        update_stage();
    }

    //deal with timed events
    if (rollover) {
      program_time++;
      cli();
      rollover = FALSE;
      rollover_count = 0;
      sei();
      if (program_time % PROGRAM_SUBDIV == 0) {
        stage_subdiv++;
        if (stage_subdiv >= STAGE_COUNTS)
          update_stage();
      }
      exec_stage(program_time);
    }
  }
}

