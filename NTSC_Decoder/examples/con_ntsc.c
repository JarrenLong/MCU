/** Copyright (C) Joonas Pihlajamaa 2012. 
 * Licenced under GNU GPL, see Licence.txt for details
 * Draw composite output image on screen. Assumes NTSC signal. */

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

// Key parameters for NTSC conversion
int treshold, align_treshold = 5800; // level below which signal is considered sync
int screen_width; // when there's more pixels than this, it's a normal scanline
int long_high, long_low; // amount of samples in a long high or low pulse within VSYNC
int level_black, level_white; // signal levels for total black and white, respectively


#define ST_WAIT_NORMAL 0
#define ST_WAIT_BLANK 1
#define ST_COUNT_LONGS 2
#define ST_WAIT_NON_BLANK 3
#define ST_DRAW 4

// Extract NTSC frame from samples, and determine if it's first or second field
// returns -1 if no frame could be extracted, 0 for first field and 1 for second field
// data in surface will get garbled in case of -1
int extractFrame(SDL_Surface *surface, short * samples, int length, int align) {
	Uint32 *buffer;
	
	int i = screen_width, j, offset, value, longs;
	int state = ST_WAIT_NORMAL;
	int is_high, count, is_transition;

	if ( SDL_LockSurface(surface) < 0 ) {
		fprintf(stderr, "Couldn't lock the display surface: %s\n",
							SDL_GetError());
		quit(2);
	}
	
	buffer=(Uint32 *)surface->pixels;
		
	// loop through data
	for(offset=0, j=0, count=0; offset < length && j < surface->h; offset++) {
		// set is_high, is_transition
		if(samples[offset] <= treshold) { // low
			is_transition = is_high ? 1 : 0;
			is_high = 0;
		} else { // high
			is_transition = is_high ? 0 : 1;
			is_high = 1;
		}
		
		if(is_transition) { // only handle state changes on transitions
			/*if(is_high)
				printf("%3d x low (state: %d) %d\n", count, state, longs);
			else
				printf("%3d x high ", count);*/
			
			switch(state) {
			case ST_WAIT_NORMAL:
				if(!is_high && count > screen_width) // start hsync, last scanline was normal
					state++;
				break;
			case ST_WAIT_BLANK:
				if(!is_high && count < screen_width) { // first vsync period
					state++;
					longs = (count > long_high) ? 1 : 0; // should always be 1
				}
				break;
			case ST_COUNT_LONGS:
				if(!is_high) {
					if(count > long_high) // another long
						longs++;
					else // longs counted
						state++;
				}
				break;
			case ST_WAIT_NON_BLANK:
				if(!is_high && count > screen_width) // start hsync, last scanline was normal
					state++;
				break;
			case ST_DRAW:
				if(!is_high) { // start hsync
					if(count < screen_width) { // start vsync
						//printf("%d lines and %d longs decoded\n", j, longs);
						if(j < 252) { // not enough scanlines
							SDL_UnlockSurface(surface);
							return -1;
						} else {
							SDL_UnlockSurface(surface);
							return (longs == 7) ? 0 : 1; // determine field number
						}
					} else { // new scanline
						j++;
						i=0;
					}
				} else {
					if(align & 1)
						i = count-28; /*+ (longs == 7 ? align : 0)*/; // remove stutter
				}
				break;
			}
			count = 1;
		} else
			count++;
			
		if(state != ST_DRAW || !is_high)
			continue;
		
		// very crude manual alignment works with odd values of "align" (cursor left/right changes "align")
		/*if((align&1) && count > 25 && count < 40 && samples[offset-1] < align_treshold && samples[offset] >= align_treshold)
			i = 39-count; //printf("%d\n", count+i);*/
			
		value = 256 * (samples[offset]-level_black) / (level_white-level_black);
		
		if(value > 255)
			value = 255;
		if(value < 0)
			value = 0;
		
		if(i < surface->w)
			buffer[j*surface->pitch/4 + (count + i) * 2] = value * 0x10101;
			buffer[j*surface->pitch/4 + (count + i) * 2 + 1] = value * 0x10101;
	}
		
	printf("Ran out of data at %d lines\n", j);
		
	SDL_UnlockSurface(surface);
	return -1; // data ran out
}

void drawScreen(SDL_Surface *screen, SDL_Surface *half, int fieldnum) {
	SDL_Rect src, dest;
		
	src.x = dest.x = 0;
	src.w = dest.w = 800;
	src.h = dest.h = 1;
	
	for(src.y=0, dest.y=fieldnum; src.y<254; src.y++, dest.y+=2)
		SDL_BlitSurface(half, &src, screen, &dest);
		
	//SDL_FillRect(screen, NULL, 0x000000);
	
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
	SDL_Surface *screen, *field;
	int done = 0, redraw, fieldnum, align=0;
	SDL_Event event;

	short handle;
	int capture_interval;
	int count[65536], i, sum, total;
	
	if(argc == 1)
		capture_interval = 150;
	else
		capture_interval = atoi(argv[1]);
	
	// calculate approximate values for signal parameters based on capture interval
	screen_width = 58000/capture_interval;
	long_high = 15000/capture_interval;
	long_low = 15000/capture_interval;
	
	handle = ps2000_open_unit();
	
	ps2000_set_channel(handle, PS2000_CHANNEL_A, TRUE, 1, PS2000_5V);
	ps2000_set_channel(handle, PS2000_CHANNEL_B, FALSE, 1, PS2000_5V);
	
	// No trigger
	ps2000_set_trigger(handle, PS2000_NONE, 0, PS2000_RISING, 0, 0);
	
	streaming_buffer = (short *)malloc(sizeof(short) * CAPTURE_LENGTH);
	
	capture_data(handle, capture_interval);
	
	/* Try to guess a good treshold value */
	treshold = 2048; // initial guess
	
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

	if ( (screen=SDL_SetVideoMode(800, 600, 32, SDL_SWSURFACE)) == NULL ) {
		fprintf(stderr, "Couldn't set 640x480x32 video mode: %s\n", SDL_GetError());
		quit(2);
	}
	
	// allocate space for field
	field = SDL_CreateRGBSurface(SDL_SWSURFACE, 800, 600, 32, 0xFF0000, 0xFF00, 0xFF, 0);
	
	// find minimum and maximum colors
	level_black = 65536;
	level_white = 0;
	
	for(i=0; i<CAPTURE_LENGTH; i++) {
		if(streaming_buffer[i] < treshold)
			continue;
			
		if(streaming_buffer[i] < level_black)
			level_black = streaming_buffer[i];
		if(streaming_buffer[i] > level_white)
			level_white = streaming_buffer[i];
	}
	
	redraw = 1;
	while(!done) {
		//if(redraw) {
			capture_data(handle, capture_interval);
			fieldnum = extractFrame(field, streaming_buffer, CAPTURE_LENGTH, align);
			if(fieldnum != -1)
				drawScreen(screen, field, fieldnum);
			redraw = 0;
		//}
		while(SDL_PollEvent(&event)) {
			switch(event.type) {
			case SDL_MOUSEBUTTONDOWN: // set black level on mouseclick
				/*align = event.button.x;
				printf("Now sampling from %d\n", align);*/
				redraw = 1;
				/*printf("Increasing level_black from %d ...", level_black);
				level_black += (((Uint32 *)screen->pixels)[event.button.y * screen->pitch/4 + event.button.y] & 255) * (level_white-level_black) / 256;
				printf(" to %d\n", level_black);*/
				break;
				
			case SDL_KEYDOWN:
				fprintf(stderr, "Key %d pressed\n", event.key.keysym.scancode);
				switch(event.key.keysym.scancode) {
				case 75: // left
					align--;
					break;
				case 77: // right
					align++;
					break;
				case 72: // up
					// raise black point
					for(i=level_black-1; i > treshold && !count[i+32768]; i--) {}
					if(i > treshold)
						level_black = i;
					break;
				case 80: // down
					// lower black point
					for(i=level_black+1; i < 32768 && !count[i+32768]; i++) {}
					if(i < 32768)
						level_black = i;
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
