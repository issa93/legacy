/* menu.c */
/*
 *   This file is part of AnonAccess, an access system which can be used
 *    to open door or doing other things with an anonymity featured
 *    account managment.
 *   Copyright (C) 2006, 2007, 2008  Daniel Otte (daniel.otte@rub.de)
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "config.h"
#include "lcd_tools.h"
#include "uart.h"
#include "menu.h"
#include "sha256.h"
#include "keypad.h"
#include "ui_primitives.h"
#include "rtc.h"
#include "entropium.h"
#include "lop.h"
#include "lop_debug.h"
#include "base64_enc.h"
#include "reset_counter.h"
#include "types.h"
#include "comm.h"
#include "24C04.h"
#include "cardio.h"
#include "keypad_charset.h"
#include "ui_tests.h"
#include "logs.h"
#include "factorizefun.h"
#include <stdint.h>
#include <util/delay.h>

extern lop_ctx_t lop1;

/******************************************************************************/

/******************************************************************************/
#define TIMEOUT_DELAY 15000
#define DISPLAY_TIME  3000

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define AT __FILE__ ":" TOSTRING(__LINE__)

#define ERR_TIMEOUT_STR(a) "(" a ") com. timeout!"
#define ERR_MALLOC_STR(a)  "(" a ") malloc failed!" 

#include "uart.h"

#ifndef MIN
 #define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
 #define MAX(a,b) (((a)>(b))?(a):(b))
#endif


#define TIMEOUT_VAL 1000

#define DBG(a) lcd_gotopos(1,15); lcd_writechar(a)

#define BUFFERSIZE 100

void run_serial_test(void){
	char tmp,tmp2, bufferout[BUFFERSIZE], bufferin[BUFFERSIZE];
	timestamp_t tsend;
	uint8_t idxin,idxout;
	uint64_t ok=0,failed=0,lost=0;
	void (*backup)(uint8_t);
	
	backup = uart_hook;
	uart_hook=NULL;
	ui_printstatusline();
	
	while(read_keypad()!='C'){
#ifdef UART_XON_XOFF
		for(idxout=0; idxout<BUFFERSIZE; ++idxout){
			do{
				tmp=entropium_getRandomByte();
			}while(tmp==0x11 || tmp==0x13);
			bufferout[idxout]=tmp;
		}
#else
		entropium_fillBlockRandom(bufferout, BUFFERSIZE);
#endif			
		/* blast it out */
		for(idxout=0; idxout<BUFFERSIZE; ++idxout){
			uart_putc(bufferout[idxout]);
		}
		tsend = gettimestamp();
		idxin=0;
		while((idxin<BUFFERSIZE) && (!uart_getc_nb(&tmp2)) && ((gettimestamp()-tsend)<TIMEOUT_VAL)){
			bufferin[idxin++]=tmp2;
		}
		if(gettimestamp()-tsend<TIMEOUT_VAL){
			if(!memcmp(bufferin, bufferout, BUFFERSIZE)){
				ok++;
			} else {
				failed++;
			}
		} else {
			lost++;
		}
		lcd_gotopos(2,2);
		lcd_hexdump(&ok, 8);
		lcd_gotopos(3,2);
		lcd_hexdump(&failed, 8);
		lcd_gotopos(4,2);
		lcd_hexdump(&lost, 8);
	}
	ui_waitforkey('E');
	uart_hook = backup;
}

void error_display(PGM_P str){
//	lcd_cls();
	uint8_t i;
	ui_drawframe(1,2,LCD_WIDTH, LCD_HEIGHT-1, '#');
	lcd_gotopos(3,2);
	for(i=0; i<LCD_WIDTH-2; ++i)
		lcd_writechar(' ');
	lcd_gotopos(3,3);	
	lcd_writestr_P(str);
	ui_waitforkey('F');
	return;
}

void errorn_display(PGM_P str, uint8_t n){
//	lcd_cls();
	uint8_t i;
	ui_drawframe(1,2,LCD_WIDTH, LCD_HEIGHT-1, '#');
	lcd_gotopos(3,2);
	for(i=0; i<LCD_WIDTH-2; ++i)
		lcd_writechar(' ');
	lcd_gotopos(3,3);	
	lcd_writestr_P(str);
	lcd_gotopos(3,LCD_WIDTH-3);
	lcd_hexdump(&n, 1);
	ui_waitforkey('F');
	return;
}

#define PIN_MAX_LEN  16
#define NAME_MAX_LEN 12

void getandsubmitpin(void){
	char pin_str[PIN_MAX_LEN+1];	
	uint8_t l;
	
	lcd_cls();
	ui_drawframe(1,1,LCD_WIDTH, LCD_HEIGHT, '*');
	lcd_gotopos(2,3);
	lcd_writestr_P(PSTR("please enter PIN:"));
	lcd_gotopos(2,3);
	l=read_pinn(3,3, '*', pin_str, PIN_MAX_LEN);
	submit_pin(pin_str, l);
}

void req_authblock(void){
	char* str_name;
	char* str_pina;
	char* str_pinb;
	uint8_t pl, plc;
	uint8_t anon;
	uint8_t pinflags=0;
	
	str_name=malloc(NAME_MAX_LEN);
	str_pina=malloc(PIN_MAX_LEN);
	str_pinb=malloc(PIN_MAX_LEN);
	if(str_name==NULL || str_pina==NULL || str_pinb==NULL){
		lcd_gotopos(2,1);
		lcd_writestr_P(PSTR(ERR_MALLOC_STR(AT)));
		uint16_t i;
		for(i=0; i<10000; ++i){
			_delay_ms(1);
		}
		return;
	}
	
	ui_printstatusline();
	session_init();
	lcd_gotopos(2,1);
	lcd_writestr_P(PSTR("name:"));
	read_strn(1,3,alphanum_cs, str_name,NAME_MAX_LEN);
	do{
		lcd_cls();
		ui_drawframe(1,1,LCD_WIDTH, LCD_HEIGHT, '*');
		lcd_gotopos(2,2);
		lcd_writestr_P(PSTR("please enter PIN:"));
		lcd_gotopos(2,3);
		pl=read_pinn(3,3, '*', str_pina, PIN_MAX_LEN);
		/***/
		lcd_cls();
		ui_drawframe(1,1,LCD_WIDTH, LCD_HEIGHT, '*');
		lcd_gotopos(2,2);
		lcd_writestr_P(PSTR("please confirm PIN:"));
		lcd_gotopos(2,3);
		plc=read_pinn(3,3, '*', str_pinb, PIN_MAX_LEN);
	}while((pl!=plc) || (memcmp(str_pina, str_pinb, MIN(pl, plc))!=0));
	free(str_pinb);
	lcd_cls();
	anon=1^ui_radioselect_P(2,2,LCD_WIDTH-1,2,PSTR("anon\0not anon\0"));
	
	lcd_cls();
	lcd_gotopos(1,1);
	lcd_writestr_P(PSTR("select pin requirement:"));
	ui_checkselect_P(2,2,LCD_WIDTH-1, LCD_HEIGHT-1, 
		PSTR("admin tasks\0normal tasks\0"),&pinflags);
	lcd_gotopos(1,1);
	lcd_writestr_P(PSTR("waiting for response   "));
	ui_statusstring[4]='~';	
	uint8_t et;
	req_bootab(str_name, str_pina, pl, anon, pinflags);
	if((et=waitformessage(TIMEOUT_DELAY))){
		if(et==2){
			error_display(PSTR(ERR_MALLOC_STR(AT)));
			return;
		}
		if(et==1){
			error_display(PSTR(ERR_TIMEOUT_STR(AT)));
			return;
		}
		error_display(PSTR(" X ERROR! "));
		return;
	}
	free(str_name);
	free(str_pina);
	if( (msg_length!=5+sizeof(authblock_t))         ||
		(getmsgid(msg_data)!=MSGID_ACTION_REPLY)    ||
		(((uint8_t*)msg_data)[3]!= ACTION_ADDUSER ) ||
		(((uint8_t*)msg_data)[4]!= DONE ) ) { 
		lcd_cls();
		lcd_gotopos(1,1);
		lcd_writestr_P(PSTR("rx wrong packet:"));
		//error_display(PSTR("rx wrong packet!"));
		//ui_hexdump(1,2,LCD_WIDTH,LCD_HEIGHT-1, msg_data, msg_length);
		lcd_gotopos(2,1);
		lcd_hexdump(msg_data, MIN(LCD_WIDTH/2, msg_length));
		lcd_gotopos(3,1);
		lcd_hexdump(&msg_length, 2);
		uint8_t tmp_msgid = getmsgid(msg_data);
		lcd_gotopos(4,1);
		lcd_writestr_P(PSTR("msg_id="));
		lcd_hexdump(&tmp_msgid, 1);
		
		ui_waitforkey('F');
		lcd_gotopos(1,1);
		lcd_writestr_P(PSTR("-_-_-_-"));
		freemsg();
		return;
	}
	card_writeAB((authblock_t*)((uint8_t*)msg_data+5));
	lcd_cls();
	lcd_gotopos(1,1);
	lcd_writestr_P(PSTR("new AB:"));
	ui_hexdump(1,2,LCD_WIDTH,LCD_HEIGHT-1, msg_data, msg_length);
	freemsg();
	return;
}


void view_authblock(void){
	authblock_t a;
	
	if(card_readAB(&a)==false){
		lcd_cls();
		ui_drawframe(1,1,LCD_WIDTH,LCD_HEIGHT,'#');
		lcd_gotopos(2,3);
		lcd_writestr_P(PSTR("could not find"));
		lcd_gotopos(3,3);
		lcd_writestr_P(PSTR("authblock"));
		ui_waitforkey('E');
	}
	lcd_cls();
	lcd_gotopos(2,1);
	lcd_writestr_P(PSTR("uid: "));
	lcd_hexdump(&(a.uid), 2);
	ui_waitforkey('E');
	
	lcd_cls();
	lcd_gotopos(1,1);
	lcd_writestr_P(PSTR("ticket: "));
	lcd_writeB64long(1,2,LCD_WIDTH, a.ticket, 32);
	ui_waitforkey('E');
	
	lcd_cls();
	lcd_gotopos(1,1);
	lcd_writestr_P(PSTR("rid: "));
	lcd_writeB64long(1,2,LCD_WIDTH, a.rid, 32);
	ui_waitforkey('E');
	
	lcd_cls();
	lcd_gotopos(1,1);
	lcd_writestr_P(PSTR("HMAC: "));
	lcd_writeB64long(1,2,LCD_WIDTH, a.hmac, 32);
	ui_waitforkey('E');
}

/******************************************************************************/
/******************************************************************************/

void read_logs(void){
	ui_loglist_t* tab[]={&bootlog, &syslog, &seclog, &masterlog};
	ui_logreader(1,1,LCD_WIDTH,LCD_HEIGHT, 
	    tab[ui_radioselect_P(1,2,LCD_WIDTH, 3, PSTR("bootlog\0syslog\0seclog\0masterlog\0"))]);
}

/******************************************************************************/
/******************************************************************************/
uint8_t login_with_card(uint8_t admin){
	authblock_t ab;
	lcd_cls();
	
	if(!card_inserated()){
		ui_drawframe(1,1,LCD_WIDTH, LCD_HEIGHT, '*');
		lcd_gotopos(2,2);
		lcd_writestr_P(PSTR("please insert card"));
		while(!card_inserated())
			;
	}
	lcd_cls();
	lcd_writestr_P(PSTR("reading card ..."));
	if(card_readAB(&ab)==false){
		error_display(PSTR("card read error!"));
		return 0;
	}
	lcd_cls();
	lcd_writestr_P(PSTR("card read, data submit ..."));
	submit_ab(&ab, admin);
	lcd_gotopos(1,2);
	lcd_writechar('D');
	if(waitformessage(TIMEOUT_DELAY)){
		error_display(PSTR(ERR_TIMEOUT_STR(AT)));
		return 0;
	}
	lcd_gotopos(1,2);
	lcd_writechar('E');
	if(getmsgid(msg_data)==MSGID_AB_PINREQ){
		freemsg();
		getandsubmitpin();
		if(waitformessage(TIMEOUT_DELAY)){
			error_display(PSTR(ERR_TIMEOUT_STR(AT)));
			return 0;
		}
	}
	if(getmsgid(msg_data)==MSGID_AB_ERROR){
		freemsg();
		errorn_display(PSTR("AB ERROR!"), ((uint8_t*)msg_data)[3]);
		return 0;
	}
	if((getmsgid(msg_data)!=MSGID_AB_REPLY) || (msg_length!=3+sizeof(authblock_t)+1)){
		freemsg();
		error_display(PSTR("AB strange ERROR!"));
		return 0;
	}
		if(card_writeAB((authblock_t*)((uint8_t*)msg_data+3))==false){
		freemsg();
		error_display(PSTR("card write ERROR!"));
		return 0;
	}
	if(((uint8_t*)msg_data)[3+sizeof(authblock_t)]!=0){
		lcd_cls();
		ui_drawframe(1,1,LCD_WIDTH, LCD_HEIGHT, '!');
		ui_textwindow_P(2,2,LCD_WIDTH-2, LCD_HEIGHT, 
		                PSTR("You lost your ADMIN privileges\n"
		                     " hit [F] to continue."));
	}
	freemsg();
	error_display(PSTR("AB fine!"));	
	lcd_cls();
	return 1;
}

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

void open_door(void){
	session_init();
	if(login_with_card(0)==0)
		return;
	send_mainopen();
	if(waitformessage(TIMEOUT_DELAY)){
		error_display(PSTR(ERR_TIMEOUT_STR(AT)));
		return;
	}
	if(getmsgid(msg_data)==MSGID_ACTION_REPLY && 
	   msg_length==5 && ((uint8_t*)msg_data)[3]==ACTION_MAINOPEN){
		lcd_gotopos(1,2); lcd_writechar('Z');
		lcd_cls();
		lcd_gotopos(2,2);
		lcd_writestr_P(PSTR("door "));
		if(((uint8_t*)msg_data)[4]==NOTDONE)
			lcd_writestr_P(PSTR("NOT "));
		lcd_writestr_P(PSTR("opening!"));
		ui_keyortimeout(DISPLAY_TIME);
		freemsg();
		return;
	}
	freemsg();
}

void lock_door(void){
	
	session_init();
	login_with_card(0);
	send_mainclose();
	if(waitformessage(TIMEOUT_DELAY)){
		error_display(PSTR(ERR_TIMEOUT_STR(AT)));
		return;
	}
	if(getmsgid(msg_data)==MSGID_ACTION_REPLY && 
	   msg_length==5 && ((uint8_t*)msg_data)[3]==ACTION_MAINCLOSE){
		lcd_gotopos(1,2); lcd_writechar('Z');
		lcd_cls();
		lcd_gotopos(2,2);
		lcd_writestr_P(PSTR("door "));
		if(((uint8_t*)msg_data)[4]==NOTDONE)
			lcd_writestr_P(PSTR("NOT "));
		lcd_writestr_P(PSTR("closing!"));
		ui_keyortimeout(DISPLAY_TIME);
		freemsg();
		return;
	}
	freemsg();
}

void admin_menu(void){}
void stat_menu(void){}

void print_resets(void){
	uint64_t t;
	t = resetcnt_read();
	ui_printstatusline();
	lcd_gotopos(2,1);
	lcd_hexdump(&t,8);
	ui_waitforkey('E');
}

void print_timestamp(void){
	timestamp_t t;
	t = gettimestamp();
	ui_printstatusline();
	lcd_gotopos(2,1);
	lcd_hexdump(&t,8);
	ui_waitforkey('E');
}

void print_timestamp_live(void){
	timestamp_t t;
	ui_printstatusline();	
	while(read_keypad()!='E'){
		t = gettimestamp();
		lcd_gotopos(2,1);
		lcd_hexdump(&t,8);
	}
	while(read_keypad()=='E')
		;
	_delay_ms(10);
}

void print_timestamp_base64(void){
	timestamp_t t;
	
	t = gettimestamp();
	ui_printstatusline();
	lcd_gotopos(2,1);
	lcd_writeB64(&t, 8);
	ui_waitforkey('E');
}

void print_timestamp_base64_live(void){
	timestamp_t t;
	ui_printstatusline();	
	while(read_keypad()!='E'){
		t = gettimestamp();
		lcd_gotopos(2,1);
		lcd_writeB64(&t, 8);
	}
	while(read_keypad()=='E')
		;
	_delay_ms(10);
	
}

void print_random(void){
	uint8_t block[30];
	entropium_fillBlockRandom(block, 30);
	ui_printstatusline();
	lcd_gotopos(2,1);
	lcd_hexdump(&block,10);
	lcd_gotopos(3,1);
	lcd_hexdump(&block+10,10);
	lcd_gotopos(4,1);
	lcd_hexdump(&block+20,10);
	ui_waitforkey('E');
}

void replace_unprinable(char * str, uint16_t len){
	while(len--){
		if((*str>='A' && *str<='Z') || 
		   (*str>='a' && *str<='z') ||
		   (*str>='0' && *str<='9') ||
		   (*str==' ')){
		   	;
		}else{
			*str='.';
		}
		str++;
	}
}

void dump_card(void){
/*	
	uint8_t buffer[17];
	uint16_t i;
//	E24C04_block_read(0xA0, 0, buffer, 16);
	buffer[16]='\0';
	lop_dbg_str_P(&lop1, PSTR("\r\nICC dump:\r\n"));
	E24C04_init();
	for(i=0; i<256; i+=16){
		E24C04_block_read(0xA0, i, buffer, 16);
		lop_dbg_hexdump(&lop1, buffer, 16);
		lop_dbg_str_P(&lop1, PSTR("   "));
		replace_unprinable((char*)buffer, 16);
		lop_dbg_str(&lop1, (char*)buffer);
		lop_dbg_str_P(&lop1, PSTR("\r\n"));
	}
*/
	uint8_t buffer[256];
	E24C04_init();
	E24C04_block_read(0xA0, 0, buffer, 256);
	ui_hexdump(1, 1, LCD_WIDTH, LCD_HEIGHT, buffer, 256);
}

void erase_card(void){
	uint16_t i;
	lcd_cls();
	ui_drawframe(1,1,LCD_WIDTH,LCD_HEIGHT,'*');
	lcd_gotopos(2,2);
	lcd_writestr_P(PSTR("erasing ..."));
	E24C04_init();
	for(i=0; i<256; ++i){
		E24C04_byte_write(0xA0, i, 0xFF);
		ui_progressbar(i/255.0, 2,3,LCD_WIDTH-2);
	}
}

void randomize_card(void){
	uint16_t i;
	lcd_cls();
	ui_drawframe(1,1,LCD_WIDTH,LCD_HEIGHT,'*');
	lcd_gotopos(2,2);
	lcd_writestr_P(PSTR("randomizing ..."));
	E24C04_init();
	for(i=0; i<256; ++i){
		E24C04_byte_write(0xA0, i, entropium_getRandomByte());
		ui_progressbar(i/255.0, 2,3,LCD_WIDTH-2);
	}
}

void system_stats(void){
	lcd_cls();
	lcd_gotopos(2,2);
	lcd_writestr_P(PSTR("statistics @ " AT));	
	session_init();
	if(login_with_card(0)==0)
		return;
	lcd_cls();
	lcd_gotopos(2,2);
	lcd_writestr_P(PSTR("sending stat req."));	
	send_getstat();
	lcd_cls();
	lcd_gotopos(2,2);
	lcd_writestr_P(PSTR("stat req. submitted"));
	if(waitformessage(TIMEOUT_DELAY*2)){
		error_display(PSTR(ERR_TIMEOUT_STR(AT)));
		return;
	}
	if((getmsgid(msg_data)==MSGID_ACTION_REPLY)   && 
	   (msg_length==5+54)                         && 
	   (((uint8_t*)msg_data)[3]==ACTION_GETSTATS) &&
	   (((uint8_t*)msg_data)[4]==DONE)){
		char* text;
		text=malloc(LCD_WIDTH*11+2);
		if(!text){
			error_display(PSTR(ERR_MALLOC_STR(AT)));
			return;
		}
		error_display(PSTR("ok @ " AT));
		memset(text, ' ', LCD_WIDTH*11);
		text[LCD_WIDTH*11]=text[LCD_WIDTH*11+1]='\0';
		memcpy_P(text+0*LCD_WIDTH, PSTR("max users"), 9);
		utoa(*((uint16_t*)msg_data+5), text+15+0*LCD_WIDTH, 10);
		memcpy_P(text+1*LCD_WIDTH, PSTR("users"), 5);
		utoa(*((uint16_t*)msg_data+7), text+15+1*LCD_WIDTH, 10);
		memcpy_P(text+2*LCD_WIDTH, PSTR("admins"), 6);
		utoa(*((uint16_t*)msg_data+9), text+15+2*LCD_WIDTH, 10);
		memcpy_P(text+3*LCD_WIDTH, PSTR("locked users"), 12);
		utoa(*((uint16_t*)msg_data+11), text+15+3*LCD_WIDTH, 10);
		memcpy_P(text+4*LCD_WIDTH, PSTR("locked admins"), 13);
		utoa(*((uint16_t*)msg_data+13), text+15+4*LCD_WIDTH, 10);
		memcpy_P(text+5*LCD_WIDTH, PSTR("flmDB entrys"), 12);
		utoa(*((uint16_t*)msg_data+15), text+15+5*LCD_WIDTH, 10);
		memcpy_P(text+6*LCD_WIDTH, PSTR("flmDB max"),  9);
		utoa(*((uint16_t*)msg_data+17), text+15+6*LCD_WIDTH, 10);
		
		
		lcd_cls();
		ui_textwindow(1,1,LCD_WIDTH,LCD_HEIGHT, text);
		free(text);
		return;
	}else{
		error_display(PSTR("worng answer"));
	}
	freemsg();
	
}

void panel_stats(void){
	;
}

/******************************************************************************/
/* MENUS                                                                      */
/******************************************************************************/


const char main_menu_PS[]             PROGMEM = "main menu";
const char serial_test_PS[]           PROGMEM = "test serial loop";
const char reset_PS[]                 PROGMEM = "print resets";
const char timestamp_PS[]             PROGMEM = "timestamp";
const char timestamp_live_PS[]        PROGMEM = "timestamp (live)";
const char timestamp_base64_PS[]      PROGMEM = "timestamp (B64)";
const char timestamp_base64_live_PS[] PROGMEM = "timestamp (l,B64)";
const char random_PS[]                PROGMEM = "random (30)";
const char dump_card_PS[]             PROGMEM = "dump ICC";
const char read_flash_PS[]            PROGMEM = "read flash";
const char read_logs_PS[]             PROGMEM = "read logs";
const char ui_tests_PS[]              PROGMEM = "UI tests";
const char erase_card_PS[]            PROGMEM = "erase ICC";
const char randomize_card_PS[]        PROGMEM = "randomize ICC";


menu_t debug_menu_mt[] PROGMEM = {
	{main_menu_PS, back, (superp)NULL},
	{read_logs_PS, execute, (superp)read_logs},
	{read_flash_PS, execute, (superp)read_flash},
	{ui_tests_PS, autosubmenu, (superp)ui_test_menu_mt},
	{serial_test_PS, execute, (superp)run_serial_test},
	{reset_PS, execute, (superp)print_resets},
	{timestamp_PS, execute, (superp)print_timestamp},
	{timestamp_live_PS, execute, (superp)print_timestamp_live},
	{timestamp_base64_PS, execute, (superp)print_timestamp_base64},
	{timestamp_base64_live_PS, execute, (superp)print_timestamp_base64_live},
	{random_PS, execute, (superp)print_random},
	{erase_card_PS, execute, (superp)erase_card},
	{randomize_card_PS, execute, (superp)randomize_card},
	{dump_card_PS, execute, (superp)dump_card},
	{NULL, terminator, (superp)NULL}
};

/******************************************************************************/

const char req_AB_PS[]    PROGMEM = "request AB";
const char view_AB_PS[]   PROGMEM = "view AB";

menu_t bootstrap_menu_mt[] PROGMEM = {
	{main_menu_PS, back, (superp)NULL},
	{req_AB_PS, execute, (superp)req_authblock},
	{view_AB_PS, execute, (superp)view_authblock},
	{NULL, terminator, (superp)NULL}
};


const char system_stats_PS[]  PROGMEM = "system stats";
const char panel_stats_PS[]   PROGMEM = "panel stats";

menu_t stat_menu_mt[] PROGMEM = {
	{main_menu_PS, back, (superp)NULL},
	{system_stats_PS, execute, (superp)system_stats},
	{panel_stats_PS,  execute, (superp)panel_stats},
	{NULL, terminator, (superp)NULL}
};

#ifdef GAMES

const char play_PS[]       PROGMEM = "Play";
const char highscore_PS[]  PROGMEM = "Highscore";
const char games_menu_PS[] PROGMEM = "games menu";


menu_t factorize_menu_mt[] PROGMEM = {
	{games_menu_PS, back, (superp)NULL},
	{play_PS, execute, (superp)factorize_play},
	{highscore_PS, execute, (superp)factorize_showhighscore},
	{NULL, terminator, (superp)NULL}
};

const char factorize_PS[]    PROGMEM = "factorize";

menu_t games_menu_mt[] PROGMEM = {
	{main_menu_PS, back, (superp)NULL},
	{factorize_PS, autosubmenu, (superp)factorize_menu_mt},
	{NULL, terminator, (superp)NULL}
};

#endif

const char open_door_PS[]      PROGMEM = "open door";
const char lock_door_PS[]      PROGMEM = "lock door";
const char admin_menu_PS[]     PROGMEM = "admin menu";
const char statistic_menu_PS[] PROGMEM = "statistic menu";
const char bootstrap_menu_PS[] PROGMEM = "bootstrap menu";
const char debug_menu_PS[]     PROGMEM = "debug menu";
menu_t main_menu_mt[] PROGMEM = {
	{open_door_PS,execute, (superp)open_door},
	{lock_door_PS,execute, (superp)lock_door},
	{admin_menu_PS,submenu, (superp)admin_menu},
	{statistic_menu_PS,autosubmenu, (superp)stat_menu_mt},
	{bootstrap_menu_PS,autosubmenu, (superp)bootstrap_menu_mt},
	{debug_menu_PS,autosubmenu, (superp)debug_menu_mt},
#ifdef GAMES
	{games_menu_PS,autosubmenu, (superp)games_menu_mt},
#endif
	{NULL, terminator, (superp)NULL}
};


