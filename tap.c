/* TAP shared library: a library for converting audio data (as a stream of
 * PCM signed 32-bit samples, mono) to raw TAP data and back
 *
 * TAP specification was written by Per Håkan Sundell and is available at
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

#define OVERFLOW_VALUE (TAP_NO_MORE_SAMPLES - 1)

struct tap_t{
  u_int32_t increasing;
  u_int32_t prev_increasing;
  u_int32_t input_pos;
  u_int32_t min_duration;
  int32_t min_height;
  int32_t *buffer, *bufend, val, prev_val, max_val, min_val;
  u_int32_t max, min, prev_max, prev_min, trigger_pos;
  int inverted;
  u_int8_t machine, videotype;
  u_int32_t to_be_consumed, this_pulse_len;
  float factor;
  u_int32_t overflow_samples;
  u_int32_t samples_still_to_process;
  u_int8_t return_zero_next_time;
  u_int8_t flush_called;
  u_int8_t sensitivity;
};

const float tap_clocks[][2]={
  {985248,1022727}, /* C64 */
  {1108405,1022727}, /* VIC */
  {886724,894886}  /* C16 */
};

static void set_factor(struct tap_t *tap, u_int32_t freq){
  tap->factor = tap_clocks[tap->machine][tap->videotype]/(float)freq;
  tap->overflow_samples = OVERFLOW_VALUE/tap->factor - 10;

  /*overcome imprecision of float arithmetics*/
  do
  {
    tap->overflow_samples++;
  }while (tap->overflow_samples*tap->factor < OVERFLOW_VALUE);
}

struct tap_t *tap_fromaudio_init(u_int32_t infreq, u_int32_t min_duration, u_int32_t min_height, int inverted){
  return tap_fromaudio_init_with_machine(infreq, min_duration, min_height, inverted, TAP_MACHINE_C64, TAP_VIDEOTYPE_PAL);
}

struct tap_t *tap_fromaudio_init_with_machine(u_int32_t infreq, u_int32_t min_duration, u_int32_t min_height, int inverted, u_int8_t machine, u_int8_t videotype){
  struct tap_t *tap;

  if (infreq==0) return NULL;
  tap=malloc(sizeof(struct tap_t));
  if (tap==NULL) return NULL;
  tap->increasing=0;
  tap->input_pos=0;
  tap->min_duration=min_duration;
  tap->min_height=0;
  tap->buffer=NULL;
  tap->bufend=NULL;
  tap->val=0;
  tap->max=0;
  tap->min=0;
  tap->prev_max=0;
  tap->prev_min=0;
  tap->trigger_pos=1;
  tap->max_val=-(20<<24);
  tap->min_val=-(20<<24);
  tap->inverted=inverted;
  tap->machine=machine;
  tap->videotype=videotype;
  tap->samples_still_to_process=0;
  tap->return_zero_next_time=0;
  tap->flush_called=0;
  tap->sensitivity=min_height > 100 ? 100 : min_height;
  set_factor(tap, infreq);
  return tap;
}

struct tap_t *tap_toaudio_init(u_int32_t outfreq, int32_t volume, int inverted){
  return tap_toaudio_init_with_machine(outfreq, volume, inverted, TAP_MACHINE_C64, TAP_VIDEOTYPE_PAL);
}

struct tap_t *tap_toaudio_init_with_machine(u_int32_t outfreq, int32_t volume, int inverted, u_int8_t machine, u_int8_t videotype){
  struct tap_t *tap;

  if (outfreq==0) return NULL;
  if (volume <= 0) return NULL;
  tap=malloc(sizeof(struct tap_t));
  if (tap==NULL) return NULL;
  tap->to_be_consumed=0;
  tap->this_pulse_len=0;
  tap->val=volume;
  tap->inverted=inverted;
  tap->machine=machine;
  tap->videotype=videotype;
  set_factor(tap, outfreq);
  return tap;
}

void tap_set_buffer(struct tap_t *tap, int32_t *buf, int len){
  tap->buffer=buf;
  tap->bufend=buf+len;
}

int tap_set_machine(struct tap_t *tap, u_int8_t machine, u_int8_t videotype){
  /* deprecated */
  return TAP_INVALID;
}

u_int32_t tap_get_pulse(struct tap_t *tap){
  while(1){
    if(tap->samples_still_to_process)
    {
      if(tap->samples_still_to_process > tap->overflow_samples){
        tap->samples_still_to_process -= tap->overflow_samples;
        return OVERFLOW_VALUE;
      }
      else if(tap->samples_still_to_process == tap->overflow_samples){
        if(tap->return_zero_next_time){
          tap->return_zero_next_time = 0;
          tap->samples_still_to_process = 0;
          return 0;
        }
        else{
          tap->return_zero_next_time = 1;
          return OVERFLOW_VALUE;
        }
      }
      else{
        u_int32_t samples_to_process_now = tap->samples_still_to_process;
        tap->samples_still_to_process = 0;
        return samples_to_process_now * tap->factor;
      }
    }

    if(!(tap->buffer<tap->bufend)){
      if(!tap->flush_called)
        return TAP_NO_MORE_SAMPLES;
      tap->samples_still_to_process = tap->input_pos - tap->trigger_pos;
      tap->flush_called=0;
      continue;
    }

    tap->prev_val = tap->val;
    tap->val = *tap->buffer++;

    tap->prev_increasing = tap->increasing;
    if (tap->val == tap->prev_val) tap->increasing = tap->prev_increasing;
    else tap->increasing = (tap->val > tap->prev_val);

    if (tap->increasing != tap->prev_increasing) /* A min or max has been reached. Is it a true one? */
    {
      if (tap->increasing
       && tap->input_pos - tap->max > tap->min_duration
       && tap->min_height > tap->prev_val){ /* A minimum */
        if(!tap->inverted
         && tap->min>0
         && tap->max>tap->min
          ){
          u_int32_t trigger_pos=(tap->min + tap->max)/2;
          tap->samples_still_to_process = trigger_pos - tap->trigger_pos;
          tap->trigger_pos=trigger_pos;
        }
        if(tap->max>tap->min)
          tap->prev_min = tap->min;
        if(tap->max>tap->min || tap->min_val > tap->prev_val){
          tap->min = tap->input_pos;
          tap->min_val = tap->prev_val;
          tap->min_height = tap->max>0 ?
            tap->min_val/200*(100+tap->sensitivity) + tap->max_val/200*(100-tap->sensitivity)
            : 20<<24;
        }
      }
      else if (!tap->increasing
             && tap->input_pos - tap->min > tap->min_duration
             && tap->prev_val > tap->min_height){ /* A maximum */
        if(tap->inverted
         && tap->max>0
         && tap->min>tap->max
          ){
          u_int32_t trigger_pos=(tap->min + tap->max)/2;
          tap->samples_still_to_process = trigger_pos - tap->trigger_pos;
          tap->trigger_pos=trigger_pos;
        }
        if(tap->min>tap->max)
          tap->prev_max = tap->max;
        if(tap->min>tap->max || tap->prev_val > tap->max_val){
          tap->max = tap->input_pos;
          tap->max_val = tap->prev_val;
          tap->min_height = tap->min>0 ?
            tap->min_val/200*(100-tap->sensitivity) + tap->max_val/200*(100+tap->sensitivity)
            : -20<<24;
        }
      }
    }
    tap->input_pos++;
  }
}

int tap_get_pos(struct tap_t *tap){
  return tap->input_pos - 2;
}

u_int32_t tap_flush(struct tap_t *tap){
  tap->flush_called = 1;
  if(
     ( tap->inverted && tap->min>tap->max)
  || (!tap->inverted && tap->min<tap->max)
    ){
    u_int32_t trigger_pos=(tap->min + tap->max)/2;
    tap->samples_still_to_process = trigger_pos - tap->trigger_pos;
    tap->trigger_pos=trigger_pos;
  }
  return 0;
}

int32_t tap_get_max(struct tap_t *tap){
  return tap->max_val;
}

void tap_set_pulse(struct tap_t *tap, u_int32_t pulse){
  tap->this_pulse_len=
    tap->to_be_consumed=
    (pulse/(float)tap->factor);
}

static int32_t tap_get_squarewave_val(u_int32_t this_pulse_len, u_int32_t to_be_consumed, int32_t volume){
  if (to_be_consumed > this_pulse_len/2)
    return volume;
  return -volume;
}

static int32_t tap_get_sawtooth_val(u_int32_t this_pulse_len, u_int32_t to_be_consumed, int32_t volume){
  /* Double cast! Don't ask. OK, it has something to do with signedness and something with
     multiplication giving results too large to fit in 32 bits */
  return volume*(int64_t)(int32_t)(2*to_be_consumed-this_pulse_len-1)/(this_pulse_len-1);
}

u_int32_t tap_get_buffer(struct tap_t *tap, int32_t *buffer, unsigned int buflen){
  int samples_done = 0;

  while(buflen > 0 && tap->to_be_consumed > 0){
    *buffer++ = tap_get_sawtooth_val(tap->this_pulse_len, tap->to_be_consumed--, tap->val)*(tap->inverted ? -1 : 1);
    samples_done++;
    buflen--;
  }

  return samples_done;
}

void tap_exit(struct tap_t *tap){
  free(tap);
}
