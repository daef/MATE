#include <avr/io.h>
#include "demo_blink_right.h"
#include "config.h"
#include "lpd8806.h"
#include "mate.h"

static uint8_t blinkState;
static uint32_t sunStart;

uint8_t demo_blink_right_init(void) {
	sunStart = sun;
	return 0;
}

//
//    ReHi                   LiHi
//        ___ ____ _ ____ ___
//        012 3456 7 8901 234
//        000 0000 0 0011 111
//        
//
//        
//        
//    ReVo _ _____ _ _____ _ LiVo
//         7 65432 1 09876 5
//         2 22222 2 21111 1
//      
        
uint8_t demo_blink_right_tick(void) {
	uint32_t deltaSun = sun-sunStart;
	if(deltaSun >= parameter[3]) {
		sunStart = sun;
		blinkState ^= 1;
	}

    for(uint8_t i = 11; i<17; i++) {
        if(i != 14) {
            state[0] = blinkState ? parameter[0] : 0;
            state[1] = blinkState ? parameter[1] : 0;
            state[2] = blinkState ? parameter[2] : 0;
        }
    }
	
	return 1;
}