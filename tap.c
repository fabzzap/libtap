/* TAP shared library: a library for converting audio data (as a stream of
 * PCM signed 32-bit samples, mono) to raw TAP data and back
 *
 * TAP specification was written by Per H�kan Sundell and is available at
 * http://www.computerbrains.com/tapformat.html
 *
 * The algorithm for TAP encoding was originally written by Janne Veli
 * Kujala, based on the one written by Andreas Matthies for Tape64.
 * Some modifications and adaptation to library format by Fabrizio Gennari
 *
 * The algorithm to decode TAP data into square waves is by Fabrizio Gennari.
 * The choice to use square waves has been inspired by 64TAPVOC by Tomaz Kac,
 * and confirmed by discussions with Luigi Di Fraia
 *
 * The clocks were taken from mtap by Markus Brenner
 *
 * Copyright (c) Fabrizio Gennari, 2003
 *
 * The program is distributed under the GNU Lesser General Public License.
 * See file LESSER-LICENSE.TXT for details.
 */

#include <stddef.h>
#include <malloc.h>
#include <sys/types.h>
#include "tap.h"

#define MIN(a,b) ( (a) < (b) ? (a) : (b) )

struct tap_t{
  u_int32_t increasing;
  u_int32_t prev_increasing;
  u_int32_t input_pos;
  u_int32_t freq;
  u_int32_t min_duration;
  u_int32_t min_height;
  int32_t *buffer, *bufstart, *bufend, val, prev_val, max_val, min_val;
  u_int32_t max, min;
  int inverted;
  u_int8_t machine, videotype;
  u_int32_t to_be_consumed, this_pulse_len;
};

const float tap_clocks[][2]={
  {985248,1022727}, /* C64 */
  {1108405,1022727}, /* VIC */
  {886724,894886}  /* C16 */
};

struct tap_t *tap_fromaudio_init(u_int32_t infreq, u_int32_t min_duration, u_int32_t min_height, int inverted){
  struct tap_t *tap;

  if (infreq==0) return NULL;
  tap=malloc(sizeof(struct tap_t));
  if (tap==NULL) return NULL;
  tap->prev_increasing=0;
  tap->input_pos=0;
  tap->freq=infreq;
  tap->min_duration=min_duration;
  tap->min_height=min_height;
  tap->buffer=NULL;
  tap->bufstart=NULL;
  tap->bufend=NULL;
  tap->prev_val=0;
  tap->val=0;
  tap->max=0;
  tap->min=0;
  tap->max_val=2147483647;
  tap->min_val=-2147483647;
  tap->inverted=inverted;
  tap->machine=TAP_MACHINE_C64;
  tap->videotype=TAP_VIDEOTYPE_PAL;
  return tap;
}

struct tap_t *tap_toaudio_init(u_int32_t outfreq, int32_t volume, int inverted){
  struct tap_t *tap;

  if (outfreq==0) return NULL;
  if (volume <= 0) return NULL;
  tap=malloc(sizeof(struct tap_t));
  if (tap==NULL) return NULL;
  tap->freq=outfreq;
  tap->to_be_consumed=0;
  tap->this_pulse_len=0;
  tap->val=volume;
  tap->inverted=inverted;
  tap->machine=TAP_MACHINE_C64;
  tap->videotype=TAP_VIDEOTYPE_PAL;
  return tap;
}

void tap_set_buffer(struct tap_t *tap, int32_t *buf, int len){
  tap->buffer=tap->bufstart=buf;
  tap->bufend=buf+len;
}

int tap_set_machine(struct tap_t *tap, u_int8_t machine, u_int8_t videotype){
  if ((machine>TAP_MACHINE_MAX)||(videotype>TAP_VIDEOTYPE_MAX))
    return TAP_INVALID;
  tap->machine=machine;
  tap->videotype=videotype;
  return TAP_OK;
}

u_int32_t tap_get_pulse(struct tap_t *tap/*, struct tap_pulse *pulse*/){
  unsigned long found_pulse;

  while(tap->buffer<tap->bufend){
    tap->prev_val = tap->val;
    tap->val = *tap->buffer++;
    if (tap->inverted) tap->val=~tap->val;
    tap->input_pos++;

    tap->prev_increasing = tap->increasing;
    if (tap->val == tap->prev_val) tap->increasing = tap->prev_increasing;
    else tap->increasing = (tap->val > tap->prev_val);

    if (tap->increasing != tap->prev_increasing) /* A min or max has been reached. Is it a true one? */
      {
	  if (tap->increasing
	      && tap->max >= tap->min
	      && tap->input_pos - tap->max > tap->min_duration
	      && tap->max_val > tap->prev_val
	      && (u_int32_t)(tap->max_val-tap->prev_val) > tap->min_height){ /* A minimum */
	    tap->min = tap->input_pos;
	    tap->min_val = tap->prev_val;
	  }
	  else if (!tap->increasing
		   && tap->min >= tap->max
		   && tap->input_pos - tap->min > tap->min_duration
		   && tap->prev_val > tap->min_val
		   && (u_int32_t)(tap->prev_val-tap->min_val) > tap->min_height){ /* A maximum */
	    tap->max_val = tap->prev_val;
	    found_pulse = (float)(tap->input_pos - tap->max)*tap_clocks[tap->machine][tap->videotype]/(float)tap->freq;
	    tap->max = tap->input_pos;
	    return (found_pulse < 1<<24 ? found_pulse :1<<24-1);
/*	    pulse->version_1[0]=0;
	    pulse->version_1[1]= found_pulse     &0xff;
	    pulse->version_1[2]=(found_pulse>> 8)&0xff;
	    pulse->version_1[3]=(found_pulse>>16)&0xff;
	    pulse->version_0=(found_pulse<256*8 ? found_pulse/8:0);
	    return TAP_OK;*/
	  }
    }
  }
  return 1<<24;
}

int tap_get_pos(struct tap_t *tap){
  return tap->input_pos - 2;
}

int32_t tap_get_max(struct tap_t *tap){
	return tap->max_val;
}

void tap_set_pulse(struct tap_t *tap, u_int32_t pulse){
  if (tap->val < 0)
    tap->val = -tap->val;
  tap->this_pulse_len=
    tap->to_be_consumed=
    pulse*(float)tap->freq/(float)tap_clocks[tap->machine][tap->videotype];
}

static int32_t tap_get_squarewave_val(u_int32_t this_pulse_len, u_int32_t to_be_consumed, int32_t volume){
	if (to_be_consumed > this_pulse_len/2)
		return volume;
	return -volume;
}

u_int32_t tap_get_buffer(struct tap_t *tap, int32_t *buffer, unsigned int buflen){
	int /*samples_now, i,*/ samples_done = 0;

	/*while(buflen){
		samples_now=MIN(buflen, tap->to_be_consumed);
		for(i=0; i < samples_now; i++)*/
	while(buflen > 0 && tap->to_be_consumed > 0){
		*buffer++ = tap_get_squarewave_val(tap->this_pulse_len, tap->to_be_consumed, tap->val)*(tap->inverted ? -1 : 1);
		samples_done += 1;
		tap->to_be_consumed -= 1;
		buflen -= 1;
	}
/*	if (tap->to_be_consumed == 0){
		if (tap->val > 0){
			tap->val = -tap->val;
			tap->to_be_consumed = tap->this_pulse_len;
		}
		else
			break;
	}*/

	return samples_done;
}

void tap_exit(struct tap_t *tap){
	free(tap);
}
