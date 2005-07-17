/**
 * UART Echo Test
 */

#include <inttypes.h>
#include <avr/io.h>

#include "config.h"
#include "uart.h"

int main (void)
{	
	DDRC = 0xFF; 	// Port C all outputs
	PORTC = 0x04;   // Start LED 3 -- LEDs1+2 machen interrupt debugging
	
	uart_init();
	
	for ( ;; ){		//for ever
		uart_putc(uart_getc());

		if( PORTC == 0x80 ) { 
			PORTC = 0x04;
		} else {
			PORTC <<= 1;
		}
	};	
}
