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
 * The algorithm to decode TAP data into square, sine or triangular waves
 * is by Fabrizio Gennari.
 *
 * Copyright (c) Fabrizio Gennari, 2003-2011
 *
 * The program is distributed under the GNU Lesser General Public License.
 * See file LESSER-LICENSE.TXT for details.
 */

#ifndef TAP_TYPES_H
#define TAP_TYPES_H

enum tapdec_waveform {
  TAPDEC_TRIANGLE,
  TAPDEC_SQUARE,
  TAPDEC_SINE
};

#endif /* TAP_TYPES_H */
