#include <avr/io.h>
#include "demo_motor.h"
#include "config.h"
#include "lpd8806.h"
#include "mate.h"

uint8_t demo_motor_tick(void) {
	setPwmOut(0, parameter[0]);
	setPwmOut(1, parameter[1]);
	return 1;
}
