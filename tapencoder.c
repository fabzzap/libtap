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

struct tap_enc_t{
  uint32_t increasing;
  uint32_t prev_increasing;
  uint32_t input_pos;
  uint32_t min_duration;
  int32_t min_height;
  int32_t trigger_level, anomalous_trigger_level;
  int32_t val, prev_val, max_val, min_val;
  uint32_t max, min, prev_max, prev_min, trigger_pos, anomalous_trigger_pos;
  uint8_t sensitivity;
  uint8_t anomalous_trigger;
  enum tap_trigger trigger_type;
  uint8_t initial_threshold;
  uint8_t triggered;
};

static void reset_state(struct tap_enc_t *tap){
  tap->increasing=0;
  tap->input_pos=0;
  tap->min_height=0;
  tap->val=0;
  tap->max=0;
  tap->min=0;
  tap->prev_max=0;
  tap->prev_min=0;
  tap->trigger_pos=0;
  tap->max_val= (tap->initial_threshold<<24);
  tap->min_val=-(tap->initial_threshold<<24);
  tap->triggered = 1;
  tap->anomalous_trigger_level = 0;
  tap->trigger_level = 0;
  tap->anomalous_trigger = 0;
  tap->anomalous_trigger_pos = 0;
}

static uint32_t set_trigger(uint32_t trigger_pos, uint32_t *stored_trigger_pos, uint8_t *got_pulse) {
  uint32_t return_value;

  *got_pulse = 1;
  return_value = trigger_pos - *stored_trigger_pos;
  *stored_trigger_pos = trigger_pos;
  return return_value;
}

struct tap_enc_t *tapenc_init(uint32_t min_duration, uint8_t sensitivity, uint8_t initial_threshold, enum tap_trigger inverted){
  struct tap_enc_t *tap;

  tap=malloc(sizeof(struct tap_enc_t));
  if (tap==NULL) return NULL;
  tap->min_duration=min_duration;
  tap->initial_threshold = initial_threshold > 127 ? 127 : initial_threshold;
  tap->trigger_type=inverted;
  tap->sensitivity=sensitivity > 100 ? 100 : sensitivity;
  reset_state(tap);
  return tap;
}

uint32_t tapenc_get_pulse(struct tap_enc_t *tap, int32_t *buffer, unsigned int buflen, uint8_t *got_pulse, uint32_t *pulse){
  uint32_t samples_done = 0;

  *got_pulse = 0;

  while(!(*got_pulse) && samples_done < buflen){
    if(tap->anomalous_trigger){
      tap->anomalous_trigger = 0;
      *pulse = set_trigger(tap->input_pos - 1, &tap->trigger_pos, got_pulse);
      break;
    }
    tap->prev_val = tap->val;
    tap->val = buffer[samples_done++];

    tap->prev_increasing = tap->increasing;
    if (tap->val > tap->prev_val)
      tap->increasing = 1;
    else if (tap->val < tap->prev_val)
      tap->increasing = 0;

    if (tap->increasing != tap->prev_increasing && tap->anomalous_trigger_pos == 0) /* A min or max has been reached. Is it a true one? */
    {
      if (tap->increasing
       && tap->input_pos - tap->max > tap->min_duration
       && tap->min_height > tap->prev_val
       && (tap->max>tap->min || tap->min_val > tap->prev_val) ){ /* A minimum */
        if (tap->max >= tap->min) {
          if (tap->triggered == 0) {
            tap->anomalous_trigger_pos = tap->input_pos / 2 + tap->max / 2;
            tap->anomalous_trigger_level = tap->trigger_level;
          }
          tap->triggered = 0;
        }
        tap->trigger_level = tap->max > 0 ?
          tap->prev_val / 2 + tap->max_val / 2
          : 0;
        tap->prev_min = tap->min;
        tap->min = tap->input_pos;
        tap->min_val = tap->prev_val;
        tap->min_height = tap->max>0 ?
        tap->min_val/200*(100+tap->sensitivity) + tap->max_val/200*(100-tap->sensitivity)
            : tap->initial_threshold<<24;
      }
      else if (!tap->increasing
             && tap->input_pos - tap->min > tap->min_duration
             && tap->prev_val > tap->min_height
             && (tap->min>tap->max || tap->prev_val > tap->max_val) ){ /* A maximum */
        if (tap->min >= tap->max) {
          if (tap->triggered == 0) {
            tap->anomalous_trigger_pos = tap->input_pos / 2 + tap->max / 2;
            tap->anomalous_trigger_level = tap->trigger_level;
          }
          tap->triggered = 0;
        }
        tap->trigger_level =  tap->min > 0 ?
          tap->prev_val / 2 + tap->min_val / 2
          : 0;
        tap->prev_max = tap->max;
        tap->max = tap->input_pos;
        tap->max_val = tap->prev_val;
        tap->min_height = tap->min>0 ?
        tap->min_val/200*(100-tap->sensitivity) + tap->max_val/200*(100+tap->sensitivity)
            : -tap->initial_threshold<<24;
      }
    }
    if(!tap->triggered) {
      if (tap->anomalous_trigger_pos > 0
       && tap->min > tap->max
       && tap->val < tap->anomalous_trigger_level){
        tap->anomalous_trigger_pos = 0;
        tap->triggered = 1;
        tap->min = tap->prev_min;
      }
      else if (tap->anomalous_trigger_pos > 0
               && tap->min < tap->max
               && tap->val > tap->anomalous_trigger_level){
        tap->anomalous_trigger_pos = 0;
        tap->triggered = 1;
        tap->max = tap->prev_max;
      }
      else if ((tap->min > tap->max && tap->val > tap->trigger_level)
            || (tap->min < tap->max && tap->val < tap->trigger_level)){
        tap->triggered = 1;
        tap->anomalous_trigger = tap->anomalous_trigger_pos > 0;
      }
      if (tap->triggered){
        uint8_t trigger_causes_pulse =
          (tap->val > tap->trigger_level && tap->trigger_type != TAP_TRIGGER_ON_FALLING_EDGE)
       || (tap->val < tap->trigger_level && tap->trigger_type != TAP_TRIGGER_ON_RISING_EDGE);
        if (trigger_causes_pulse && !tap->anomalous_trigger)
          *pulse = set_trigger(tap->input_pos, &tap->trigger_pos, got_pulse);
        else if(tap->anomalous_trigger &&
                (
                 (tap->val > tap->trigger_level && tap->trigger_type != TAP_TRIGGER_ON_RISING_EDGE)
              || (tap->val < tap->trigger_level && tap->trigger_type != TAP_TRIGGER_ON_FALLING_EDGE)
                )
               )
          *pulse = set_trigger(tap->anomalous_trigger_pos, &tap->trigger_pos, got_pulse);
        tap->anomalous_trigger_pos = 0;
        tap->anomalous_trigger = tap->anomalous_trigger && trigger_causes_pulse;
      }
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

