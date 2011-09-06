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
 * Copyright (c) Fabrizio Gennari, 2003-2011
 *
 * The program is distributed under the GNU Lesser General Public License.
 * See file LESSER-LICENSE.TXT for details.
 */

#include <stdlib.h>
#include "tapencoder.h"

struct anomalies{
  int32_t resolution_level;
  uint32_t pos;
  uint8_t rising;
};

enum tap_trigger {
  TAP_TRIGGER_ON_RISING_EDGE,
  TAP_TRIGGER_ON_FALLING_EDGE,
  TAP_TRIGGER_ON_BOTH_EDGES
};

struct tap_enc_t{
  uint32_t increasing;
  uint32_t prev_increasing;
  uint32_t input_pos;
  uint32_t min_duration;
  int32_t min_height;
  int32_t trigger_level;
  int32_t val, prev_val, max_val, min_val;
  uint32_t max, min, prev_max, prev_min, trigger_pos;
  uint8_t sensitivity;
  enum tap_trigger trigger_type;
  uint8_t initial_threshold;
  uint8_t triggered, cached_trigger;
  uint8_t start_with_positive_semiwave;
  struct anomalies *anomaly, *old_anomaly;
};

static void reset_state(struct tap_enc_t *tap){
  tap->increasing=0;
  tap->input_pos=0;
  /* When creating semiwaves, the first trigger must be after the first max,
     else the TAP won't work with VICE. This ensures that the first min
     comes after the first max */
  tap->min_height = tap->trigger_type != TAP_TRIGGER_ON_BOTH_EDGES ? 0 :
                    tap->start_with_positive_semiwave ? 1<<31 : (~(1<<31));
  tap->val=0;
  tap->max=0;
  tap->min=0;
  tap->prev_max=0;
  tap->prev_min=0;
  tap->trigger_pos=0;
  tap->max_val= (tap->initial_threshold<<24);
  tap->min_val=-(tap->initial_threshold<<24);
  tap->triggered = 1;
  tap->cached_trigger = 0;
  tap->trigger_level = 0;
  free(tap->anomaly);
  free(tap->old_anomaly);
  tap->anomaly = NULL;
  tap->old_anomaly = NULL;
}

static uint32_t set_trigger(uint32_t trigger_pos
                          ,uint32_t *stored_trigger_pos
                          ,uint8_t rising
                          ,enum tap_trigger trigger_type) {
  if(
    (!rising && trigger_type != TAP_TRIGGER_ON_RISING_EDGE)
 || ( rising && trigger_type != TAP_TRIGGER_ON_FALLING_EDGE)
    ){
    uint32_t return_value = trigger_pos - *stored_trigger_pos;
    *stored_trigger_pos = trigger_pos;
    return return_value;
  }
  return 0;
}

struct tap_enc_t *tapencoder_init(uint32_t min_duration, uint8_t sensitivity, uint8_t initial_threshold, uint8_t inverted, uint8_t semiwaves){
  struct tap_enc_t *tap;

  tap=malloc(sizeof(struct tap_enc_t));
  if (tap==NULL) return NULL;
  tap->min_duration=min_duration;
  tap->initial_threshold = initial_threshold > 127 ? 127 : initial_threshold;
  tap->trigger_type=semiwaves ? TAP_TRIGGER_ON_BOTH_EDGES
                  : inverted ? TAP_TRIGGER_ON_FALLING_EDGE
                  : TAP_TRIGGER_ON_RISING_EDGE;
  if(semiwaves)
    tap->start_with_positive_semiwave = !inverted;
  tap->sensitivity=sensitivity > 100 ? 100 : sensitivity;
  tap->anomaly = NULL;
  tap->old_anomaly = NULL;

  reset_state(tap);
  return tap;
}

static void set_anomaly(struct tap_enc_t *tap, uint32_t prev_minmax, uint8_t rising){
  struct anomalies *anomaly = malloc(sizeof(struct anomalies));

  anomaly->resolution_level = tap->trigger_level;
  anomaly->pos = tap->input_pos / 2 + prev_minmax / 2
    /* avoid rounding errors when both are odd */
    + ((tap->input_pos & 1) & (prev_minmax & 1));
  anomaly->rising = rising;

  if(tap->anomaly != NULL)
    tap->old_anomaly = tap->anomaly;
  tap->anomaly = anomaly;
}

uint32_t tapenc_get_pulse(struct tap_enc_t *tap, int32_t *buffer, unsigned int buflen, uint32_t *pulse){
  uint32_t samples_done = 0;

  *pulse = 0;

  while(*pulse == 0){
    if(tap->cached_trigger){
      if(tap->anomaly){
        *pulse = set_trigger(tap->anomaly->pos, &tap->trigger_pos, tap->anomaly->rising, tap->trigger_type);
        free(tap->anomaly);
        tap->anomaly = NULL;
      }
      else{
        *pulse = set_trigger(tap->input_pos - 1, &tap->trigger_pos, tap->min > tap->max, tap->trigger_type);
        tap->cached_trigger = 0;
      }
      if (*pulse)
        break;
    }
    if(samples_done >= buflen)
      break;

    tap->prev_val = tap->val;
    tap->val = buffer[samples_done++];

    tap->prev_increasing = tap->increasing;
    if (tap->val > tap->prev_val)
      tap->increasing = 1;
    else if (tap->val < tap->prev_val)
      tap->increasing = 0;

    if (tap->increasing != tap->prev_increasing) /* A min or max has been reached. Is it a true one? */
    {
      if (tap->increasing
       && (tap->input_pos - tap->max > tap->min_duration || tap->triggered)
       && tap->min_height > tap->prev_val
       && (tap->max>tap->min || tap->min_val > tap->prev_val) ){ /* A minimum */
        if (tap->max >= tap->min) {
          if (tap->triggered == 0) 
            set_anomaly(tap, tap->max, 0);
          else
            tap->triggered = 0;
          tap->prev_min = tap->min;
          tap->min = tap->input_pos;
        }
        tap->trigger_level = tap->max > 0 ?
          tap->prev_val / 2 + tap->max_val / 2
          : 0;
        tap->min_val = tap->prev_val;
        tap->min_height = tap->max>0 ?
        tap->min_val/200*(100+tap->sensitivity) + tap->max_val/200*(100-tap->sensitivity)
            : tap->initial_threshold<<24;
      }
      else if (!tap->increasing
             && (tap->input_pos - tap->min > tap->min_duration || tap->triggered)
             && tap->prev_val > tap->min_height
             && (tap->min>tap->max || tap->prev_val > tap->max_val) ){ /* A maximum */
        if (tap->min >= tap->max) {
          if (tap->triggered == 0)
            set_anomaly(tap, tap->min, 1);
          else
            tap->triggered = 0;
          tap->prev_max = tap->max;
          tap->max = tap->input_pos;
        }
        tap->trigger_level =  tap->min > 0 ?
          tap->prev_val / 2 + tap->min_val / 2
          : 0;
        tap->max_val = tap->prev_val;
        tap->min_height = tap->min>0 ?
        tap->min_val/200*(100-tap->sensitivity) + tap->max_val/200*(100+tap->sensitivity)
            : -tap->initial_threshold<<24;
      }
    }
    if(!tap->triggered && tap->anomaly != NULL){
      if(tap->min > tap->max
      && tap->val < tap->anomaly->resolution_level){
        free(tap->anomaly);
        tap->anomaly = NULL;
        tap->triggered = 1;
        tap->cached_trigger = 1;
        tap->min = tap->prev_min;
      }
      else if (tap->min < tap->max
            && tap->val > tap->anomaly->resolution_level){
        free(tap->anomaly);
        tap->anomaly = NULL;
        tap->triggered = 1;
        tap->cached_trigger = 1;
        tap->max = tap->prev_max;
      }
    }
    if(!tap->triggered && 
       (
        (tap->min > tap->max && tap->val > tap->trigger_level)
     || (tap->min < tap->max && tap->val < tap->trigger_level)
       )
      ){
      tap->triggered = 1;
      tap->cached_trigger = 1;
    }
    if (tap->anomaly &&
        (tap->cached_trigger || tap->old_anomaly)
       ){
      if (!tap->old_anomaly){
        tap->old_anomaly = tap->anomaly;
        tap->anomaly = NULL;
      }
      *pulse = set_trigger(tap->old_anomaly->pos, &tap->trigger_pos, tap->old_anomaly->rising, tap->trigger_type);
      free(tap->old_anomaly);
      tap->old_anomaly = NULL;
    }
    tap->input_pos++;
  }
  return samples_done;
}

uint32_t tapenc_flush(struct tap_enc_t *tap){
  uint32_t return_value;

  return_value = tap->input_pos - tap->trigger_pos;
  reset_state(tap);
  return return_value;
}

int32_t tapenc_get_max(struct tap_enc_t *tap){
  return tap->max_val;
}

