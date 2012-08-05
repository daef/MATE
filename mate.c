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

typedef uint8_t(*demoDelegate)(void);

typedef struct {
    demoDelegate init;
    demoDelegate tick;
    uint8_t parameter[4];
    uint8_t active;
} demo_t;

demo_t demos[] = {
    {0,               demo_rgb_tick,        	 {0,1,0,0}, 1},
    {demo_blink_left_init, demo_blink_left_tick, {10,0,0,0}, 0},
};

volatile uint16_t rcInput, rcPot;
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
    } else {
        if(lastChannel == 0) {
            rcInput = TCNT1 - rcLast;
        } else {
            rcPot = TCNT1 - rcLast;
            rcSet = 1;
        }
    }
}

int main(void) {
    DDRB |= (1<<DATA) | (1<<CLOCK); //DDR DATA CLOCK (OUT)    
    DDRC &= ~(1<<PC0 | 1<<PC1);     //DDR PWM          (IN)
    TCCR1B = 2;    //set timer to CLK/8
    PCICR = 2;  //PCINT 8...14 on
    PCMSK1 = 3; //PCINT 8 & 9 enable
    
    TCCR2A = (1<<WGM21);
    TCCR2B = (1<<CS22) | (1<<CS21) | (1<<CS20); //prescale 1k
    OCR2A = 156;
    TIMSK2 |= (1<<OCIE2A);
    
    rcBar = parameter[choosenParameter];
    sei();
    
    while(1) {
        static uint8_t oldRcFoo;
        static uint8_t sameCount;
        if(rcSet) {
            rcSet = 0;
            
            cli();
            rcFoo = rcInput >> 5;
            rcBarPoti = rcPot;
            sei();
            
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
                                rcBarPotiMin = 0xffff;
                                rcBarPotiMax = 0;
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
                        if(rcFoo < 9 && rcFoo > 0) {
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
        } else {
            for(uint8_t i=0;i<(sizeof(demos) / sizeof(demo_t));i++) {
                if(demos[i].active) {
                    parameter = demos[i].parameter;
                    demos[i].active = demos[i].tick();
                }
            }
        }
        updateState();
    }
}
