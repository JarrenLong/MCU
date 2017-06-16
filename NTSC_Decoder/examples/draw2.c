/** Copyright (C) Joonas Pihlajamaa 2012. 
 * Licenced under GNU GPL, see Licence.txt for details
 * Draw composite output image on screen. No interlacing support. */

#include "windows.h"
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#include "ps2000.h"

#include "SDL/SDL.h"

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void quit(int rc) {
	SDL_Quit();
	exit(rc);
}

void drawScreen(SDL_Surface *screen, short * samples, int offset, int length, int treshold, int screen_width, int minc, int maxc) {
	Uint32 *buffer;
	int i = screen_width, j = 0;
	int value;
	int in_treshold = 0, state = -2; // -N...0 = wait blank, 1 = wait blank end, 2 = draw, 3 = done
	
	if ( SDL_LockSurface(screen) < 0 ) {
		fprintf(stderr, "Couldn't lock the display surface: %s\n",
							SDL_GetError());
		quit(2);
	}
	
	//SDL_FillRect(screen, NULL, 0x000000);
	
	buffer=(Uint32 *)screen->pixels;
	
	// loop through data
	for(; offset < length && j < screen->h; offset++) {
		if(samples[offset] < treshold) {
			if(in_treshold == 0) { // line ended
				if(state == 2) // drawing
					j++; // next line
				
				if(state < 1 && i < screen_width)
					state++; // blanking period started
				if(state == 1 && i > screen_width)
					state++; // ready to draw
				if(state == 2 && i < screen_width)
					state++; // screen updated
					
				//printf("%d point line ended at %d, j = %d, state = %d\n", i, offset, j, state);
				i = 0;
			}
			in_treshold++; // blanking
		} else {
			in_treshold=0;
			
			if(state == 2) { // drawing
				value = 256 * (samples[offset]-minc) / (maxc-minc);
				
				if(value > 255)
					value = 255;
				if(value < 0)
					value = 0;
				
				if(i < screen->w)
					buffer[j*screen->pitch/4 + i] = value * 0x10101;
			}
			i++;
		}
	}
	
	printf("%d lines drawn\n", j);
	SDL_UnlockSurface(screen);
	SDL_UpdateRect(screen, 0, 0, 0, 0);
}

#define CAPTURE_LENGTH (500*1000)

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
	SDL_Surface *screen;
	int done = 0, redraw;
	SDL_Event event;

	short handle;
	int capture_interval;
	int offset=0;
	int treshold = 2048, screen_width;
	int count[65536], i, sum, total;
	
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
	
	capture_data(handle, capture_interval);
	
	/* Try to guess a good treshold value */
	for(i=0; i<65536; i++)
		count[i] = 0;
		
	total = 0;
	for(i=0; i<CAPTURE_LENGTH; i++) {
		if(streaming_buffer[i] > -32768) {
			count[streaming_buffer[i] + 32768]++;
			total++;
		}
	}
	
	sum = 5 * total / 100; // blanking periods are about 9.5 % of the signal
	
	for(i=0; i<65536 && sum >= 0; i++) {
		if(!count[i])
			continue;
			
		printf("At %d, sum is still %d, decreasing by %d\n", i, sum, count[i]);
		sum -= count[i];
		
		if(sum < 0)
			treshold = i - 32768 + 1;
	}
	
	printf("Treshold set at %d\n", treshold);
	
	if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
		fprintf(stderr, "Couldn't initialize SDL: %s\n",SDL_GetError());
		return(1);
	}

	if ( (screen=SDL_SetVideoMode(640, 480, 32, SDL_SWSURFACE)) == NULL ) {
		fprintf(stderr, "Couldn't set 640x480x32 video mode: %s\n", SDL_GetError());
		quit(2);
	}
	
	// find minimum and maximum colors
	int minc = 65536, maxc = 0;
	
	for(i=0; i<CAPTURE_LENGTH; i++) {
		if(streaming_buffer[i] < treshold)
			continue;
			
		if(streaming_buffer[i] < minc)
			minc = streaming_buffer[i];
		if(streaming_buffer[i] > maxc)
			maxc = streaming_buffer[i];
	}
	
	redraw = 1;
	while(!done) {
		//if(redraw) {
			capture_data(handle, capture_interval);
			drawScreen(screen, streaming_buffer, offset, CAPTURE_LENGTH, treshold, screen_width, minc, maxc);
			redraw = 0;
		//}
		while(SDL_PollEvent(&event)) {
			switch(event.type) {
			case SDL_MOUSEBUTTONDOWN: // set black level on mouseclick
				printf("Increasing minc from %d ...", minc);
				minc += (((Uint32 *)screen->pixels)[event.button.y * screen->pitch/4 + event.button.y] & 255) * (maxc-minc) / 256;
				printf(" to %d\n", minc);
				break;
				
			case SDL_KEYDOWN:
				fprintf(stderr, "Key %d pressed\n", event.key.keysym.scancode);
				switch(event.key.keysym.scancode) {
				case 75: // left
				case 77: // right
				case 72: // up
					// raise black point
					for(i=minc-1; i > treshold && !count[i+32768]; i--) {}
					if(i > treshold)
						minc = i;
					break;
				case 80: // down
					// lower black point
					for(i=minc+1; i < 32768 && !count[i+32768]; i++) {}
					if(i < 32768)
						minc = i;
					break;
				case 78: // +
					//redraw = 1;
					break;
				case 74: // -
					break;
				}					
				break;
			case SDL_QUIT:
				done = 1;
				break;
			}
		}
	}
	
	ps2000_close_unit(handle);
	
	SDL_Quit();
		
	return(0);
}
