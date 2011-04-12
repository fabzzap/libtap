/* TAP shared library: a library for converting audio data (as a stream of
 * PCM signed 32-bit samples, mono) to raw TAP data and back
 *
 * TAP specification was written by Per Håkan Sundell and is available at
 * http://www.computerbrains.com/tapformat.html
 *
 * The algorithm to decode TAP data into square, sine or triangular waves
 * is by Fabrizio Gennari.
 *
 * Copyright (c) Fabrizio Gennari, 2003-2011
 *
 * The program is distributed under the GNU Lesser General Public License.
 * See file LESSER-LICENSE.TXT for details.
 */

#include <stddef.h>
#include <malloc.h>
#include <sys/types.h>
#ifdef HAVE_SINE_WAVE
#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#endif /* _MSC_VER*/
#include <math.h>
#endif /*HAVE_SINE_WAVE*/
#include "tapdecoder.h"


struct tap_dec_t{
  enum tap_trigger trigger_type;
  uint32_t first_consumed, second_consumed, first_semiwave, second_semiwave, volume;
  unsigned char negative;
  int32_t (*get_val)(uint32_t to_be_consumed, uint32_t this_pulse_len, uint32_t volume);
};

static int32_t tap_get_triangle_val(uint32_t to_be_consumed, uint32_t this_pulse_len, uint32_t volume){
  int64_t in;

  to_be_consumed = (to_be_consumed < this_pulse_len / 2) ?
    to_be_consumed + 1 :
    this_pulse_len - 1 - to_be_consumed;
  in = ((int64_t)to_be_consumed)*volume*2;
  return (int32_t)(in / this_pulse_len);
}

#ifdef HAVE_SINE_WAVE
static int32_t tap_get_sinewave_val(uint32_t to_be_consumed, uint32_t this_pulse_len, uint32_t volume){
  double angle=(M_PI*(to_be_consumed+1))/this_pulse_len;
  return (int32_t)(volume*sin(angle));
}
#endif

static int32_t tap_get_squarewave_val(uint32_t this_pulse_len, uint32_t to_be_consumed, uint32_t volume){
  return volume;
}

static uint32_t tap_semiwave(struct tap_dec_t *tap, int32_t **buffer, uint32_t *buflen, uint8_t get_first_semiwave){
  uint32_t samples_done = 0;
  uint32_t semiwave_len = get_first_semiwave ? tap->first_semiwave : tap->second_semiwave;
  uint32_t *consumed = get_first_semiwave ? &tap->first_consumed : &tap->second_consumed;

  for(;*buflen > 0 && *consumed < semiwave_len; (*buffer)++, (*buflen)--, (*consumed)++, samples_done++){
    **buffer = tap->get_val(*consumed, semiwave_len, tap->volume);
    if (tap->negative)
      **buffer = ~(**buffer);
  }

  if (*consumed == semiwave_len && samples_done != 0)
    tap->negative = !tap->negative;

  return samples_done;
}  

struct tap_dec_t *tapdec_init(uint32_t volume, enum tap_trigger trigger_type, enum tapdec_waveform waveform){
  struct tap_dec_t *tap;

  tap=malloc(sizeof(struct tap_dec_t));
  if (tap==NULL) return NULL;
  tap->trigger_type=trigger_type;
  tap->volume=volume;
  tap->negative=trigger_type==TAP_TRIGGER_ON_FALLING_EDGE;
  switch (waveform){
  case TAPDEC_TRIANGLE:
  default:
    tap->get_val = tap_get_triangle_val;
    break;
  case TAPDEC_SQUARE:
    tap->get_val = tap_get_squarewave_val;
#ifdef HAVE_SINE_WAVE
  case TAPDEC_SINE:
    tap->get_val = tap_get_sinewave_val;
    break;
#endif
  }
  
  return tap;
}

void tapdec_set_pulse(struct tap_dec_t *tap, uint32_t pulse){
  tap->first_consumed=tap->second_consumed=0;
  if (pulse==1 && tap->trigger_type!=TAP_TRIGGER_ON_BOTH_EDGES)
    pulse=0;
  tap->second_semiwave = 
    tap->trigger_type!=TAP_TRIGGER_ON_BOTH_EDGES ? pulse / 2 : 0;
  tap->first_semiwave=pulse - tap->second_semiwave;
}

uint32_t tapdec_get_buffer(struct tap_dec_t *tap, int32_t *buffer, uint32_t buflen){
  uint32_t samples_done_first = tap_semiwave(tap, &buffer, &buflen, 1);
  uint32_t samples_done_second = tap_semiwave(tap, &buffer, &buflen, 0);

  return samples_done_first + samples_done_second;
}

