/** Copyright (C) Joonas Pihlajamaa 2012. 
 * Licenced under GNU GPL, see Licence.txt for details
 * Draw composite output image on screen. Assumes NTSC signal. */

#include "windows.h"
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#include "util.h"
#include "picoutil.h"

#include "SDL/SDL.h"

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void quit(int rc) {
	SDL_Quit();
	exit(rc);
}

// Key parameters for NTSC conversion
int treshold; // level below which signal is considered sync
int scanline_w; // scanline length, approximate
int screen_width; // when there's more pixels than this, it's a normal scanline
int long_high, long_low; // amount of samples in a long high or low pulse within VSYNC
int level_black, level_white; // signal levels for total black and white, respectively

// rough scanline timings from HSYNC start:
// sync length 4.3 us
// colourburst area ends 9.0 us
// visible area ends 61.9 us
// scanline ends 63.3 us
void calculate_parameters(long timeInterval, short * buffer, long samples) {
	// calculate approximate values for signal parameters based on capture interval (ns)
	scanline_w = 63556/timeInterval; // 1s / 29.97 / 525 = ca. 63.5556 us
	screen_width = 58000/timeInterval;
	long_high = 15000/timeInterval;
	long_low = 15000/timeInterval;

	// Try to guess a good treshold value
	treshold = get_min(buffer, samples);
	treshold = get_next(buffer, samples, treshold);
	
	// find minimum and maximum colors
	level_white = get_max(buffer, samples);
	level_black = get_next(buffer, samples, treshold);
}

#define MIN_SCALE_X -2
#define MAX_SCALE_X 1

#define MIN_SCALE_Y 0
#define MAX_SCALE_Y 1

#define ST_WAIT_NORMAL 0
#define ST_WAIT_BLANK 1
#define ST_COUNT_LONGS 2
#define ST_WAIT_NON_BLANK 3
#define ST_DRAW 4

void draw_scanline(Uint32 *buffer, SDL_Surface *surface, short *samples, int scanline_start, int offset, int row, int scale_x) {
	int count, value, sync_len = offset - scanline_start;
	
	for(count = 0; count < scanline_w - sync_len; count++, offset++) {
		switch(scale_x) {
		case -2: // pixel every fourth pixel, count averages of samples 1 and 3
			value = 128 * (samples[offset]-level_black) / (level_white-level_black);		
			value = MAX(0, MIN(value, 127));
			
			if(count/4 < surface->w) {
				if((count & 3) == 1)
					buffer[row*surface->pitch/4 + (count>>2)] = value * 0x10101;
				else if((count & 3) == 3)
					buffer[row*surface->pitch/4 + (count>>2)] += value * 0x10101;
			}
			
			break;
			
		case -1: // count averages of every two pixels
			value = 128 * (samples[offset]-level_black) / (level_white-level_black);		
			value = MAX(0, MIN(value, 127));
			
			if(count/2 < surface->w) {
				if((count & 1) == 0)
					buffer[row*surface->pitch/4 + (count>>1)] = value * 0x10101;
				else
					buffer[row*surface->pitch/4 + (count>>1)] += value * 0x10101;
			}
			
			break;
			
		case 1:
			value = 256 * (samples[offset]-level_black) / (level_white-level_black);		
			value = MAX(0, MIN(value, 255));
			
			if(count*2 < surface->w) {
				buffer[row*surface->pitch/4 + count*2] = value * 0x10101;
				buffer[row*surface->pitch/4 + count*2+1] = value * 0x10101;
			}
			
			break;
			
		default:
			value = 256 * (samples[offset]-level_black) / (level_white-level_black);		
			value = MAX(0, MIN(value, 255));
			
			if(count < surface->w)
				buffer[row*surface->pitch/4 + count] = value * 0x10101;
		}
	}
}

// Extract NTSC field from samples, and determine if it's partial, first, or second field
// returns the amount of samples processed
// sets fieldtype to -1 for partial field, 0 for first field and 1 for second field
// set scale_x to -1 to halve the width, 0 to leave intact, 1 to double
int extract_field(SDL_Surface *surface, short * samples, int length, int scale_x, int *field_type) {
	Uint32 *buffer;
	
	int j, offset, scanline_start = 0, longs = 0;
	int state = ST_WAIT_NORMAL;
	int is_sync = 0, count, is_transition = 0;

	if ( SDL_LockSurface(surface) < 0 ) {
		fprintf(stderr, "Couldn't lock the display surface: %s\n",
				SDL_GetError());
		quit(2);
	}
	
	buffer=(Uint32 *)surface->pixels;
		
	// loop through data
	for(offset=0, j=0, count=0; offset < length && j < surface->h; offset++) {
		// set is_sync, is_transition
		if(samples[offset] <= treshold) { // low
			is_transition = is_sync ? 0 : 1;
			is_sync = 1;
		} else { // high
			is_transition = is_sync ? 1 : 0;
			is_sync = 0;
		}
		
		if(is_transition) { // only handle state changes on transitions			
			switch(state) {
			case ST_WAIT_NORMAL:
				if(is_sync && count > screen_width) // start hsync, last scanline was normal
					state++;
				break;
			case ST_WAIT_BLANK:
				if(is_sync && count < screen_width) { // first vsync period
					state++;
					longs = (count > long_high) ? 1 : 0; // should always be 1
				}
				break;
			case ST_COUNT_LONGS:
				if(is_sync) {
					if(count > long_high) // another long
						longs++;
					else // longs counted
						state++;
				}
				break;
			case ST_WAIT_NON_BLANK:
				if(is_sync && count > screen_width) // start hsync, last scanline was normal
					state++;
				break;
			case ST_DRAW:
				if(is_sync) { // start hsync
					if(count < screen_width) { // start vsync
						//printf("%d lines and %d longs decoded\n", j, longs);
						if(j < 252) { // not enough scanlines - partial field
							SDL_UnlockSurface(surface);
							*field_type = -1;
							return offset;
						} else {
							SDL_UnlockSurface(surface);
							*field_type = (longs == 7) ? 0 : 1; // determine field number
							return offset;
						}
					} else { // next scanline
						j++;
						scanline_start = offset;
					}
				} else { // end hsync
					if(scanline_start + scanline_w < length)
						draw_scanline(buffer, surface, samples, scanline_start, offset, j, scale_x);
					// remove stutter
				}
				break;
			}
			count = 0;
		} else
			count++;
	}
				
	SDL_UnlockSurface(surface);
	*field_type = -1;
	return offset; // data ran out
}

int crop_left, copy_width, crop_top, crop_bottom;

void draw_screen(SDL_Surface *screen, SDL_Surface *half, int field_num, int scale_x, int scale_y) {
	SDL_Rect src, dest;
	int crop_x, copy_w;
	
	switch(scale_x) {
	case -2:
		crop_x = crop_left/4;
		copy_w = copy_width/4;
		break;
	case -1:
		crop_x = crop_left/2;
		copy_w = copy_width/2;
		break;
	case 0:
		crop_x = crop_left;
		copy_w = copy_width;
		break;
	case 1:
	default:
		crop_x = crop_left*2;
		copy_w = copy_width*2;
		break;
	}
		
	src.x = crop_x;
	dest.x = 0;
	src.w = MIN(screen->w, copy_w); // never exceed screen width
	src.h = 1;
	
	if(scale_y == 0) {
		for(src.y=crop_top, dest.y=field_num; src.y<252-crop_bottom && dest.y < screen->h; src.y++, dest.y+=2)
			SDL_BlitSurface(half, &src, screen, &dest);
	} else {
		for(src.y=crop_top, dest.y=field_num*2; src.y<252-crop_bottom && dest.y < screen->h; src.y++, dest.y+=3) {
			SDL_BlitSurface(half, &src, screen, &dest);
			dest.y += 1;
			SDL_BlitSurface(half, &src, screen, &dest);
		}
	}
}

void update_screen(SDL_Surface *screen) {
	SDL_UpdateRect(screen, 0, 0, 0, 0);
}

void clear_surface(SDL_Surface *surface) {
	if ( SDL_LockSurface(surface) < 0 ) {
		fprintf(stderr, "Couldn't lock the display surface: %s\n",
				SDL_GetError());
		quit(2);
	}
	
	SDL_FillRect(surface, NULL, 0x000000);
	
	SDL_UnlockSurface(surface);
}

int main(int argc, char *argv[]) {
	SDL_Surface *screen, *field = NULL;
	int done = 0, field_num, scale_x, scale_y;
	SDL_Event event;

	short handle, *buffer, overflow;
	long timeInterval, samples, gotSamples;
	unsigned long timebase;
	int i, first_run = 1;
	
	char inifile[80];
	
	// with timebase > 2, sampling interval = (timebase - 2) * 16 ns
	if(argc > 1) {
		strcpy(inifile, argv[1]);
		strcat(inifile, ".ini");
		load_settings(inifile);
	} else
		load_settings("console.ini");
	
	timebase = get_setting_or("timebase", 6); // 64 ns gets us about 1000 samples per scan line
	scale_x = get_setting_or("scale_x", 0);
	scale_y = get_setting_or("scale_y", 0);
	
	switch(timebase) {
	case 0: timeInterval = 2; break;
	case 1: timeInterval = 4; break;
	case 2: timeInterval = 8; break;
	default: timeInterval = 16 * (timebase - 2);
	}
	
	printf("Time interval is %ld ns, estimating %ld samples for a scanline\n",
		timeInterval, 64000 / timeInterval);
	samples = (64000/timeInterval) * 2*525; // We'll need two frames long buffer to ensure one whole
	
	handle = init_ps3000(timebase, samples, &timeInterval);
	if(handle == -1) {
		printf("Could not initialize the scope! Exiting...\n");
		return -1;
	}
	
	printf("Got time interval of %ld\n", timeInterval);
	
	// We can now calculate default crop values
	crop_left = get_setting_or("crop_left", 0);
	copy_width = get_setting_or("copy_width", 64000/timeInterval);
	crop_top = get_setting_or("crop_top", 0);
	crop_bottom = get_setting_or("crop_bottom", 0);
	
	buffer = (short *)malloc(sizeof(short) * samples);
	if(buffer == NULL) {
		printf("Ran out of memory while allocating %ld sample buffer\n", samples);
		deinit_ps3000(handle);
		return -1;
	}
	
	if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
		fprintf(stderr, "Couldn't initialize SDL: %s\n",SDL_GetError());
		return(1);
	}

	if ( (screen=SDL_SetVideoMode(get_setting_or("win_width", 800), 
			get_setting_or("win_height", 600), 32, SDL_SWSURFACE)) == NULL ) {
		fprintf(stderr, "Couldn't set video mode: %s\n", SDL_GetError());
		quit(2);
	}
	
	SDL_WM_SetCaption("PS3000 Composite Video Decoder", "PS3000 Composite...");
	
	while(!done) {
		gotSamples = capture_ps3000(handle, buffer, samples, timebase, &overflow);
		
		if(gotSamples == samples && !overflow) {
			if(first_run) {
				// calculate parameters
				calculate_parameters(timeInterval, buffer, samples);
				
				// on first run, we use default black and white points if available
				level_black = get_setting_or("level_black", level_black);
				level_white = get_setting_or("level_white", level_white);
				
				printf("Scanline should be about %d samples\n", scanline_w);
				// allocate space for field buffer
				field = SDL_CreateRGBSurface(SDL_SWSURFACE, 
					11 * scanline_w / 10, // allocate about 10% extra for scanline
					300, // 252 visible scanlines so this should be plenty
					32, 0xFF0000, 0xFF00, 0xFF, 0);
					
				first_run = 0;
			}
			
			for(i=0; i<samples;) {
				// skip a bit back for consecutive frames to allow VSYNC detection
				if(i > 2 * scanline_w)
					i -= 2 * scanline_w;
					
				i += extract_field(field, buffer+i, samples-i, scale_x, &field_num);
				
				if(field_num != -1)
					draw_screen(screen, field, field_num, scale_x, scale_y);
			}
			update_screen(screen);
		}
		
		while(SDL_PollEvent(&event)) {
			switch(event.type) {
			/*case SDL_MOUSEBUTTONDOWN:
				x = event.mouse.x;
				break;*/
				
			case SDL_KEYDOWN:
				//fprintf(stderr, "Key %d pressed\n", event.key.keysym.scancode);
				switch(event.key.keysym.scancode) {
				case 57: // space
					// reinitialize values based on current sample set
					calculate_parameters(timeInterval, buffer, samples); 
					break;
				case 75: // left
					if(scale_x > MIN_SCALE_X)
						scale_x--;
					clear_surface(field);
					break;
				case 77: // right
					if(scale_x < MAX_SCALE_X)
						scale_x++;
					clear_surface(field);
					break;
				case 72: // up
					if(event.key.keysym.mod & KMOD_SHIFT) {
						if(scale_y > MIN_SCALE_Y)
							scale_y--;
						clear_surface(field);
					} else {
						// lower black point
						i = get_prev(buffer, samples, level_black);
						if(i > treshold) {
							level_black = i;
							printf("New black point at %d\n", level_black);
						}	
					}
					break;
				case 80: // down
					if(event.key.keysym.mod & KMOD_SHIFT) {
						if(scale_y < MAX_SCALE_Y)
							scale_y++;
						clear_surface(field);
					} else {
						// raise black point
						i = get_next(buffer, samples, level_black);
						if(i < 32767) {
							level_black = i;
							printf("New black point at %d\n", level_black);
						}
					}
					break;
				case 78: // +
					// lower white point
					i = get_prev(buffer, samples, level_white);
					if(i > treshold) {
						level_white = i;
						printf("New white point at %d\n", level_white);
					}
					break;
				case 74: // -
					// raise white point
					i = get_next(buffer, samples, level_white);
					if(i < 32767) {
						level_white = i;
						printf("New white point at %d\n", level_white);
					}
					break;
				}				
				break;
			case SDL_QUIT:
				done = 1;
				break;
			}
		}
	}
	
	deinit_ps3000(handle);
	
	SDL_Quit();
		
	return(0);
}
