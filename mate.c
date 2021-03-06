#include <avr/io.h>
#include <stdlib.h>
#include <math.h>
#include <util/delay.h>
#include <stdint.h>
#include <inttypes.h>
#include <avr/interrupt.h>

#include "mate.h"
#include "config.h"
#include "lpd8806.h"

#include "demo_rgb.h"
#include "demo_blink_left.h"
#include "demo_blink_right.h"
#include "demo_motor.h"

typedef uint8_t(*demoDelegate)(void);

typedef struct {
    demoDelegate init;
    demoDelegate tick;
    uint8_t parameter[4];
    uint8_t active;
} demo_t;

demo_t demos[] = {
    {0,                     demo_rgb_tick,         {0,0,0,0,},  0},
    {demo_blink_left_init,  demo_blink_left_tick,  {10,0,0,0,}, 0},
    {demo_blink_right_init, demo_blink_right_tick, {10,0,0,0,}, 0},
    {0,                     demo_motor_tick,       {0,0,0,0,},  1},
};

volatile uint16_t rcInput, rcPot, rcMot0, rcMot1;
volatile uint8_t  rcSet;

uint8_t boehseTasetn[] = {
    0x44, 0x49, 0x4f, 0x55,
    0x5b, 0x60, 0x66, 0x6d,
    0x74, 0x7c, 0xff, };

uint8_t *parameter;

uint8_t demo,
        choosenParameter,
        updatingParameter,
        lastRcBarPoti,
        dbg,
        remoteMode,
        lastChannel;
    
uint16_t rcFoo = 0xffff,
         rcBar,
         rcBarPoti,
         rcBaz,
         rcBuz,
         rcBarPotiMin = 0xffff,
         rcBarPotiMax,
         rcLast = 0;

         
//   9   0    MENUMODE CONFIGMODE    choose DEMO
//   
//   8   1    p0   p2           choose parameter
//   
//   7   2    p1   p3             choose parameter
//   
//   6   3    p++  p--          modify parameter w/o poti         
//   
//   5   4    dbg  resetScale     debug last set value (binary output)


volatile uint32_t sun;
ISR(TIMER2_COMPA_vect) {
    ++sun;
}

ISR(PCINT1_vect) {
    if(PINC & (1<<PC0)) {
        rcLast = TCNT1;
        lastChannel = 0;
    } else if(PINC & (1<<PC1)) {
        rcLast = TCNT1;
        lastChannel = 1;
    } else if(PINC & (1<<PC2)) {
        rcLast = TCNT1;
        lastChannel = 2;
    } else if(PINC & (1<<PC3)) {
        rcLast = TCNT1;
        lastChannel = 3;
    } else {
        uint16_t time = TCNT1;
        if(time<rcLast) {
            //fix it
            time = (PWM_PERIOD - rcLast) + time; 
        } else {
            time = time - rcLast;
        }
        
        if(lastChannel == 0) {
            rcInput = time;
        } else if(lastChannel == 1) {
            rcPot = time;
        } else if(lastChannel == 2) {
            rcMot0 = time;
        } else if(lastChannel == 3) {
            rcMot1 = time;
            rcSet = 1;
        }
    }
}

#define MOTOR0 OCR1A
#define MOTOR1 OCR1B
// MAX REV 0x32
// STOP    0xBA
//         
void setPwmOut(uint8_t chan, uint8_t value) {
    cli();
    if (chan == 0) {
        MOTOR0 = 1964L + ((uint16_t)value<<3);
    } else {
        MOTOR1 = 1964L + ((uint16_t)value<<3);
    }
    sei();
}

void rc_io_init(void) {
    ////// OUT //////
    OCR1A = 2000;
    OCR1B = 2000;
    ICR1 = PWM_PERIOD;
    TCCR1B |= (1<<CS11) | (1<<WGM13) | (1<<WGM12);    //prescaler 1024 and some of the pwmmodebits (mode 14 fast pwm)
    TCCR1A |= (1<<WGM11) | (1<<COM1A1) | (1<<COM1B1); // pwmmodebit    , waveformbits
    //TCCR1B = 2;    //set timer to CLK/8
    
    DDRB |= (1<<PB2) | (1<<PB1);// configure PB0-PB2 as outputs
    
    ////// IN //////
    DDRC &= ~(1<<PC0 | 1<<PC1 | 1<<PC2 | 1<<PC3);     //DDR PWM          (IN)
    PCICR = 2;  //PCINT 8...14 on
    PCMSK1 = 0x0f; //PCINT 8...11 enable
}

void led_init(void) {
    DDRB |= (1<<DATA) | (1<<CLOCK); //DDR DATA CLOCK (OUT)    
}

void sun_init(void) {
    TCCR2A = (1<<WGM21); //CTC (at least we guess so...)
    TCCR2B = (1<<CS22) | (1<<CS21) | (1<<CS20); //prescale 1k
    OCR2A = 156;
    TIMSK2 |= (1<<OCIE2A);
}

int main(void) {
    rc_io_init();   //rc I&O pwm
    led_init();     //set ddrb for lpd8806...
    sun_init();     //init 2nd timer (demo-tixx)

    sei();
    
    while(1) {
        static uint8_t oldRcFoo;
        static uint8_t sameCount;
        if(rcSet) {
            rcSet = 0;
            
            cli();  rcFoo = rcInput >> 5;    sei();
            cli();  rcBarPoti = rcPot;       sei();
            cli();  rcBaz = rcMot0;          sei();
            cli();  rcBuz = rcMot1;          sei();
            
            //remember, remember...
            if(rcBarPotiMin>rcBarPoti)
                rcBarPotiMin = rcBarPoti;
            if(rcBarPotiMax<rcBarPoti)
                rcBarPotiMax = rcBarPoti;
                
            //autoscale rcBarPoti 0...255 (calibrate vs historical max/min)
            rcBarPoti = 0xffL*(rcBarPoti-rcBarPotiMin)/(rcBarPotiMax-rcBarPotiMin);            
            
            //dirty kewl hack - do not attempt to understand
            //this huge dogpile of shit - you won't get it anyway...
            for(uint8_t i=0;i<(sizeof(boehseTasetn)+1);i++)
                if(rcFoo <= boehseTasetn[i]) {
                    rcFoo = i;
                    break;
                }
            --rcFoo;
            //</dirtyhack>
            //rcFoo holds the id of the pressed key (0...9)
            //or $FFFF if no key is pressed

            if(rcFoo != oldRcFoo) {
                oldRcFoo = rcFoo;
                rcFoo = 0xFFFF;
                sameCount = 0;
            } else {
                sameCount++;
                if(sameCount < 5) {
                    rcFoo = 0xFFFF;
                } else if (sameCount == 5 || ((sameCount%10) == 0) || sameCount > 200) {
                    if(sameCount > 250)
                        sameCount = 200;
                    switch(rcFoo) {
                            case 9:
                                remoteMode = 0;
                                break;
                            case 0:
                                remoteMode = 1;
                                break;
                    }
                    if(remoteMode) {
                        switch(rcFoo) {
                            case 1:
                                choosenParameter = 2;
                                updatingParameter = 0;
                                break;
                            case 2:
                                choosenParameter = 3;
                                updatingParameter = 0;
                                break;
                            case 3:
                                if(rcBar == 0)
                                    rcBar = 255;
                                else
                                    --rcBar;
                                updatingParameter = 2;
                                break;
                            case 4:
                                aMin = uMin = rcBarPotiMin = 0xffff;
                                aMax = uMax = rcBarPotiMax = 0; //see demo_motor.c
                                break;
                            case 5:
                                dbg ^= 1;
                                break;
                            case 6:
                                if(++rcBar > 255)
                                    rcBar = 0;
                                updatingParameter = 2;
                                break;
                            case 7:
                                choosenParameter = 1;
                                updatingParameter = 0;
                                break;
                            case 8:
                                choosenParameter = 0;
                                updatingParameter = 0;
                                break;
                        }
                    } else {
                        if(rcFoo >= (sizeof(demos) / sizeof(demo_t)) && rcFoo > 0) {
                            updatingParameter = 0;
                            demo = 8-rcFoo;
                            demos[demo].active ^= 1;
                            if(demos[demo].active)
                                if(demos[demo].init)
                                    demos[demo].init();
                        }
                    }
                } else {
                    rcFoo = 0xFFFF;
                }
            }
            
            if (updatingParameter) {
                demos[demo].parameter[choosenParameter] =
                    updatingParameter == 2
                        ? rcBar
                        : (lastRcBarPoti = rcBarPoti);
            } else if((updatingParameter == 0)
                && (abs((int16_t)rcBarPoti - lastRcBarPoti) > UPDATE_DELTA)) {
                updatingParameter = 1;
            }
        }

        for(uint8_t i=0;i<LEDS*3;i++)
            state[i] = 0;
    
        for(uint8_t i=0;i<(sizeof(demos) / sizeof(demo_t));i++) {
            if(demos[i].active) {
                parameter = demos[i].parameter;
                demos[i].active = demos[i].tick();
            }
        }
        if(dbg) {
            uint8_t mask = 128;
            uint8_t param = parameter[choosenParameter];
            
            for(uint8_t i=0;i<8;i++) {
                //GGGG GGGG  RRRR RRRR  BBBB BBBB
                state[i*3+0] = (param & mask) ? 255 : 0;
                mask >>= 1;
                state[i*3+1] = 0;
                state[i*3+2] = 0;
            }

            mask = 128;
            for(uint8_t i=0;i<8;i++) {
                //GGGG GGGG  RRRR RRRR  BBBB BBBB
                state[(8+i)*3+1] = 0;
                state[(8+i)*3+2] = 0;
                state[(8+i)*3+0] = (rcBaz & mask) ? 255 : 0;
                mask >>= 1;
            }
            for(uint8_t i=0;i<sizeof(demos)/sizeof(demo_t);i++) {
                state[(16+i)*3+0] = 0;
                state[(16+i)*3+1] = demos[i].active ? 255 : 0;
                state[(16+i)*3+2] = 0;
            }
        }
        updateState();
    }
}
