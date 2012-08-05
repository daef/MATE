#include <avr/io.h>
#include "demo_blink_left.h"
#include "config.h"
#include "lpd8806.h"
#include "mate.h"

static uint8_t blinkState;
static uint32_t sunStart;

uint8_t demo_blink_left_init(void) {
	sunStart = sun;
}


uint8_t demo_blink_left_tick(void) {
	uint32_t deltaSun = sun-sunStart;
	if(deltaSun >= parameter[0]) {
		sunStart = sun;
		blinkState ^= 1;
	}
	
	state[0] = blinkState ? 255 : 0;
	
	return 1;
}