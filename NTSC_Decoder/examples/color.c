/** Copyright (C) Joonas Pihlajamaa 2012. 
 * Licenced under GNU GPL, see Licence.txt for details
 * Draw composite output image on screen. Assumes NTSC signal. */

#include "windows.h"
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <math.h>
#include <time.h>

#include "draw.h"
#include "util.h"
#include "picoutil.h"

#include "SDL/SDL.h"

// Uncomment to add color compensation
//#define COLOR_COMP

// Uncomment to add dumping scanlines to line.csv with left mouse click
//#define DEBUG

// Color subcarrier frequency in MHz
#define COLOR_SUBCARRIER  3.579545

#define CALC_RGB(r,g,b) (((r)<<16) + ((g)<<8) + (b))
#define SCALE(c, min, max) MIN(1.0, MAX(0.0, ((c)-(min))/((max)-(min))))

#define TP_FRAME 0
#define TP_PICO 10
#define TP_FIELD 20
#define TP_SCANLINE 25
#define TP_BURST 26
#define TP_DRAW 30

// using .8 fixed point here
#define MUL_WAVE 256
#define SHIFT_WAVE 8

int *color_wave1;
int *color_wave2;

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void quit(int rc) {
	SDL_Quit();
	exit(rc);
}

// Key parameters for NTSC conversion
int treshold; // level below which signal is considered sync
int scanline_w; // scanline length, approximate
int color_burst_start, color_burst_len; // approximate offset interval
int screen_width; // when there's more pixels than this, it's a normal scanline
int long_high, long_low; // amount of samples in a long high or low pulse within VSYNC
float f_wavelength; // length of a single color waveform in samples
int i_wavelength; // approximate length
int wave_before, wave_after; // integer values, before + after = i_wavelength - 1

// Crop values for nicer display
int crop_left, copy_width, crop_top, crop_bottom;

// rough scanline timings from HSYNC start:
// sync length 4.3 us
// colourburst area ends 9.0 us
// visible area ends 61.9 us
// scanline ends 63.3 us
void calculate_parameters(long timeInterval) {
	// calculate approximate values for signal parameters based on capture interval (ns)
	scanline_w = 63556/timeInterval; // 1s / 29.97 / 525 = ca. 63.5556 us
	
	screen_width = 58000/timeInterval;
	
	long_high = 15000/timeInterval;
	long_low = 15000/timeInterval;
	
	color_burst_start = 5300/timeInterval;
	color_burst_len = 2500/timeInterval;
	
	// we'll do running calculation with a window of data points before and after current pixel
	f_wavelength = 1000.0 / COLOR_SUBCARRIER / (float)timeInterval;
	i_wavelength = (int)(f_wavelength + 0.5);
	wave_before = i_wavelength / 2;
	wave_after = i_wavelength - wave_before - 1;
	
	// set vertical crop values
	crop_top = get_setting_or("crop_top", 0);
	crop_bottom = get_setting_or("crop_bottom", 0);

	// calculate horizontal crop values
	crop_left = scanline_w * get_setting_or("crop_left", 0) / 100;
	copy_width = scanline_w - crop_left - scanline_w * get_setting_or("crop_right", 0) / 100;
}

// Lookup tables are used for YIQ -> RGB conversion, but values need to be shifted
// so the tables won't get unnecessarily long
#define SHIFT_Y 5
#define SHIFT_I 16
#define SHIFT_Q 16

int * lookup_Y = NULL, * lookup_I = NULL, * lookup_Q = NULL;
int min_Y, min_I, min_Q, max_Y, max_I, max_Q;

#ifdef COLOR_COMP

#define MAX_AMP 32768 // actually max amp is 32767, but we need 1 more in arrays

int * comp_I = NULL, * comp_Q = NULL;
short amp_histogram[MAX_AMP];
short amps_measured = 0;

#endif // COLOR_COMP

// Analyze potential scanline to find Y/I/Q min/max values
void analyze_scanline(short * samples, int scanline_start) {
	int sync_end, next_sync;
	int count;
	int adj; // color waveform adjustment, best fit
	int sum, bestSum = -1000000000, bestAdj = 0;
	int Y, run_I, run_Q;
		
	// find sync end
	for(count = 0; count < scanline_w; count++)
		if(samples[scanline_start + count] > treshold)
			break;
	
	if(count < 1) { // way too short sync
		return; // don't process as normal scanline
	} else if(count > color_burst_start) { // way too long sync
		return; // don't process as normal scanline
	} else
		sync_end = count;
		
	// find next sync start
	for(next_sync = sync_end; next_sync < scanline_w; next_sync++)
		if(samples[scanline_start + next_sync] <= treshold)
			break;
	
	if(next_sync < 9 * scanline_w / 10) { // seems like a VSYNC
		//printf("%7d: Too short non-sync (%d)!\n", scanline_start, next_sync);
		return; // don't process as normal scanline
	}

#ifdef COLOR_COMP
	short min, max;
	
	// measure color burst amplitude and add to histogram
	get_minmax(samples + scanline_start + color_burst_start, color_burst_len, &min, &max);
	amp_histogram[(max - min) / 2]++;
	amps_measured++;
#endif // COLOR_COMP

	// fit reference waveform to data
	for(adj = 0; adj < i_wavelength; adj++) { // it's not least squares, but maximum products :)
		sum = 0;
		
		for(count = color_burst_start; count < color_burst_start + color_burst_len; count++)
			sum += (color_wave1[count - adj] * samples[scanline_start + count]) >> 4; // drop a bit of accuracy
		
		if(sum > bestSum) {
			bestAdj = adj;
			bestSum = sum;
		}
	}
	
	// to avoid accessing values beyond the scanline, we'll start a bit before color burst end
	run_I = run_Q = 0;
	
	for(count = color_burst_start - wave_before; count < color_burst_start + wave_after; count++) {
		run_I += samples[scanline_start + count] * color_wave1[count - bestAdj];
		run_Q += samples[scanline_start + count] * color_wave2[count - bestAdj];
	}
		
	// color components are estimated using running averages
	for(count = color_burst_start; count + wave_after < scanline_w; count++) {	
		run_I += samples[scanline_start + count + wave_after] * color_wave1[count + wave_after - bestAdj];
		run_Q += samples[scanline_start + count + wave_after] * color_wave2[count + wave_after - bestAdj];

		Y = samples[scanline_start + count];	
		
		min_Y = MIN(min_Y, Y);
		max_Y = MAX(max_Y, Y);		
		min_I = MIN(min_I, run_I);
		max_I = MAX(max_I, run_I);
		min_Q = MIN(min_Q, run_Q);
		max_Q = MAX(max_Q, run_Q);
		
		run_I -= samples[scanline_start + count - wave_before] * color_wave1[count - wave_before - bestAdj];
		run_Q -= samples[scanline_start + count - wave_before] * color_wave2[count - wave_before - bestAdj];
	}
}

void analyze_samples(short * samples, long length) {
	int offset, is_sync = 1, is_transition = 0, range;
	float comp;
	
	// Try to guess a good treshold value
	treshold = get_min(samples, length);
	treshold = get_next(samples, length, treshold);
	
	// initialize min/max Y/I/Q values
	max_Y = max_I = max_Q = -1000000000;
	min_Y = min_I = min_Q = 1000000000;

#ifdef COLOR_COMP
	int color_amp, max_amps;
	int color_comp = get_setting_or("color_compensation", 4);
	
	// Reset color burst amplitude histogram
	memset(amp_histogram, 0, sizeof(amp_histogram));
#endif
	
	for(offset=0; offset < length; offset++) { // loop through data
		// set is_sync, is_transition
		if(samples[offset] <= treshold) { // low
			is_transition = is_sync ? 0 : 1;
			is_sync = 1;
		} else { // high
			is_transition = is_sync ? 1 : 0;
			is_sync = 0;
		}
		
		if(is_transition && is_sync && offset + scanline_w < length) // potential scanline start
			analyze_scanline(samples, offset);
	}
	
	printf("Y: %d - %d (%d)  I: %d - %d (%d)  Q: %d - %d (%d)\n",
		min_Y, max_Y, max_Y - min_Y,
		min_I, max_I, max_I - min_I,
		min_Q, max_Q, max_Q - min_Q);
	
#ifdef COLOR_COMP
	// find the most common color burst amplitude to use in compensation calculations
	color_amp = 0;
	max_amps = amp_histogram[0];
	
	for(offset=1; offset<MAX_AMP; offset++) {
		if(amp_histogram[offset] > max_amps) {
			printf("%3d x color amplitude %4d\n", amp_histogram[offset], offset);
			max_amps = amp_histogram[offset];
			color_amp = offset;
		}
	}

	if(comp_I == NULL)
		comp_I = (int *)malloc(sizeof(int) * (((max_I - min_I) >> SHIFT_I) + 1));
	else
		comp_I = (int *)realloc(comp_I, sizeof(int) * (((max_I - min_I) >> SHIFT_I) + 1));
	
	if(comp_Q == NULL)
		comp_Q = (int *)malloc(sizeof(int) * (((max_Q - min_Q) >> SHIFT_Q) + 1));
	else
		comp_Q = (int *)realloc(comp_Q, sizeof(int) * (((max_Q - min_Q) >> SHIFT_Q) + 1));
#endif // COLOR_COMP

	if(lookup_Y == NULL)
		lookup_Y = (int *)malloc(sizeof(int) * (((max_Y - min_Y) >> SHIFT_Y) + 1));
	else
		lookup_Y = (int *)realloc(lookup_Y, sizeof(int) * (((max_Y - min_Y) >> SHIFT_Y) + 1));
	
	if(lookup_I == NULL)
		lookup_I = (int *)malloc(sizeof(int) * (((max_I - min_I) >> SHIFT_I) * 4 + 4));
	else
		lookup_I = (int *)realloc(lookup_I, sizeof(int) * (((max_I - min_I) >> SHIFT_I) * 4 + 4));
	
	if(lookup_Q == NULL)
		lookup_Q = (int *)malloc(sizeof(int) * (((max_Q - min_Q) >> SHIFT_Q) * 4 + 4));
	else
		lookup_Q = (int *)realloc(lookup_Q, sizeof(int) * (((max_Q - min_Q) >> SHIFT_Q) * 4 + 4));
	
#ifdef COLOR_COMP
	if(lookup_Y == NULL || lookup_I == NULL || lookup_Q == NULL || comp_I == NULL || comp_Q == NULL) {
#else
	if(lookup_Y == NULL || lookup_I == NULL || lookup_Q == NULL) {
#endif
		printf("Couldn't allocate memory for lookup tables (%d / %d / %d bytes)!\n",
		sizeof(int) * (((max_Y - min_Y) >> SHIFT_Y) + 1),
		sizeof(int) * (((max_I - min_I) >> SHIFT_I) * 4 + 4),
		sizeof(int) * (((max_Q - min_Q) >> SHIFT_Q) * 4 + 4));
		quit(-1);
	}
	
	// prepare lookup tables
	float Ymin = (float)get_setting_or("Ymin", 0)/100.0, Ymax = (float)get_setting_or("Ymax", 100)/100.0, 
	      Imin = (float)get_setting_or("Imin", 0)/100.0, Imax = (float)get_setting_or("Imax", 100)/100.0, 
	      Qmin = (float)get_setting_or("Qmin", 0)/100.0, Qmax = (float)get_setting_or("Qmax", 100)/100.0;
		
	for(offset = 0, range = ((max_Y - min_Y) >> SHIFT_Y); offset <= range; offset++) {
		comp = SCALE((float)offset / (float)range, Ymin, Ymax);
		lookup_Y[offset] = (int)(255 * comp);
	}
	
	for(offset = 0, range = ((max_I - min_I) >> SHIFT_I); offset <= range; offset++) {
		comp = SCALE((float)offset / (float)range, Imin, Imax);
		lookup_I[offset * 4 + 0] = (int)(255 *  0.000 * (0.437 * 2 * comp - 0.437)); // R
		lookup_I[offset * 4 + 1] = (int)(255 * -0.394 * (0.437 * 2 * comp - 0.437)); // G
		lookup_I[offset * 4 + 2] = (int)(255 * -2.028 * (0.437 * 2 * comp - 0.437)); // B
#ifdef COLOR_COMP
		comp_I[offset] = (int)(color_amp * color_comp * (2*comp-1)); // Y compensation for I
#endif
	}
	
	for(offset = 0, range = ((max_Q - min_Q) >> SHIFT_Q); offset <= range; offset++) {
		comp = SCALE((float)offset / (float)range, Qmin, Qmax);
		lookup_Q[offset * 4 + 0] = (int)(255 *  1.140 * (0.615 * 2 * comp - 0.615)); // R
		lookup_Q[offset * 4 + 1] = (int)(255 * -0.581 * (0.615 * 2 * comp - 0.615)); // G
		lookup_Q[offset * 4 + 2] = (int)(255 *  0.000 * (0.615 * 2 * comp - 0.615)); // B
#ifdef COLOR_COMP
		comp_Q[offset] = (int)(color_amp * color_comp * (2*comp-1)); // Y compensation for Q
#endif
	}	
}

#define ST_WAIT_NORMAL 0
#define ST_WAIT_BLANK 1
#define ST_COUNT_LONGS 2
#define ST_WAIT_NON_BLANK 3
#define ST_DRAW 4

#ifdef DEBUG
int dumpLine = -1;
#endif

void extract_color(Uint32 *buffer, short *samples, int dump) {
	int adj, bestAdj = 0, sum, bestSum = -1000000000; // color waveform adjustment, best fit
	int start, end, count, Y, I, Q, run_I, run_Q, r, g, b;
	
	start_timeprofile(TP_SCANLINE);
	
	for(adj = 0; adj < i_wavelength; adj++) { // it's not least squares, but maximum products :)
		sum = 0;
		
		for(count = color_burst_start; count < color_burst_start + color_burst_len; count++)
			sum += (color_wave1[count - adj] * samples[count]) >> 4; // drop a bit of accuracy
		
		if(sum > bestSum) {
			bestAdj = adj;
			bestSum = sum;
		}
	}
	
	// to avoid accessing values beyond the scanline, we'll start a bit before color burst end
	run_I = run_Q = 0;
	
	start = MAX(color_burst_start, MAX(crop_left, wave_before)); // start as late as possible
	end = MIN(scanline_w - wave_after, crop_left + copy_width); // end as early as possible
	
	// count initial running average values
	for(count = start - wave_before; count < start + wave_after; count++) {
		run_I += samples[count] * color_wave1[count - bestAdj];
		run_Q += samples[count] * color_wave2[count - bestAdj];
	}
	
	start_timeprofile(TP_BURST);
	
#ifdef DEBUG	
	FILE * out;
	
	if(dump) {
		out = fopen("line.csv", "wt");
		fprintf(out, "count;sample;run_I;run_Q;Y;I;Q;c1;c2;r;g;b\n");
		fprintf(out, "min;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d\n", -wave_before, run_I, run_Q, min_Y, min_I>>SHIFT_I, min_Q>>SHIFT_Q, -1, -1, 0, 0, 0);
		fprintf(out, "max;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d\n", wave_after, run_I, run_Q, max_Y, max_I>>SHIFT_I, max_Q>>SHIFT_Q, -1, -1, 0, 0, 0);
	}
#endif

	// color components are estimated using running averages
	for(count = start; count < end; count++) {	
		run_I += samples[count + wave_after] * color_wave1[count + wave_after - bestAdj];
		run_Q += samples[count + wave_after] * color_wave2[count + wave_after - bestAdj];
				
		Y = samples[count];
		I = (MAX(MIN(run_I, max_I), min_I) - min_I) >> SHIFT_I;
		Q = (MAX(MIN(run_Q, max_Q), min_Q) - min_Q) >> SHIFT_Q;

#ifdef COLOR_COMP
		Y -= (color_wave1[count - bestAdj] * comp_I[I] + color_wave2[count - bestAdj] * comp_Q[Q]) >> SHIFT_WAVE;
#endif

		Y = (MAX(MIN(Y, max_Y), min_Y) - min_Y) >> SHIFT_Y; // scale Y
					
		I <<= 2;
		Q <<= 2;
		
		r = lookup_Y[Y] + lookup_I[I + 0] + lookup_Q[Q + 0];
		g = lookup_Y[Y] + lookup_I[I + 1] + lookup_Q[Q + 1];
		b = lookup_Y[Y] + lookup_I[I + 2] + lookup_Q[Q + 2];

		r = MAX(MIN(r, 255), 0);
		g = MAX(MIN(g, 255), 0);
		b = MAX(MIN(b, 255), 0);
		
#ifdef DEBUG
		if(dump) {
			fprintf(out, "%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d\n", count, samples[count], run_I, run_Q, 
				Y, I>>2, Q>>2, color_wave1[count - bestAdj], color_wave2[count - bestAdj], r, g, b);
			buffer[count] = 0xFFFFFF;
		} else
#endif
		buffer[count] = CALC_RGB(r, g, b);
				
		run_I -= samples[count - wave_before] * color_wave1[count - wave_before - bestAdj];
		run_Q -= samples[count - wave_before] * color_wave2[count - wave_before - bestAdj];
	}	
	
#ifdef DEBUG
	if(dump) {
		fclose(out);
	}
#endif

	end_timeprofile(TP_BURST);
	end_timeprofile(TP_SCANLINE);	
}

void extract_bw(Uint32 *buffer, short *samples, int dump) {
	int start, end, count, Y;
	
	start_timeprofile(TP_SCANLINE);
		
	start = MAX(color_burst_start, MAX(crop_left, wave_before)); // start as late as possible
	end = MIN(scanline_w - wave_after, crop_left + copy_width); // end as early as possible
	
	start_timeprofile(TP_BURST);

	// color components are estimated using running averages
	for(count = start; count < end; count++) {	
		Y = samples[count];

#ifdef COLOR_COMP
		Y -= (color_wave1[count - bestAdj] * comp_I[I] + color_wave2[count - bestAdj] * comp_Q[Q]) >> SHIFT_WAVE;
#endif

		Y = (MAX(MIN(Y, max_Y), min_Y) - min_Y) >> SHIFT_Y; // scale Y
				
		Y = MAX(MIN(lookup_Y[Y], 255), 0);

		buffer[count] = CALC_RGB(Y, Y, Y);
	}	

	end_timeprofile(TP_BURST);
	end_timeprofile(TP_SCANLINE);	
}

// Extract NTSC field from samples, and determine if it's partial, first, or second field
// returns the amount of samples processed
// sets fieldtype to -1 for partial field, 0 for first field and 1 for second field
int extract_field(SDL_Surface *surface, short * samples, int length, int *field_type, void (*extract_func)(Uint32 *, short *, int)) {
	Uint32 *buffer;
	
	int line = 0, offset, scanline_start = 0, longs = 0;
	int state = ST_WAIT_NORMAL;
	int is_sync = 0, count = 0, is_transition = 0;

	if(min_I == max_I || min_Q == max_Q)
		return -1; // TODO: Run B/W if no I/Q variance
	
	if ( SDL_LockSurface(surface) < 0 ) {
		fprintf(stderr, "Couldn't lock the display surface: %s\n",
				SDL_GetError());
		quit(2);
	}
	
	buffer=(Uint32 *)surface->pixels;
		
	// loop through data
	for(offset=0; offset < length; offset++) {
		// set is_sync, is_transition
		if(samples[offset] <= treshold) { // low
			is_transition = is_sync ? 0 : 1;
			is_sync = 1;
		} else { // high
			is_transition = is_sync ? 1 : 0;
			is_sync = 0;
		}
		
		if(!is_transition) { // only handle state changes on transitions
			count++;
			continue;
		}
			
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
					if(line < 252) { // not enough scanlines - partial field
						SDL_UnlockSurface(surface);
						*field_type = -1;
						return offset;
					} else {
						SDL_UnlockSurface(surface);
						*field_type = (longs == 7) ? 0 : 1; // determine field number
						return offset;
					}
				} else { // next scanline
					line++;
					scanline_start = offset;
				}
			} else { // end hsync
				if(line >= crop_top && line < 252-crop_bottom && scanline_start + scanline_w < length)
#ifndef DEBUG
					extract_func(buffer + line * surface->pitch/4, samples + scanline_start, 0);
#else
					extract_func(buffer + line * surface->pitch/4, samples + scanline_start, line == dumpLine);
				
				if(line == dumpLine)
					dumpLine = -1; // mark the line as printed
#endif
			}
			break;
		}
		count = 0;
	}
				
	SDL_UnlockSurface(surface);
	*field_type = -1;
	
	return offset; // data ran out
}

int main(int argc, char *argv[]) {
	SDL_Surface *screen, *field = NULL;
	int done = 0, field_num = -1, field_count, scale_x, scale_y, blur = 0, sample = 1, bw = 0;
	SDL_Event event;

	short handle, *buffer, overflow;
	long timeInterval, samples, gotSamples;
	unsigned long timebase;
	int i, first_run = 1;
	char inifile[80];
				
	// profiling
	init_timeprofiles();
	
	// with timebase > 2, sampling interval = (timebase - 2) * 16 ns
	if(argc > 1) {
		strcpy(inifile, argv[1]);
		strcat(inifile, ".ini");
		load_settings(inifile);
	} else
		load_settings("color.ini");
	
	int adj_crop_left = get_setting_or("crop_left", 0), adj_crop_right = get_setting_or("crop_right", 0);
		
	timebase = get_setting_or("timebase", 6); // 64 ns gets us about 1000 samples per scan line
	scale_x = get_setting_or("scale_x", 0);
	scale_y = get_setting_or("scale_y", 0);
	
	switch(timebase) {
	case 0: timeInterval = 2; break;
	case 1: timeInterval = 4; break;
	case 2: timeInterval = 8; break;
	default: timeInterval = 16 * (timebase - 2);
	}
	
	samples = (64000/timeInterval) * 3/2*525; // We'll need 1.5 frames long buffer to ensure two whole fields
	
	handle = init_ps3000(timebase, samples, &timeInterval);
	if(handle == -1) {
		printf("Could not initialize the scope! Exiting...\n");
		return -1;
	}
	
	// calculate parameters
	calculate_parameters(timeInterval);
	
	if(SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "Couldn't initialize SDL: %s\n",SDL_GetError());
		return(1);
	}

	// initialize screen size based on need
	if((screen=SDL_SetVideoMode(copy_width >> (-scale_x), 2 * (252 - crop_top - crop_bottom), 
			32, SDL_SWSURFACE)) == NULL ) {
		fprintf(stderr, "Couldn't set video mode: %s\n", SDL_GetError());
		quit(2);
	}
	printf("Set window size to %d x %d\n", copy_width >> (-scale_x), 2 * (252 - crop_top - crop_bottom));
	
	SDL_WM_SetCaption("PS3000 Composite Video Decoder", "PS3000 Composite...");
		
	buffer = (short *)malloc(sizeof(short) * samples);
	if(buffer == NULL) {
		printf("Ran out of memory while allocating %ld sample buffer\n", samples);
		deinit_ps3000(handle);
		return -1;
	}
	
	// allocate space for field buffer
	field = SDL_CreateRGBSurface(SDL_SWSURFACE, scanline_w, 252,
		32, 0xFF0000, 0xFF00, 0xFF, 0);
	
	// calculate reference color waveforms
	color_wave1 = (int *)malloc(sizeof(int) * scanline_w * 2);
	color_wave2 = (int *)malloc(sizeof(int) * scanline_w * 2);
	if(color_wave1 == NULL || color_wave2 == NULL) {
		printf("Could not allocate color buffers!");
		quit(2);
	}
	
	for(i = 0; i < scanline_w * 2; i++) {
		color_wave1[i] = (int)(MUL_WAVE * sin(2.0 * M_PI * (float)i / f_wavelength));
		color_wave2[i] = (int)(MUL_WAVE * sin(2.0 * M_PI * (float)i / f_wavelength - M_PI / 2.0));
	}
	
	while(!done) {
		start_timeprofile(TP_FRAME);
		
		start_timeprofile(TP_PICO);
		gotSamples = capture_ps3000(handle, buffer, samples, timebase, &overflow);
		end_timeprofile(TP_PICO);
		
		if(gotSamples == samples && !overflow) {
			if(first_run) {
				analyze_samples(buffer, samples);				
				first_run = 0;
			}
			
			field_count = 0;
			for(i=0; i<samples && field_count < 2;) {
				// skip a bit back for consecutive frames to allow VSYNC detection
				if(i > 2 * scanline_w)
					i -= 2 * scanline_w;
				
				start_timeprofile(TP_FIELD);
				i += extract_field(field, buffer+i, samples-i, &field_num, bw ? &extract_bw : &extract_color);
				end_timeprofile(TP_FIELD);
				
				if(field_num != -1) {
					start_timeprofile(TP_DRAW);
					draw_screen(screen, field, field_num, scale_x, scale_y, blur, sample);
					end_timeprofile(TP_DRAW);
					field_count++; // field successfully drawn
				}
			}
			
			update_screen(screen);
		} else {
			printf("Failed capture\n");
		}
		
		while(SDL_PollEvent(&event)) {
			switch(event.type) {
			case SDL_MOUSEBUTTONDOWN:
#ifdef DEBUG
				if(event.button.button == SDL_BUTTON_LEFT)
					dumpLine = (event.button.y >> scale_y) / 2 + crop_top;
#endif
				break;
				
			case SDL_KEYDOWN:
				switch(event.key.keysym.scancode) {
				case 57: // space
					bw = !bw;
					break;
				case 28: // enter
					first_run = 1; // reinitialize values based on current display
					break;
				case 75: // left
					if(scale_x > MIN_SCALE_X)
						scale_x--;
					clear_surface(field);
					clear_surface(screen);
					break;
				case 77: // right
					if(scale_x < MAX_SCALE_X)
						scale_x++;
					clear_surface(field);
					clear_surface(screen);
					break;
				case 72: // up
					if(scale_y > MIN_SCALE_Y)
						scale_y--;
					clear_surface(field);
					clear_surface(screen);
					break;
				case 80: // down
					if(scale_y < MAX_SCALE_Y)
						scale_y++;
					clear_surface(field);
					clear_surface(screen);
					break;
				case 78: // +
					if(event.key.keysym.mod & KMOD_SHIFT) {
						adj_crop_right++;
						set_setting("crop_right", adj_crop_right);
					} else {
						adj_crop_left++;
						set_setting("crop_left", adj_crop_left);
					}
					calculate_parameters(timeInterval);
					clear_surface(field);
					clear_surface(screen);
					break;
				case 74: // -
					if(event.key.keysym.mod & KMOD_SHIFT) {
						if(adj_crop_right > 0)
							adj_crop_right--;
						set_setting("crop_right", adj_crop_right);
					} else {
						if(adj_crop_left > 0)
							adj_crop_left--;
						set_setting("crop_left", adj_crop_left);
					}
					calculate_parameters(timeInterval);
					clear_surface(field);
					clear_surface(screen);
					break;
				case 2: case 3: case 4: // 1, 2, 3
					blur = event.key.keysym.scancode - 2;
					break;
				case 16: case 17: case 18: // q, w, e
					sample = event.key.keysym.scancode - 15;
					break;
				default:
					printf("No function for key %d\n", event.key.keysym.scancode);
				}				
				break;
			case SDL_QUIT:
				done = 1;
				break;
			}
		}
		
		end_timeprofile(TP_FRAME);
	}
	
	printf("\n\n");
	print_timeprofile("Frames", TP_FRAME);
	printf("\n\n");
	compare_timeprofiles("Picoscope", TP_PICO, "Frames", TP_FRAME);
	compare_timeprofiles("Field", TP_FIELD, "Frames", TP_FRAME);
	compare_timeprofiles("Draw", TP_DRAW, "Frames", TP_FRAME);
	printf("\n\n");
	compare_timeprofiles("Scanline", TP_SCANLINE, "Field", TP_FIELD);
	compare_timeprofiles("Color burst", TP_BURST, "Scanline", TP_SCANLINE);
	
	deinit_ps3000(handle);
	
	SDL_FreeSurface(field);
	free(color_wave1);
	free(color_wave2);
	
	SDL_Quit();
		
	return(0);
}
