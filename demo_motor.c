#include <avr/io.h>
#include <util/delay.h>
#include "demo_motor.h"
#include "config.h"
#include "lpd8806.h"
#include "mate.h"

uint16_t aMin = 0xffff, uMin = 0xffff, uMax, aMax;

uint8_t demo_motor_tick(void) {
    if(aMin == 0xffff)
        _delay_ms(1000);
        
    if(aMin > rcBaz)
        aMin = rcBaz;
    if(aMax < rcBaz)
        aMax = rcBaz;
    if(uMin > rcBuz)
        uMin = rcBuz;
    if(uMax < rcBuz)
        uMax = rcBuz;
    
    if(((aMax-aMin) < MOTOR_DELTA) || (uMax - uMin) < MOTOR_DELTA) {
        setPwmOut(0, 127);
        setPwmOut(1, 127);        
    }
	setPwmOut(0, 255L * (rcBaz - aMin) / (aMax-aMin));
	setPwmOut(1, 255L * (rcBuz - uMin) / (uMax-uMin));
	return 1;
}