/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
#ifndef FELUCCA_UART
#define FELUCCA_UART 1
#endif
#define FELUCCA_UAC 1
#define FM1_TICKS_PER_US 24u
static struct { uint32_t timer_irqs, nested, in_audio; } felucca_dbg;
static uint32_t fm1_ms,t5_nested_ticks,ticks,scans,uart_calls,uart_nested,usb_calls,usb_nested,uac_calls;
static uint32_t fm1_ticks(void){return ticks;}
static void fm1_timer5_ack(void){}
static void fm1_input_tick(void){scans++;}
static void uart_midi_poll(void){uart_calls++;uart_nested+=felucca_dbg.in_audio!=0;}
static void usb_poll(void){usb_calls++;usb_nested+=felucca_dbg.in_audio!=0;}
static void uac_service(void){uac_calls++;}
#include "../firmware/src/timer5.c"
int main(void){
    for(uint32_t i=0;i<1000;i++){ticks+=2400;felucca_dbg.in_audio=i%50<45;fm1_timer5_irq();}
    int ok=scans==1000&&fm1_ms==100&&!usb_nested&&usb_calls&&uac_calls;
    ok&=FELUCCA_UART?(uart_calls==200&&uart_nested==180):uart_calls==0;
    printf("Production timer: scan/ms, TRS during render, USB deferred: %s\n",ok?"PASS":"FAIL");return !ok;
}
