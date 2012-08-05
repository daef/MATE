#include <avr/io.h>
#include "demo_rgb.h"
#include "config.h"
#include "lpd8806.h"
#include "mate.h"

uint8_t demo_rgb_tick() {
    for(int i=0;i<LEDS;i++) {
        state[i*3+0]=parameter[0]>>1;
        state[i*3+1]=parameter[1]>>1;
        state[i*3+2]=parameter[2]>>1;
    }
	return 1;
}
