/** Copyright (C) Joonas Pihlajamaa 2012. 
 * Licenced under GNU GPL, see Licence.txt for details
 * Analyze NTSC sync pulses for length. */

#include "windows.h"
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#include "ps2000.h"

#define CAPTURE_LENGTH (1000*1000)

short * streaming_buffer;
long streaming_ptr;

void  __stdcall ps2000FastStreamingReady(short **overviewBuffers, 
		short overflow, unsigned long triggeredAt, short triggered, 
		short auto_stop, unsigned long nValues) {
	long i;
	short * channel_A = overviewBuffers[0];
	
	for(i=0; i<nValues && streaming_ptr < CAPTURE_LENGTH; i++)
		streaming_buffer[streaming_ptr++] = channel_A[i];
}

void capture_data(short handle, int capture_interval) {
	streaming_ptr = 0;
	
	// Collect a maximum of 1000 000 samples, no auto-stop, no aggregation (1 samples aggregated), 200 k overview buffer
	if(capture_interval < 1000)
		ps2000_run_streaming_ns(handle, capture_interval, PS2000_NS, 1000000, 0, 1, 200000);
	else
		ps2000_run_streaming_ns(handle, capture_interval/1000, PS2000_US, 1000000, 0, 1, 200000);
	
	while(streaming_ptr < CAPTURE_LENGTH) {
		ps2000_get_streaming_last_values(handle, ps2000FastStreamingReady);
		Sleep (0);
	}
	
	ps2000_stop(handle);
}

int main(int argc, char *argv[]) {
	short handle;
	int capture_interval;
	int treshold = 2048, screen_width;
	int i;
	
	if(argc == 1)
		capture_interval = 150;
	else
		capture_interval = atoi(argv[1]);
	
	screen_width = 58000/capture_interval; // lines shorter than this are definitely part of screen blanking
	
	handle = ps2000_open_unit();
	
	ps2000_set_channel(handle, PS2000_CHANNEL_A, TRUE, 1, PS2000_5V);
	ps2000_set_channel(handle, PS2000_CHANNEL_B, FALSE, 1, PS2000_5V);
	
	// No trigger
	ps2000_set_trigger(handle, PS2000_NONE, 0, PS2000_RISING, 0, 0);
	
	streaming_buffer = (short *)malloc(sizeof(short) * CAPTURE_LENGTH);
	
	printf("Treshold set at %d\n", treshold);
	
	int long_high = 100, long_low = 100;
	int low, count, lines, vsync;
	
	capture_data(handle, capture_interval);
	
	low = 0;
	count = 0;
	lines = 0;
	vsync = 0;
	
	// Analyze data
	for(i=0; i<CAPTURE_LENGTH; i++) {
		if(streaming_buffer[i] < treshold) { // signal low
			if(!low) { // just went down
				low = 1;
				if(count > screen_width) { // should be normal scanline
					vsync = 0; // not in vertical sync
					lines++;
				} else { // short one!
					vsync = 1; // in vertical sync
					if(lines > 0) { // display leftover lines
						printf("\n%4d ", lines);
						lines = 0;
					}
					if(count > long_high)
						printf("H ", count);
					else
						printf("- ", count);
				}
				count = 0; // initialize counter
			}
			count++;
		} else { // signal high
			if(low) { // just went up
				low = 0;
				if(vsync) { // only display low lengths on vsync period
					if(count > long_low)
						printf("_ ", count);
					else
						printf(". ", count);
				}
				count = 0;
			}
			count++;
		}
	}
	
	ps2000_close_unit(handle);
	
	return 0;
}
