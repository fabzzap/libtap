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

#include "tap_types.h"

#define TAP_OK          0
#define TAP_END_BUFFER -1
#define TAP_INVALID    -1


struct tap_t;

struct tap_t *tap_fromaudio_init(u_int32_t infreq, u_int32_t min_duration, u_int32_t min_height, int inverted);
struct tap_t *tap_toaudio_init(u_int32_t outfreq, int32_t volume, int inverted);
void tap_set_buffer(struct tap_t *tap, int32_t *buf, int len);
int tap_set_machine(struct tap_t *tap, unsigned char machine, unsigned char videotype);
/*int tap_get_pulse(struct tap_t *tap, struct tap_pulse *pulse);*/
u_int32_t tap_get_pulse(struct tap_t *tap);
int tap_get_pos(struct tap_t *tap);void tap_exit(struct tap_t *tap);
void tap_set_pulse(struct tap_t *tap, u_int32_t pulse);
u_int32_t tap_get_buffer(struct tap_t *tap, int32_t *buffer, unsigned int buflen);
int32_t tap_get_max(struct tap_t *tap);
