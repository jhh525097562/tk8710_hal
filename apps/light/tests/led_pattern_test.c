#define _POSIX_C_SOURCE 200809L
#include "led_policy.h"
#include <gpiod.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
static volatile sig_atomic_t done;
static void stop(int s){(void)s;done=1;}
static uint64_t ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
int main(void)
{
    struct gpiod_chip *c=gpiod_chip_open_by_name("gpiochip1");
    struct gpiod_line_bulk b;unsigned pins[]={23,24};int off[]={0,0},rc=0;
    if(!c || gpiod_chip_get_lines(c,pins,2,&b) || gpiod_line_request_bulk_output(&b,"led_pattern_test",off))return 1;
    signal(SIGTERM,stop);signal(SIGINT,stop);setvbuf(stdout,NULL,_IOLBF,0);
    const int modes[]={LED_PREPARING,LED_GREEN_NS,LED_GREEN_GPS,LED_RED_MQTT,LED_RED_NS,LED_RED_GPS,LED_BUSINESS_FAULT,LED_MAINTENANCE,LED_READY,LED_NETWORK_FAULT};
    const int periods[]={2250,2750,3250,2250,2750,3250,3750,2000,1000,1000};
    for(int i=0;i<10 && !done;i++) {
        uint64_t begin=ms();int last[]={-1,-1};
        printf("MODE %d\n",modes[i]);
        while(!done && ms()-begin<(uint64_t)periods[i]*2) {
            uint64_t elapsed=ms()-begin;int v[2],actual[2];LedLevels(modes[i],elapsed,0,v);
            if(v[0]!=last[0] || v[1]!=last[1]) {
                if(gpiod_line_set_value_bulk(&b,off) || gpiod_line_set_value_bulk(&b,v) || gpiod_line_get_value_bulk(&b,actual)) {rc=2;done=1;break;}
                if(actual[0]!=v[0] || actual[1]!=v[1] || (actual[0] && actual[1])) {rc=3;done=1;break;}
                printf("EDGE %llu %d %d\n",(unsigned long long)elapsed,actual[0],actual[1]);
                last[0]=v[0];last[1]=v[1];
            }
            struct timespec delay={0,10000000};nanosleep(&delay,NULL);
        }
    }
    (void)gpiod_line_set_value_bulk(&b,off);gpiod_line_release_bulk(&b);gpiod_chip_close(c);
    printf("RESULT %d\n",rc);return rc;
}
