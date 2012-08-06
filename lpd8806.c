#include <avr/io.h>
#include <util/delay.h>
#include "config.h"
#include "lpd8806.h"

uint8_t state[LEDS * 3];

static void tick() {
    //_delay_us(1);
    PORTB |= (1<<CLOCK);
    //_delay_us(1);
    PORTB &= ~(1<<CLOCK);
    //_delay_us(1);
}

static void sendZero() {
    uint8_t i;
    PORTB &= ~(1<<DATA);
    for(i=0;i<8;i++)
        tick();
}

static void sendByte(uint8_t data) {
    uint8_t i;
    data |= 128;
    for(i=0;i<8;i++) {
        if(data&(1<<(7-i)))
            PORTB |= (1<<DATA);
        else
            PORTB &= ~(1<<DATA);
        tick();
    }
}

void updateState() {
    uint8_t m;
    sendZero();
    for(m=0;m<LEDS*3;m++)
        sendByte( m>=(2*3) && m<(4*3) ? 255- state[m] : state[m]);
}
