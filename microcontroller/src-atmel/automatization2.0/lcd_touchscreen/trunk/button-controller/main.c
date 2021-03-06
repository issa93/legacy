
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "uart/uart.h"


#define LED_COMMON_PORT PORTB
#define LED_COMMON_BIT PB6


static uint8_t leds;

ISR(TIMER0_COMPA_vect){
	static uint8_t toggle;
	toggle ^= 1;
	if(toggle){
		LED_COMMON_PORT |= _BV(LED_COMMON_BIT);
	}else{
		LED_COMMON_PORT &= ~_BV(LED_COMMON_BIT);
	}
}



void timer_0_init(){
	TCCR0A = _BV(WGM01); //CTC
	TCCR0B = 2;//clk/8
	OCR0A = 250; //4000Hz interrupt
	TIMSK |= _BV(OCIE0A);
}



void set_leds (uint8_t msk) {
	//PB4:0,PD6
	msk &= 0x3F;
	PORTB = (PORTB & 0xE0) | (msk >> 1);
	PORTD = (PORTD & 0xBF) | ((msk & 0x01)<<6);

}

uint8_t read_buttons() {
	//PD5:2, PDA0:1
	return ~( (PIND & 0x3C) | ((PINA & 0x02)>>1) | ((PINA & 0x01)<<1) );
}


void handle_buttons () {
	static uint8_t old_buttons;
	uint8_t buttons = read_buttons();
	uint8_t buttons_xor = buttons ^ old_buttons;//changes in button states
	uint8_t i;
	uint8_t msk = 0x01;
	for (i = 0; i < 6; i++){
		if (buttons_xor & msk) { //did this button change?
			//it changed. was it pushed or released?
			if (buttons & msk) {
				//pushed
				uart_putc('A' + i);
			} else {
				//released
				uart_putc('a' + i);
			}
		}
		msk <<= 1;
	}
	
	old_buttons = buttons;
}

void handle_leds () {
	char c;
	if ( uart_getc_nb(&c) ) {
		uint8_t num;
		if (c < 'a') {
			num = c - 'A';
			leds |= 0x20 >> num;
		} else {
			num = c - 'a';
			leds &= ~(0x20 >> num);
		}
		set_leds(leds);
	}
}

int main(){
	
	DDRD = 0x40;
	DDRB = 0x5F;
	
	PORTA = 0x03;
	PORTD = 0x3C;
	
	uart_init();

	timer_0_init();
	
	sei();
	
	while (1) {
		handle_buttons();
		handle_leds();
	}
}
